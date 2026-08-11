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
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

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
        auto Hash = HashCombine(GetTypeHash(InNode.Kind), GetTypeHash(InNode.Label));
        Hash = HashCombine(Hash, GetTypeHash(InNode.bExpandTasks));
        Hash = HashCombine(Hash, GetTypeHash(InNode.bHasOverride));
        Hash = HashCombine(Hash, GetTypeHash(InNode.bFullyEventDriven));
        Hash = HashCombine(Hash, GetTypeHash(InNode.bCurrent));
        Hash = HashCombine(Hash, GetTypeHash(InNode.bBreakpoint));
        Hash = HashCombine(Hash, GetTypeHash(InNode.bScrubActive));
        Hash = HashCombine(Hash, GetTypeHash(InNode.bScrubExited));
        Hash = HashCombine(Hash, GetTypeHash(InNode.bPrevious));
        if (InNode.State)
        {
            if (InNode.bExpandTasks)
            {
                for (const auto& Task : InNode.State->Tasks)
                {
                    Hash = HashCombine(Hash, GetTypeHash(Task.ClassName));
                    Hash = HashCombine(Hash, GetTypeHash(Task.LastResult));
                }
            }
            Hash = HashCombine(Hash, GetTypeHash(InNode.State->DwellTimeSeconds));
            Hash = HashCombine(Hash, GetTypeHash(InNode.State->HasBeenVisited));
            Hash = HashCombine(Hash, GetTypeHash(InNode.State->HasEntryBreakpoint));
            Hash = HashCombine(Hash, GetTypeHash(InNode.State->HasExitBreakpoint));
            Hash = HashCombine(Hash, GetTypeHash(InNode.State->IsBreakpointHit));
        }
        if (InNode.Transition)
        {
            for (const auto& Condition : InNode.Transition->Conditions)
            {
                Hash = HashCombine(Hash, GetTypeHash(Condition.ClassName));
                Hash = HashCombine(Hash, GetTypeHash(Condition.Result));
            }
        }
        return Hash;
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
    if (_SmInfo == InInfo && InInfo == nullptr)
    {
        return;
    }
    _SmInfo = InInfo;
    RebuildScene();
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
    _StatePills.Reset();
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
        auto StructureHash = ck_sm_runtime_graph::GetCardStructureHash(Node);
        if (Node.Kind == ECkSmRuntimeGraphNodeKind::Transition)
        {
            StructureHash = HashCombine(StructureHash, GetTypeHash(SelectedIds.Contains(Node.Id)));
        }
        const auto* CachedHash = _CardStructureHashes.Find(Node.Id);
        auto Card = _CardCache.FindRef(Node.Id);
        if (NOT Card || CachedHash == nullptr || *CachedHash != StructureHash)
        {
            _StatePills.Remove(Node.Id);
            Card = MakeCard(Node, SelectedIds.Contains(Node.Id));
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
            _StatePills.Remove(It.Key());
            It.RemoveCurrent();
        }
    }
    const auto InstallingGuard = TGuardValue<bool>(_IsInstallingScene, true);
    _Canvas->Set_Scene(MoveTemp(CanvasScene));
}

auto SCkSmRuntimeGraph::MakeCard(const FCkSmRuntimeGraphNode& InNode, const bool bInSelected)
    -> TSharedRef<SWidget>
{
    if (InNode.Kind == ECkSmRuntimeGraphNodeKind::Compound)
    {
        return SNew(SBorder)
            .BorderImage(ck_sm_runtime_graph::GetCompoundBorderBrush())
            .BorderBackgroundColor(InNode.bCurrent ? CkStyle::Warn() : InNode.Accent)
            .Padding(
                1.0f)[SNew(SBorder)
                          .BorderImage(ck_sm_runtime_graph::GetCompoundFillBrush())
                          .BorderBackgroundColor(CkStyle::NodeFill_Inactive())
                          .Padding(
                              8.0f)[SNew(STextBlock)
                                        .Text(FText::FromString(InNode.Label))
                                        .ColorAndOpacity(InNode.bCurrent ? CkStyle::Warn()
                                                                         : CkStyle::TextDim())]];
    }

    if (InNode.Kind == ECkSmRuntimeGraphNodeKind::Entry)
    {
        return SNew(STextBlock)
            .Text(FText::FromString(InNode.Label))
            .ColorAndOpacity(CkStyle::TextDim());
    }

    if (InNode.Kind == ECkSmRuntimeGraphNodeKind::Transition)
    {
        auto TransitionBody = SNew(SVerticalBox);
        TransitionBody->AddSlot().AutoHeight()[SNew(STextBlock)
                                                   .Text(FText::FromString(InNode.Label))
                                                   .ColorAndOpacity(CkStyle::TextStrong())];
        if (InNode.Transition)
        {
            for (const auto& Condition : InNode.Transition->Conditions)
            {
                TransitionBody->AddSlot().AutoHeight().Padding(
                    0.0f, 1.0f)[SNew(STextBlock)
                                    .Text(FText::FromString(FString::Printf(
                                        TEXT("%s %s"),
                                        CkSmDebugger::GetConditionResultLabel(Condition.Result),
                                        *SCkDebug_NameLabel::Get_ShortName(Condition.ClassName,
                                                                           _Layout.NameDepth))))
                                    .ColorAndOpacity(
                                        Condition.Result == ECk_SmConditionResult::Pass
                                            ? CkStyle::Ok()
                                            : (Condition.Result == ECk_SmConditionResult::Fail
                                                   ? CkStyle::Err()
                                                   : CkStyle::TextDim()))];
            }
        }
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(InNode.Accent)
            .Padding(bInSelected ? 1.0f : 3.0f)
            [
                SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(bInSelected ? CkStyle::Accent() : InNode.Accent)
                    .Padding(bInSelected ? 2.0f : 0.0f)
                    [TransitionBody]
            ];
    }

    auto Body = SNew(SVerticalBox);
    if (InNode.bHasOverride || InNode.bFullyEventDriven)
    {
        auto Flags = FString{};
        if (InNode.bHasOverride)
        {
            Flags += TEXT("OVERRIDE");
        }
        if (InNode.bFullyEventDriven)
        {
            Flags += Flags.IsEmpty() ? TEXT("EVENT-DRIVEN") : TEXT("  EVENT-DRIVEN");
        }
        Body->AddSlot().AutoHeight()
            [SNew(STextBlock).Text(FText::FromString(Flags)).ColorAndOpacity(CkStyle::TextDim())];
    }
    if (InNode.State && InNode.bExpandTasks)
    {
        for (const auto& Task : InNode.State->Tasks)
        {
            Body->AddSlot().AutoHeight().Padding(
                0.0f,
                1.0f)[SNew(STextBlock)
                          .Text(FText::FromString(
                              SCkDebug_NameLabel::Get_ShortName(Task.ClassName, _Layout.NameDepth)))
                          .ColorAndOpacity(CkSmDebugger::GetTaskResultColor(Task.LastResult))];
        }
    }
    if (InNode.State)
    {
        const auto& State = *InNode.State;
        const auto Status = State.IsCurrentState
                                ? FString::Printf(TEXT("Active %.2fs"), State.DwellTimeSeconds)
                                : (State.HasBeenVisited ? FString::Printf(TEXT("Visited  %.2fs"),
                                                                          State.DwellTimeSeconds)
                                                        : TEXT("Not visited"));
        Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [SNew(STextBlock)
                 .Text(FText::FromString(Status))
                 .ColorAndOpacity(State.IsCurrentState ? CkStyle::Warn() : CkStyle::TextDim())];
        if (State.HasEntryBreakpoint || State.HasExitBreakpoint || State.IsBreakpointHit)
        {
            auto BreakpointText = FString{};
            if (State.HasEntryBreakpoint)
            {
                BreakpointText += TEXT("ENTRY ");
            }
            if (State.HasExitBreakpoint)
            {
                BreakpointText += TEXT("EXIT ");
            }
            if (State.IsBreakpointHit)
            {
                BreakpointText += TEXT("HIT");
            }
            Body->AddSlot().AutoHeight()[SNew(STextBlock)
                                             .Text(FText::FromString(BreakpointText.TrimEnd()))
                                             .ColorAndOpacity(CkStyle::Err())];
        }
    }
    auto Pill = TSharedPtr<SCkDebug_NodePill>{};
    auto Card =
        SAssignNew(Pill, SCkDebug_NodePill)
            .Variant((InNode.bCurrent || InNode.bScrubActive) ? ECkDebug_NodePillVariant::InPlan
                                                              : ECkDebug_NodePillVariant::Inactive)
            .Title(FText::FromString(InNode.Label))
            .ShowCost(false)
            .AccentColor(InNode.Accent)
            .BorderColorOverride(
                InNode.bBreakpoint ? CkStyle::Err()
                                   : (InNode.bScrubActive ? CkStyle::Ok()
                                                          : (InNode.bScrubExited || InNode.bPrevious
                                                                 ? CkStyle::TextDim()
                                                                 : FLinearColor::Transparent)))
            .Selected(bInSelected)
            .BodyContent()[Body];
    _StatePills.Add(InNode.Id, Pill);
    return Card;
}

auto SCkSmRuntimeGraph::HandleSelectionChanged(const TSet<uint64>& InSelection) -> void
{
    if (_IsInstallingScene)
    {
        return;
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
