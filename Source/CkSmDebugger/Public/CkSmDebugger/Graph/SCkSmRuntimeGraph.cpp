#include "CkSmDebugger/Graph/SCkSmRuntimeGraph.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "CkDebuggerCommon/Graph/SCkDebug_GraphCanvas.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NodePill.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

struct FCkSmRuntimeGraphCardPresentation
{
    FCkSmRuntimeGraphNode Node;
    bool bSelected = false;
};

namespace ck_sm_runtime_graph
{
    constexpr auto CompoundCornerRadius = 8.0f;

    auto GetCompoundBorderBrush() -> const FSlateBrush*
    {
        static auto Standard = FSlateRoundedBoxBrush(FLinearColor::White, CompoundCornerRadius);
        static auto Tight = FSlateRoundedBoxBrush(FLinearColor::White, CompoundCornerRadius * 0.5f);
        return ck_sm_debugger_axes::Get_NodeRadiusScale() < 1.0f ? &Tight : &Standard;
    }

    auto GetCompoundFillBrush() -> const FSlateBrush*
    {
        static auto Standard = FSlateRoundedBoxBrush(FLinearColor::White,
                                                     CompoundCornerRadius - 1.0f);
        static auto Tight = FSlateRoundedBoxBrush(FLinearColor::White,
                                                  (CompoundCornerRadius - 1.0f) * 0.5f);
        return ck_sm_debugger_axes::Get_NodeRadiusScale() < 1.0f ? &Tight : &Standard;
    }

    auto GetCardStructureHash(const FCkSmRuntimeGraphNode& InNode) -> uint32
    {
        auto Hash = GetTypeHash(InNode.Kind);
        Hash = HashCombine(Hash, GetTypeHash(InNode.bExpandTasks));
        if (InNode.State && InNode.bExpandTasks)
        {
            for (const auto& Task : InNode.State->Tasks)
            {
                Hash = HashCombine(Hash, GetTypeHash(Task.ClassName));
            }
        }
        if (InNode.Transition)
        {
            for (const auto& Condition : InNode.Transition->Conditions)
            {
                Hash = HashCombine(Hash, GetTypeHash(Condition.ClassName));
            }
        }
        return Hash;
    }

    auto IsInlineBreakpointStyle(const int32 InBreakpointStyle) -> bool
    {
        return InBreakpointStyle == 22 || InBreakpointStyle == 23;
    }
} // namespace ck_sm_runtime_graph

void SCkSmRuntimeGraph::Construct(const FArguments& InArgs)
{
    _OnSelectionChanged = InArgs._OnSelectionChanged;
    ChildSlot[SAssignNew(_Canvas, SCkDebug_GraphCanvas)
                  .OnSelectionChanged(FOnCkDebug_GraphCanvasSelectionChanged::CreateSP(
                      this, &SCkSmRuntimeGraph::HandleSelectionChanged))
                  .OnNodeContextMenu(FOnCkDebug_GraphCanvasNodeContextMenu::CreateSP(
                      this, &SCkSmRuntimeGraph::HandleNodeContextMenu))];
}

auto SCkSmRuntimeGraph::SetSmInfo(const FCkSmDebugger_SmInfo* InInfo) -> void
{
    if (InInfo == nullptr)
    {
        if (_SmInfo != nullptr)
        {
            Clear();
        }
        return;
    }

    _SmInfo = InInfo;
    const auto NewStructureHash = FCkSmRuntimeGraphModel::ComputeStructureHash(
        *InInfo,
        _Layout.ExpandTasks,
        _Layout.NameDepth,
        _Layout.SpacingX,
        _Layout.SpacingY,
        _Layout.UndirectedBFS);
    if (NOT _HasStructureHash || _StructureHash != NewStructureHash)
    {
        RebuildScene();
        return;
    }

    _Model.UpdateRuntimeState(*InInfo);
    InstallScene();
}
auto SCkSmRuntimeGraph::SetExpandTasks(const bool bInExpandTasks) -> void
{
    if (_Layout.ExpandTasks == bInExpandTasks)
    {
        return;
    }
    _Layout.ExpandTasks = bInExpandTasks;
    RebuildScene();
}
auto SCkSmRuntimeGraph::SetNameDepth(const int32 InNameDepth) -> void
{
    const auto ClampedDepth = FMath::Max(0, InNameDepth);
    if (_Layout.NameDepth == ClampedDepth)
    {
        return;
    }
    _Layout.NameDepth = ClampedDepth;
    RebuildScene();
}
auto SCkSmRuntimeGraph::SetLayout(const FCkSmRuntimeGraphLayout& InParams) -> void
{
    if (_Layout == InParams)
    {
        return;
    }
    _Layout = InParams;
    RebuildScene();
}

auto SCkSmRuntimeGraph::SetBreakpointStyle(const int32 InBreakpointStyle) -> void
{
    if (_BreakpointStyle == InBreakpointStyle)
    {
        return;
    }
    _BreakpointStyle = InBreakpointStyle;
    RebuildScene();
}
auto SCkSmRuntimeGraph::ApplyScrubHighlight(const int32 InActiveStateIndex,
                                            const int32 InExitedStateIndex) -> void
{
    _Model.ApplyScrubHighlight(InActiveStateIndex, InExitedStateIndex);
    InstallScene();
}
auto SCkSmRuntimeGraph::ClearPresentation() -> void
{
    _Model.ClearPresentation();
    InstallScene();
}
auto SCkSmRuntimeGraph::TickLivePresentation(const float InDeltaTime,
                                             const int32 InPreviousStateIndex,
                                             const int32 InCurrentStateIndex,
                                             const TSet<FString>& InPreviousStateNames) -> void
{
    _Model.TickLivePresentation(InDeltaTime,
                                InPreviousStateIndex,
                                InCurrentStateIndex,
                                InPreviousStateNames);
    InstallScene();
}
auto SCkSmRuntimeGraph::FrameAll() -> void
{
    if (_Canvas)
    {
        _Canvas->Frame_All();
    }
}
auto SCkSmRuntimeGraph::Clear() -> void
{
    _SmInfo = nullptr;
    _Model.Clear();
    _CardCache.Reset();
    _CardStructureHashes.Reset();
    _CardPresentations.Reset();
    _StatePills.Reset();
    _StructureHash = 0;
    _HasStructureHash = false;
    if (_Canvas)
    {
        const auto InstallingGuard = TGuardValue<bool>(_IsInstallingScene, true);
        _Canvas->Set_Scene({});
    }
}

auto SCkSmRuntimeGraph::RebuildScene() -> void
{
    if (NOT _Canvas)
    {
        return;
    }
    if (NOT _SmInfo)
    {
        Clear();
        return;
    }
    _Model.Rebuild(*_SmInfo,
                   _Layout.ExpandTasks,
                   _Layout.NameDepth,
                   _Layout.SpacingX,
                   _Layout.SpacingY,
                   _Layout.UndirectedBFS);
    _StructureHash = FCkSmRuntimeGraphModel::ComputeStructureHash(*_SmInfo,
                                                                  _Layout.ExpandTasks,
                                                                  _Layout.NameDepth,
                                                                  _Layout.SpacingX,
                                                                  _Layout.SpacingY,
                                                                  _Layout.UndirectedBFS);
    _HasStructureHash = true;
    InstallScene();
}

auto SCkSmRuntimeGraph::InstallScene() -> void
{
    if (NOT _Canvas)
    {
        return;
    }
    auto CanvasScene = FCkDebug_GraphCanvasScene{};
    const auto& SelectedIds = _Canvas->Get_SelectedNodeIds();
    auto PresentIds = TSet<uint64>{};
    for (const auto& Node : _Model.GetScene().Nodes)
    {
        PresentIds.Add(Node.Id);
        auto Presentation = _CardPresentations.FindRef(Node.Id);
        if (NOT Presentation)
        {
            Presentation = MakeShared<FCkSmRuntimeGraphCardPresentation>();
            _CardPresentations.Add(Node.Id, Presentation);
        }
        Presentation->Node = Node;
        Presentation->bSelected = SelectedIds.Contains(Node.Id);

        auto StructureHash = ck_sm_runtime_graph::GetCardStructureHash(Node);
        StructureHash = HashCombine(StructureHash, GetTypeHash(_BreakpointStyle));
        const auto* CachedHash = _CardStructureHashes.Find(Node.Id);
        auto Card = _CardCache.FindRef(Node.Id);
        if (NOT Card || CachedHash == nullptr || *CachedHash != StructureHash)
        {
            _StatePills.Remove(Node.Id);
            Card = MakeCard(Presentation.ToSharedRef());
            _CardCache.Add(Node.Id, Card);
            _CardStructureHashes.Add(Node.Id, StructureHash);
        }
        if (const auto Pill = _StatePills.FindRef(Node.Id))
        {
            Pill->Set_Selected(SelectedIds.Contains(Node.Id));
        }
        auto CanvasNode = FCkDebug_GraphCanvasNode{};
        CanvasNode.Id = Node.Id;
        CanvasNode.Position = Node.Position;
        CanvasNode.Size = Node.Size;
        CanvasNode.Layer = Node.Kind == ECkSmRuntimeGraphNodeKind::Compound ? 0 : 1;
        CanvasNode.Widget = Card;
        CanvasScene.Nodes.Add(MoveTemp(CanvasNode));
    }
    for (const auto& Edge : _Model.GetScene().Edges)
    {
        auto CanvasEdge = FCkDebug_GraphCanvasEdge{};
        CanvasEdge.SourceId = Edge.SourceId;
        CanvasEdge.TargetId = Edge.TargetId;
        CanvasEdge.Color = Edge.bScrubHighlighted
                               ? CkStyle::Ok()
                               : (Edge.LiveFlashAlpha > 0.0f ? FLinearColor::White : Edge.Color);
        CanvasEdge.Thickness =
            Edge.bScrubHighlighted
                ? 3.0f
                : FMath::Lerp(Edge.Thickness, 3.5f, FMath::Clamp(Edge.LiveFlashAlpha, 0.0f, 1.0f));
        if (Edge.LiveFlashAlpha > 0.0f)
        {
            CanvasEdge.Color =
                FMath::Lerp(CkStyle::TextDim(), CkStyle::Warn(), Edge.LiveFlashAlpha);
        }
        CanvasEdge.IsDirected = Edge.bDirected;
        CanvasEdge.IsDashed = Edge.bReverse;
        CanvasEdge.LineSeparation = 4.5f;
        CanvasEdge.RoutePoints = Edge.RoutePoints;
        CanvasScene.Edges.Add(MoveTemp(CanvasEdge));
    }
    for (auto It = _CardCache.CreateIterator(); It; ++It)
    {
        if (NOT PresentIds.Contains(It.Key()))
        {
            _CardStructureHashes.Remove(It.Key());
            _CardPresentations.Remove(It.Key());
            _StatePills.Remove(It.Key());
            It.RemoveCurrent();
        }
    }
    const auto InstallingGuard = TGuardValue<bool>(_IsInstallingScene, true);
    _Canvas->Set_Scene(MoveTemp(CanvasScene));
}

auto SCkSmRuntimeGraph::MakeCard(
    const TSharedRef<FCkSmRuntimeGraphCardPresentation>& InPresentation)
    -> TSharedRef<SWidget>
{
    const auto& InNode = InPresentation->Node;
    const auto WeakPresentation = TWeakPtr<FCkSmRuntimeGraphCardPresentation>{InPresentation};
    if (InNode.Kind == ECkSmRuntimeGraphNodeKind::Compound)
    {
        return SNew(SBorder)
            .BorderImage(ck_sm_runtime_graph::GetCompoundBorderBrush())
            .BorderBackgroundColor_Lambda([WeakPresentation]() -> FSlateColor
            {
                const auto Presentation = WeakPresentation.Pin();
                if (NOT Presentation)
                {
                    return CkStyle::TextDim();
                }
                return Presentation->Node.bCurrent ? CkStyle::Warn()
                                                   : Presentation->Node.Accent;
            })
            .Padding(
                1.0f)[SNew(SBorder)
                          .BorderImage(ck_sm_runtime_graph::GetCompoundFillBrush())
                          .BorderBackgroundColor(CkStyle::NodeFill_Inactive())
                          .Padding(
                              8.0f)[SNew(STextBlock)
                                        .Text_Lambda([WeakPresentation]()
                                        {
                                            const auto Presentation = WeakPresentation.Pin();
                                            return Presentation
                                                       ? FText::FromString(Presentation->Node.Label)
                                                       : FText::GetEmpty();
                                        })
                                        .ColorAndOpacity_Lambda([WeakPresentation]() -> FSlateColor
                                        {
                                            const auto Presentation = WeakPresentation.Pin();
                                            return Presentation && Presentation->Node.bCurrent
                                                       ? CkStyle::Warn()
                                                       : CkStyle::TextDim();
                                        })]];
    }

    if (InNode.Kind == ECkSmRuntimeGraphNodeKind::Entry)
    {
        return SNew(STextBlock)
            .Text(FText::FromString(InNode.Label))
            .ColorAndOpacity(CkStyle::TextDim());
    }

    if (InNode.Kind == ECkSmRuntimeGraphNodeKind::Transition)
    {
        const auto BreakpointRed = CkStyle::Err();
        const auto BreakpointHollow = CkStyle::OverlayOf(BreakpointRed, 0.25f);
        const auto IndicatorSize = TAttribute<FOptionalSize>::CreateLambda([]()
        {
            return FOptionalSize{ck_sm_debugger_axes::Get_IndicatorSize()};
        });
        // The editor transition node is a compact ColorSpill/Icon badge. Conditions belong to the
        // existing Details surface; retaining them here changed the graph topology and obscured wires.
        return SNew(SBox)
            .WidthOverride(16.0f)
            .HeightOverride(16.0f)
            [SNew(SOverlay)
             + SOverlay::Slot()[SNew(SImage)
                                    .Image(FAppStyle::GetBrush(TEXT("Graph.TransitionNode.ColorSpill")))
                                    .ColorAndOpacity_Lambda([WeakPresentation]() -> FSlateColor
                                    {
                                        const auto Presentation = WeakPresentation.Pin();
                                        return Presentation && Presentation->bSelected
                                                   ? CkStyle::Accent()
                                                   : CkStyle::TextStrong();
                                    })]
             + SOverlay::Slot()[SNew(SImage)
                                    .Image(FAppStyle::GetBrush(TEXT("Graph.TransitionNode.Icon")))]
             + SOverlay::Slot()
                   .HAlign(HAlign_Left)
                   .VAlign(VAlign_Top)
                       [SNew(SBox)
                            .WidthOverride(IndicatorSize)
                            .HeightOverride(IndicatorSize)
                                [SNew(SBorder)
                                     .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                     .BorderBackgroundColor_Lambda(
                                         [WeakPresentation,
                                          BreakpointRed,
                                          BreakpointHollow]() -> FSlateColor
                                         {
                                             const auto Presentation = WeakPresentation.Pin();
                                             return Presentation && Presentation->Node.Transition
                                                            && Presentation->Node.Transition->HasBreakpoint
                                                        ? BreakpointRed
                                                        : BreakpointHollow;
                                         })
                                     .RenderTransformPivot(FVector2D(0.5f, 0.5f))
                                     .RenderTransform(
                                         FSlateRenderTransform(FQuat2D(PI / 4.0)))]]];
    }

    auto Body = SNew(SVerticalBox);
    const auto BreakpointRed = CkStyle::Err();
    const auto BreakpointHollow = CkStyle::OverlayOf(BreakpointRed, 0.25f);
    const auto IndicatorSize = TAttribute<FOptionalSize>::CreateLambda([]()
    {
        return FOptionalSize{ck_sm_debugger_axes::Get_IndicatorSize()};
    });
    const auto bUseInlineBreakpoints = ck_sm_runtime_graph::IsInlineBreakpointStyle(_BreakpointStyle);
    const auto bUseDiamondBreakpoints = _BreakpointStyle == 23;
    const auto BreakpointTransform = bUseDiamondBreakpoints
                                         ? FSlateRenderTransform(FQuat2D(PI / 4.0))
                                         : FSlateRenderTransform();
    const auto LeftIndicator = bUseInlineBreakpoints
                                   ? StaticCastSharedRef<SWidget>(
                                         SNew(SBorder)
                                             .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                                             .BorderBackgroundColor_Lambda(
                                                 [WeakPresentation,
                                                  BreakpointRed,
                                                  BreakpointHollow]() -> FSlateColor
                                                 {
                                                     const auto Presentation = WeakPresentation.Pin();
                                                     return Presentation && Presentation->Node.State
                                                                    && Presentation->Node.State->HasEntryBreakpoint
                                                                ? BreakpointRed
                                                                : BreakpointHollow;
                                                 })
                                             .RenderTransformPivot(FVector2D(0.5f, 0.5f))
                                             .RenderTransform(BreakpointTransform))
                                   : StaticCastSharedRef<SWidget>(
                                         SNew(SBorder)
                                             .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                                             .BorderBackgroundColor_Lambda([WeakPresentation]() -> FSlateColor
                                             {
                                                 const auto Presentation = WeakPresentation.Pin();
                                                 return Presentation ? Presentation->Node.Accent
                                                                     : CkStyle::TextMute();
                                             }));
    auto TitleRow = SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
              .AutoWidth()
              .VAlign(VAlign_Center)
              .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                   [SNew(SBox)
                        .WidthOverride(IndicatorSize)
                        .HeightOverride(IndicatorSize)
                            [LeftIndicator]]
        + SHorizontalBox::Slot()
              .AutoWidth()
              .VAlign(VAlign_Center)
                  [SNew(STextBlock)
                       .Text_Lambda([WeakPresentation]()
                       {
                           const auto Presentation = WeakPresentation.Pin();
                           return Presentation ? FText::FromString(Presentation->Node.Label)
                                               : FText::GetEmpty();
                       })
                       .Font_Lambda([]()
                       {
                           return ck::debug_axes::ScaledFont("Bold", CkStyle::NodeTitleFontSize());
                       })
                       .ColorAndOpacity_Lambda([WeakPresentation]() -> FSlateColor
                       {
                           const auto Presentation = WeakPresentation.Pin();
                           auto Color = CkStyle::Text();
                           if (Presentation && Presentation->Node.State
                               && Presentation->Node.State->IsSubSmNode
                               && NOT Presentation->Node.bParentActive)
                           {
                               Color.A *= 0.35f;
                           }
                           return Color;
                       })]
         + SHorizontalBox::Slot()
               .AutoWidth()
               .VAlign(VAlign_Center)
               .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                   [SNew(SBox)
                        .WidthOverride(IndicatorSize)
                        .HeightOverride(IndicatorSize)
                        .Visibility(bUseInlineBreakpoints ? EVisibility::SelfHitTestInvisible
                                                          : EVisibility::Collapsed)
                            [SNew(SBorder)
                                .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                                .BorderBackgroundColor_Lambda(
                                    [WeakPresentation,
                                     BreakpointRed,
                                     BreakpointHollow]() -> FSlateColor
                                    {
                                        const auto Presentation = WeakPresentation.Pin();
                                        return Presentation && Presentation->Node.State
                                                       && Presentation->Node.State->HasExitBreakpoint
                                                   ? BreakpointRed
                                                   : BreakpointHollow;
                                    })
                                 .RenderTransformPivot(FVector2D(0.5f, 0.5f))
                                  .RenderTransform(BreakpointTransform)]]
         + SHorizontalBox::Slot()
               .AutoWidth()
               .VAlign(VAlign_Center)
               .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                   [SNew(SBox)
                        // Reserve this small slot so a live breakpoint hit never changes card geometry.
                        .WidthOverride(18.0f)
                            [SNew(STextBlock)
                                 .Text(FText::FromString(TEXT("HIT")))
                                 .Font_Lambda([]() { return ck::debug_axes::ScaledFont("Bold", 7); })
                                 .ColorAndOpacity(CkStyle::Err())
                                 .Visibility_Lambda([WeakPresentation]()
                                 {
                                     const auto Presentation = WeakPresentation.Pin();
                                     return Presentation && Presentation->Node.State
                                                        && Presentation->Node.State->IsBreakpointHit
                                                ? EVisibility::SelfHitTestInvisible
                                                : EVisibility::Collapsed;
                                 })]];
    Body->AddSlot().AutoHeight()[TitleRow];
    Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [SNew(STextBlock)
             .Text(FText::FromString(TEXT("OVERRIDE")))
             .Font_Lambda([]() { return ck::debug_axes::ScaledFont("Bold", 7); })
             .ColorAndOpacity(FCkSmDebuggerStyle::Color_Sm_Override)
             .Visibility_Lambda([WeakPresentation]()
             {
                 const auto Presentation = WeakPresentation.Pin();
                 return Presentation && Presentation->Node.bHasOverride
                            ? EVisibility::SelfHitTestInvisible
                            : EVisibility::Collapsed;
             })];
    Body->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
        [SNew(STextBlock)
             .Text(FText::FromString(TEXT("EVENT-DRIVEN")))
             .Font_Lambda([]() { return ck::debug_axes::ScaledFont("Bold", 7); })
             .ColorAndOpacity(CkStyle::Warn())
             .Visibility_Lambda([WeakPresentation]()
             {
                 const auto Presentation = WeakPresentation.Pin();
                 return Presentation && Presentation->Node.bFullyEventDriven
                            ? EVisibility::SelfHitTestInvisible
                            : EVisibility::Collapsed;
             })];
    if (InNode.State && InNode.bExpandTasks)
    {
        for (auto TaskIndex = 0; TaskIndex < InNode.State->Tasks.Num(); ++TaskIndex)
        {
            Body->AddSlot()
                .AutoHeight()
                .Padding(FCkSmDebuggerStyle::Sm_NodePadding, 2.0f)
                    [SNew(SHorizontalBox)
                     + SHorizontalBox::Slot()
                           .AutoWidth()
                           .VAlign(VAlign_Center)
                           .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                               [SNew(SBox)
                                    .WidthOverride(FCkSmDebuggerStyle::Sm_StateIconSize)
                                    .HeightOverride(FCkSmDebuggerStyle::Sm_StateIconSize)
                                        [SNew(SBorder)
                                             .BorderImage(FCoreStyle::Get().GetBrush(
                                                 TEXT("WhiteBrush")))
                                             .BorderBackgroundColor_Lambda(
                                                 [WeakPresentation,
                                                  TaskIndex]() -> FSlateColor
                                                 {
                                                     const auto Presentation =
                                                         WeakPresentation.Pin();
                                                     if (NOT Presentation
                                                         || NOT Presentation->Node.State
                                                         || NOT Presentation->Node.State->Tasks
                                                                    .IsValidIndex(TaskIndex))
                                                     {
                                                         return CkStyle::TextMute();
                                                     }
                                                     auto Color = CkSmDebugger::GetTaskResultColor(
                                                         Presentation->Node.State->Tasks[TaskIndex]
                                                             .LastResult);
                                                     if (NOT Presentation->Node.State->IsCurrentState)
                                                     {
                                                         Color.A *= 0.3f;
                                                     }
                                                     return Color;
                                                 })]]
                     + SHorizontalBox::Slot()
                           .FillWidth(1.0f)
                           .VAlign(VAlign_Center)
                               [SNew(STextBlock)
                                    .Text_Lambda([WeakPresentation,
                                                  TaskIndex,
                                                  NameDepth = _Layout.NameDepth]()
                                    {
                                        const auto Presentation = WeakPresentation.Pin();
                                        if (NOT Presentation || NOT Presentation->Node.State
                                            || NOT Presentation->Node.State->Tasks.IsValidIndex(
                                                TaskIndex))
                                        {
                                            return FText::GetEmpty();
                                        }
                                        return FText::FromString(
                                            SCkDebug_NameLabel::Get_ShortName(
                                                Presentation->Node.State->Tasks[TaskIndex]
                                                    .ClassName,
                                                NameDepth));
                                    })
                                    .Font_Lambda([]()
                                    {
                                        return ck::debug_axes::ScaledFont(
                                            "Regular", CkStyle::FontSizeMicro());
                                    })
                                    .ColorAndOpacity_Lambda(
                                        [WeakPresentation]() -> FSlateColor
                                        {
                                            const auto Presentation = WeakPresentation.Pin();
                                            auto Color = CkStyle::TextDim();
                                            if (Presentation && Presentation->Node.State
                                                && NOT Presentation->Node.State->IsCurrentState)
                                            {
                                                Color.A *= 0.4f;
                                            }
                                            return Color;
                                        })]
                     + SHorizontalBox::Slot()
                           .AutoWidth()
                           .VAlign(VAlign_Center)
                           .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                               [SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("TICK")))
                                    .Font_Lambda([]()
                                    {
                                        return ck::debug_axes::ScaledFont("Bold", 7);
                                    })
                                    .ColorAndOpacity_Lambda(
                                        [WeakPresentation]() -> FSlateColor
                                        {
                                            const auto Presentation = WeakPresentation.Pin();
                                            auto Color = CkStyle::Warn();
                                            if (Presentation && Presentation->Node.State
                                                && NOT Presentation->Node.State->IsCurrentState)
                                            {
                                                Color.A *= 0.45f;
                                            }
                                            return Color;
                                        })
                                    .Visibility_Lambda([WeakPresentation, TaskIndex]()
                                    {
                                        const auto Presentation = WeakPresentation.Pin();
                                        return Presentation && Presentation->Node.State
                                                       && Presentation->Node.State->Tasks
                                                              .IsValidIndex(TaskIndex)
                                                       && Presentation->Node.State->Tasks[TaskIndex]
                                                                  .Mode
                                                              == ECk_SmTaskMode::Tick
                                                   ? EVisibility::SelfHitTestInvisible
                                                   : EVisibility::Collapsed;
                                    })]];
        }
    }
    auto Pill = TSharedPtr<SCkDebug_NodePill>{};
    auto Card =
        SAssignNew(Pill, SCkDebug_NodePill)
            .Variant(ECkDebug_NodePillVariant::Inactive)
            .Title(FText::GetEmpty())
            .ShowCost(false)
            .AccentColor_Lambda([WeakPresentation]()
            {
                const auto Presentation = WeakPresentation.Pin();
                if (NOT Presentation)
                {
                    return FLinearColor::Transparent;
                }
                auto Accent = Presentation->Node.Accent;
                if (Presentation->Node.State && Presentation->Node.State->IsSubSmNode
                    && NOT Presentation->Node.bParentActive)
                {
                    Accent.A *= 0.35f;
                }
                return Accent;
            })
            .AccentWidth_Lambda([]() { return FCkSmDebuggerStyle::Sm_AccentBarWidth; })
            .BorderColorOverride_Lambda([WeakPresentation]()
            {
                const auto Presentation = WeakPresentation.Pin();
                if (NOT Presentation)
                {
                    return CkStyle::NodeFill_Inactive();
                }
                return Presentation->Node.bCurrent || Presentation->Node.bScrubActive
                           ? CkStyle::Ok()
                           : (Presentation->Node.bPrevious
                                      || Presentation->Node.bScrubExited
                                  ? CkStyle::TextDim()
                                  : CkStyle::NodeFill_Inactive());
            })
            .FillColorOverride_Lambda([WeakPresentation]()
            {
                const auto Presentation = WeakPresentation.Pin();
                return Presentation
                               && (Presentation->Node.bCurrent
                                   || Presentation->Node.bScrubActive)
                           ? CkStyle::NodeFill_InPlan()
                           : CkStyle::NodeFill_Inactive();
            })
            .OpacityOverride_Lambda([WeakPresentation]()
            {
                const auto Presentation = WeakPresentation.Pin();
                return Presentation
                               && (Presentation->Node.bCurrent
                                   || Presentation->Node.bScrubActive
                                   || Presentation->Node.bPrevious
                                   || Presentation->Node.bScrubExited)
                           ? 1.0f
                           : ck::debug_axes::Get_NodeInactiveOpacity();
            })
            .BorderThickness_Lambda([]()
            {
                return ck::debug_axes::Get_NodeBorderThickness();
            })
            .Selected(InPresentation->bSelected)
            .BodyContent()[Body];
    Card->SetToolTipText(TAttribute<FText>::CreateLambda([WeakPresentation]()
    {
        const auto Presentation = WeakPresentation.Pin();
        if (NOT Presentation || NOT Presentation->Node.State)
        {
            return FText::GetEmpty();
        }
        const auto& State = *Presentation->Node.State;
        if (State.IsCurrentState)
        {
            return FText::FromString(
                FString::Printf(TEXT("Active for %.2f secs"), State.DwellTimeSeconds));
        }
        if (Presentation->Node.bPrevious)
        {
            return FText::FromString(
                FString::Printf(TEXT("Was active for %.2f secs"), State.DwellTimeSeconds));
        }
        return FText::GetEmpty();
    }));
    _StatePills.Add(InNode.Id, Pill);
    return Card;
}

auto SCkSmRuntimeGraph::HandleSelectionChanged(const TSet<uint64>& InSelection) -> void
{
    if (_IsInstallingScene)
    {
        return;
    }
    for (auto& Pair : _CardPresentations)
    {
        Pair.Value->bSelected = InSelection.Contains(Pair.Key);
    }
    for (auto& Pair : _StatePills)
    {
        Pair.Value->Set_Selected(InSelection.Contains(Pair.Key));
    }
    int32 StateIndex = INDEX_NONE;
    int32 TransitionIndex = INDEX_NONE;
    for (const auto Id : InSelection)
    {
        if (Id >= 0x100000000ull && Id < 0x200000000ull)
        {
            StateIndex = static_cast<int32>(Id - 0x100000000ull);
            break;
        }
        if (Id >= 0x200000000ull && Id < 0x300000000ull)
        {
            TransitionIndex = static_cast<int32>(Id - 0x200000000ull);
            break;
        }
    }
    _OnSelectionChanged.ExecuteIfBound(StateIndex, TransitionIndex);
}

auto SCkSmRuntimeGraph::HandleNodeContextMenu(const uint64 InNodeId,
                                              const FPointerEvent& InMouseEvent) -> void
{
    const auto Payload = _Model.BuildCopyPayload(InNodeId);
    if (NOT Payload.IsSet() || NOT _Canvas)
    {
        return;
    }

    auto Menu = FMenuBuilder(true, nullptr);
    if (Payload->Target == ECkSmRuntimeGraphCopyTarget::State)
    {
        ck::DebugCopyMenu::AddCopyEntry(Menu,
                                        FText::FromString(TEXT("Copy Display Name")),
                                        FText::FromString(TEXT("Copy the visible state label")),
                                        Payload->DisplayName);
        ck::DebugCopyMenu::AddCopyEntry(Menu,
                                        FText::FromString(TEXT("Copy State Class Name")),
                                        FText::FromString(
                                            TEXT("Copy the full underlying state class name")),
                                        Payload->ClassName);
    }
    else
    {
        ck::DebugCopyMenu::AddCopyEntry(Menu,
                                        FText::FromString(TEXT("Copy Group Label")),
                                        FText::FromString(TEXT("Copy this group's title")),
                                        Payload->GroupLabel);
        if (NOT Payload->ChildDisplayNames.IsEmpty())
        {
            ck::DebugCopyMenu::AddCopyEntry(Menu,
                                            FText::FromString(TEXT("Copy Child State Names")),
                                            FText::FromString(TEXT(
                                                "Copy visible child state labels, one per line")),
                                            FString::Join(Payload->ChildDisplayNames, TEXT("\n")));
            ck::DebugCopyMenu::AddCopyEntry(Menu,
                                            FText::FromString(TEXT("Copy Child State Class Names")),
                                            FText::FromString(TEXT(
                                                "Copy full child state class names, one per line")),
                                            FString::Join(Payload->ChildClassNames, TEXT("\n")));
        }
        ck::DebugCopyMenu::AddCopyEntry(Menu,
                                        FText::FromString(TEXT("Copy All")),
                                        FText::FromString(
                                            TEXT("Copy the group label and child state names")),
                                        Payload->All);
    }

    FSlateApplication::Get().PushMenu(_Canvas.ToSharedRef(),
                                      FWidgetPath{},
                                      Menu.MakeWidget(),
                                      InMouseEvent.GetScreenSpacePosition(),
                                      FPopupTransitionEffect{FPopupTransitionEffect::ContextMenu});
}
