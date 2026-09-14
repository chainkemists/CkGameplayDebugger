#include "CkInspector_Inventories.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Entity/CkEntity.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkInventory/Inventory/CkInventory_Utils.h"
#include "CkInventory/Inventory/DataOnly/CkInventory_DataOnly_Utils.h"
#include "CkInventory/Item/CkItem_Utils.h"
#include "CkInventory/Item/CkItem_Definition.h"
#include "CkInventory/ItemTrait/Stackable/CkItemTrait_Stackable_Utils.h"
#include "CkInventory/ItemTrait/Tags/CkItemTrait_Tags_Utils.h"

#include "CkGrid/2dGridSystem/Grid/Ck2dGridSystem_Utils.h"
#include "CkGrid/2dGridSystem/Cell/Ck2dGridCell_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Inventories)

FCkInspector_Inventories::~FCkInspector_Inventories()
{
    OnDeactivated();
}

// =====================================================================================================================

namespace ck_inspector_inventories
{
    auto Make_RecordIdentity(const FCk_Handle& InHandle) -> FString
    {
        const auto& Entity = InHandle.Get_Entity();
        return FString::Printf(TEXT("%u:%u"), static_cast<uint32>(Entity.Get_ID()),
            static_cast<uint32>(Entity.Get_VersionNumber()));
    }

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
    //   Color_DetachedItem       -> CkStyle::Err()   a lifetime child that is NOT an inventory member
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
    auto Is_Destroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity)
            || InEntity.Has_Any<
                ck::FTag_DestroyEntity_Initiate,
                ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown,
                ck::FTag_DestroyEntity_Await,
                ck::FTag_DestroyEntity_Finalize>();
    }

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
        if (Is_Destroying(InEntity))
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

    auto UiText(const FString& InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)}; }

    auto UiBool(const bool InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Bool, .Bool = InValue}; }

    auto UiInteger(const int32 InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Integer, .Integer = InValue}; }

    auto UiNumber(const float InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Number, .Number = InValue}; }

    auto UiColor(const FLinearColor& InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Color, .Color = InValue}; }
}

// =====================================================================================================================

auto SCkInspector_InventoriesAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _Filter = InArgs._Filter;
    _SelectionModel = InArgs._SelectionModel;
    _DiffLabels = InArgs._DiffLabels;
    _OpenSpatialGrid = InArgs._OpenSpatialGrid;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_InventoriesAuthored::~SCkInspector_InventoriesAuthored()
{
    Release();
}

auto SCkInspector_InventoriesAuthored::Get_IsAvailable() const -> bool
{
    return _Active && NOT ck_inspector_inventories::Resolve_InspectableInventories(_Entity).IsEmpty();
}

auto SCkInspector_InventoriesAuthored::Get_CanRequest() const -> bool
{
    return Get_IsAvailable()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled;
}

auto SCkInspector_InventoriesAuthored::Get_RequestDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable()) { return TEXT("Inventory is unavailable."); }
    return ck::DebugRequestGate::Evaluate(
        _Entity, ECk_DebugRequest_Requirement::AuthorityOnly).Reason.ToString();
}

auto SCkInspector_InventoriesAuthored::Refresh_Records() -> bool
{
    if (NOT _Active || NOT _Records.IsValid()) { return false; }
    const auto Inventories = ck_inspector_inventories::Resolve_InspectableInventories(_Entity);
    auto Records = TArray<FCkUiRecordData>{};
    auto Routes = TMap<FString, FRoute>{};
    auto LiveItemKeys = TSet<FString>{};

    const auto AddField = [](FCkUiRecordData& Record, const TCHAR* Key, FCkUiFieldValue Value)
    { Record.Fields.Add(Key, MoveTemp(Value)); };
    const auto MakeBaseRecord = [&AddField](const FString& Key) -> FCkUiRecordData
    {
        FCkUiRecordData Record;
        Record.Key = Key;
        for (const TCHAR* Field : {TEXT("label"), TEXT("summary"), TEXT("grid-label"), TEXT("grid-tooltip"),
            TEXT("meter-label"), TEXT("meter-value"), TEXT("consume-label"), TEXT("consume-tooltip"),
            TEXT("tag-value"), TEXT("add-label"), TEXT("remove-label"), TEXT("tag-tooltip")})
        { AddField(Record, Field, ck_inspector_inventories::UiText({})); }
        for (const TCHAR* Field : {TEXT("inventory-visible"), TEXT("grid-visible"), TEXT("meter-visible"),
            TEXT("bound-visible"), TEXT("item-visible"), TEXT("stack-visible"), TEXT("consume-visible"),
            TEXT("tags-visible"), TEXT("diagnostic-visible")})
        { AddField(Record, Field, ck_inspector_inventories::UiBool(false)); }
        AddField(Record, TEXT("fraction"), ck_inspector_inventories::UiNumber(0.0f));
        AddField(Record, TEXT("bound-value"), ck_inspector_inventories::UiInteger(-1));
        AddField(Record, TEXT("consume-value"), ck_inspector_inventories::UiInteger(1));
        AddField(Record, TEXT("accent-color"), ck_inspector_inventories::UiColor(CkStyle::Info()));
        AddField(Record, TEXT("diff-color"), ck_inspector_inventories::UiColor(CkStyle::Text()));
        return Record;
    };

    for (const auto& Inventory : Inventories)
    {
        if (ck::Is_NOT_Valid(Inventory)) { continue; }
        const bool IsSpatial = UCk_Utils_Inventory_UE::Get_IsSpatial(Inventory);
        const FString InventoryKey = TEXT("inventory/")
            + ck_inspector_inventories::Make_RecordIdentity(Inventory);
        const FString Type = IsSpatial ? TEXT("Spatial") : TEXT("DataOnly");
        const FString Header = ck::Format_UE(TEXT("{} ({})"), Inventory.ToString(), Type);
        auto HeaderRecord = MakeBaseRecord(InventoryKey);
        AddField(HeaderRecord, TEXT("inventory-visible"), ck_inspector_inventories::UiBool(true));
        AddField(HeaderRecord, TEXT("label"), ck_inspector_inventories::UiText(Header));
        AddField(HeaderRecord, TEXT("summary"), ck_inspector_inventories::UiText(
            ck_inspector_inventories::Format_ItemSummary(Inventory).ToString()));
        AddField(HeaderRecord, TEXT("grid-visible"), ck_inspector_inventories::UiBool(IsSpatial));
        FString GridLabel = TEXT("View Tilemap");
        if (IsSpatial)
        {
            auto Mutable = FCk_Handle{Inventory};
            const auto Spatial = UCk_Utils_Inventory_Spatial_UE::Cast(Mutable);
            if (ck::IsValid(Spatial))
            {
                const auto Grid = UCk_Utils_Inventory_Spatial_UE::Get_Grid(Spatial);
                const auto Dims = ck::IsValid(Grid) ? UCk_Utils_2dGridSystem_UE::Get_Dimensions(Grid) : FIntPoint::ZeroValue;
                GridLabel = ck::Format_UE(TEXT("View Tilemap ({}\u00D7{})"), Dims.X, Dims.Y);
            }
        }
        AddField(HeaderRecord, TEXT("grid-label"), ck_inspector_inventories::UiText(GridLabel));
        AddField(HeaderRecord, TEXT("grid-tooltip"), ck_inspector_inventories::UiText(
            TEXT("Open the spatial inventory grid in a separate debugger window.")));
        AddField(HeaderRecord, TEXT("accent-color"), ck_inspector_inventories::UiColor(
            IsSpatial ? CkStyle::Info() : CkStyle::TextDim()));
        AddField(HeaderRecord, TEXT("diff-color"), ck_inspector_inventories::UiColor(
            _DiffLabels.Contains(Header) ? CkStyle::Accent() : CkStyle::Text()));
        Routes.Add(InventoryKey, FRoute{Inventory, {}, true});
        Records.Add(MoveTemp(HeaderRecord));

        auto MeterRecord = MakeBaseRecord(InventoryKey + TEXT("/meter"));
        MeterRecord.Key = InventoryKey + TEXT("/meter");
        Routes.Add(MeterRecord.Key, FRoute{Inventory, {}, true});
        const bool MeterDiff = _DiffLabels.Contains(TEXT("  Cells:"))
            || _DiffLabels.Contains(TEXT("  Capacity:")) || _DiffLabels.Contains(TEXT("  Bound Max:"));
        AddField(MeterRecord, TEXT("diff-color"), ck_inspector_inventories::UiColor(
            MeterDiff ? CkStyle::Accent() : CkStyle::Text()));
        if (IsSpatial)
        {
            auto Mutable = FCk_Handle{Inventory};
            const auto Spatial = UCk_Utils_Inventory_Spatial_UE::Cast(Mutable);
            if (ck::IsValid(Spatial))
            {
                const auto Occupancy = ck_inspector_inventories::Get_CellOccupancy(
                    Spatial, MakeShared<ck_inspector_inventories::FCellOccupancyCache>());
                AddField(MeterRecord, TEXT("meter-visible"), ck_inspector_inventories::UiBool(true));
                AddField(MeterRecord, TEXT("meter-label"), ck_inspector_inventories::UiText(TEXT("Cells:")));
                AddField(MeterRecord, TEXT("meter-value"), ck_inspector_inventories::UiText(
                    ck::Format_UE(TEXT("{} / {}"), Occupancy.Occupied, Occupancy.Active)));
                AddField(MeterRecord, TEXT("fraction"), ck_inspector_inventories::UiNumber(
                    Occupancy.Active > 0 ? static_cast<float>(Occupancy.Occupied) / Occupancy.Active : 0.0f));
            }
        }
        else
        {
            auto Mutable = FCk_Handle{Inventory};
            const auto DataOnly = UCk_Utils_Inventory_DataOnly_UE::Cast(Mutable);
            if (ck::IsValid(DataOnly))
            {
                const auto Bound = UCk_Utils_Inventory_DataOnly_UE::Get_BoundMax(DataOnly);
                AddField(MeterRecord, TEXT("bound-visible"), ck_inspector_inventories::UiBool(true));
                AddField(MeterRecord, TEXT("bound-value"), ck_inspector_inventories::UiInteger(Bound.Get(-1)));
                if (Bound.IsSet() && Bound.GetValue() > 0)
                {
                    AddField(MeterRecord, TEXT("meter-visible"), ck_inspector_inventories::UiBool(true));
                    AddField(MeterRecord, TEXT("meter-label"), ck_inspector_inventories::UiText(TEXT("Capacity:")));
                    AddField(MeterRecord, TEXT("meter-value"), ck_inspector_inventories::UiText(
                        ck::Format_UE(TEXT("{} / {}"), UCk_Utils_Inventory_UE::Get_NumItems(Inventory), Bound.GetValue())));
                    AddField(MeterRecord, TEXT("fraction"), ck_inspector_inventories::UiNumber(
                        static_cast<float>(UCk_Utils_Inventory_UE::Get_NumItems(Inventory)) / Bound.GetValue()));
                }
            }
        }
        Records.Add(MoveTemp(MeterRecord));

        for (const auto& Item : UCk_Utils_Inventory_UE::Get_Items(Inventory))
        {
            if (ck::Is_NOT_Valid(Item)) { continue; }
            const auto* Definition = UCk_Utils_Item_UE::Get_Definition(Item);
            const FString Name = Definition != nullptr ? Definition->Get_CoreInfo().Get_Name().ToString() : Item.ToString();
            if (NOT _Filter.IsEmpty() && NOT Name.Contains(_Filter, ESearchCase::IgnoreCase)) { continue; }
            const FString Key = InventoryKey + TEXT("/item/")
                + ck_inspector_inventories::Make_RecordIdentity(Item);
            auto Record = MakeBaseRecord(Key);
            const bool IsStackable = UCk_Utils_ItemTrait_Stackable_UE::Get_IsStackable(Item);
            const bool HasTags = UCk_Utils_ItemTrait_Tags_UE::Get_HasTagsFeature(Item);
            const int32 PendingConsume = _PendingConsume.FindOrAdd(Key, 1);
            const FString PendingTag = _PendingTags.FindOrAdd(Key);
            AddField(Record, TEXT("item-visible"), ck_inspector_inventories::UiBool(true));
            AddField(Record, TEXT("label"), ck_inspector_inventories::UiText(Name));
            AddField(Record, TEXT("stack-visible"), ck_inspector_inventories::UiBool(IsStackable));
            AddField(Record, TEXT("summary"), ck_inspector_inventories::UiText(
                IsStackable ? ck_inspector_inventories::Get_StackPillText(Item).ToString() : FString{}));
            AddField(Record, TEXT("consume-visible"), ck_inspector_inventories::UiBool(IsStackable));
            AddField(Record, TEXT("consume-value"), ck_inspector_inventories::UiInteger(PendingConsume));
            AddField(Record, TEXT("consume-label"), ck_inspector_inventories::UiText(TEXT("Consume")));
            AddField(Record, TEXT("consume-tooltip"), ck_inspector_inventories::UiText(
                TEXT("Consume less than the current stack count; removing the whole stack is intentionally refused.")));
            AddField(Record, TEXT("tags-visible"), ck_inspector_inventories::UiBool(HasTags));
            AddField(Record, TEXT("tag-value"), ck_inspector_inventories::UiText(PendingTag));
            AddField(Record, TEXT("add-label"), ck_inspector_inventories::UiText(TEXT("Add Tag")));
            AddField(Record, TEXT("remove-label"), ck_inspector_inventories::UiText(TEXT("Remove Tag")));
            AddField(Record, TEXT("tag-tooltip"), ck_inspector_inventories::UiText(TEXT("Apply the staged gameplay tag.")));
            AddField(Record, TEXT("accent-color"), ck_inspector_inventories::UiColor(
                IsStackable && UCk_Utils_ItemTrait_Stackable_UE::Get_IsStackFull(Item) ? CkStyle::Warn() : CkStyle::Info()));
            const FString NativeLabel = TEXT("  ") + Name;
            AddField(Record, TEXT("diff-color"), ck_inspector_inventories::UiColor(
                _DiffLabels.Contains(NativeLabel) ? CkStyle::Accent() : CkStyle::Text()));
            Routes.Add(Key, FRoute{Inventory, Item, false});
            LiveItemKeys.Add(Key);
            Records.Add(MoveTemp(Record));
        }

        const auto AddDiagnostic = [&](const FCk_Handle& Item, const FString& Prefix, const FLinearColor& Color)
        {
            if (ck::Is_NOT_Valid(Item)) { return; }
            auto Mutable = Item;
            const auto Typed = UCk_Utils_Item_UE::Cast(Mutable);
            const auto* Definition = ck::IsValid(Typed) ? UCk_Utils_Item_UE::Get_Definition(Typed) : nullptr;
            const FString Name = Definition != nullptr ? Definition->Get_CoreInfo().Get_Name().ToString() : Item.ToString();
            if (NOT _Filter.IsEmpty() && NOT Name.Contains(_Filter, ESearchCase::IgnoreCase)) { return; }
            const FString Key = InventoryKey + TEXT("/diagnostic/") + Prefix + TEXT("/") + Item.ToString();
            auto Record = MakeBaseRecord(Key);
            AddField(Record, TEXT("diagnostic-visible"), ck_inspector_inventories::UiBool(true));
            AddField(Record, TEXT("label"), ck_inspector_inventories::UiText(TEXT("[") + Prefix + TEXT("] ") + Name));
            AddField(Record, TEXT("accent-color"), ck_inspector_inventories::UiColor(Color));
            Routes.Add(Key, FRoute{Inventory, Item, false});
            Records.Add(MoveTemp(Record));
        };
        for (const auto& Item : ck_inspector_inventories::Get_PendingRemovalLifetimeOwnedItems(Inventory))
        { AddDiagnostic(Item, TEXT("Pending removal"), CkStyle::Warn()); }
        for (const auto& Item : ck_inspector_inventories::Get_DetachedLifetimeOwnedItems(Inventory))
        { AddDiagnostic(Item, TEXT("Detached lifetime child"), CkStyle::Err()); }
    }

    const FCkUiLoadResult Result = _Records->TrySetRecords(MoveTemp(Records));
    if (NOT Result.Succeeded)
    {
        _LoadError = FString::Join(Result.Errors, TEXT("\n"));
        return false;
    }
    _Routes = MoveTemp(Routes);
    for (auto It = _PendingConsume.CreateIterator(); It; ++It)
    { if (NOT LiveItemKeys.Contains(It.Key())) { It.RemoveCurrent(); } }
    for (auto It = _PendingTags.CreateIterator(); It; ++It)
    { if (NOT LiveItemKeys.Contains(It.Key())) { It.RemoveCurrent(); } }
    return true;
}

auto SCkInspector_InventoriesAuthored::Resolve_Inventory(
    const FString& InKey,
    FCk_Handle& OutInventory) const -> bool
{
    OutInventory = {};
    const FRoute* Route = _Routes.Find(InKey);
    if (NOT _Active || Route == nullptr || NOT Route->IsInventory) { return false; }
    for (const auto& Inventory : ck_inspector_inventories::Resolve_InspectableInventories(_Entity))
    {
        if (Inventory == Route->Inventory)
        {
            OutInventory = Route->Inventory;
            return ck::IsValid(OutInventory);
        }
    }
    return false;
}

auto SCkInspector_InventoriesAuthored::Resolve_Item(
    const FString& InKey,
    FCk_Handle& OutInventory,
    FCk_Handle& OutItem) const -> bool
{
    OutInventory = {};
    OutItem = {};
    const FRoute* Route = _Routes.Find(InKey);
    if (NOT _Active || Route == nullptr || Route->IsInventory
        || ck_inspector_inventories::Is_Destroying(Route->Inventory)
        || ck_inspector_inventories::Is_Destroying(Route->Item))
    { return false; }
    bool InventoryIsCurrent = false;
    for (const auto& Current : ck_inspector_inventories::Resolve_InspectableInventories(_Entity))
    { InventoryIsCurrent = InventoryIsCurrent || Current == Route->Inventory; }
    if (NOT InventoryIsCurrent) { return false; }
    auto MutableInventory = Route->Inventory;
    auto MutableItem = Route->Item;
    const auto Inventory = UCk_Utils_Inventory_UE::Cast(MutableInventory);
    const auto Item = UCk_Utils_Item_UE::Cast(MutableItem);
    if (ck::Is_NOT_Valid(Inventory) || ck::Is_NOT_Valid(Item)
        || NOT UCk_Utils_Inventory_UE::Get_ContainsItem(Inventory, Item))
    { return false; }
    OutInventory = Route->Inventory;
    OutItem = Route->Item;
    return true;
}

auto SCkInspector_InventoriesAuthored::Navigate(const FString& InKey) -> void
{
    const FRoute* Route = _Routes.Find(InKey);
    if (NOT _Active || Route == nullptr) { return; }
    bool InventoryIsCurrent = false;
    for (const auto& Current : ck_inspector_inventories::Resolve_InspectableInventories(_Entity))
    { InventoryIsCurrent = InventoryIsCurrent || Current == Route->Inventory; }
    if (NOT InventoryIsCurrent) { return; }
    const FCk_Handle Target = Route->IsInventory ? Route->Inventory : Route->Item;
    if (ck_inspector_inventories::Is_Destroying(Target)) { return; }
    if (const auto Selection = _SelectionModel.Pin(); Selection.IsValid())
    { Selection->Set_SelectedEntities({Target}); }
}

auto SCkInspector_InventoriesAuthored::Open_Grid(const FString& InKey) -> void
{
    FCk_Handle Inventory;
    if (NOT Resolve_Inventory(InKey, Inventory) || NOT _OpenSpatialGrid) { return; }
    auto Mutable = Inventory;
    const auto Typed = UCk_Utils_Inventory_UE::Cast(Mutable);
    if (ck::Is_NOT_Valid(Typed) || NOT UCk_Utils_Inventory_UE::Get_IsSpatial(Typed)) { return; }
    _OpenSpatialGrid(Inventory);
}

auto SCkInspector_InventoriesAuthored::Commit_Bound(const FString& InKey, const int32 InValue) -> void
{
    FCk_Handle Inventory;
    if (InValue < -1 || NOT Resolve_Inventory(InKey, Inventory)
        || NOT ck::DebugRequestGate::Evaluate(Inventory, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    auto Mutable = Inventory;
    auto DataOnly = UCk_Utils_Inventory_DataOnly_UE::Cast(Mutable);
    if (ck::IsValid(DataOnly)) { UCk_Utils_Inventory_DataOnly_UE::Request_OverrideBounds(DataOnly, InValue); }
}

auto SCkInspector_InventoriesAuthored::Commit_Consume(const FString& InKey, const int32 InValue) -> void
{
    FCk_Handle Inventory; FCk_Handle Item;
    if (InValue >= 1 && Resolve_Item(InKey, Inventory, Item)) { _PendingConsume.Add(InKey, InValue); }
}

auto SCkInspector_InventoriesAuthored::Commit_Tag(const FString& InKey, const FText& InValue) -> void
{
    FCk_Handle Inventory; FCk_Handle Item;
    if (Resolve_Item(InKey, Inventory, Item))
    { _PendingTags.Add(InKey, InValue.ToString().TrimStartAndEnd()); }
}

auto SCkInspector_InventoriesAuthored::Consume(const FString& InKey) -> void
{
    FCk_Handle Inventory; FCk_Handle ItemEntity;
    if (NOT Resolve_Item(InKey, Inventory, ItemEntity)
        || NOT ck::DebugRequestGate::Evaluate(ItemEntity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    auto Mutable = ItemEntity;
    auto Item = UCk_Utils_Item_UE::Cast(Mutable);
    const int32 Count = _PendingConsume.FindRef(InKey);
    if (ck::Is_NOT_Valid(Item) || NOT UCk_Utils_ItemTrait_Stackable_UE::Get_IsStackable(Item)
        || Count < 1 || Count >= UCk_Utils_ItemTrait_Stackable_UE::Get_StackCount(Item))
    { return; }
    UCk_Utils_ItemTrait_Stackable_UE::Request_ConsumeFromStack(Item, Count);
}

auto SCkInspector_InventoriesAuthored::Change_Tag(const FString& InKey, const bool bInAdd) -> void
{
    FCk_Handle Inventory; FCk_Handle ItemEntity;
    if (NOT Resolve_Item(InKey, Inventory, ItemEntity)
        || NOT ck::DebugRequestGate::Evaluate(ItemEntity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    auto Mutable = ItemEntity;
    auto Item = UCk_Utils_Item_UE::Cast(Mutable);
    const FString Value = _PendingTags.FindRef(InKey);
    const FGameplayTag ItemTag = Value.IsEmpty() ? FGameplayTag{} : FGameplayTag::RequestGameplayTag(FName{Value}, false);
    if (ck::Is_NOT_Valid(Item) || NOT UCk_Utils_ItemTrait_Tags_UE::Get_HasTagsFeature(Item) || NOT ItemTag.IsValid())
    { return; }
    if (bInAdd) { UCk_Utils_ItemTrait_Tags_UE::Request_AddTag(Item, ItemTag); }
    else { UCk_Utils_ItemTrait_Tags_UE::Request_RemoveTag(Item, ItemTag); }
}

auto SCkInspector_InventoriesAuthored::Build_AuthoredView() -> bool
{
    const FCkUiLoadResult Schema = FCkUiCollection::TryCreate({
        {TEXT("label"), ECkUiFieldKind::Text}, {TEXT("summary"), ECkUiFieldKind::Text},
        {TEXT("grid-label"), ECkUiFieldKind::Text}, {TEXT("grid-tooltip"), ECkUiFieldKind::Text},
        {TEXT("meter-label"), ECkUiFieldKind::Text}, {TEXT("meter-value"), ECkUiFieldKind::Text},
        {TEXT("consume-label"), ECkUiFieldKind::Text}, {TEXT("consume-tooltip"), ECkUiFieldKind::Text},
        {TEXT("tag-value"), ECkUiFieldKind::Text}, {TEXT("add-label"), ECkUiFieldKind::Text},
        {TEXT("remove-label"), ECkUiFieldKind::Text}, {TEXT("tag-tooltip"), ECkUiFieldKind::Text},
        {TEXT("inventory-visible"), ECkUiFieldKind::Bool}, {TEXT("grid-visible"), ECkUiFieldKind::Bool},
        {TEXT("meter-visible"), ECkUiFieldKind::Bool}, {TEXT("bound-visible"), ECkUiFieldKind::Bool},
        {TEXT("item-visible"), ECkUiFieldKind::Bool}, {TEXT("stack-visible"), ECkUiFieldKind::Bool},
        {TEXT("consume-visible"), ECkUiFieldKind::Bool}, {TEXT("tags-visible"), ECkUiFieldKind::Bool},
        {TEXT("diagnostic-visible"), ECkUiFieldKind::Bool}, {TEXT("fraction"), ECkUiFieldKind::Number},
        {TEXT("bound-value"), ECkUiFieldKind::Integer}, {TEXT("consume-value"), ECkUiFieldKind::Integer},
        {TEXT("accent-color"), ECkUiFieldKind::Color}, {TEXT("diff-color"), ECkUiFieldKind::Color}}, _Records);
    if (NOT Schema.Succeeded || NOT _Records.IsValid() || NOT Refresh_Records())
    {
        if (NOT Schema.Succeeded) { _LoadError = FString::Join(Schema.Errors, TEXT("\n")); }
        if (_LoadError.IsEmpty()) { _LoadError = TEXT("Inventory records are unavailable."); }
        return false;
    }
    auto Registry = TSharedPtr<const FCkUiWidgetRegistrySnapshot>{};
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }
    const TWeakPtr<SCkInspector_InventoriesAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Visibility.Add(TEXT("inventories-available"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("inventories-unavailable"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("inventories-can-request"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Data.Text.Add(TEXT("inventories-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return FText::FromString(Widget.IsValid() ? Widget->Get_RequestDisabledReason() : FString{}); }));
    Data.Collections.Add(TEXT("inventory-records"), _Records);
    Data.ItemActions.Add(TEXT("inventory-navigate"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& Key)
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Navigate(Key); } }));
    Data.ItemActions.Add(TEXT("inventory-open-grid"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& Key)
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Open_Grid(Key); } }));
    Data.ItemActions.Add(TEXT("inventory-consume"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& Key)
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Consume(Key); } }));
    Data.ItemActions.Add(TEXT("inventory-add-tag"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& Key)
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Change_Tag(Key, true); } }));
    Data.ItemActions.Add(TEXT("inventory-remove-tag"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& Key)
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Change_Tag(Key, false); } }));
    Data.ItemIntegerCommitted.Add(TEXT("inventory-bound-committed"), FCkUiOnItemIntegerCommitted::CreateLambda(
        [WeakWidget](const FString& Key, const int32 Value, ETextCommit::Type)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_Bound(Key, Value); } }));
    Data.ItemIntegerCommitted.Add(TEXT("inventory-consume-committed"), FCkUiOnItemIntegerCommitted::CreateLambda(
        [WeakWidget](const FString& Key, const int32 Value, ETextCommit::Type)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_Consume(Key, Value); } }));
    Data.ItemTextCommitted.Add(TEXT("inventory-tag-committed"), FCkUiOnItemTextCommitted::CreateLambda(
        [WeakWidget](const FString& Key, const FText Value, ETextCommit::Type)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_Tag(Key, Value); } }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorInventories.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorInventories.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }
    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_InventoriesAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    const bool RecordsOk = Refresh_Records();
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else if (RecordsOk) { _LoadError.Reset(); }
}

auto SCkInspector_InventoriesAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _Filter.Reset();
    _SelectionModel.Reset();
    _DiffLabels.Reset();
    _OpenSpatialGrid = {};
    _Records.Reset();
    _Routes.Reset();
    _PendingConsume.Reset();
    _PendingTags.Reset();
    _View.Reset();
    _Mounted = false;
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
    return Build_AuthoredOrNative(Entity, FString());
}

auto FCkInspector_Inventories::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return Build_AuthoredOrNative(Entity, InFilter);
}

auto FCkInspector_Inventories::Build_AuthoredOrNative(
    const FCk_Handle& Entity,
    const FString& InFilter) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> Native = BuildInventoryGrid(Entity, InFilter);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return Native; }

    auto DiffLabels = TSet<FString>{};
    for (const auto& Inventory : ck_inspector_inventories::Resolve_InspectableInventories(Entity))
    {
        if (ck::Is_NOT_Valid(Inventory)) { continue; }
        const auto Type = UCk_Utils_Inventory_UE::Get_IsSpatial(Inventory) ? TEXT("Spatial") : TEXT("DataOnly");
        const FString Header = ck::Format_UE(TEXT("{} ({})"), Inventory.ToString(), Type);
        for (const FString& Label : {Header, FString{TEXT("  Cells:")}, FString{TEXT("  Capacity:")}, FString{TEXT("  Bound Max:")}})
        { if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); } }
        for (const auto& Item : UCk_Utils_Inventory_UE::Get_Items(Inventory))
        {
            if (ck::Is_NOT_Valid(Item)) { continue; }
            const auto* Definition = UCk_Utils_Item_UE::Get_Definition(Item);
            const FString Name = Definition != nullptr ? Definition->Get_CoreInfo().Get_Name().ToString() : Item.ToString();
            const FString Label = ck::Format_UE(TEXT("  {}"), Name);
            if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); }
        }
    }

    const TSharedRef<SCkInspector_InventoriesAuthored> Authored = SNew(SCkInspector_InventoriesAuthored)
        .Entity(Entity)
        .Filter(InFilter)
        .SelectionModel(SelectionModel)
        .DiffLabels(MoveTemp(DiffLabels))
        .OpenSpatialGrid([this](const FCk_Handle& Inventory) { OpenOrFocus_SpatialGridPopup(Inventory); });
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return Native;
    }

    _LastAuthoredLoadError.Reset();
    _ViewStates.RemoveAll([&Entity](const FInventoryViewState& State) { return State.Entity == Entity; });
    _AuthoredInstances.Add(Authored);
    return Authored;
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
        Builder.SetEditGuard(Get_EditGuard());
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

            // Bound override. AuthorityOnly (BlueprintAuthorityOnly on the Util): the bound lives in an
            // integer attribute the server owns. -1 is the documented "unbounded" sentinel, hence the min.
            if (ck::IsValid(DataOnlyHandle))
            {
                const auto CapturedDataOnly = DataOnlyHandle;

                Builder.AddIntegerRow(
                    FText::FromString(TEXT("  Bound Max:")),
                    TAttribute<int32>::CreateLambda([CapturedDataOnly]()
                    {
                        if (ck::Is_NOT_Valid(CapturedDataOnly)) { return -1; }
                        return UCk_Utils_Inventory_DataOnly_UE::Get_BoundMax(CapturedDataOnly).Get(-1);
                    }),
                    [CapturedDataOnly](int32 InNewBound)
                    {
                        auto Mutable = CapturedDataOnly;
                        if (ck::Is_NOT_Valid(Mutable)) { return; }

                        UCk_Utils_Inventory_DataOnly_UE::Request_OverrideBounds(Mutable, InNewBound);
                    },
                    -1,
                    TOptional<int32>{},
                    ECk_DebugRequest_Requirement::AuthorityOnly);
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
    Builder.SetEditGuard(Get_EditGuard());
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

        // ---- Per-item verbs. All three are AuthorityOnly (BlueprintAuthorityOnly on the Utils):
        // inventory mutation is server-owned, and a client press would ensure-and-drop. ----

        const auto CapturedItem = ItemHandle;

        // Consume — staged count + explicit button, because a count field that fired on commit would
        // eat a unit every time the user tabbed out. The Util REFUSES the whole stack by design
        // (emptying an entry must go through the inventory's Request_RemoveItem), so a request for
        // StackCount or more silently changes nothing; the button tooltip says so.
        if (UCk_Utils_ItemTrait_Stackable_UE::Get_IsStackable(ItemHandle))
        {
            const auto PendingConsume = MakeShared<int32>(1);

            Builder.AddIntegerRow(
                FText::FromString(TEXT("    Consume:")),
                TAttribute<int32>::CreateLambda([PendingConsume]() { return *PendingConsume; }),
                [PendingConsume](int32 InCount) { *PendingConsume = InCount; },
                1,
                TOptional<int32>{},
                ECk_DebugRequest_Requirement::AuthorityOnly);

            Builder.AddActionRow(
                FText::FromString(TEXT("    ")),
                {
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Consume")),
                        FText::FromString(TEXT(
                            "Request_ConsumeFromStack with the count above. Refuses the WHOLE stack by design — "
                            "a count of StackCount or more changes nothing; remove the entry through the inventory instead.")),
                        [CapturedItem, PendingConsume]()
                        {
                            auto Mutable = CapturedItem;
                            if (ck::Is_NOT_Valid(Mutable)) { return; }

                            UCk_Utils_ItemTrait_Stackable_UE::Request_ConsumeFromStack(Mutable, *PendingConsume);
                        },
                        ECk_DebugRequest_Requirement::AuthorityOnly
                    },
                });
        }

        // Item tags — one staged tag, two verbs. Only offered when the item actually carries the
        // Tags trait; otherwise both requests would land on an item with nowhere to put them.
        if (UCk_Utils_ItemTrait_Tags_UE::Get_HasTagsFeature(ItemHandle))
        {
            const auto PendingItemTag = MakeShared<FGameplayTag>();

            Builder.AddTagEntryRow(
                FText::FromString(TEXT("    Tag:")),
                TAttribute<FText>::CreateLambda([PendingItemTag]()
                {
                    return PendingItemTag->IsValid()
                        ? FText::FromName(PendingItemTag->GetTagName())
                        : FText::FromString(TEXT("(none)"));
                }),
                [PendingItemTag](FGameplayTag InTag) { *PendingItemTag = InTag; },
                ECk_DebugRequest_Requirement::AuthorityOnly);

            Builder.AddActionRow(
                FText::FromString(TEXT("    ")),
                {
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Add Tag")),
                        FText::FromString(TEXT("Request_AddTag with the staged tag.")),
                        [CapturedItem, PendingItemTag]()
                        {
                            auto Mutable = CapturedItem;
                            if (ck::Is_NOT_Valid(Mutable) || NOT PendingItemTag->IsValid()) { return; }

                            UCk_Utils_ItemTrait_Tags_UE::Request_AddTag(Mutable, *PendingItemTag);
                        },
                        ECk_DebugRequest_Requirement::AuthorityOnly
                    },
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Remove Tag")),
                        FText::FromString(TEXT("Request_RemoveTag with the staged tag.")),
                        [CapturedItem, PendingItemTag]()
                        {
                            auto Mutable = CapturedItem;
                            if (ck::Is_NOT_Valid(Mutable) || NOT PendingItemTag->IsValid()) { return; }

                            UCk_Utils_ItemTrait_Tags_UE::Request_RemoveTag(Mutable, *PendingItemTag);
                        },
                        ECk_DebugRequest_Requirement::AuthorityOnly
                    },
                });
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
            CkStyle::Err(),
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
    for (const auto& WeakInstance : _AuthoredInstances)
    {
        if (const auto Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
    Close_AllSpatialGridPopups();
    _ViewStates.Reset();
}

// =====================================================================================================================

auto FCkInspector_Inventories::Wants_TickWhenNotInspectable(const FCk_Handle& Entity) const -> bool
{
    return _ViewStates.ContainsByPredicate([&Entity](const FInventoryViewState& InState)
    {
        return InState.Entity == Entity && InState.InventoryGridHost.IsValid();
    }) || _AuthoredInstances.ContainsByPredicate([&Entity](const TWeakPtr<SCkInspector_InventoriesAuthored>& InInstance)
    {
        const auto Instance = InInstance.Pin();
        return Instance.IsValid() && NOT Instance->Is_Inert() && Instance->Is_ForEntity(Entity);
    });
}

// =====================================================================================================================

auto FCkInspector_Inventories::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_InventoriesAuthored>& InInstance)
    {
        const auto Instance = InInstance.Pin();
        return NOT Instance.IsValid() || Instance->Is_Inert();
    });
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
