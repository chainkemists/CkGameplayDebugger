#include "CkDebuggerPage_Overview.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Graph/SCkDebug_GraphCanvas.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkEcsDebugger/Graph/SCkEcsEntityGraphCard.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_InspectorFilter.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_ecs_debugger_page_overview
{
    auto MakeCanvasId(const FCk_Handle& InEntity) -> uint64
    {
        if (ck::Is_NOT_Valid(InEntity))
        {
            return 0;
        }

        const auto Entity = InEntity.Get_Entity();
        const auto Number = static_cast<uint64>(Entity.Get_EntityNumber());
        const auto Version = static_cast<uint64>(Entity.Get_VersionNumber());
        return ((Version << 32) | Number) + 1;
    }

    auto MakeLegendEntry(const FLinearColor& InColor, const FString& InLabel) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox) +
               SHorizontalBox::Slot()
                   .AutoWidth()
                   .VAlign(VAlign_Center)
                   .Padding(0.0f,
                            0.0f,
                            CkStyle::SpaceS,
                            0.0f)[SNew(SBox)
                                      .WidthOverride_Lambda(
                                          []()
                                          {
                                              return FOptionalSize{
                                                  ck::debug_axes::Apply_IconSize(10.0f)};
                                          })
                                      .HeightOverride_Lambda(
                                          []()
                                          {
                                              return FOptionalSize{
                                                  ck::debug_axes::Apply_IconSize(10.0f)};
                                          })[SNew(SImage)
                                                 .Image(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                                 .ColorAndOpacity(InColor)]] +
               SHorizontalBox::Slot().AutoWidth().VAlign(
                   VAlign_Center)[SNew(STextBlock)
                                      .Text(FText::FromString(InLabel))
                                      .TextStyle(
                                          &FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>(
                                              "CkDebugger.Text.Normal"))
                                      .ColorAndOpacity(CkStyle::TextDim())];
    }
} // namespace ck_ecs_debugger_page_overview
// =====================================================================================================================

FCkDebuggerPage_Overview::FCkDebuggerPage_Overview() = default;

FCkDebuggerPage_Overview::~FCkDebuggerPage_Overview()
{
    if (_GraphCanvas.IsValid())
    {
        _GraphCanvas->Clear_InteractionDelegates();
    }
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

    ClearGraph();
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::Get_PageName() const -> FText
{
    return FText::FromString(TEXT("Graph"));
}

auto FCkDebuggerPage_Overview::Get_PageIcon() const -> const FSlateBrush*
{
    return nullptr;
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::Build_Content(const FCkDebuggerPageContext& InContext)
    -> TSharedRef<SWidget>
{
    SelectionModel = InContext.SelectionModel;
    WorldModel = InContext.WorldModel;
    FilterModel = InContext.FilterModel;

    _GraphCanvas =
        SNew(SCkDebug_GraphCanvas)
            .AllowNodeDragging(true)
            .OnSelectionChanged_Raw(this, &FCkDebuggerPage_Overview::OnCanvasSelectionChanged)
            .OnNodeDoubleClicked_Raw(this, &FCkDebuggerPage_Overview::OnCanvasNodeDoubleClicked)
            .OnNodeContextMenu_Raw(this, &FCkDebuggerPage_Overview::OnCanvasNodeContextMenu);

    const auto LegendPadding = TAttribute<FMargin>::CreateLambda(
        []() -> FMargin
        {
            return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS});
        });

    auto LegendBar =
        SNew(SHorizontalBox)
            .Visibility_Lambda(
                []()
                {
                    return ck::debug_axes::Legend_IsVisible(
                               UCkDebuggerStyleSettings::Get_Selection())
                               ? EVisibility::Visible
                               : EVisibility::Collapsed;
                }) +
        SHorizontalBox::Slot().AutoWidth().Padding(
            LegendPadding)[ck_ecs_debugger_page_overview::MakeLegendEntry(
            CkStyle::Graph_Node_Border_Center(), TEXT("Selected Entity"))] +
        SHorizontalBox::Slot().AutoWidth().Padding(
            LegendPadding)[ck_ecs_debugger_page_overview::MakeLegendEntry(CkStyle::Relationship(),
                                                                          TEXT("Lifetime Owner"))] +
        SHorizontalBox::Slot().AutoWidth().Padding(
            LegendPadding)[ck_ecs_debugger_page_overview::MakeLegendEntry(CkStyle::Reference(),
                                                                          TEXT("Context Owner"))] +
        SHorizontalBox::Slot().AutoWidth().Padding(
            LegendPadding)[ck_ecs_debugger_page_overview::MakeLegendEntry(CkStyle::Transform(),
                                                                          TEXT("Dependent"))];

    if (SelectionModel.IsValid())
    {
        SelectionChangedHandle = SelectionModel->OnSelectionChanged.AddRaw(
            this, &FCkDebuggerPage_Overview::OnSelectionChanged);
    }
    if (WorldModel.IsValid())
    {
        WorldChangedHandle =
            WorldModel->OnWorldChanged.AddRaw(this, &FCkDebuggerPage_Overview::OnWorldChanged);
    }
    if (FilterModel.IsValid())
    {
        FilterChangedHandle = FilterModel->OnFilterChanged.AddRaw(
            this, &FCkDebuggerPage_Overview::OnInspectorFilterChanged);
    }

    RebuildGraph();

    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Graph.Background"))
        .Padding(0.0f)[SNew(SVerticalBox) +
                       SVerticalBox::Slot().AutoHeight().Padding(4.0f, 2.0f)[LegendBar] +
                       SVerticalBox::Slot().FillHeight(1.0f)[_GraphCanvas.ToSharedRef()]];
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::Tick(float InDeltaTime) -> void
{
    (void)InDeltaTime;
    if (NOT SelectionModel.IsValid() || NOT _GraphCanvas.IsValid())
    {
        return;
    }

    if (SelectionModel->Get_SelectionCount() == 0 ||
        ck::Is_NOT_Valid(SelectionModel->Get_PrimarySelection()))
    {
        ClearGraph();
    }
    else
    {
        RebuildGraph();
    }

    if (_PendingFrameAll && _GraphCanvas->GetCachedGeometry().GetLocalSize().X > 0.0f)
    {
        _GraphCanvas->Frame_All();
        _PendingFrameAll = false;
    }
}

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
        return;
    }

    if (SelectionModel.IsValid() && NOT SelectionChangedHandle.IsValid())
    {
        SelectionChangedHandle = SelectionModel->OnSelectionChanged.AddRaw(
            this, &FCkDebuggerPage_Overview::OnSelectionChanged);
        RebuildGraph();
    }
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::OnSelectionChanged(const TArray<FCk_Handle>& InEntities) -> void
{
    (void)InEntities;
    if (_bNavigatingFromGraph)
    {
        return;
    }

    RebuildGraph();
    Apply_InspectorFilterToGraph();
}

auto FCkDebuggerPage_Overview::OnWorldChanged(UWorld* InWorld) -> void
{
    (void)InWorld;
    ClearGraph();
}

auto FCkDebuggerPage_Overview::OnInspectorFilterChanged() -> void
{
    Apply_InspectorFilterToGraph();
}

auto FCkDebuggerPage_Overview::OnCanvasSelectionChanged(const TSet<uint64>& InSelectedNodeIds)
    -> void
{
    for (const auto& Card : _EntityCards)
    {
        if (Card.Value.IsValid())
        {
            Card.Value->SetSelected(InSelectedNodeIds.Contains(Card.Key));
        }
    }
}

auto FCkDebuggerPage_Overview::OnCanvasNodeDoubleClicked(uint64 InNodeId) -> void
{
    if (NOT SelectionModel.IsValid())
    {
        return;
    }

    const auto* Node = _CanvasNodes.Find(InNodeId);
    if (Node == nullptr || NOT Node->IsValid() || ck::Is_NOT_Valid((*Node)->Entity))
    {
        return;
    }

    _bNavigatingFromGraph = true;
    SelectionModel->Set_SelectedEntities({(*Node)->Entity});
    _bNavigatingFromGraph = false;
    RebuildGraph();
}

auto FCkDebuggerPage_Overview::OnCanvasNodeContextMenu(uint64 InNodeId,
                                                       const FPointerEvent& InMouseEvent) -> void
{
    const auto* Node = _CanvasNodes.Find(InNodeId);
    if (Node == nullptr || NOT Node->IsValid())
    {
        return;
    }

    ck::DebugCopyMenu::Push_CopyTextMenu(_GraphCanvas.ToSharedRef(),
                                         InMouseEvent.GetScreenSpacePosition(),
                                         (*Node)->DisplayName);
}

// =====================================================================================================================

auto FCkDebuggerPage_Overview::RebuildGraph(bool InForceCards) -> void
{
    if (NOT SelectionModel.IsValid())
    {
        return;
    }

    if (SelectionModel->Get_SelectionCount() == 0)
    {
        ClearGraph();
        return;
    }

    const auto PrimaryEntity = SelectionModel->Get_PrimarySelection();
    if (ck::Is_NOT_Valid(PrimaryEntity))
    {
        ClearGraph();
        return;
    }

    const auto TopologyChanged = _RuntimeGraphModel.RebuildFromEntity(PrimaryEntity);
    if (TopologyChanged || InForceCards)
    {
        RebuildCanvasScene(true);
        Apply_InspectorFilterToGraph();
    }
}

auto FCkDebuggerPage_Overview::RebuildCanvasScene(bool InFrameAll) -> void
{
    if (NOT _GraphCanvas.IsValid())
    {
        return;
    }

    auto PreviousCards = MoveTemp(_EntityCards);
    auto NewCards = TMap<uint64, TSharedPtr<SCkEcsEntityGraphCard>>{};
    auto NewCanvasNodes = TMap<uint64, TSharedPtr<FCkEcsRuntimeGraphNode>>{};
    auto StableToCanvasId = TMap<FString, uint64>{};
    auto Scene = FCkDebug_GraphCanvasScene{};

    for (const auto& RuntimeNode : _RuntimeGraphModel.GetNodes())
    {
        if (NOT RuntimeNode.IsValid() || ck::Is_NOT_Valid(RuntimeNode->Entity))
        {
            continue;
        }

        const auto CanvasId = ck_ecs_debugger_page_overview::MakeCanvasId(RuntimeNode->Entity);
        if (CanvasId == 0)
        {
            continue;
        }

        auto Card = TSharedPtr<SCkEcsEntityGraphCard>{};
        if (const auto* ExistingCard = PreviousCards.Find(CanvasId); ExistingCard != nullptr &&
                                                           ExistingCard->IsValid())
        {
            Card = *ExistingCard;
            Card->SetNode(RuntimeNode);
        }
        else
        {
            Card = SNew(SCkEcsEntityGraphCard).Node(RuntimeNode).bSelected(false);
        }
        Card->SlatePrepass();

        auto CanvasNode = FCkDebug_GraphCanvasNode{};
        CanvasNode.Id = CanvasId;
        CanvasNode.Position = FVector2D{static_cast<double>(RuntimeNode->Position.X),
                                        static_cast<double>(RuntimeNode->Position.Y)};
        CanvasNode.Size = Card->GetDesiredSize();
        CanvasNode.Widget = Card;
        Scene.Nodes.Add(MoveTemp(CanvasNode));

        StableToCanvasId.Add(RuntimeNode->StableId, CanvasId);
        NewCards.Add(CanvasId, Card);
        NewCanvasNodes.Add(CanvasId, RuntimeNode);
    }

    for (const auto& RuntimeEdge : _RuntimeGraphModel.GetEdges())
    {
        const auto* SourceId = StableToCanvasId.Find(RuntimeEdge.SourceNodeId);
        const auto* TargetId = StableToCanvasId.Find(RuntimeEdge.TargetNodeId);
        if (SourceId == nullptr || TargetId == nullptr)
        {
            continue;
        }

        auto CanvasEdge = FCkDebug_GraphCanvasEdge{};
        CanvasEdge.SourceId = *SourceId;
        CanvasEdge.TargetId = *TargetId;
        CanvasEdge.Color = RuntimeEdge.Color;
        CanvasEdge.Thickness = 1.5f;
        CanvasEdge.LineSeparation = 4.5f;
        CanvasEdge.IsDirected = RuntimeEdge.bIsDirected;
        Scene.Edges.Add(MoveTemp(CanvasEdge));
    }

    _EntityCards = MoveTemp(NewCards);
    _CanvasNodes = MoveTemp(NewCanvasNodes);
    _GraphCanvas->Set_Scene(MoveTemp(Scene));
    if (InFrameAll)
    {
        _PendingFrameAll = true;
    }
}

auto FCkDebuggerPage_Overview::Apply_InspectorFilterToGraph() -> void
{
    const auto HasFilter = FilterModel.IsValid();
    for (const auto& Node : _RuntimeGraphModel.GetNodes())
    {
        if (NOT Node.IsValid())
        {
            continue;
        }
        Node->bIsFilterMatch = HasFilter ? FilterModel->Test_Entity(Node->Entity) : true;
    }

    if (_GraphCanvas.IsValid())
    {
        _GraphCanvas->Invalidate(EInvalidateWidgetReason::Paint);
    }
}

auto FCkDebuggerPage_Overview::ClearGraph() -> void
{
    const auto WasPopulated = _RuntimeGraphModel.Clear();
    _EntityCards.Reset();
    _CanvasNodes.Reset();
    _PendingFrameAll = false;
    if (WasPopulated && _GraphCanvas.IsValid())
    {
        _GraphCanvas->Set_Scene(FCkDebug_GraphCanvasScene{});
    }
}
