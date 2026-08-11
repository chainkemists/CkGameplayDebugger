#include "CkDebuggerPage_Overview.h"

#if WITH_EDITOR

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEcsDebugger/Graph/CkEcsDebugGraph.h"
#include "CkEcsDebugger/Graph/CkEcsDebugGraphSchema.h"
#include "CkEcsDebugger/Graph/CkEcsDebugNode_Entity.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_InspectorFilter.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "GraphEditor.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
// =====================================================================================================================

FCkDebuggerPage_Overview::FCkDebuggerPage_Overview() = default;

FCkDebuggerPage_Overview::~FCkDebuggerPage_Overview()
{
    if (SelectionModel.IsValid() && SelectionChangedHandle.IsValid())
    {
        SelectionModel->OnSelectionChanged.Remove(SelectionChangedHandle);
    }
    if (WorldModel.IsValid() && WorldChangedHandle.IsValid())
    {
        WorldModel->OnWorldChanged.Remove(WorldChangedHandle);
    }
    if (FilterModel.IsValid() && FilterChangedHandle.IsValid())
    {
        FilterModel->OnFilterChanged.Remove(FilterChangedHandle);
    }

    if (_Graph && UObjectInitialized())
    {
        _Graph->RemoveFromRoot();
        _Graph = nullptr;
    }
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::Get_PageName() const -> FText
{
    // The dashboard took the "Overview" slot (mockup parity); the hierarchy/relationship
    // graph lives on its own tab now.
    return FText::FromString(TEXT("Graph"));
}

auto FCkDebuggerPage_Overview::Get_PageIcon() const -> const FSlateBrush*
{
    return nullptr;
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget>
{
    SelectionModel = InContext.SelectionModel;
    WorldModel = InContext.WorldModel;
    FilterModel = InContext.FilterModel;

    // Create the debug graph — prevent GC
    _Graph = NewObject<UCkEcsDebugGraph>(GetTransientPackage());
    _Graph->AddToRoot();
    _Graph->Schema = UCkEcsDebugGraphSchema::StaticClass();

    // Wire double-click callback through the graph object
    _Graph->OnNodeDoubleClicked = [this](UCkEcsDebugNode_Entity* InNode)
    {
        OnGraphNodeDoubleClicked(InNode);
    };

    _GraphEditor = SNew(SGraphEditor)
        .GraphToEdit(_Graph)
        .IsEditable(true);

    // Helper to create a legend entry: colored swatch + label
    auto MakeLegendEntry = [](const FLinearColor& InColor, const FString& InLabel) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SBox)
                        .WidthOverride_Lambda([]() { return FOptionalSize{ck::debug_axes::Apply_IconSize(10.0f)}; })
                        .HeightOverride_Lambda([]() { return FOptionalSize{ck::debug_axes::Apply_IconSize(10.0f)}; })
                        [
                            SNew(SImage)
                                .Image(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                .ColorAndOpacity(InColor)
                        ]
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(InLabel))
                        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                        .ColorAndOpacity(CkStyle::TextDim())
                ];
    };

    // Bound, not computed: SBoxPanel slot padding is a TAttribute, so RowDensity reaches a legend
    // bar that was built long before the flip.
    const auto LegendPadding = TAttribute<FMargin>::CreateLambda([]() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS});
    });

    auto LegendBar = SNew(SHorizontalBox)
        .Visibility_Lambda([]()
        {
            return ck::debug_axes::Legend_IsVisible(UCkDebuggerStyleSettings::Get_Selection())
                ? EVisibility::Visible
                : EVisibility::Collapsed;
        })
        + SHorizontalBox::Slot().AutoWidth().Padding(LegendPadding)
            [ MakeLegendEntry(CkStyle::Graph_Node_Border_Center(), TEXT("Selected Entity")) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(LegendPadding)
            [ MakeLegendEntry(CkStyle::Relationship(), TEXT("Lifetime Owner")) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(LegendPadding)
            [ MakeLegendEntry(CkStyle::Reference(), TEXT("Context Owner")) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(LegendPadding)
            [ MakeLegendEntry(CkStyle::Transform(), TEXT("Dependent")) ];

    auto Result = SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Graph.Background"))
        .Padding(0.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f, 2.0f)
                [
                    LegendBar
                ]

            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    _GraphEditor.ToSharedRef()
                ]
        ];

    // Bind selection and world changes
    if (SelectionModel.IsValid())
    {
        SelectionChangedHandle = SelectionModel->OnSelectionChanged.AddRaw(
            this, &FCkDebuggerPage_Overview::OnSelectionChanged);

        if (SelectionModel->Get_SelectionCount() > 0)
        {
            RebuildGraph();
        }
    }

    if (WorldModel.IsValid())
    {
        WorldChangedHandle = WorldModel->OnWorldChanged.AddRaw(
            this, &FCkDebuggerPage_Overview::OnWorldChanged);
    }

    if (FilterModel.IsValid())
    {
        FilterChangedHandle = FilterModel->OnFilterChanged.AddRaw(
            this, &FCkDebuggerPage_Overview::OnInspectorFilterChanged);
    }

    return Result;
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::Tick(float InDeltaTime) -> void
{
    if (NOT SelectionModel.IsValid() || NOT _Graph)
    { return; }

    if (SelectionModel->Get_SelectionCount() > 0)
    {
        const auto PrimaryEntity = SelectionModel->Get_PrimarySelection();
        if (ck::Is_NOT_Valid(PrimaryEntity))
        {
            _Graph->ForceRebuild();
            _Graph->RebuildFromEntity(FCk_Handle());
        }
        else
        {
            RebuildGraph();
        }
    }
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::IsActive() const -> bool
{
    return IsActivePage;
}

auto FCkDebuggerPage_Overview::Set_IsActive(bool InIsActive) -> void
{
    IsActivePage = InIsActive;

    if (NOT InIsActive)
    {
        if (SelectionModel.IsValid() && SelectionChangedHandle.IsValid())
        {
            SelectionModel->OnSelectionChanged.Remove(SelectionChangedHandle);
            SelectionChangedHandle.Reset();
        }
    }
    else
    {
        if (SelectionModel.IsValid() && NOT SelectionChangedHandle.IsValid())
        {
            SelectionChangedHandle = SelectionModel->OnSelectionChanged.AddRaw(
                this, &FCkDebuggerPage_Overview::OnSelectionChanged);
            RebuildGraph();
        }
    }
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::OnSelectionChanged(const TArray<FCk_Handle>& InEntities) -> void
{
    // Skip rebuild if we triggered this ourselves from a graph node click
    if (_bNavigatingFromGraph)
    { return; }

    if (_Graph)
    {
        _Graph->ForceRebuild();
    }
    RebuildGraph();
    Apply_InspectorFilterToGraph();
}

auto FCkDebuggerPage_Overview::OnWorldChanged(UWorld* InWorld) -> void
{
    if (_Graph)
    {
        _Graph->ForceRebuild();
        _Graph->RebuildFromEntity(FCk_Handle());
    }
    Apply_InspectorFilterToGraph();
}

auto FCkDebuggerPage_Overview::OnInspectorFilterChanged() -> void
{
    Apply_InspectorFilterToGraph();
}

auto FCkDebuggerPage_Overview::Apply_InspectorFilterToGraph() -> void
{
    if (NOT _Graph)
    { return; }

    const auto HasFilter = FilterModel.IsValid();

    for (UEdGraphNode* Node : _Graph->Nodes)
    {
        auto* EntityNode = Cast<UCkEcsDebugNode_Entity>(Node);
        if (NOT EntityNode)
        { continue; }

        const auto Matches = HasFilter
            ? FilterModel->Test_Entity(EntityNode->Get_Entity())
            : true;

        EntityNode->Set_IsFilterMatch(Matches);
    }

    // Each SGraphNode_EcsEntity is marked ForceVolatile and reads its colors via TAttribute
    // callbacks, so the next paint pass will pick up the new flag automatically. Trigger an
    // explicit invalidation on the graph editor for snappier feedback.
    if (_GraphEditor.IsValid())
    {
        _GraphEditor->Invalidate(EInvalidateWidgetReason::Paint);
    }
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::OnGraphNodeDoubleClicked(UCkEcsDebugNode_Entity* InNode) -> void
{
    if (NOT SelectionModel.IsValid() || NOT InNode)
    { return; }

    const auto& Entity = InNode->Get_Entity();
    if (ck::Is_NOT_Valid(Entity))
    { return; }

    _bNavigatingFromGraph = true;
    SelectionModel->Set_SelectedEntities({ Entity });
    _bNavigatingFromGraph = false;

    _Graph->ForceRebuild();
    RebuildGraph();
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::RebuildGraph() -> void
{
    if (NOT _Graph || NOT SelectionModel.IsValid())
    { return; }

    if (SelectionModel->Get_SelectionCount() == 0)
    {
        _Graph->RebuildFromEntity(FCk_Handle());
        return;
    }

    const auto PrimaryEntity = SelectionModel->Get_PrimarySelection();

    if (ck::Is_NOT_Valid(PrimaryEntity))
    {
        _Graph->RebuildFromEntity(FCk_Handle());
        return;
    }

    const auto bChanged = _Graph->RebuildFromEntity(PrimaryEntity);

    if (bChanged)
    {
        // Newly created nodes default IsFilterMatch=true; reapply so they pick up the active filter.
        Apply_InspectorFilterToGraph();

        if (_GraphEditor.IsValid())
        {
            _GraphEditor->ZoomToFit(false);
        }
    }
}

// =====================================================================================================================

#endif // WITH_EDITOR
