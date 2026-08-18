#include "SCkGoapDebugger_GraphPane.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Graph/SCkDebug_GraphCanvas.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkGoap/Planner/CkGoap_Planner_Fragment_Data.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/CkGoapDebugger_Axes.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DecisionModel.h"
#include "CkGoapDebugger/Graph/CkGoapRuntimeGraphModel.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_goap_debugger_graph_pane
{
    auto MakePlannerScopeId(const FCk_Handle_Goap_Planner& InPlanner) -> uint64
    {
        if (ck::Is_NOT_Valid(InPlanner))
        {
            return 0;
        }

        const auto Entity = InPlanner.Get_Entity();
        const auto Number = static_cast<uint64>(Entity.Get_EntityNumber());
        const auto Version = static_cast<uint64>(Entity.Get_VersionNumber());
        return ((Version << 32) | Number) + 1;
    }

    auto TagLeaf(const FGameplayTag& InTag) -> FString
    {
        const FString Full = InTag.ToString();
        int32 Dot = INDEX_NONE;
        return Full.FindLastChar(TEXT('.'), Dot) ? Full.Mid(Dot + 1) : Full;
    }

    auto MakeDot(const TAttribute<FSlateColor>& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride_Lambda([]() -> FOptionalSize
            {
                return FOptionalSize{ck_goap_debugger_axes::Get_DotSize()};
            })
            .HeightOverride_Lambda([]() -> FOptionalSize
            {
                return FOptionalSize{ck_goap_debugger_axes::Get_DotSize()};
            })
            [SNew(SBorder)
                 .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                 .BorderBackgroundColor(InColor)];
    }

    auto BuildActionCard(const TSharedPtr<FCkGoapRuntimeGraphNode>& Node, int32 NameDepth)
        -> TSharedRef<SWidget>
    {
        const TWeakPtr<FCkGoapRuntimeGraphNode> Weak = Node;
        auto Conditions = SNew(SVerticalBox);
        auto Sorted = Node->Action.Preconditions;
        Sorted.Sort([](const auto& A, const auto& B)
        {
            const auto ALeaf = TagLeaf(A.Key);
            const auto BLeaf = TagLeaf(B.Key);
            return ALeaf == BLeaf ? A.Key.ToString() < B.Key.ToString() : ALeaf < BLeaf;
        });
        for (const auto& Pre : Sorted)
        {
            const auto Key = Pre.Key;
            const bool Wants = Pre.Value;
            Conditions->AddSlot().AutoHeight().Padding(
                ck_goap_debugger_axes::Live_RowDensity(FMargin{0.0f, 1.0f}))
                [SNew(SHorizontalBox)
                     .ToolTipText_Lambda(
                         [Weak, Key, Wants]()
                         {
                             const auto P = Weak.Pin();
                             const bool* Current = P.IsValid() ? P->WorldState.Find(Key) : nullptr;
                             return FText::FromString(FString::Printf(
                                 TEXT("%s\nWants: %s\nCurrent: %s — %s"),
                                 *Key.ToString(),
                                 Wants ? TEXT("true") : TEXT("false"),
                                 Current == nullptr ? TEXT("(not in WorldState)")
                                                    : (*Current ? TEXT("TRUE") : TEXT("false")),
                                 Current == nullptr ? TEXT("unknown")
                                                    : (*Current == Wants ? TEXT("satisfied")
                                                                         : TEXT("NOT satisfied"))));
                         }) +
                 SHorizontalBox::Slot().AutoWidth().VAlign(
                     VAlign_Center)[MakeDot(FSlateColor(Wants ? CkStyle::Ok() : CkStyle::Err()))] +
                 SHorizontalBox::Slot().AutoWidth().Padding(
                     2, 0)[SNew(STextBlock)
                               .Text(FText::FromString(TEXT("→")))
                               .Font_Lambda(
                                   []
                                   {
                                       return ck::debug_axes::ScaledFont(
                                           "Regular", CkStyle::FontSizeMicro());
                                   })
                               .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))] +
                 SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)[
                     MakeDot(TAttribute<FSlateColor>::CreateLambda(
                         [Weak, Key]()
                         {
                             const auto P = Weak.Pin();
                             const bool* Current = P.IsValid() ? P->WorldState.Find(Key) : nullptr;
                             return FSlateColor(Current == nullptr
                                                    ? CkStyle::TextMute()
                                                    : (*Current ? CkStyle::Ok() : CkStyle::Err()));
                         }))] +
                 SHorizontalBox::Slot().FillWidth(
                     1)[SNew(STextBlock)
                            .Text(FText::FromString(TagLeaf(Key)))
                            .Font_Lambda(
                                []
                                {
                                    return ck::debug_axes::ScaledFont("Regular",
                                                                      CkStyle::NodeMetaFontSize());
                                })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)]];
        }
        auto Effects = SNew(SVerticalBox);
        auto SortedEffects = Node->Action.Effects;
        SortedEffects.Sort([](const auto& A, const auto& B)
        {
            const auto ALeaf = TagLeaf(A.Key);
            const auto BLeaf = TagLeaf(B.Key);
            return ALeaf == BLeaf ? A.Key.ToString() < B.Key.ToString() : ALeaf < BLeaf;
        });
        for (const auto& Effect : SortedEffects)
            Effects->AddSlot().AutoHeight().Padding(
                ck_goap_debugger_axes::Live_RowDensity(FMargin{0.0f, 1.0f}))[
                SNew(SHorizontalBox).ToolTipText(FText::FromString(Effect.Key.ToString())) +
                      SHorizontalBox::Slot().FillWidth(1).HAlign(
                          HAlign_Right)[SNew(STextBlock)
                                            .Text(FText::FromString(TagLeaf(Effect.Key)))
                                            .Font_Lambda(
                                                []
                                                {
                                                    return ck::debug_axes::ScaledFont(
                                                        "Regular", CkStyle::NodeMetaFontSize());
                                             })
                                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)] +
                       SHorizontalBox::Slot().AutoWidth().Padding(
                           4, 0, 0, 0)[MakeDot(FSlateColor(CkStyle::Accent()))]];
        TSharedRef<SWidget> Relations = SNullWidget::NullWidget;
        if (NOT Sorted.IsEmpty() && NOT SortedEffects.IsEmpty())
        {
            Relations = SNew(SHorizontalBox) +
                        SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 12.0f, 0.0f)
                            [Conditions] +
                        SHorizontalBox::Slot().FillWidth(1.0f)[Effects];
        }
        else if (NOT Sorted.IsEmpty())
        {
            Relations = Conditions;
        }
        else if (NOT SortedEffects.IsEmpty())
        {
            Relations = Effects;
        }
        return SNew(SBox).MinDesiredWidth(180.0f).MaxDesiredWidth(420.0f)
            [SNew(SOverlay) +
             SOverlay::Slot().Padding(
                 -10.0f)[SNew(SImage)
                             .Image(FCkDebuggerCommonStyle::Get_GlowTightBrush())
                             .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                             .Visibility_Lambda(
                                 [Weak]
                                 {
                                     const auto P = Weak.Pin();
                                     return P.IsValid() && P->IsInPlan
                                                ? EVisibility::HitTestInvisible
                                                : EVisibility::Collapsed;
                                 })] +
             SOverlay::Slot()
                 [SNew(SBorder)
                      .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                      .BorderBackgroundColor_Lambda(
                          [Weak]
                          {
                              const auto P = Weak.Pin();
                              return FSlateColor(!P.IsValid() ? CkStyle::NodeBorder_Inactive()
                                                  : P->IsSelected       ? CkStyle::Warn()
                                                  : P->IsFailureBlocked ? CkStyle::Err()
                                                 : P->IsInPlan ? CkStyle::NodeBorder_InPlan()
                                                 : P->Action.ChildActionHandles.Num() > 0
                                                     ? CkStyle::CategoryAge()
                                                     : CkStyle::Accent());
                          })
                      .Padding_Lambda(
                          []
                          {
                              return FMargin(ck::debug_axes::Get_NodeBorderThickness());
                          })
                          [SNew(SBorder)
                               .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                               .BorderBackgroundColor_Lambda(
                                   [Weak]
                                   {
                                       if (!ck_goap_debugger_axes::Get_NodeDrawsFill())
                                           return FSlateColor(FLinearColor::Transparent);
                                       const auto P = Weak.Pin();
                                       return FSlateColor(P.IsValid() && P->IsInPlan
                                                              ? CkStyle::NodeFill_InPlan()
                                                              : CkStyle::NodeFill_Inactive());
                                   })
                               .Padding_Lambda(
                                   []
                                   {
                                       return FMargin(
                                           CkStyle::SpaceM *
                                               ck_goap_debugger_axes::Get_NodePaddingScale(),
                                           CkStyle::SpaceS *
                                               ck_goap_debugger_axes::Get_NodePaddingScale());
                                   })
                                   [SNew(SVerticalBox) +
                                    SVerticalBox::Slot().AutoHeight()
                                        [SNew(SHorizontalBox) +
                                         SHorizontalBox::Slot().FillWidth(1)
                                             [SNew(STextBlock)
                                                  .Text(FText::FromString(
                                                      SCkDebug_NameLabel::Get_ShortName(
                                                          Node->Action
                                                              .ClassName,
                                                          NameDepth)))
                                                  .Font_Lambda(
                                                      []
                                                      {
                                                          return ck::debug_axes::ScaledFont(
                                                              "Bold", CkStyle::NodeTitleFontSize());
                                                      })
                                                  .ColorAndOpacity(FSlateColor(CkStyle::Text()))] +
                                         SHorizontalBox::Slot().AutoWidth()
                                             [SNew(STextBlock)
                                                  .Text_Lambda(
                                                      [Weak]
                                                       {
                                                           const auto P = Weak.Pin();
                                                           return FText::FromString(
                                                              P.IsValid()
                                                                  ? FString::Printf(TEXT("$%.0f"),
                                                                                    P->Action.Cost)
                                                                   : TEXT(""));
                                                       })
                                                   .Font_Lambda(
                                                       []
                                                       {
                                                           return ck::debug_axes::ScaledFont(
                                                               "Bold", CkStyle::NodeCostFontSize());
                                                       })
                                                   .ColorAndOpacity(FSlateColor(CkStyle::Warn()))]] +
                                    SVerticalBox::Slot().AutoHeight()
                                        [SNew(STextBlock)
                                             .Text_Lambda(
                                                 [Weak]()
                                                 {
                                                     const auto P = Weak.Pin();
                                                     if (NOT P.IsValid())
                                                     {
                                                         return FText::GetEmpty();
                                                     }
                                                     if (P->Action.IsPlannerRole)
                                                     {
                                                         return FText::FromString(
                                                             TEXT("◆● ACTION+PLANNER"));
                                                     }
                                                     if (P->Action.Cost >=
                                                         ck_goap_debugger_decision_model::
                                                             k_FallbackCostFloor)
                                                     {
                                                         return FText::FromString(
                                                             TEXT("● FALLBACK"));
                                                     }
                                                     return FText::FromString(TEXT("● ACTION"));
                                                 })
                                             .Font_Lambda(
                                                 []
                                                 {
                                                     return ck::debug_axes::ScaledFont(
                                                         "Bold", CkStyle::FontSizeMicro());
                                                 })
                                             .ColorAndOpacity_Lambda(
                                                 [Weak]()
                                                 {
                                                     const auto P = Weak.Pin();
                                                     return FSlateColor(
                                                         P.IsValid() &&
                                                                 P->Action.Cost >=
                                                                     ck_goap_debugger_decision_model::
                                                                         k_FallbackCostFloor
                                                             ? CkStyle::Warn()
                                                             : CkStyle::TextMute());
                                                 })] +
                                    SVerticalBox::Slot().AutoHeight().Padding(0, CkStyle::SpaceXS)
                                        [SNew(SBorder)
                                             .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrus"
                                                                                          "h")))
                                             .BorderBackgroundColor(
                                                 CkStyle::OverlayOf(CkStyle::CategoryAge(), 0.15f))
                                             .Visibility_Lambda(
                                                 [Weak]
                                                 {
                                                     const auto P = Weak.Pin();
                                                     return P.IsValid() &&
                                                                    P->Action.ChildActionHandles
                                                                            .Num() > 0
                                                                ? EVisibility::SelfHitTestInvisible
                                                                : EVisibility::Collapsed;
                                                 })[SNew(STextBlock)
                                                         .Text(FText::FromString(FString::Printf(
                                                             TEXT("› %s"),
                                                             *TagLeaf(Node->Action.ActionTag))))
                                                         .Font_Lambda(
                                                             []
                                                             {
                                                                 return ck::debug_axes::ScaledFont(
                                                                     "Regular",
                                                                     CkStyle::NodeMetaFontSize());
                                                             })
                                                         .ColorAndOpacity(
                                                             FSlateColor(CkStyle::CategoryAge()))]] +
                                     SVerticalBox::Slot().AutoHeight().Padding(0, CkStyle::SpaceS)
                                         [Relations]]]] +
              SOverlay::Slot()
                  .HAlign(HAlign_Left)
                  .VAlign(VAlign_Top)
                  .Padding(
                      -6.0f)[SNew(SBox)
                                 .WidthOverride_Lambda(
                                     []() -> FOptionalSize
                                     {
                                         return FOptionalSize{
                                             ck_goap_debugger_axes::Get_IconSize()};
                                     })
                                 .HeightOverride_Lambda(
                                     []() -> FOptionalSize
                                     {
                                         return FOptionalSize{
                                             ck_goap_debugger_axes::Get_IconSize()};
                                     })
                                 .Visibility_Lambda(
                                     [Weak]
                                     {
                                         const auto P = Weak.Pin();
                                         return P.IsValid() && P->IsInPlan && P->PlanStepIndex > 0
                                                    ? EVisibility::SelfHitTestInvisible
                                                    : EVisibility::Collapsed;
                                     })[SNew(SBorder)
                                            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                                            .BorderBackgroundColor(CkStyle::NodeBorder_InPlan())
                                            .HAlign(HAlign_Center)
                                            .VAlign(VAlign_Center)[SNew(STextBlock)
                                                                       .Text_Lambda(
                                                                           [Weak]
                                                                           {
                                                                               const auto P = Weak.Pin();
                                                                               return FText::FromString(
                                                                                   P.IsValid()
                                                                                       ? FString::FromInt(
                                                                                             P->PlanStepIndex)
                                                                                       : TEXT(""));
                                                                           })
                                                                       .Font_Lambda(
                                                                           []
                                                                           {
                                                                               return ck::debug_axes::ScaledFont(
                                                                                   "Bold",
                                                                                   CkStyle::NodeCostFontSize());
                                                                           })
                                                                       .ColorAndOpacity(
                                                                           FSlateColor(CkStyle::TextStrong()))]]]];
    }

    auto BuildGoalCard(const TSharedPtr<FCkGoapRuntimeGraphNode>& Node) -> TSharedRef<SWidget>
    {
        auto Rows = SNew(SVerticalBox);
        for (const auto& Condition : Node->GoalConditions)
            Rows->AddSlot().AutoHeight().Padding(ck_goap_debugger_axes::Live_RowDensity(FMargin{
                0.0f,
                1.0f}))[SNew(STextBlock)
                            .Text(FText::FromString(
                                FString::Printf(TEXT("%s = %s"),
                                                *TagLeaf(Condition.Key),
                                                Condition.Value ? TEXT("true") : TEXT("false"))))
                            .Font_Lambda(
                                []
                                {
                                    return ck::debug_axes::ScaledFont("Regular",
                                                                      CkStyle::NodeMetaFontSize());
                                })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)];
        return SNew(SBox)
            .MinDesiredWidth(FCkGoapDebuggerStyle::GraphNode_Width)
            .MaxDesiredWidth(FCkGoapDebuggerStyle::GraphNode_MaxWidth)
                [SNew(SBorder)
                     .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                     .BorderBackgroundColor(CkStyle::NodeBorder_Goal())
                     .Padding_Lambda(
                         []
                         {
                             return FMargin{ck::debug_axes::Get_NodeBorderThickness()};
                         })[SNew(SBorder)
                                .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                                .BorderBackgroundColor_Lambda(
                                    []
                                    {
                                        return ck_goap_debugger_axes::Get_NodeDrawsFill()
                                                   ? FSlateColor(CkStyle::NodeFill_Goal())
                                                   : FSlateColor(FLinearColor::Transparent);
                                    })
                                .Padding_Lambda(
                                    []
                                    {
                                        const float Scale =
                                            ck_goap_debugger_axes::Get_NodePaddingScale();
                                        return FMargin{CkStyle::SpaceM * Scale,
                                                       CkStyle::SpaceS * Scale};
                                    })[SNew(SVerticalBox) +
                                       SVerticalBox::Slot().AutoHeight()
                                           [SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("◆ Goal")))
                                                .Font_Lambda(
                                                    []
                                                    {
                                                        return ck::debug_axes::ScaledFont(
                                                            "Bold", CkStyle::NodeTitleFontSize());
                                                    })
                                                .ColorAndOpacity(
                                                    FSlateColor(CkStyle::NodeBorder_Goal()))] +
                                       SVerticalBox::Slot().AutoHeight().Padding(
                                           0, CkStyle::SpaceS)[Rows]]]];
    }
} // namespace ck_goap_debugger_graph_pane

SCkGoapDebugger_GraphPane::SCkGoapDebugger_GraphPane() = default;
SCkGoapDebugger_GraphPane::~SCkGoapDebugger_GraphPane()
{
    if (_GraphCanvas.IsValid())
        _GraphCanvas->Clear_InteractionDelegates();
    if (_OnChangedHandle.IsValid() && _ViewModel.IsValid())
        _ViewModel->OnChanged.Remove(_OnChangedHandle);
    // Member destruction releases graph snapshots and cards. Do not run the live-widget reset here:
    // module shutdown can destroy this pane after SlateApplication has already shut down.
}

auto SCkGoapDebugger_GraphPane::Construct(const FArguments& InArgs) -> void
{
    constexpr auto IsNodeDraggingEnabled = true;
    _ViewModel = InArgs._ViewModel;
    _Graph = MakeShared<FCkGoapRuntimeGraphModel>();
    ChildSlot[SNew(SBorder).BorderImage(
        FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Surface")))
                  [SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[BuildHeader()] +
                   SVerticalBox::Slot().FillHeight(
                       1)[SAssignNew(_GraphCanvas, SCkDebug_GraphCanvas)
                                .AllowNodeDragging(IsNodeDraggingEnabled)
                               .OnSelectionChanged(FOnCkDebug_GraphCanvasSelectionChanged::CreateSP(
                                   this, &SCkGoapDebugger_GraphPane::OnGraphSelectionChanged))
                               .OnNodeMoved(FOnCkDebug_GraphCanvasNodeMoved::CreateSP(
                                   this, &SCkGoapDebugger_GraphPane::OnGraphNodeMoved))]]];
    if (_ViewModel.IsValid())
        _OnChangedHandle =
            _ViewModel->OnChanged.AddSP(this, &SCkGoapDebugger_GraphPane::RefreshFromViewModel);
    RefreshFromViewModel();
}

auto SCkGoapDebugger_GraphPane::Reset_ForWorldChange() -> void
{
    if (_Graph.IsValid())
        _Graph->Reset();
    if (_GraphCanvas.IsValid())
    {
        _SuppressSelectionEcho = true;
        _GraphCanvas->Set_Scene({});
        _SuppressSelectionEcho = false;
    }
    _LastTopologyHash = 0;
    _LastEffectiveGoalHash = 0;
    _LastNameDepth = INDEX_NONE;
    _LastSelectedAction = {};
    _CardWidgets.Reset();
    _NodePositionOverrides.Reset();
    _ManualPositionScopeId = 0;
    if (_HeaderText.IsValid())
        _HeaderText->SetText(FText::FromString(TEXT("Action graph - (no selection)")));
}
auto SCkGoapDebugger_GraphPane::Get_MaxNameDepth() const -> int32
{
    return _Graph.IsValid() ? _Graph->GetMaxNameDepth() : 1;
}

auto SCkGoapDebugger_GraphPane::Refresh_ForStyleChange() -> void
{
    if (!_Graph.IsValid())
    {
        return;
    }
    _Graph->Relayout(_ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1);
    RebuildCanvasScene();
}

auto SCkGoapDebugger_GraphPane::RefreshFromViewModel() -> void
{
    if (!_Graph.IsValid())
        return;
    const auto* Planner = _ViewModel.IsValid() ? _ViewModel->GetSelectedPlannerInfo() : nullptr;
    if (Planner == nullptr)
    {
        Reset_ForWorldChange();
        if (_HeaderText.IsValid())
            _HeaderText->SetText(FText::FromString(TEXT("Action graph - (no Planner selected)")));
        return;
    }
    const auto Selected = _ViewModel->GetSelectedAction();
    const auto ManualPositionScopeId =
        ck_goap_debugger_graph_pane::MakePlannerScopeId(Planner->PlannerHandle);
    if (_ManualPositionScopeId != ManualPositionScopeId)
    {
        _ManualPositionScopeId = ManualPositionScopeId;
        _NodePositionOverrides.Reset();
    }
    const auto Hash = FCkGoapRuntimeGraphModel::ComputeTopologyHash(*Planner);
    const auto* SelectedInfo = _ViewModel->GetSelectedActionInfo();
    const auto EffectiveGoalHash = FCkGoapRuntimeGraphModel::ComputeEffectiveGoalHash(*Planner,
                                                                                      SelectedInfo);
    const bool TopologyChanged = Hash != _LastTopologyHash;
    const bool NameDepthChanged = _ViewModel->Get_NameDepth() != _LastNameDepth;
    const bool SelectedGoalChanged = EffectiveGoalHash != _LastEffectiveGoalHash;
    const bool SceneStructureChanged = TopologyChanged || NameDepthChanged || SelectedGoalChanged;
    if (SceneStructureChanged)
    {
        // Selection participates because a selected dual-role Action can own
        // a different effective goal. Rebuild model and goal-producing edges
        // together so the card and its connections never describe different
        // selected Actions for a frame.
        _CardWidgets.Reset();
        _Graph->Rebuild(*Planner, Selected, _ViewModel->Get_NameDepth());
        _LastTopologyHash = Hash;
        _LastEffectiveGoalHash = EffectiveGoalHash;
        _LastNameDepth = _ViewModel->Get_NameDepth();
        RebuildCanvasScene();
    }
    else
    {
        // Existing cards bind to these DTOs. Ordinary live broadcasts repaint
        // them without reinstalling every Slate child in the canvas.
        if (_Graph->UpdateRuntimeState(*Planner, Selected))
        {
            RebuildCanvasScene();
        }
    }
    _LastSelectedAction = Selected;
    if (_HeaderText.IsValid())
    {
        const auto* Info = _ViewModel->GetSelectedActionInfo();
        _HeaderText->SetText(FText::FromString(
            FString::Printf(TEXT("Action graph - selected: %s - %d actions - %d edges"),
                            Info ? *Info->ClassName : TEXT("(none)"),
                            _Graph->GetActionCount(),
                            _Graph->GetEdgeCount())));
    }
    if (_GraphCanvas.IsValid())
    {
        TSet<uint64> Selection;
        const uint64 Id = _Graph->FindActionId(Selected);
        if (Id)
            Selection.Add(Id);
        _SuppressSelectionEcho = true;
        _GraphCanvas->Set_SelectedNodeIds(MoveTemp(Selection), false);
        _SuppressSelectionEcho = false;
    }
}

auto SCkGoapDebugger_GraphPane::RebuildCanvasScene() -> void
{
    if (!_GraphCanvas.IsValid())
        return;

    auto MeasuredSizes = TMap<uint64, FVector2D>{};
    for (const auto& Node : _Graph->GetNodes())
    {
        auto* CardWidget = _CardWidgets.Find(Node->Id);
        if (CardWidget == nullptr)
        {
            auto NewCardWidget = Node->Kind == ECkGoapRuntimeGraphNodeKind::Action
                                     ? ck_goap_debugger_graph_pane::BuildActionCard(
                                           Node, _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1)
                                     : ck_goap_debugger_graph_pane::BuildGoalCard(Node);
            CardWidget = &_CardWidgets.Add(Node->Id, MoveTemp(NewCardWidget));
        }
        (*CardWidget)->SlatePrepass();
        MeasuredSizes.Add(Node->Id, (*CardWidget)->GetDesiredSize());
    }
    _Graph->ApplyMeasuredNodeSizes(MeasuredSizes);

    auto FullGraphNodeIds = TSet<uint64>{};
    for (const auto& Node : _Graph->GetNodes())
    {
        FullGraphNodeIds.Add(Node->Id);
    }
    for (auto It = _NodePositionOverrides.CreateIterator(); It; ++It)
    {
        if (!FullGraphNodeIds.Contains(It.Key()))
        {
            It.RemoveCurrent();
        }
    }

    FCkDebug_GraphCanvasScene Scene;
    for (const auto& Node : _Graph->GetNodes())
    {
        if (_HideDimmed && Node->Kind == ECkGoapRuntimeGraphNodeKind::Action && NOT Node->IsInPlan)
        {
            continue;
        }
        FCkDebug_GraphCanvasNode CanvasNode;
        CanvasNode.Id = Node->Id;
        CanvasNode.Position = Node->Position;
        if (const auto* PositionOverride = _NodePositionOverrides.Find(Node->Id))
        {
            CanvasNode.Position = *PositionOverride;
            CanvasNode.bHasManualPosition = true;
        }
        CanvasNode.Size = Node->Size;
        CanvasNode.Widget = _CardWidgets.FindChecked(Node->Id).ToSharedRef();
        Scene.Nodes.Add(MoveTemp(CanvasNode));
    }
    const auto FindNode = [this](uint64 InId) -> const FCkGoapRuntimeGraphNode*
    {
        for (const auto& Node : _Graph->GetNodes())
            if (Node.IsValid() && Node->Id == InId)
                return Node.Get();
        return nullptr;
    };
    const auto IsEdgeInPlan = [](const FCkGoapRuntimeGraphNode* InSource,
                                 const FCkGoapRuntimeGraphNode* InTarget) -> bool
    {
        return InSource && InTarget && InSource->Kind == ECkGoapRuntimeGraphNodeKind::Action &&
               InSource->IsInPlan &&
               (InTarget->Kind == ECkGoapRuntimeGraphNodeKind::Goal || InTarget->IsInPlan);
    };
    const auto IsEdgeFailureBlocked = [](const FCkGoapRuntimeGraphNode* InSource,
                                         const FCkGoapRuntimeGraphNode* InTarget) -> bool
    {
        return (InSource && InSource->Kind == ECkGoapRuntimeGraphNodeKind::Action &&
                InSource->IsFailureBlocked) ||
               (InTarget && InTarget->Kind == ECkGoapRuntimeGraphNodeKind::Action &&
                InTarget->IsFailureBlocked);
    };
    const float ThicknessScale = ck_goap_debugger_axes::Get_NodeBorderScale();
    const float DimScale = ck_goap_debugger_axes::Get_NodeDimScale();
    const auto IsVisibleNode = [this](const FCkGoapRuntimeGraphNode* InNode) -> bool
    {
        return InNode != nullptr &&
               (NOT _HideDimmed || InNode->Kind != ECkGoapRuntimeGraphNodeKind::Action ||
                InNode->IsInPlan);
    };
    for (const auto& Edge : _Graph->GetEdges())
    {
        const auto* SourceNode = FindNode(Edge.SourceId);
        const auto* TargetNode = FindNode(Edge.TargetId);
        if (NOT IsVisibleNode(SourceNode) || NOT IsVisibleNode(TargetNode))
        {
            continue;
        }
        const bool IsInPlan = IsEdgeInPlan(SourceNode, TargetNode);
        FCkDebug_GraphCanvasEdge CanvasEdge;
        CanvasEdge.SourceId = Edge.SourceId;
        CanvasEdge.TargetId = Edge.TargetId;
        CanvasEdge.SourceAnchor = ECkDebug_GraphAnchor::Right;
        CanvasEdge.TargetAnchor = ECkDebug_GraphAnchor::Left;
        CanvasEdge.IsDashed = Edge.Kind == ECkGoapRuntimeGraphEdgeKind::Tree;
        CanvasEdge.DeemphasizeWhenUnrelatedHovered = false;
        if (CanvasEdge.IsDashed)
        {
            CanvasEdge.DashGap = 6.0f;
            CanvasEdge.Color = IsInPlan ? CkStyle::Ok() : CkStyle::CategoryAge();
            CanvasEdge.Color.A = IsInPlan ? 1.0f : FMath::Clamp(0.55f * DimScale, 0.05f, 1.0f);
            CanvasEdge.Thickness = (IsInPlan ? 2.0f : 1.25f) * ThicknessScale;
        }
        else if (IsEdgeFailureBlocked(SourceNode, TargetNode))
        {
            CanvasEdge.LineSeparation = 4.5f;
            CanvasEdge.Color = CkStyle::Err();
            CanvasEdge.Thickness = 2.0f * ThicknessScale;
        }
        else if (IsInPlan)
        {
            CanvasEdge.LineSeparation = 4.5f;
            CanvasEdge.Color = CkStyle::Ok();
            CanvasEdge.Thickness = 2.0f * ThicknessScale;
        }
        else
        {
            CanvasEdge.LineSeparation = 4.5f;
            CanvasEdge.Color = CkStyle::Border();
            CanvasEdge.Color.A = FMath::Clamp(0.65f * DimScale, 0.05f, 1.0f);
            CanvasEdge.Thickness = 1.0f * ThicknessScale;
        }
        Scene.Edges.Add(MoveTemp(CanvasEdge));
    }
    _SuppressSelectionEcho = true;
    _GraphCanvas->Set_Scene(MoveTemp(Scene));
    _SuppressSelectionEcho = false;
}

auto SCkGoapDebugger_GraphPane::OnGraphSelectionChanged(const TSet<uint64>& InSelection) -> void
{
    if (_SuppressSelectionEcho || !_ViewModel.IsValid())
        return;
    for (uint64 Id : InSelection)
        if (const auto* Action = _Graph->FindActionById(Id))
        {
            _ViewModel->SetSelectedAction(*Action);
            return;
        }
    _ViewModel->SetSelectedAction({});
}

auto SCkGoapDebugger_GraphPane::OnGraphNodeMoved(const uint64 InNodeId,
                                                 const FVector2D& InPosition) -> void
{
    _NodePositionOverrides.Add(InNodeId, InPosition);
    RebuildCanvasScene();
}

auto SCkGoapDebugger_GraphPane::ResetManualNodePositions() -> void
{
    if (_NodePositionOverrides.IsEmpty())
    {
        return;
    }

    _NodePositionOverrides.Reset();
    RebuildCanvasScene();
}

auto SCkGoapDebugger_GraphPane::Request_SetHideDimmed(bool InHideDimmed) -> void
{
    if (_HideDimmed == InHideDimmed)
    {
        return;
    }

    _HideDimmed = InHideDimmed;
    RebuildCanvasScene();
}

auto SCkGoapDebugger_GraphPane::BuildHeader() -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Black")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().FillWidth(
                 1)[SAssignNew(_HeaderText, SCkDebug_SelectableLabel)
                        .Text(FText::FromString(TEXT("Action graph - (no selection)")))
                        .ColorAndOpacity(FSlateColor(CkStyle::Text()))] +
              SHorizontalBox::Slot().AutoWidth()[SNew(SButton)
                                                     .Text(FText::FromString(TEXT("Fit")))
                                                     .ToolTipText(FText::FromString(TEXT("Fit graph to view")))
                                                    .OnClicked_Lambda(
                                                        [this]
                                                        {
                                                            if (_GraphCanvas.IsValid())
                                                                _GraphCanvas->Frame_All();
                                                            return FReply::Handled();
                                                        })] +
              SHorizontalBox::Slot().AutoWidth().Padding(
                  4, 0)[SNew(SButton)
                            .Text(FText::FromString(TEXT("Reset nodes")))
                            .ToolTipText(FText::FromString(
                                TEXT("Restore automatic graph node positions.")))
                            .IsEnabled_Lambda([this] { return !_NodePositionOverrides.IsEmpty(); })
                            .OnClicked_Lambda(
                                [this]
                                {
                                    ResetManualNodePositions();
                                    return FReply::Handled();
                                })] +
              SHorizontalBox::Slot().AutoWidth().Padding(
                  4, 0)[SNew(SButton)
                            .Text(FText::FromString(TEXT("1:1")))
                            .ToolTipText(FText::FromString(TEXT("Reset pan and zoom")))
                            .OnClicked_Lambda(
                                [this]
                                {
                                    if (_GraphCanvas.IsValid())
                                    {
                                        _GraphCanvas->Reset_View();
                                    }
                                    return FReply::Handled();
                                })] +
              SHorizontalBox::Slot().AutoWidth()[SNew(SCkDebug_IconToggle)
                                                     .IconId(ECk_Icon::Dormant)
                                                     .Label(FText::FromString(TEXT("Hide dimmed")))
                                                     .ToolTip(FText::FromString(
                                                         TEXT("Hide off-plan nodes and their edges.")))
                                                     .IsOn_Lambda([this]() -> bool { return _HideDimmed; })
                                                     .OnStateChanged(
                                                         FOnCkDebug_IconToggleChanged::CreateSP(
                                                             this,
                                                             &SCkGoapDebugger_GraphPane::Request_SetHideDimmed))
                                                     .ShowLabel(true)] +
             SHorizontalBox::Slot().AutoWidth().Padding(
                 8, 0, 0, 0)[SNew(STextBlock)
                                 .Text(FText::FromString(TEXT("Name")))
                                 .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))] +
              SHorizontalBox::Slot().AutoWidth()[SNew(SButton)
                                                     .Text(FText::FromString(TEXT("◀")))
                                                     .ToolTipText(FText::FromString(
                                                         TEXT("Shorter display name (fewer segments)")))
                                                    .OnClicked_Lambda(
                                                        [this]
                                                        {
                                                            if (_ViewModel.IsValid())
                                                            {
                                                                const int32 D =
                                                                    _ViewModel->Get_NameDepth();
                                                                _ViewModel->Set_NameDepth(
                                                                    D == 0 ? Get_MaxNameDepth()
                                                                           : D - 1);
                                                                _ViewModel->Broadcast_Changed();
                                                            }
                                                            return FReply::Handled();
                                                        })] +
             SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock)
                                                    .Text_Lambda(
                                                        [this]
                                                        {
                                                            const int32 D =
                                                                _ViewModel.IsValid()
                                                                    ? _ViewModel->Get_NameDepth()
                                                                    : 1;
                                                            return FText::FromString(
                                                                D == 0 ? TEXT("Full")
                                                                       : FString::FromInt(D));
                                                        })
                                                    .MinDesiredWidth(28.0f)
                                                    .Justification(ETextJustify::Center)] +
              SHorizontalBox::Slot().AutoWidth()[SNew(SButton)
                                                     .Text(FText::FromString(TEXT("▶")))
                                                     .ToolTipText(FText::FromString(
                                                         TEXT("Longer display name (more segments)")))
                                                    .OnClicked_Lambda(
                                                        [this]
                                                        {
                                                            if (_ViewModel.IsValid())
                                                            {
                                                                const int32 D =
                                                                    _ViewModel->Get_NameDepth();
                                                                _ViewModel->Set_NameDepth(
                                                                    D >= Get_MaxNameDepth()
                                                                        ? 0
                                                                        : D + 1);
                                                                _ViewModel->Broadcast_Changed();
                                                            }
                                                            return FReply::Handled();
                                                        })]];
}
