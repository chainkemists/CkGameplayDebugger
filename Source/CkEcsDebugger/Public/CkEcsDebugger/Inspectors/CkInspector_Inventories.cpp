#include "CkInspector_Inventories.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkInventory/Inventory/CkInventory_Utils.h"
#include "CkInventory/Item/CkItem_Utils.h"
#include "CkInventory/Item/CkItem_Definition.h"
#include "CkInventory/InventorySlot/CkInventorySlot_Fragment.h"

#include "CkGrid/2dGridSystem/Grid/Ck2dGridSystem_Utils.h"
#include "CkGrid/2dGridSystem/Cell/Ck2dGridCell_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Inventories)

static constexpr FLinearColor Color_InventoryName = FLinearColor(0.55f, 0.78f, 0.95f);
static constexpr FLinearColor Color_InventoryType = FLinearColor(0.75f, 0.75f, 0.75f);
static constexpr FLinearColor Color_ItemName = FLinearColor(0.85f, 0.75f, 0.55f);

// ---- Distinct item colors (tetris-like palette) ----

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
};

static constexpr FLinearColor Color_CellEmpty    = FLinearColor(0.12f, 0.12f, 0.12f);
static constexpr FLinearColor Color_CellDisabled = FLinearColor(0.06f, 0.06f, 0.06f);
static constexpr FLinearColor Color_CellBorder   = FLinearColor(0.25f, 0.25f, 0.25f);

// =====================================================================================================================

auto FCkInspector_Inventories::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Inventories"));
}

auto FCkInspector_Inventories::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Inventory_UE::Has_Any(Entity);
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
    auto Builder = FCkInspectorWidgetBuilder();
    auto WeakSelectionModel = SelectionModel;

    _SpatialGridStates.Reset();

    auto MutableEntity = Entity;
    const auto Inventories = UCk_Utils_Inventory_UE::RecordOfInventories_Utils::Get_ValidEntries(MutableEntity);

    // Cache initial total item count for structural change detection in Tick
    auto InitialTotalItems = 0;
    for (const auto& Inv : Inventories)
    {
        if (ck::Is_NOT_Valid(Inv)) { continue; }
        InitialTotalItems += UCk_Utils_Inventory_UE::Get_NumItems(UCk_Utils_Inventory_UE::CastChecked(Inv));
    }
    _CachedTotalItemCount = InitialTotalItems;

    // ---- Grid visualization toggle (compact, same line as header area) ----

    {
        auto ToggleWidget = SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([this]() { return _ShowGridVisualization ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                {
                    _ShowGridVisualization = (InState == ECheckBoxState::Checked);

                    const auto Visibility = _ShowGridVisualization
                        ? EVisibility::Visible
                        : EVisibility::Collapsed;

                    for (auto& State : _SpatialGridStates)
                    {
                        if (State.GridWidget.IsValid())
                        {
                            State.GridWidget->SetVisibility(Visibility);
                        }
                    }
                })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Show Grid")))
                .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Secondary)
            ];

        Builder.AddWidgetRow(FText::GetEmpty(), ToggleWidget);
    }

    for (const auto& InventoryHandle : Inventories)
    {
        if (ck::Is_NOT_Valid(InventoryHandle)) { continue; }

        const auto Inventory = UCk_Utils_Inventory_UE::CastChecked(InventoryHandle);

        const auto InventoryType = UCk_Utils_Inventory_UE::Get_InventoryType(Inventory);
        const auto IsSpatial = InventoryType == ECk_InventoryType::Spatial;
        const auto CapturedInventory = Inventory;

        // Clickable inventory header
        const auto TypeStr = IsSpatial ? TEXT("Spatial") : TEXT("DataOnly");
        Builder.AddClickableRow(
            FText::FromString(ck::Format_UE(TEXT("{} ({})"), InventoryHandle.ToString(), TypeStr)),
            [CapturedInventory](const FCk_Handle& E)
            {
                const auto NumItems = UCk_Utils_Inventory_UE::Get_NumItems(CapturedInventory);
                return FText::FromString(ck::Format_UE(TEXT("{} items"), NumItems));
            },
            IsSpatial ? Color_InventoryName : Color_InventoryType,
            [WeakSelectionModel, InventoryHandle]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(InventoryHandle))
                {
                    WeakSelectionModel->Set_SelectedEntities({ InventoryHandle });
                }
            });

        // Spatial grid visualization
        if (IsSpatial)
        {
            auto GridWidget = BuildSpatialGridWidget(InventoryHandle);
            Builder.AddWidgetRow(FText::FromString(TEXT("  Grid")), GridWidget);
        }

        // List items in this inventory
        const auto Items = UCk_Utils_Inventory_UE::Get_Items(Inventory);
        for (const auto& ItemHandle : Items)
        {
            if (ck::Is_NOT_Valid(ItemHandle)) { continue; }

            const auto* Definition = UCk_Utils_Item_UE::Get_Definition(ItemHandle);
            const auto ItemName = (Definition != nullptr)
                ? Definition->Get_CoreInfo().Get_Name().ToString()
                : ItemHandle.ToString();

            const auto ItemEntity = FCk_Handle(ItemHandle);

            Builder.AddClickableRow(
                FText::FromString(ck::Format_UE(TEXT("  {}"), ItemName)),
                [](const FCk_Handle& E) { return FText::GetEmpty(); },
                Color_ItemName,
                [WeakSelectionModel, ItemEntity]()
                {
                    if (WeakSelectionModel.IsValid() && ck::IsValid(ItemEntity))
                    {
                        WeakSelectionModel->Set_SelectedEntities({ ItemEntity });
                    }
                });
        }
    }

    return Builder.Build(Entity, InFilter);
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

    auto ItemColorMap = TMap<FCk_Handle, int32>{};
    auto ColorIndex = int32{0};

    for (const auto& ItemHandle : Items)
    {
        if (ck::IsValid(ItemHandle))
        {
            ItemColorMap.Add(ItemHandle, ColorIndex);
            ColorIndex++;
        }
    }

    return ItemColorMap;
}

// =====================================================================================================================

auto FCkInspector_Inventories::BuildSpatialGridWidget(const FCk_Handle& InInventoryHandle) -> TSharedRef<SWidget>
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
    auto GridPanel = BuildGridPanel(GridHandle, ItemColorMap);

    auto Container = SNew(SBox)
        .Padding(FMargin(4.0f, 2.0f))
        [
            GridPanel
        ];

    const auto Visibility = _ShowGridVisualization
        ? EVisibility::Visible
        : EVisibility::Collapsed;
    Container->SetVisibility(Visibility);

    _SpatialGridStates.Add(FSpatialGridState{InInventoryHandle, Container});

    return Container;
}

// =====================================================================================================================

auto FCkInspector_Inventories::RefreshSpatialGridWidget(const FCk_Handle& InInventoryHandle) -> void
{
    for (auto& State : _SpatialGridStates)
    {
        if (State.InventoryHandle != InInventoryHandle || NOT State.GridWidget.IsValid())
        { continue; }

        if (NOT _ShowGridVisualization)
        { continue; }

        auto MutableHandle = InInventoryHandle;
        auto SpatialHandle = UCk_Utils_Inventory_Spatial_UE::Cast(MutableHandle);
        if (ck::Is_NOT_Valid(SpatialHandle))
        { continue; }

        auto GridHandle = UCk_Utils_Inventory_Spatial_UE::Get_Grid(SpatialHandle);
        if (ck::Is_NOT_Valid(GridHandle))
        { continue; }

        const auto Inventory = UCk_Utils_Inventory_UE::CastChecked(MutableHandle);
        const auto ItemColorMap = BuildItemColorMap(Inventory);
        auto GridPanel = BuildGridPanel(GridHandle, ItemColorMap);

        auto* ContainerBox = static_cast<SBox*>(State.GridWidget.Get());
        ContainerBox->SetContent(GridPanel);

        break;
    }
}

// =====================================================================================================================

auto FCkInspector_Inventories::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::Is_NOT_Valid(Entity)) { return; }

    auto MutableEntity = Entity;
    const auto Inventories = UCk_Utils_Inventory_UE::RecordOfInventories_Utils::Get_ValidEntries(MutableEntity);

    auto TotalItems = 0;
    for (const auto& InventoryHandle : Inventories)
    {
        if (ck::Is_NOT_Valid(InventoryHandle)) { continue; }
        const auto Inventory = UCk_Utils_Inventory_UE::CastChecked(InventoryHandle);
        TotalItems += UCk_Utils_Inventory_UE::Get_NumItems(Inventory);
    }

    if (_CachedTotalItemCount != TotalItems)
    {
        _CachedTotalItemCount = TotalItems;

        // ---- Refresh grids in-place instead of full rebuild ----

        for (const auto& InventoryHandle : Inventories)
        {
            if (ck::Is_NOT_Valid(InventoryHandle)) { continue; }
            if (UCk_Utils_Inventory_UE::Get_IsSpatial(UCk_Utils_Inventory_UE::CastChecked(InventoryHandle)))
            {
                RefreshSpatialGridWidget(InventoryHandle);
            }
        }
    }
}
