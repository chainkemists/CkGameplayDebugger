#include "CkInspector_Inventories.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Entity/CkEntity.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkInventory/Inventory/CkInventory_Utils.h"
#include "CkInventory/Inventory/DataOnly/CkInventory_DataOnly_Utils.h"
#include "CkInventory/Item/CkItem_Utils.h"
#include "CkInventory/Item/CkItem_Definition.h"
#include "CkInventory/ItemTrait/Stackable/CkItemTrait_Stackable_Utils.h"

#include "CkGrid/2dGridSystem/Grid/Ck2dGridSystem_Utils.h"
#include "CkGrid/2dGridSystem/Cell/Ck2dGridCell_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Inventories)

FCkInspector_Inventories::~FCkInspector_Inventories()
{
    Close_AllSpatialGridPopups();
}

// =====================================================================================================================

namespace ck_inspector_inventories
{
    // ---- Palette ----
    // This file used to own a private 20-color "tetris" palette plus six loose literals. Every one of them
    // now resolves through CkStyle roles or a documented derivation:
    //
    //   Color_InventoryName -> CkStyle::Info()      spatial inventory header (the tier that owns a grid)
    //   Color_InventoryType -> CkStyle::TextDim()   data-only inventory header (the quieter tier)
    //   Color_ItemName      -> (dropped)            it tinted an always-empty value cell; item rows now
    //                                               carry a stack pill in that cell instead
    //   ItemColors[20]      -> Get_ItemTint()       hash -> HSV, the SAME idiom as
    //                                               SCkDebug_EntityRef's Get_HashTint and the overlay's
    //                                               provider hues, so an occupied grid cell and that item's
    //                                               entity pill land on the same hue — and 256 hues retire
    //                                               the old palette's near-neighbour-collision caveat
    //   Color_CellEmpty     -> CkStyle::Bg3()       lightest background tier: an empty, usable cell
    //   Color_CellDisabled  -> CkStyle::Bg1()       a tier darker: an unusable cell (border matches fill)
    //   Color_CellBorder    -> CkStyle::Border()
    //   Color_DetachedItem       -> CkStyle::Error() a lifetime child that is NOT an inventory member
    //   Color_PendingRemovalItem -> CkStyle::Warn()  an item mid-destruction, still listed for diagnosis
    //
    // Occupied-cell borders stay a derivation of the item tint, held at half brightness.
    constexpr auto ItemBorderDim = 0.5f;

    auto Get_ItemTint(
        const FCk_Handle& InItem)
        -> FLinearColor
    {
        const auto Id  = static_cast<uint32>(InItem.Get_Entity().Get_ID());
        const auto Hue = static_cast<uint8>(GetTypeHash(Id) % 256);
        return FLinearColor::MakeFromHSV8(Hue, 150, 205);
    }

    // ---- Spatial cell occupancy ----

    struct FCellOccupancy
    {
        int32 Occupied = 0;
        int32 Active   = 0;
    };

    // Occupancy is an O(W*H) walk, so the meter's fraction and its value text share one ROW-OWNED cache
    // refreshed at most once per engine frame (Slate evaluates a row's attributes several times per frame:
    // desired-size pass, then paint). The cache dies with the row — nothing for OnDeactivated to release.
    struct FCellOccupancyCache
    {
        FCellOccupancy Value;
        uint64         Frame = TNumericLimits<uint64>::Max();
    };

    auto Get_CellOccupancy(
        const FCk_Handle_Inventory_Spatial& InInventory,
        const TSharedRef<FCellOccupancyCache>& InCache)
        -> FCellOccupancy
    {
        if (InCache->Frame == GFrameCounter)
        { return InCache->Value; }

        InCache->Frame = GFrameCounter;
        InCache->Value = FCellOccupancy{};

        if (ck::Is_NOT_Valid(InInventory))
        { return InCache->Value; }

        const auto GridHandle = UCk_Utils_Inventory_Spatial_UE::Get_Grid(InInventory);

        if (ck::Is_NOT_Valid(GridHandle))
        { return InCache->Value; }

        UCk_Utils_2dGridSystem_UE::ForEach_Cell(GridHandle, ECk_2dGridSystem_CellFilter::OnlyActiveCells,
            [&InCache](FCk_Handle_2dGridCell InCell)
            {
                ++InCache->Value.Active;

                if (ck::IsValid(ck::TUtils_InventorySlot_ItemRef::Get_StoredEntity(InCell)))
                { ++InCache->Value.Occupied; }
            });

        return InCache->Value;
    }

    // ---- Item stack pill ----
    // Only stackable items get a pill — a "x1" on every unique item would be noise. Full stacks read Warn
    // because that is the actionable fact when debugging a failed insert; partial stacks read Info.

    auto Get_StackPillText(
        const FCk_Handle_Item& InItem)
        -> FText
    {
        if (ck::Is_NOT_Valid(InItem))
        { return FText::GetEmpty(); }

        const auto Count = UCk_Utils_ItemTrait_Stackable_UE::Get_StackCount(InItem);

        return UCk_Utils_ItemTrait_Stackable_UE::Get_HasMaxStackSize(InItem)
            ? FText::FromString(ck::Format_UE(TEXT("{} / {}"), Count, UCk_Utils_ItemTrait_Stackable_UE::Get_MaxStackSize(InItem)))
            : FText::FromString(ck::Format_UE(TEXT("\u00D7{}"), Count));
    }

    auto Get_StackPillTone(
        const FCk_Handle_Item& InItem)
        -> ECk_Tone
    {
        if (ck::Is_NOT_Valid(InItem))
        { return ECk_Tone::Neutral; }

        return UCk_Utils_ItemTrait_Stackable_UE::Get_IsStackFull(InItem) ? ECk_Tone::Warn : ECk_Tone::Info;
    }
}

// =====================================================================================================================

namespace ck_inspector_inventories
{
    auto Get_DetachedLifetimeOwnedItems(const FCk_Handle& InInventory) -> TArray<FCk_Handle>
    {
        if (ck::Is_NOT_Valid(InInventory))
        { return {}; }

        auto MutableInventory = InInventory;
        const auto Inventory = UCk_Utils_Inventory_UE::Cast(MutableInventory);
        if (ck::Is_NOT_Valid(Inventory))
        { return {}; }

        auto DetachedItems = TArray<FCk_Handle>{};
        for (const auto& Dependent : UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InInventory))
        {
            if (ck::Is_NOT_Valid(Dependent))
            { continue; }

            if (UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Dependent) != InInventory)
            { continue; }

            auto MutableDependent = Dependent;
            const auto Item = UCk_Utils_Item_UE::Cast(MutableDependent);
            if (ck::Is_NOT_Valid(Item) || UCk_Utils_Inventory_UE::Get_ContainsItem(Inventory, Item))
            { continue; }

            if (UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
                Dependent,
                ECk_EntityLifetime_DestructionPhase::BeginDestroy))
            { continue; }

            DetachedItems.Add(Dependent);
        }

        return DetachedItems;
    }

    auto Get_PendingRemovalLifetimeOwnedItems(const FCk_Handle& InInventory) -> TArray<FCk_Handle>
    {
        if (ck::Is_NOT_Valid(InInventory))
        { return {}; }

        auto MutableInventory = InInventory;
        const auto Inventory = UCk_Utils_Inventory_UE::Cast(MutableInventory);
        if (ck::Is_NOT_Valid(Inventory))
        { return {}; }

        auto PendingItems = TArray<FCk_Handle>{};
        for (const auto& Dependent : UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InInventory))
        {
            if (ck::Is_NOT_Valid(Dependent))
            { continue; }

            if (UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Dependent) != InInventory)
            { continue; }

            auto MutableDependent = Dependent;
            const auto Item = UCk_Utils_Item_UE::Cast(MutableDependent);
            if (ck::Is_NOT_Valid(Item) || UCk_Utils_Inventory_UE::Get_ContainsItem(Inventory, Item))
            { continue; }

            if (UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
                Dependent,
                ECk_EntityLifetime_DestructionPhase::BeginDestroy))
            { PendingItems.Add(Dependent); }
        }

        return PendingItems;
    }

    auto Resolve_InspectableInventories(const FCk_Handle& InEntity) -> TArray<FCk_Handle_Inventory>
    {
        if (ck::Is_NOT_Valid(InEntity))
        { return {}; }

        auto MutableEntity = InEntity;
        const auto DirectInventory = UCk_Utils_Inventory_UE::Cast(MutableEntity);
        if (ck::IsValid(DirectInventory))
        { return { DirectInventory }; }

        return UCk_Utils_Inventory_UE::RecordOfInventories_Utils::Get_ValidEntries(MutableEntity);
    }

    auto Gather_StructureHandles(const TArray<FCk_Handle_Inventory>& InInventories) -> TArray<FCk_Handle>
    {
        auto Handles = TArray<FCk_Handle>{};
        for (const auto& Inventory : InInventories)
        {
            if (ck::Is_NOT_Valid(Inventory))
            { continue; }

            Handles.Add(Inventory);
            for (const auto& Item : UCk_Utils_Inventory_UE::Get_Items(Inventory))
            { Handles.Add(Item); }

            Handles.Append(Get_DetachedLifetimeOwnedItems(Inventory));
            Handles.Append(Get_PendingRemovalLifetimeOwnedItems(Inventory));
        }

        return Handles;
    }

    auto Format_ItemSummary(const FCk_Handle_Inventory& InInventory) -> FText
    {
        if (ck::Is_NOT_Valid(InInventory))
        { return FText::GetEmpty(); }

        const auto NumMembers = UCk_Utils_Inventory_UE::Get_NumItems(InInventory);
        const auto NumDetached = Get_DetachedLifetimeOwnedItems(InInventory).Num();
        const auto NumPendingRemoval = Get_PendingRemovalLifetimeOwnedItems(InInventory).Num();
        if (NumDetached == 0 && NumPendingRemoval == 0)
        { return FText::FromString(ck::Format_UE(TEXT("{} items"), NumMembers)); }

        const auto MemberLabel = NumMembers == 1 ? TEXT("member") : TEXT("members");
        auto Summary = ck::Format_UE(TEXT("{} {}"), NumMembers, MemberLabel);
        if (NumPendingRemoval > 0)
        {
            const auto PendingLabel = NumPendingRemoval == 1 ? TEXT("item") : TEXT("items");
            Summary += ck::Format_UE(TEXT(" + {} {} pending removal"), NumPendingRemoval, PendingLabel);
        }
        if (NumDetached > 0)
        {
            const auto ChildLabel = NumDetached == 1 ? TEXT("child") : TEXT("children");
            Summary += ck::Format_UE(TEXT(" + {} detached lifetime {}"), NumDetached, ChildLabel);
        }
        return FText::FromString(Summary);
    }
}

// =====================================================================================================================

auto FCkInspector_Inventories::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Inventories"));
}

auto FCkInspector_Inventories::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return NOT ck_inspector_inventories::Resolve_InspectableInventories(Entity).IsEmpty();
}

auto FCkInspector_Inventories::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildInventoryGrid(Entity, FString());
}

auto FCkInspector_Inventories::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return BuildInventoryGrid(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_Inventories::BuildInventoryGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Host = TSharedPtr<SVerticalBox>{};
    SAssignNew(Host, SVerticalBox);

    _ViewStates.RemoveAll([&Entity](const FInventoryViewState& InState)
    {
        return InState.Entity == Entity;
    });

    auto& ViewState = _ViewStates.AddDefaulted_GetRef();
    ViewState.Entity = Entity;
    ViewState.ActiveFilter = InFilter;
    ViewState.InventoryGridHost = Host;
    PopulateInventoryGrid(ViewState);
    return Host.ToSharedRef();
}

auto FCkInspector_Inventories::PopulateInventoryGrid(
    FInventoryViewState& InViewState) -> void
{
    const auto Host = InViewState.InventoryGridHost.Pin();
    if (NOT Host.IsValid())
    { return; }

    Host->ClearChildren();
    InViewState.InventoryItemRowsHosts.Reset();

    const auto Inventories = ck_inspector_inventories::Resolve_InspectableInventories(InViewState.Entity);

    InViewState.CachedInventoryHandles.Reset();
    for (const auto& Inventory : Inventories)
    { InViewState.CachedInventoryHandles.Add(Inventory); }
    InViewState.CachedStructureHandles = ck_inspector_inventories::Gather_StructureHandles(Inventories);

    if (Inventories.IsEmpty())
    {
        Host->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("No inventories")))
                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
            ];
        return;
    }

    for (const auto& InventoryHandle : Inventories)
    {
        if (ck::Is_NOT_Valid(InventoryHandle)) { continue; }

        auto Builder = FCkInspectorWidgetBuilder();
        auto WeakSelectionModel = SelectionModel;
        const auto Inventory = UCk_Utils_Inventory_UE::CastChecked(InventoryHandle);

        const auto InventoryType = UCk_Utils_Inventory_UE::Get_InventoryType(Inventory);
        const auto IsSpatial = InventoryType == ECk_InventoryType::Spatial;
        const auto CapturedInventory = Inventory;

        const auto TypeStr     = IsSpatial ? TEXT("Spatial") : TEXT("DataOnly");
        const auto HeaderLabel = FText::FromString(ck::Format_UE(TEXT("{} ({})"), InventoryHandle.ToString(), TypeStr));
        const auto HeaderColor = IsSpatial ? CkStyle::Info() : CkStyle::TextDim();

        const auto SelectInventoryClick = [WeakSelectionModel, InventoryHandle]()
        {
            if (WeakSelectionModel.IsValid() && ck::IsValid(InventoryHandle))
            {
                WeakSelectionModel->Set_SelectedEntities({ InventoryHandle });
            }
        };

        // For spatial inventories, build a combined value widget [N items] + [View Tilemap (W×H)] button
        // so the tilemap entry point lives inline with the inventory header instead of in its own row.
        if (IsSpatial)
        {
            auto MutableInvHandle = InventoryHandle;
            auto SpatialHandle = UCk_Utils_Inventory_Spatial_UE::Cast(MutableInvHandle);
            const auto Dims = ck::IsValid(SpatialHandle)
                ? UCk_Utils_2dGridSystem_UE::Get_Dimensions(UCk_Utils_Inventory_Spatial_UE::Get_Grid(SpatialHandle))
                : FIntPoint::ZeroValue;

            const auto ButtonLabel = FText::FromString(
                ck::Format_UE(TEXT("View Tilemap ({}\u00D7{})"), Dims.X, Dims.Y));

            auto ValueRow = SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
                [
                    SNew(STextBlock)
                    .Text_Lambda([CapturedInventory]() -> FText
                    {
                        if (ck::Is_NOT_Valid(CapturedInventory))
                        { return FText::GetEmpty(); }

                        return ck_inspector_inventories::Format_ItemSummary(CapturedInventory);
                    })
                    .ColorAndOpacity(FSlateColor(HeaderColor))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .ToolTipText(FText::FromString(TEXT(
                        "Open a separate, scrollable window showing the spatial grid for this inventory.")))
                    .OnClicked_Lambda([this, InventoryHandle]() -> FReply
                    {
                        OpenOrFocus_SpatialGridPopup(InventoryHandle);
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .Text(ButtonLabel)
                        .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]
                ];

            Builder.AddClickableWidgetRow(HeaderLabel, ValueRow, SelectInventoryClick);

            // Cell occupancy is the bounded quantity a spatial inventory actually has — item COUNT says
            // nothing about how full the grid is once footprints differ.
            if (ck::IsValid(SpatialHandle))
            {
                const auto CapturedSpatial = SpatialHandle;
                const auto Cache = MakeShared<ck_inspector_inventories::FCellOccupancyCache>();

                Builder.AddMeterRow(
                    FText::FromString(TEXT("  Cells:")),
                    TAttribute<float>::CreateLambda([CapturedSpatial, Cache]() -> float
                    {
                        const auto Occupancy = ck_inspector_inventories::Get_CellOccupancy(CapturedSpatial, Cache);
                        return Occupancy.Active > 0
                            ? static_cast<float>(Occupancy.Occupied) / static_cast<float>(Occupancy.Active)
                            : 0.0f;
                    }),
                    ECk_Tone::Accent,
                    TAttribute<FText>::CreateLambda([CapturedSpatial, Cache]() -> FText
                    {
                        const auto Occupancy = ck_inspector_inventories::Get_CellOccupancy(CapturedSpatial, Cache);
                        return FText::FromString(ck::Format_UE(TEXT("{} / {}"), Occupancy.Occupied, Occupancy.Active));
                    }));
            }
        }
        else
        {
            Builder.AddClickableRow(
                HeaderLabel,
                [CapturedInventory](const FCk_Handle& E)
                {
                    return ck_inspector_inventories::Format_ItemSummary(CapturedInventory);
                },
                HeaderColor,
                SelectInventoryClick);

            // A data-only inventory is only meter-able when it declares a bound; unbounded ones keep the
            // plain count above. Boundedness is config, so the choice is made once at compose time.
            auto MutableInvHandle = InventoryHandle;
            const auto DataOnlyHandle = UCk_Utils_Inventory_DataOnly_UE::Cast(MutableInvHandle);
            const auto BoundMax = ck::IsValid(DataOnlyHandle)
                ? UCk_Utils_Inventory_DataOnly_UE::Get_BoundMax(DataOnlyHandle)
                : TOptional<int32>{};

            if (BoundMax.IsSet() && BoundMax.GetValue() > 0)
            {
                const auto Bound = BoundMax.GetValue();

                Builder.AddMeterRow(
                    FText::FromString(TEXT("  Capacity:")),
                    TAttribute<float>::CreateLambda([CapturedInventory, Bound]() -> float
                    {
                        if (ck::Is_NOT_Valid(CapturedInventory)) { return 0.0f; }
                        return static_cast<float>(UCk_Utils_Inventory_UE::Get_NumItems(CapturedInventory)) / static_cast<float>(Bound);
                    }),
                    ECk_Tone::Accent,
                    TAttribute<FText>::CreateLambda([CapturedInventory, Bound]() -> FText
                    {
                        if (ck::Is_NOT_Valid(CapturedInventory)) { return FText::FromString(TEXT("--")); }
                        return FText::FromString(ck::Format_UE(TEXT("{} / {}"),
                            UCk_Utils_Inventory_UE::Get_NumItems(CapturedInventory), Bound));
                    }));
            }
        }

        Host->AddSlot()
            .AutoHeight()
            [
                Builder.Build(InViewState.Entity, InViewState.ActiveFilter)
            ];

        auto ItemRowsHost = TSharedPtr<SVerticalBox>{};
        SAssignNew(ItemRowsHost, SVerticalBox);
        PopulateInventoryItemRows(*ItemRowsHost, Inventory, InViewState.ActiveFilter);

        Host->AddSlot()
            .AutoHeight()
            [
                ItemRowsHost.ToSharedRef()
            ];

        InViewState.InventoryItemRowsHosts.Add(FInventoryItemRowsHost{Inventory, ItemRowsHost});
    }
}

auto FCkInspector_Inventories::PopulateInventoryItemRows(
    SVerticalBox& InHost,
    const FCk_Handle& InInventory,
    const FString& InFilter) -> void
{
    InHost.ClearChildren();

    if (ck::Is_NOT_Valid(InInventory))
    { return; }

    auto MutableInventory = InInventory;
    const auto Inventory = UCk_Utils_Inventory_UE::Cast(MutableInventory);
    if (ck::Is_NOT_Valid(Inventory))
    { return; }

    auto Builder = FCkInspectorWidgetBuilder();
    auto WeakSelectionModel = SelectionModel;

    for (const auto& ItemHandle : UCk_Utils_Inventory_UE::Get_Items(Inventory))
    {
        if (ck::Is_NOT_Valid(ItemHandle)) { continue; }

        const auto* Definition = UCk_Utils_Item_UE::Get_Definition(ItemHandle);
        const auto ItemName = Definition != nullptr
            ? Definition->Get_CoreInfo().Get_Name().ToString()
            : ItemHandle.ToString();
        const auto ItemEntity = FCk_Handle{ItemHandle};
        const auto ItemLabel  = FText::FromString(ck::Format_UE(TEXT("  {}"), ItemName));

        const auto SelectItemClick = [WeakSelectionModel, ItemEntity]()
        {
            if (WeakSelectionModel.IsValid() && ck::IsValid(ItemEntity))
            {
                WeakSelectionModel->Set_SelectedEntities({ ItemEntity });
            }
        };

        if (UCk_Utils_ItemTrait_Stackable_UE::Get_IsStackable(ItemHandle))
        {
            const auto CapturedItem = ItemHandle;

            Builder.AddClickableWidgetRow(
                ItemLabel,
                SNew(SCkDebug_StatusPill)
                    .Text_Lambda([CapturedItem]() { return ck_inspector_inventories::Get_StackPillText(CapturedItem); })
                    .Tone_Lambda([CapturedItem]() { return ck_inspector_inventories::Get_StackPillTone(CapturedItem); })
                    .ShowDot(false),
                SelectItemClick);
        }
        else
        {
            Builder.AddClickableRow(
                ItemLabel,
                [](const FCk_Handle& E) { return FText::GetEmpty(); },
                CkStyle::Text(),
                SelectItemClick);
        }
    }

    for (const auto& PendingItemEntity : ck_inspector_inventories::Get_PendingRemovalLifetimeOwnedItems(Inventory))
    {
        auto MutablePendingItem = PendingItemEntity;
        const auto PendingItem = UCk_Utils_Item_UE::Cast(MutablePendingItem);
        if (ck::Is_NOT_Valid(PendingItem))
        { continue; }

        const auto* Definition = UCk_Utils_Item_UE::Get_Definition(PendingItem);
        const auto ItemName = Definition != nullptr
            ? Definition->Get_CoreInfo().Get_Name().ToString()
            : PendingItemEntity.ToString();

        Builder.AddClickableRow(
            FText::FromString(ck::Format_UE(TEXT("  [Pending removal] {}"), ItemName)),
            [](const FCk_Handle& E)
            {
                return FText::FromString(TEXT("entity is being destroyed"));
            },
            CkStyle::Warn(),
            [WeakSelectionModel, PendingItemEntity]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(PendingItemEntity))
                { WeakSelectionModel->Set_SelectedEntities({ PendingItemEntity }); }
            });
    }

    for (const auto& DetachedItemEntity : ck_inspector_inventories::Get_DetachedLifetimeOwnedItems(Inventory))
    {
        auto MutableDetachedItem = DetachedItemEntity;
        const auto DetachedItem = UCk_Utils_Item_UE::Cast(MutableDetachedItem);
        if (ck::Is_NOT_Valid(DetachedItem))
        { continue; }

        const auto* Definition = UCk_Utils_Item_UE::Get_Definition(DetachedItem);
        const auto ItemName = Definition != nullptr
            ? Definition->Get_CoreInfo().Get_Name().ToString()
            : DetachedItemEntity.ToString();

        Builder.AddClickableRow(
            FText::FromString(ck::Format_UE(TEXT("  [Detached lifetime child] {}"), ItemName)),
            [](const FCk_Handle& E)
            {
                return FText::FromString(TEXT("not an inventory member"));
            },
            CkStyle::Error(),
            [WeakSelectionModel, DetachedItemEntity]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(DetachedItemEntity))
                { WeakSelectionModel->Set_SelectedEntities({ DetachedItemEntity }); }
            });
    }

    InHost.AddSlot()
        .AutoHeight()
        [
            Builder.Build(InInventory, InFilter)
        ];
}

// =====================================================================================================================

static auto BuildGridPanel(
    const FCk_Handle_2dGridSystem& InGridHandle) -> TSharedRef<SGridPanel>
{
    const auto Dims = UCk_Utils_2dGridSystem_UE::Get_Dimensions(InGridHandle);
    constexpr auto CellSize = 22.0f;

    auto GridPanel = SNew(SGridPanel);

    for (auto Y = 0; Y < Dims.Y; ++Y)
    {
        for (auto X = 0; X < Dims.X; ++X)
        {
            auto CellHandle = UCk_Utils_2dGridSystem_UE::Get_CellAt(InGridHandle, FIntPoint{X, Y});
            auto CellColor = CkStyle::Bg3();
            auto TooltipText = FString::Printf(TEXT("(%d, %d)"), X, Y);
            auto BorderColor = CkStyle::Border();

            if (ck::IsValid(CellHandle))
            {
                if (UCk_Utils_2dGridCell_UE::Get_IsDisabled(CellHandle))
                {
                    CellColor = CkStyle::Bg1();
                    BorderColor = CkStyle::Bg1();
                    TooltipText += TEXT(" [Disabled]");
                }
                else if (ck::TUtils_InventorySlot_ItemRef::Has(CellHandle))
                {
                    auto StoredItem = ck::TUtils_InventorySlot_ItemRef::Get_StoredEntity(CellHandle);
                    if (ck::IsValid(StoredItem))
                    {
                        CellColor = ck_inspector_inventories::Get_ItemTint(StoredItem);
                        BorderColor = CellColor * ck_inspector_inventories::ItemBorderDim;
                        BorderColor.A = 1.0f;

                        if (const auto* Def = UCk_Utils_Item_UE::Get_Definition(StoredItem))
                        {
                            TooltipText += TEXT("\n") + Def->Get_CoreInfo().Get_Name().ToString();
                        }
                    }
                }
            }

            GridPanel->AddSlot(X, Y)
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                .BorderBackgroundColor(BorderColor)
                .Padding(1.0f)
                .ToolTipText(FText::FromString(TooltipText))
                [
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                    .BorderBackgroundColor(CellColor)
                    [
                        SNew(SBox)
                        .WidthOverride(CellSize)
                        .HeightOverride(CellSize)
                    ]
                ]
            ];
        }
    }

    return GridPanel;
}

// =====================================================================================================================

auto FCkInspector_Inventories::Build_SpatialGridContent(const FCk_Handle& InInventoryHandle) -> TSharedRef<SWidget>
{
    auto MutableHandle = InInventoryHandle;
    auto SpatialHandle = UCk_Utils_Inventory_Spatial_UE::Cast(MutableHandle);
    if (ck::Is_NOT_Valid(SpatialHandle))
    { return SNullWidget::NullWidget; }

    auto GridHandle = UCk_Utils_Inventory_Spatial_UE::Get_Grid(SpatialHandle);
    if (ck::Is_NOT_Valid(GridHandle))
    { return SNullWidget::NullWidget; }

    // Each occupied cell takes its tint straight from the stored item's handle hash, so the color is stable
    // across refreshes without a precomputed map — moving or removing other items never recolors an entry.
    return BuildGridPanel(GridHandle);
}

// =====================================================================================================================

auto FCkInspector_Inventories::OpenOrFocus_SpatialGridPopup(const FCk_Handle& InInventoryHandle) -> void
{
    if (ck::Is_NOT_Valid(InInventoryHandle))
    { return; }

    // Reuse an existing popup if it's still alive — bring it to front instead of opening a duplicate.
    for (auto& Existing : _SpatialGridPopups)
    {
        if (Existing.InventoryHandle != InInventoryHandle)
        { continue; }

        if (auto ExistingWindow = Existing.Window.Pin())
        {
            ExistingWindow->BringToFront();
            ExistingWindow->FlashWindow();
            return;
        }
    }

    auto MutableHandle = InInventoryHandle;
    auto SpatialHandle = UCk_Utils_Inventory_Spatial_UE::Cast(MutableHandle);
    if (ck::Is_NOT_Valid(SpatialHandle))
    { return; }

    auto GridHandle = UCk_Utils_Inventory_Spatial_UE::Get_Grid(SpatialHandle);
    if (ck::Is_NOT_Valid(GridHandle))
    { return; }

    const auto Dims = UCk_Utils_2dGridSystem_UE::Get_Dimensions(GridHandle);
    const auto WindowTitle = FText::FromString(
        ck::Format_UE(TEXT("Spatial Grid {} ({}\u00D7{})"), InInventoryHandle.ToString(), Dims.X, Dims.Y));

    auto GridContent = Build_SpatialGridContent(InInventoryHandle);

    // Host SBox lets us swap the grid panel in place when the inventory contents change in Tick.
    auto GridHost = SNew(SBox)
        .Padding(FMargin(4.0f, 2.0f))
        [
            GridContent
        ];

    constexpr auto InitialClientWidth  = 600.0f;
    constexpr auto InitialClientHeight = 500.0f;

    auto Window = SNew(SWindow)
        .Title(WindowTitle)
        .ClientSize(FVector2D(InitialClientWidth, InitialClientHeight))
        .SizingRule(ESizingRule::UserSized)
        .SupportsMaximize(true)
        .SupportsMinimize(false)
        .IsTopmostWindow(false)
        .HasCloseButton(true)
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                // Nested scroll boxes give both horizontal and vertical scrolling for very large grids.
                SNew(SScrollBox)
                .Orientation(Orient_Horizontal)
                + SScrollBox::Slot()
                [
                    SNew(SScrollBox)
                    .Orientation(Orient_Vertical)
                    + SScrollBox::Slot()
                    [
                        GridHost
                    ]
                ]
            ]
        ];

    // Track the popup so Tick can refresh it and OnDeactivated can close it.
    auto PopupEntry = FSpatialGridPopup{};
    PopupEntry.InventoryHandle = InInventoryHandle;
    PopupEntry.Window          = Window;
    PopupEntry.GridHost        = GridHost;
    _SpatialGridPopups.Add(PopupEntry);

    // Drop the entry from our tracking when the user closes the window.
    Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda(
        [this, InInventoryHandle](const TSharedRef<SWindow>&)
        {
            _SpatialGridPopups.RemoveAll([&](const FSpatialGridPopup& InPopup)
            {
                return InPopup.InventoryHandle == InInventoryHandle;
            });
        }));

    constexpr auto ShowWindowImmediately = true;
    FSlateApplication::Get().AddWindow(Window, ShowWindowImmediately);
}

// =====================================================================================================================

auto FCkInspector_Inventories::RefreshSpatialGridPopup(const FCk_Handle& InInventoryHandle) -> void
{
    for (auto& Popup : _SpatialGridPopups)
    {
        if (Popup.InventoryHandle != InInventoryHandle)
        { continue; }

        const auto GridHost = Popup.GridHost.Pin();
        if (NOT GridHost.IsValid())
        { continue; }

        if (NOT Popup.Window.IsValid())
        { continue; }

        auto NewContent = Build_SpatialGridContent(InInventoryHandle);
        GridHost->SetContent(NewContent);
        break;
    }
}

// =====================================================================================================================

auto FCkInspector_Inventories::Close_AllSpatialGridPopups() -> void
{
    // Take a copy because RequestDestroyWindow triggers OnWindowClosed which mutates _SpatialGridPopups.
    auto Snapshot = _SpatialGridPopups;
    _SpatialGridPopups.Reset();

    if (NOT FSlateApplication::IsInitialized())
    { return; }

    auto& SlateApp = FSlateApplication::Get();

    for (auto& Popup : Snapshot)
    {
        if (auto Window = Popup.Window.Pin())
        {
            SlateApp.RequestDestroyWindow(Window.ToSharedRef());
        }
    }
}

// =====================================================================================================================

auto FCkInspector_Inventories::OnDeactivated() -> void
{
    Close_AllSpatialGridPopups();
    _ViewStates.Reset();
}

// =====================================================================================================================

auto FCkInspector_Inventories::Wants_TickWhenNotInspectable(const FCk_Handle& Entity) const -> bool
{
    return _ViewStates.ContainsByPredicate([&Entity](const FInventoryViewState& InState)
    {
        return InState.Entity == Entity && InState.InventoryGridHost.IsValid();
    });
}

// =====================================================================================================================

auto FCkInspector_Inventories::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::Is_NOT_Valid(Entity)) { return; }

    auto* ViewState = _ViewStates.FindByPredicate([&Entity](const FInventoryViewState& InState)
    {
        return InState.Entity == Entity && InState.InventoryGridHost.IsValid();
    });
    if (ViewState == nullptr)
    { return; }

    // Drop tracked popups whose window has been destroyed externally (e.g. user clicked X) and we
    // missed the OnWindowClosed callback (defensive — usually the callback already cleaned up).
    _SpatialGridPopups.RemoveAll([](const FSpatialGridPopup& InPopup)
    {
        return NOT InPopup.Window.IsValid();
    });

    const auto Inventories = ck_inspector_inventories::Resolve_InspectableInventories(Entity);

    auto InventoryHandles = TArray<FCk_Handle>{};
    for (const auto& Inventory : Inventories)
    { InventoryHandles.Add(Inventory); }

    if (ViewState->CachedInventoryHandles != InventoryHandles)
    {
        PopulateInventoryGrid(*ViewState);
        return;
    }

    const auto StructureHandles = ck_inspector_inventories::Gather_StructureHandles(Inventories);

    if (ViewState->CachedStructureHandles != StructureHandles)
    {
        ViewState->CachedStructureHandles = StructureHandles;

        for (const auto& ItemRowsHost : ViewState->InventoryItemRowsHosts)
        {
            if (const auto Host = ItemRowsHost.Host.Pin())
            { PopulateInventoryItemRows(*Host, ItemRowsHost.Inventory, ViewState->ActiveFilter); }
        }

        // ---- Refresh open popups in-place ----

        for (const auto& InventoryHandle : Inventories)
        {
            if (ck::Is_NOT_Valid(InventoryHandle)) { continue; }
            if (UCk_Utils_Inventory_UE::Get_IsSpatial(UCk_Utils_Inventory_UE::CastChecked(InventoryHandle)))
            {
                RefreshSpatialGridPopup(InventoryHandle);
            }
        }
    }
}
