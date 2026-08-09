#include "CkInspector_Inventories.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkInventory/Inventory/CkInventory_Utils.h"
#include "CkInventory/Item/CkItem_Utils.h"
#include "CkInventory/Item/CkItem_Definition.h"

#include "CkGrid/2dGridSystem/Grid/Ck2dGridSystem_Utils.h"
#include "CkGrid/2dGridSystem/Cell/Ck2dGridCell_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

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

static constexpr FLinearColor Color_InventoryName = FLinearColor(0.55f, 0.78f, 0.95f);
static constexpr FLinearColor Color_InventoryType = FLinearColor(0.75f, 0.75f, 0.75f);
static constexpr FLinearColor Color_ItemName = FLinearColor(0.85f, 0.75f, 0.55f);
static constexpr FLinearColor Color_DetachedItem = FLinearColor(0.95f, 0.45f, 0.25f);
static constexpr FLinearColor Color_PendingRemovalItem = FLinearColor(0.75f, 0.65f, 0.45f);

// ---- Distinct item colors (tetris-like palette) ----
// 20 visually distinct hues with consistent saturation/brightness. Ordered so that
// adjacent indices are far apart on the color wheel — this helps when the hash-into-palette
// produces near-neighbor collisions for typical small inventories.

static const TArray<FLinearColor> ItemColors =
{
    FLinearColor(0.20f, 0.60f, 0.85f),  // Blue
    FLinearColor(0.85f, 0.45f, 0.20f),  // Orange
    FLinearColor(0.30f, 0.75f, 0.40f),  // Green
    FLinearColor(0.80f, 0.30f, 0.35f),  // Red
    FLinearColor(0.65f, 0.45f, 0.80f),  // Purple
    FLinearColor(0.85f, 0.75f, 0.25f),  // Yellow
    FLinearColor(0.40f, 0.80f, 0.80f),  // Cyan
    FLinearColor(0.85f, 0.50f, 0.65f),  // Pink
    FLinearColor(0.50f, 0.85f, 0.30f),  // Lime
    FLinearColor(0.90f, 0.55f, 0.30f),  // Amber
    FLinearColor(0.35f, 0.50f, 0.90f),  // Indigo
    FLinearColor(0.85f, 0.30f, 0.60f),  // Magenta
    FLinearColor(0.25f, 0.80f, 0.60f),  // Teal
    FLinearColor(0.90f, 0.65f, 0.20f),  // Gold
    FLinearColor(0.55f, 0.75f, 0.95f),  // Sky
    FLinearColor(0.75f, 0.35f, 0.20f),  // Rust
    FLinearColor(0.60f, 0.85f, 0.70f),  // Mint
    FLinearColor(0.80f, 0.60f, 0.90f),  // Lavender
    FLinearColor(0.50f, 0.35f, 0.25f),  // Brown
    FLinearColor(0.95f, 0.85f, 0.60f),  // Cream
};

static constexpr FLinearColor Color_CellEmpty    = FLinearColor(0.12f, 0.12f, 0.12f);
static constexpr FLinearColor Color_CellDisabled = FLinearColor(0.06f, 0.06f, 0.06f);
static constexpr FLinearColor Color_CellBorder   = FLinearColor(0.25f, 0.25f, 0.25f);

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
        const auto HeaderColor = IsSpatial ? Color_InventoryName : Color_InventoryType;

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

        Builder.AddClickableRow(
            FText::FromString(ck::Format_UE(TEXT("  {}"), ItemName)),
            [](const FCk_Handle& E) { return FText::GetEmpty(); },
            Color_ItemName,
            [WeakSelectionModel, ItemEntity]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(ItemEntity))
                { WeakSelectionModel->Set_SelectedEntities({ ItemEntity }); }
            });
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
            Color_PendingRemovalItem,
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
            Color_DetachedItem,
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
    const FCk_Handle_2dGridSystem& InGridHandle,
    const TMap<FCk_Handle, int32>& InItemColorMap) -> TSharedRef<SGridPanel>
{
    const auto Dims = UCk_Utils_2dGridSystem_UE::Get_Dimensions(InGridHandle);
    constexpr auto CellSize = 22.0f;

    auto GridPanel = SNew(SGridPanel);

    for (auto Y = 0; Y < Dims.Y; ++Y)
    {
        for (auto X = 0; X < Dims.X; ++X)
        {
            auto CellHandle = UCk_Utils_2dGridSystem_UE::Get_CellAt(InGridHandle, FIntPoint{X, Y});
            auto CellColor = Color_CellEmpty;
            auto TooltipText = FString::Printf(TEXT("(%d, %d)"), X, Y);
            auto BorderColor = Color_CellBorder;

            if (ck::IsValid(CellHandle))
            {
                if (UCk_Utils_2dGridCell_UE::Get_IsDisabled(CellHandle))
                {
                    CellColor = Color_CellDisabled;
                    BorderColor = Color_CellDisabled;
                    TooltipText += TEXT(" [Disabled]");
                }
                else if (ck::TUtils_InventorySlot_ItemRef::Has(CellHandle))
                {
                    auto StoredItem = ck::TUtils_InventorySlot_ItemRef::Get_StoredEntity(CellHandle);
                    if (ck::IsValid(StoredItem))
                    {
                        const auto* Index = InItemColorMap.Find(StoredItem);
                        const auto ItemIdx = Index ? *Index : 0;
                        CellColor = ItemColors[ItemIdx % ItemColors.Num()];
                        BorderColor = CellColor * 0.5f;
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

static auto BuildItemColorMap(const FCk_Handle_Inventory& InInventory) -> TMap<FCk_Handle, int32>
{
    const auto Items = UCk_Utils_Inventory_UE::Get_Items(InInventory);

    // Derive each item's color index from the handle's hash so the color stays stable across
    // refreshes — moving, adding, or removing other items doesn't recolor existing entries.
    auto ItemColorMap = TMap<FCk_Handle, int32>{};

    for (const auto& ItemHandle : Items)
    {
        if (ck::Is_NOT_Valid(ItemHandle))
        { continue; }

        const auto Hash = GetTypeHash(ItemHandle);
        const auto ColorIndex = static_cast<int32>(Hash % static_cast<uint32>(ItemColors.Num()));
        ItemColorMap.Add(ItemHandle, ColorIndex);
    }

    return ItemColorMap;
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

    const auto Inventory = UCk_Utils_Inventory_UE::CastChecked(MutableHandle);
    const auto ItemColorMap = BuildItemColorMap(Inventory);
    return BuildGridPanel(GridHandle, ItemColorMap);
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
