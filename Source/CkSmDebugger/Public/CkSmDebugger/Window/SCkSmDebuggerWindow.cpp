#include "CkSmDebugger/Window/SCkSmDebuggerWindow.h"
#include "CkSmDebugger/ViewModel/CkSmDebugger_ViewModel.h"
#include "CkSmDebugger/Data/CkSmDebugger_DataCollector.h"
#include "CkSmDebugger/Window/SCkSmDebugger_HistoryList.h"
#include "CkSmDebugger/Preview/SCkSmDebugger_PreviewPane.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"

#include "CkSmDebugger/Graph/CkSmDebugGraph.h"
#include "CkSmDebugger/Graph/CkSmDebugGraphSchema.h"
#include "CkSmDebugger/Graph/CkSmDebugNode_State.h"
#include "CkSmDebugger/Graph/CkSmDebugNode_Transition.h"

#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkStateMachine/Debug/CkStateMachine_Debug_Utils.h"

#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ScrubTimeline.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "GraphEditor.h"
#include "Editor.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameDepthCycler.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"

// --------------------------------------------------------------------------------------------------------------------
// Detail panel — small widget helpers
// --------------------------------------------------------------------------------------------------------------------

// Named, not anonymous: this module builds with unity on, and a merged TU makes these generic
// helper names (MakePill, MakeSectionHeader, ...) collide with any other file's local versions.
namespace ck_sm_debugger_window
{
    // Detail-panel roles. Every one of these used to be a hardcoded literal in this block; they are
    // now thin aliases onto CkStyle:: so a palette edit (Editor Preferences -> Ck -> Style) moves the
    // panel, and so the SM panel converges on the rest of the suite.
    auto Color_Detail_Label()     -> FLinearColor { return CkStyle::TextDim(); }
    auto Color_Detail_Value()     -> FLinearColor { return CkStyle::Text(); }
    auto Color_Detail_ClassName() -> FLinearColor { return CkStyle::Reference(); }
    auto Color_Detail_Bullet()    -> FLinearColor { return CkStyle::Warn(); }
    auto Color_Detail_Arrow()     -> FLinearColor { return CkStyle::TextDim(); }

    // -----------------------------------------------------------------------------------------------------------------
    // Section header: axis-driven title + separator underline. The underline collapses entirely when
    // the SeparatorWeight axis is None, which is why the slot is conditional rather than zero-height.
    // -----------------------------------------------------------------------------------------------------------------
    auto MakeSectionHeader(const FString& InText) -> TSharedRef<SWidget>
    {
        const auto& Selection = UCkDebuggerStyleSettings::Get_Selection();
        const auto SeparatorThickness = ck::debug_axes::Get_SeparatorThickness(Selection);

        auto Box = SNew(SVerticalBox);

        Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
        [
            ck::debug_axes::Make_SectionHeader(
                Selection, FText::FromString(InText), ECk_Tone::Neutral)
        ];

        if (SeparatorThickness > 0.0f)
        {
            Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(SBox).HeightOverride(SeparatorThickness)
                [
                    SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                        .BorderBackgroundColor(CkStyle::Border())
                ]
            ];
        }

        return Box;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Colored pill used for status chips (ACTIVE, RUNNING, SATISFIED, etc).
    // Background is a 22%-alpha fill of the accent color; text is the full accent.
    // -----------------------------------------------------------------------------------------------------------------
    auto MakePill(const FString& InText, const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(CkStyle::OverlayOf(InColor, 0.22f))
            .Padding(FMargin(6.0f, 1.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(InText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FSlateColor(InColor))
            ];
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Monospace-ish class name text (dim accent color).
    // -----------------------------------------------------------------------------------------------------------------
    auto MakeClassName(const FString& InText) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_SelectableLabel)
            .Text(FText::FromString(InText))
            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
            .ColorAndOpacity(FSlateColor(Color_Detail_ClassName()));
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Right-aligned key / value row. `KeyText` is provided via a lambda so values
    // that change every frame (dwell time, yes/no flags) update live.
    // -----------------------------------------------------------------------------------------------------------------
    auto MakeKeyValue(const FString& InKey, TAttribute<FText> InValue) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SBox).MinDesiredWidth(64.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(InKey))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Label()))
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(InValue)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(Color_Detail_Value()))
            ];
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Task result pill, updating live to reflect the current task state.
    // -----------------------------------------------------------------------------------------------------------------
    auto MakeTaskResultPill(TAttribute<ECk_SmTaskResult> InResult) -> TSharedRef<SWidget>
    {
        auto TextAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([InResult]()
        {
            switch (InResult.Get())
            {
            case ECk_SmTaskResult::Succeeded: return FText::FromString(TEXT("SUCCEEDED"));
            case ECk_SmTaskResult::Failed:    return FText::FromString(TEXT("FAILED"));
            default:                          return FText::FromString(TEXT("RUNNING"));
            }
        }));

        auto ColorAttr = TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([InResult]()
        {
            return FSlateColor(CkSmDebugger::GetTaskResultColor(InResult.Get()));
        }));

        auto BgColorAttr = TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([InResult]()
        {
            return FSlateColor(CkStyle::OverlayOf(CkSmDebugger::GetTaskResultColor(InResult.Get()), 0.22f));
        }));

        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(BgColorAttr)
            .Padding(FMargin(6.0f, 1.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(TextAttr)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(ColorAttr)
            ];
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Condition result pill (Pass / Fail / Undetermined) — static snapshot.
    // -----------------------------------------------------------------------------------------------------------------
    auto Get_ConditionResultColor(ECk_SmConditionResult InResult) -> FLinearColor
    {
        switch (InResult)
        {
        case ECk_SmConditionResult::Pass: return CkStyle::Ok();
        case ECk_SmConditionResult::Fail: return CkStyle::Err();
        default:                          return CkStyle::TextDim();
        }
    }

    auto MakeConditionPill(ECk_SmConditionResult InResult) -> TSharedRef<SWidget>
    {
        auto Label = FString{TEXT("?")};
        switch (InResult)
        {
        case ECk_SmConditionResult::Pass: Label = TEXT("PASS"); break;
        case ECk_SmConditionResult::Fail: Label = TEXT("FAIL"); break;
        default: break;
        }
        return MakePill(Label, Get_ConditionResultColor(InResult));
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Condition result pill — updates its label/color live from a TAttribute.
    // -----------------------------------------------------------------------------------------------------------------
    auto MakeConditionPillLive(TAttribute<ECk_SmConditionResult> InResult) -> TSharedRef<SWidget>
    {
        auto TextAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([InResult]()
        {
            switch (InResult.Get())
            {
            case ECk_SmConditionResult::Pass: return FText::FromString(TEXT("PASS"));
            case ECk_SmConditionResult::Fail: return FText::FromString(TEXT("FAIL"));
            default:                          return FText::FromString(TEXT("?"));
            }
        }));

        auto FgAttr = TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([InResult]()
        {
            return FSlateColor(Get_ConditionResultColor(InResult.Get()));
        }));
        auto BgAttr = TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([InResult]()
        {
            return FSlateColor(CkStyle::OverlayOf(Get_ConditionResultColor(InResult.Get()), 0.22f));
        }));

        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(BgAttr)
            .Padding(FMargin(6.0f, 1.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(TextAttr)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FgAttr)
            ];
    }

}

// --------------------------------------------------------------------------------------------------------------------

SCkSmDebuggerWindow::~SCkSmDebuggerWindow()
{
    UCk_Utils_StateMachineDebug_UE::Set_IsDebuggerCaptureVisible(false);

    if (_WorldModel.IsValid() && _WorldChangedHandle.IsValid())
    { _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle); }

    if (_SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }

    if (_Graph && UObjectInitialized())
    {
        _Graph->RemoveFromRoot();
        _Graph = nullptr;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    HandleWorldTornDown()
    -> void
{
    _CachedWorld.Reset();
    _SelectedTransitionIndex = -1;
    _SelectedHistoryEntry.Reset();
    _LastCurrentStateIdx = -1;
    _AutoSelectActiveState = true;
    _LastDetailSig = FDetailSignature{};

    // Preview walker uses the PIE world — tear down its entity too.
    if (_PreviewPane.IsValid())
    { _PreviewPane->Set_WorldContext(TWeakObjectPtr<UWorld>()); }

    _SmSelectorItems.Empty();
    _SmSelectorHandles.Empty();

    // The timeline holds no handles, but it does hold a view window into the OLD run. Drop its
    // content, re-anchor on the origin the next session starts from at the zoom the user chose
    // (that mirrored duration is what OnViewChanged is for), then restore live-follow.
    if (_Timeline.IsValid())
    {
        _Timeline->Set_Content({}, {});
        _Timeline->Set_View(0.0, _TimelineViewDuration);
        _Timeline->Focus_OnCursor();
    }

    if (_ViewModel.IsValid())
    { _ViewModel->Reset_ForWorldChange(); }

    if (_DataCollector.IsValid())
    { _DataCollector->Reset(); }

    if (ck::IsValid(_Graph))
    {
        _Graph->ForceRebuild();
        _Graph->Nodes.Empty();
        _Graph->NotifyGraphChanged();
    }

    // Force the detail panel to repaint its "No selection" state now. Both the
    // pre- and post-reset signatures are default-initialized, so RefreshDetailContent's
    // signature early-exit would otherwise leave the previous widget in place.
    if (_DetailContentBox.IsValid())
    { _DetailContentBox->SetContent(BuildDetailContent()); }
}

auto SCkSmDebuggerWindow::HandleWorldChanged(UWorld*) -> void
{
    HandleWorldTornDown();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    StepScrubToTransition(
        int32 InDirection)
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }
    auto* SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (NOT SmInfo) { return; }

    auto& ScrubState = _ViewModel->Get_ScrubState();
    auto RIdx = ScrubState.SelectedRunIndex;
    auto& Run = (RIdx < 0)
        ? SmInfo->CurrentRun
        : (RIdx < SmInfo->CompletedRuns.Num() ? SmInfo->CompletedRuns[RIdx] : SmInfo->CurrentRun);
    if (Run.History.IsEmpty()) { return; }

    // ScrubState.ScrubTime is run-relative; History.RealTimeSeconds is absolute logical.
    // Find the next/previous transition relative to the current scrub position.
    auto ScrubAbs = ScrubState.ScrubTime + Run.StartTime;

    auto Target = -1.0;
    if (InDirection > 0)
    {
        for (auto& Entry : Run.History)
        {
            if (Entry.RealTimeSeconds > ScrubAbs + 1e-4)
            { Target = Entry.RealTimeSeconds; break; }
        }
    }
    else
    {
        for (auto i = Run.History.Num() - 1; i >= 0; --i)
        {
            if (Run.History[i].RealTimeSeconds < ScrubAbs - 1e-4)
            { Target = Run.History[i].RealTimeSeconds; break; }
        }
    }
    if (Target < 0.0) { return; }

    auto NewScrubState = ScrubState;
    NewScrubState.ScrubTime = Target - Run.StartTime;
    _ViewModel->Set_ScrubState(NewScrubState);

    // The old timeline re-derived its origin from ScrubTime every paint, so a step always landed
    // framed. The window is widget-owned now, so a jump has to ask for the re-frame explicitly.
    FocusTimelineOnCursor();
}

// ====================================================================================================================
// Timeline — content feed + view control for the shared SCkDebug_ScrubTimeline
// ====================================================================================================================

auto
    SCkSmDebuggerWindow::
    Get_ScrubbedRun() const
    -> const FCkSmDebugger_RunInfo*
{
    if (NOT _ViewModel.IsValid())
    { return nullptr; }

    const auto* SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (NOT SmInfo)
    { return nullptr; }

    // SelectedRunIndex < 0 (and any stale out-of-range index) means "the live run".
    const auto RunIndex = _ViewModel->Get_ScrubState().SelectedRunIndex;
    return SmInfo->CompletedRuns.IsValidIndex(RunIndex)
        ? &SmInfo->CompletedRuns[RunIndex]
        : &SmInfo->CurrentRun;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    ComputeFrameAtTime(
        const FCkSmDebugger_RunInfo& InRun,
        double InRunRelativeTime)
    -> int64
{
    if (InRun.FrameSegments.IsEmpty())
    { return 0; }

    const auto& First = InRun.FrameSegments[0];
    const auto& Last = InRun.FrameSegments.Last();

    if (InRunRelativeTime <= First.StartTime)
    { return static_cast<int64>(First.StartFrame); }

    if (InRunRelativeTime >= Last.EndTime)
    { return static_cast<int64>(Last.EndFrame); }

    for (const auto& Segment : InRun.FrameSegments)
    {
        if (InRunRelativeTime < Segment.StartTime || InRunRelativeTime > Segment.EndTime)
        { continue; }

        const auto Span = Segment.EndTime - Segment.StartTime;
        if (Span <= 0.0)
        { return static_cast<int64>(Segment.StartFrame); }

        const auto Fraction = (InRunRelativeTime - Segment.StartTime) / Span;
        const auto FrameSpan = static_cast<int64>(Segment.EndFrame) - static_cast<int64>(Segment.StartFrame);

        // Floor (not round) so the reported frame matches the cell visually under the needle:
        // cell N covers [N/total, (N+1)/total), and Floor maps any time in that range to N.
        // Round would tick to N+1 at the cell midpoint, ahead of the needle.
        return static_cast<int64>(Segment.StartFrame) + FMath::FloorToInt64(Fraction * FrameSpan);
    }

    return static_cast<int64>(Last.EndFrame);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    FocusTimelineOnCursor()
    -> void
{
    if (NOT _Timeline.IsValid())
    { return; }

    _Timeline->Focus_OnCursor();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    RefreshTimelineContent()
    -> void
{
    if (NOT _Timeline.IsValid() || NOT _ViewModel.IsValid())
    { return; }

    const auto* SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (NOT SmInfo)
    {
        _Timeline->Set_Content({}, {});
        return;
    }

    const auto& ScrubState = _ViewModel->Get_ScrubState();
    const auto IsLiveRun = NOT SmInfo->CompletedRuns.IsValidIndex(ScrubState.SelectedRunIndex);
    const auto& SourceRun = IsLiveRun
        ? SmInfo->CurrentRun
        : SmInfo->CompletedRuns[ScrubState.SelectedRunIndex];

    // Live-segment freeze: while scrubbing the live run, the trailing segment must stop growing or
    // its per-frame cells shrink under the needle as real time advances. The override applies only
    // while the SM is still in the captured state — a transition releases the freeze on its own.
    auto FrozenRun = FCkSmDebugger_RunInfo{};
    auto UseFrozenRun = false;
    if (IsLiveRun && _ViewModel->Get_LiveSegmentFreezeActive())
    {
        const auto& SourceSegments = SourceRun.Segments;
        if (SourceSegments.Num() > 0
            && SourceSegments.Last().StateName == _ViewModel->Get_LiveSegmentFreezeStateName())
        {
            FrozenRun = SourceRun;
            auto& LastSegment = FrozenRun.Segments.Last();
            LastSegment.EndTime = _ViewModel->Get_LiveSegmentFreezeEndTime();
            LastSegment.EndFrame = _ViewModel->Get_LiveSegmentFreezeEndFrame();
            UseFrozenRun = true;
        }
    }
    const auto& Run = UseFrozenRun ? FrozenRun : SourceRun;

    const auto NameDepth = _Graph ? _Graph->LayoutParams.NameDepth : 1;

    auto Segments = TArray<FCkDebug_ScrubSegment>{};
    Segments.Reserve(Run.Segments.Num());

    for (const auto& Source : Run.Segments)
    {
        auto Segment = FCkDebug_ScrubSegment{};
        Segment.StartSeconds = Source.StartTime;
        Segment.EndSeconds = Source.EndTime;
        Segment.Color = Source.Color;
        Segment.Label = SCkDebug_NameLabel::Get_ShortName(Source.StateName, NameDepth);
        Segment.Tooltip = ck::Format_UE(TEXT("{}\nframes {} - {}  ({}s)"),
            Source.StateName,
            Source.StartFrame,
            Source.EndFrame,
            FString::Printf(TEXT("%.3f"), Source.EndTime - Source.StartTime));

        // One subdivision cell per engine frame the SM spent in this state; the widget suppresses
        // the boundary lines on its own once the cells fall under ~2px.
        const auto FrameCount = static_cast<int64>(Source.EndFrame) - static_cast<int64>(Source.StartFrame);
        Segment.SubdivisionCount = static_cast<int32>(
            FMath::Clamp<int64>(FrameCount, 1, TNumericLimits<int32>::Max()));

        Segments.Add(MoveTemp(Segment));
    }

    const auto& Bookmarks = _ViewModel->Get_Bookmarks();

    auto Marks = TArray<FCkDebug_ScrubMark>{};
    Marks.Reserve(Run.PauseMarkers.Num() + Run.BusyFrames.Num() + Bookmarks.Num());

    for (const auto& Pause : Run.PauseMarkers)
    {
        auto Mark = FCkDebug_ScrubMark{};
        Mark.TimeSeconds = Pause.Time;
        Mark.Kind = ECkDebug_ScrubMarkKind::Cut;
        Mark.Color = Pause.IsBreakpoint ? CkStyle::Err() : CkStyle::Accent();
        Mark.Tooltip = Pause.IsBreakpoint
            ? FString{TEXT("Breakpoint pause")}
            : FString{TEXT("Manual pause")};
        Marks.Add(MoveTemp(Mark));
    }

    for (const auto& Busy : Run.BusyFrames)
    {
        auto Mark = FCkDebug_ScrubMark{};
        Mark.TimeSeconds = Busy.Time;
        Mark.Kind = ECkDebug_ScrubMarkKind::Tick;
        Mark.Color = CkStyle::Warn();
        Mark.Tooltip = ck::Format_UE(TEXT("Frame {} ran {} transitions"),
            Busy.FrameNumber, Busy.TransitionCount);
        Marks.Add(MoveTemp(Mark));
    }

    for (const auto BookmarkTime : Bookmarks)
    {
        auto Mark = FCkDebug_ScrubMark{};
        Mark.TimeSeconds = BookmarkTime;
        Mark.Kind = ECkDebug_ScrubMarkKind::Flag;
        Mark.Color = CkStyle::Warn();
        Mark.Tooltip = ck::Format_UE(TEXT("Bookmark @ {}s"),
            FString::Printf(TEXT("%.2f"), BookmarkTime));
        Marks.Add(MoveTemp(Mark));
    }

    _Timeline->Set_Content(MoveTemp(Segments), MoveTemp(Marks));
}

// --------------------------------------------------------------------------------------------------------------------

const FName SCkSmDebuggerWindow::WindowId = FName(TEXT("SmDebugger"));

auto
    SCkSmDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _ViewModel = MakeShared<FCkSmDebugger_ViewModel>();
    _DataCollector = MakeShared<FCkSmDebugger_DataCollector>();
    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();

    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddSP(
        this, &SCkSmDebuggerWindow::HandleWorldChanged);

    // Create the preview pane eagerly so its picker can live in the main toolbar
    // (keeps the preview graph top aligned with the live graph top on the left).
    _PreviewPane = SNew(SCkSmDebugger_PreviewPane);

    // Common emits on both BeginPIE and EndPIE. The feature keeps ownership of
    // what must be cleared; Tick repopulates after the new world begins play.
    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkSmDebuggerWindow::HandleWorldTornDown);

    // Create the debug graph — prevent GC
    _Graph = NewObject<UCkSmDebugGraph>(GetTransientPackage());
    _Graph->AddToRoot();
    _Graph->Schema = UCkSmDebugGraphSchema::StaticClass();

    // Wire command callback so context menu actions reach the ViewModel
    _Graph->OnIssueCommand = [this](const FCkSmDebugger_Command& InCommand)
    {
        if (_ViewModel.IsValid())
        { _ViewModel->IssueCommand(InCommand); }
    };

    // Create graph editor with selection callback
    SGraphEditor::FGraphEditorEvents GraphEvents;
    GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateLambda(
        [this](const TSet<UObject*>& InSelection)
        {
            if (NOT _ViewModel.IsValid())
            { return; }

            for (auto* Obj : InSelection)
            {
                if (auto* StateNode = Cast<UCkSmDebugNode_State>(Obj))
                {
                    _ViewModel->Set_SelectedNodeIndex(StateNode->Get_StateIndex());
                    _SelectedTransitionIndex = -1;
                    _AutoSelectActiveState = false;
                    return;
                }

                if (auto* TransNode = Cast<UCkSmDebugNode_Transition>(Obj))
                {
                    _SelectedTransitionIndex = TransNode->Get_TransitionIndex();
                    _ViewModel->Set_SelectedNodeIndex(-1);
                    _AutoSelectActiveState = false;
                    return;
                }
            }

            _ViewModel->Set_SelectedNodeIndex(-1);
            _SelectedTransitionIndex = -1;
            _AutoSelectActiveState = true;
        });

    _GraphEditor = SNew(SGraphEditor)
        .GraphToEdit(_Graph)
        .IsEditable(true)
        .GraphEvents(GraphEvents);

    // Build the full layout
    //
    //   Toolbar
    //   ├─ Graph (full width, resizable)
    //   ├─ Timeline (full width, auto-height)
    //   └─ History | Details (side-by-side, resizable)

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
             .WindowId(WindowId)
             .ToolTabId(TEXT("CkSmDebugger"))
             .DisplayName(FText::FromString(TEXT("CK State Machine Debugger")))
             .MenuActionsContent()
             [
                 BuildMenuActions()
             ]
             .Content()
            [
                SNew(SVerticalBox)

            // Toolbar
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f)
                [
                    BuildToolbar()
                ]

            // Main content area — wrapped in a horizontal splitter so the preview
            // pane can slide in on the right at 50% width without disturbing the
            // existing vertical layout on the left.
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SAssignNew(_RootSplitter, SSplitter)
                        .Orientation(Orient_Horizontal)

                        + SSplitter::Slot()
                            .Value(1.0f)
                            [
                    SNew(SSplitter)
                        .Orientation(Orient_Vertical)

                        // Graph canvas (full width, ~65%)
                        + SSplitter::Slot()
                            .Value(0.65f)
                            [
                                _GraphEditor.ToSharedRef()
                            ]

                        // Bottom area (~35%): timeline + history|details
                        + SSplitter::Slot()
                            .Value(0.35f)
                            [
                                SNew(SVerticalBox)

                                // Timeline bar (full width, auto-height)
                                // Timeline toolbar — scrub navigation kept close to the timeline.
                                + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(4.0f, 2.0f, 4.0f, 0.0f)
                                    [
                                        BuildTimelineToolbar()
                                    ]

                                + SVerticalBox::Slot()
                                    .AutoHeight()
                                    [
                                        SAssignNew(_Timeline, SCkDebug_ScrubTimeline)
                                            .DesiredHeight(56.0f)
                                            .InitialViewDuration(_TimelineViewDuration)
                                            .SegmentHeightFraction(0.5f)
                                            .RulerLabelCount(5)
                                            .Mode_Lambda([this]()
                                            {
                                                return (_ViewModel.IsValid()
                                                        && _ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                                                    ? ECkDebug_ScrubMode::Scrub
                                                    : ECkDebug_ScrubMode::Live;
                                            })
                                            .ScrubTime_Lambda([this]() -> double
                                            {
                                                return _ViewModel.IsValid()
                                                    ? _ViewModel->Get_ScrubState().ScrubTime
                                                    : 0.0;
                                            })
                                            // "Now" for the scrubbed run — also the widget's upper
                                            // clamp, which is what used to be an explicit
                                            // FMath::Clamp(ScrubTime, 0, RunDuration).
                                            .LiveTime_Lambda([this]() -> double
                                            {
                                                const auto* Run = Get_ScrubbedRun();
                                                return Run ? Run->Duration : 0.0;
                                            })
                                            // Frame labels carry a [mod 1000] tail so nearby labels
                                            // stay comparable once the absolute frame runs into the
                                            // millions; the raw number stays greppable against logs.
                                            .OnFormatTime_Lambda([this](double InTime) -> FString
                                            {
                                                const auto ShowFrames = _ViewModel.IsValid()
                                                    && _ViewModel->Get_ScrubState().ShowFramesOnTimeline;

                                                if (NOT ShowFrames)
                                                { return FString::Printf(TEXT("%.1fs"), InTime); }

                                                const auto* Run = Get_ScrubbedRun();
                                                const auto Frame = Run ? ComputeFrameAtTime(*Run, InTime) : int64{0};
                                                return FString::Printf(TEXT("f%lld [%03lld]"), Frame, Frame % 1000);
                                            })
                                            .OnScrubbed_Lambda([this](double InTime)
                                            {
                                                if (NOT _ViewModel.IsValid()) { return; }

                                                _ViewModel->Set_ViewMode(ECkSmDebugger_ViewMode::Scrub);

                                                auto NewScrubState = _ViewModel->Get_ScrubState();
                                                NewScrubState.ViewMode = ECkSmDebugger_ViewMode::Scrub;
                                                NewScrubState.ScrubTime = FMath::Max(InTime, 0.0);
                                                _ViewModel->Set_ScrubState(NewScrubState);
                                            })
                                            // The widget owns the window; we only mirror the zoom so
                                            // a new PIE session can re-anchor to t=0 without losing it.
                                            .OnViewChanged_Lambda([this](double, double InViewDuration)
                                            {
                                                _TimelineViewDuration = InViewDuration;
                                            })
                                    ]

                                // History | Details (side-by-side, fills remaining)
                                + SVerticalBox::Slot()
                                    .FillHeight(1.0f)
                                    [
                                        SNew(SSplitter)
                                            .Orientation(Orient_Horizontal)

                                            // History list (left, 60%)
                                            + SSplitter::Slot()
                                                .Value(0.6f)
                                                [
                                                    SAssignNew(_HistoryList, SCkSmDebugger_HistoryList, _ViewModel, _Graph)
                                                ]

                                            // Detail panel (right, 40%)
                                            + SSplitter::Slot()
                                                .Value(0.4f)
                                                [
                                                    BuildDetailPanel()
                                                ]
                                    ]
                            ]
                            ]
                ]
            ]
    ];

    // Bind history selection → detail panel. The list has already moved ScrubTime by the time this
    // fires, so re-centring here is what replaces the old "TimelineScrollX = 0" the list used to do.
    _HistoryList->OnEntrySelected.BindLambda([this](TSharedPtr<FCkSmDebugger_HistoryEntry> InEntry)
    {
        _SelectedHistoryEntry = InEntry;
        FocusTimelineOnCursor();
    });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (_IsTestMode)
    {
        UCk_Utils_StateMachineDebug_UE::Set_IsDebuggerCaptureVisible(false);
        return;
    }

    UCk_Utils_StateMachineDebug_UE::Set_IsDebuggerCaptureVisible(
        FCkDebuggerRefreshGate::Is_WindowVisible(WindowId));

    // Per-window refresh gate — short-circuits when hidden / paused / rate-capped.
    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    // Resolve the inspected world via the shared selector (re-validated each tick
    // since PIE can end at any time).
    _WorldModel->Ensure_AutoSelect();
    _CachedWorld = _WorldModel->Get_SelectedWorld();

    // Keep the preview pane in sync with the current PIE world so its walker can
    // spawn a throwaway SM entity to discover the graph when a class is picked.
    if (_PreviewPane.IsValid())
    { _PreviewPane->Set_WorldContext(_CachedWorld); }

    auto* World = _CachedWorld.Get();
    if (ck::Is_NOT_Valid(World) || NOT World->HasBegunPlay())
    { return; }

    // Collect data from ECS
    _DataCollector->Collect(World);

    // Tick ViewModel — broadcasts delegates to sub-widgets
    _ViewModel->Tick(World, InDeltaTime);

    // Auto-select the active state when the user hasn't explicitly picked
    // something — saves them a click to populate the detail panel. While
    // scrubbing this mirrors the segment under the needle (so STATE / TASKS /
    // TRANSITIONS reflect the scrubbed moment); in live mode it follows the
    // live current-state flag.
    //
    // In scrub mode we run the mirror unconditionally — the user may have
    // clicked a history row earlier, but as they scrub the panel must keep
    // tracking the segment under the needle, not freeze on the click target.
    // In live mode we still defer to explicit history/transition selections.
    auto IsScrubbingForMirror = _ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub;
    if (_AutoSelectActiveState
        && (IsScrubbingForMirror
            || (NOT _SelectedHistoryEntry.IsValid() && _SelectedTransitionIndex < 0)))
    {
        if (auto SmInfo = _ViewModel->Get_CurrentSmInfo())
        {
            auto ActiveIdx = -1;
            if (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
            {
                ActiveIdx = _ViewModel->Get_ScrubSnapshot().ActiveStateIndex;
            }
            else
            {
                for (auto i = 0; i < SmInfo->States.Num(); ++i)
                {
                    if (SmInfo->States[i].IsCurrentState)
                    { ActiveIdx = i; break; }
                }
            }

            // Don't clobber an existing valid selection with -1: ScrubSnapshot
            // can be transiently empty between the live tick reaching a state
            // boundary and the snapshot recompute, and we don't want the panel
            // to flash "No selection" in that gap.
            if (ActiveIdx >= 0 && _ViewModel->Get_SelectedNodeIndex() != ActiveIdx)
            { _ViewModel->Set_SelectedNodeIndex(ActiveIdx); }
        }
    }

    // Refresh SM selector combo
    RefreshSmSelector();

    // Push segments + marks into the shared scrub timeline (immediate-mode paint; two array moves).
    RefreshTimelineContent();

    if (_PendingTarget.IsSet())
    {
        const auto Target = _PendingTarget.GetValue();
        _PendingTarget.Reset();
        for (const auto& Candidate : _SmSelectorHandles)
        {
            if (Candidate.Get_Entity() != Target) { continue; }
            _ViewModel->Set_SelectedSmHandle(Candidate);
            break;
        }
    }

    // Update graph from SmInfo
    auto SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (SmInfo && _Graph)
    {
        _Graph->SetSuppressNotifications(false);
        _Graph->UpdateFromSmInfo(*SmInfo);

        // ----- Highlight pass: scrub mode or live flash -----
        if (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
        {
            auto HighlightTarget = _ViewModel->Get_ScrubHighlightTarget();
            auto HighlightSource = _ViewModel->Get_ScrubHighlightSource();
            _Graph->ApplyScrubHighlight(HighlightTarget, HighlightSource);
        }
        else
        {
            _Graph->ClearScrubHighlight();

            // Compute one "previous state" per hierarchy level (outer SM +
            // each sub-SM) by walking history backwards and keeping the first
            // FromStateName we see for each distinct SubSmParentStateName.
            // Name-based matching survives graph rebuilds (sub-SM live/cached
            // swaps invalidate indices but not state names).
            auto PreviousStateNames = TSet<FString>{};
            auto SeenLevels = TSet<FString>{};
            for (auto HistIdx = SmInfo->History.Num() - 1; HistIdx >= 0; --HistIdx)
            {
                const auto& Entry = SmInfo->History[HistIdx];
                if (SeenLevels.Contains(Entry.SubSmParentStateName)) { continue; }
                // Skip boot/start markers (empty or literal "(start)" From) without marking
                // the level seen — otherwise a recent boot marker blocks us from finding a
                // real previous state at that hierarchy level. The backend stamps
                // FromStateName = "(start)" on sub-SM initial-state entries (see
                // CkStateMachine_Debug_Processor.cpp DoHandleRequest).
                if (Entry.FromStateName.IsEmpty()
                    || Entry.FromStateName == TEXT("(start)"))
                { continue; }

                SeenLevels.Add(Entry.SubSmParentStateName);
                PreviousStateNames.Add(Entry.FromStateName);
            }

            _Graph->TickLiveFlash(InDeltaTime, _LastCurrentStateIdx, SmInfo->CurrentStateIndex, PreviousStateNames);
        }

        // ----- Breakpoint detection: pause PIE on state entry/exit -----
        auto CurrentStateIdx = SmInfo->CurrentStateIndex;
        if (CurrentStateIdx != _LastCurrentStateIdx && _LastCurrentStateIdx >= 0)
        {
            // Check EXIT breakpoint on the state we just left
            if (auto* OldNode = _Graph->FindStateNode(_LastCurrentStateIdx))
            {
                if (OldNode->Get_HasExitBreakpoint())
                {
                    if (GEditor && GEditor->PlayWorld)
                    { GEditor->PlayWorld->bDebugPauseExecution = true; }
                }
            }

            // Check ENTRY breakpoint on the state we just entered
            if (auto* NewNode = _Graph->FindStateNode(CurrentStateIdx))
            {
                if (NewNode->Get_HasEntryBreakpoint())
                {
                    if (GEditor && GEditor->PlayWorld)
                    { GEditor->PlayWorld->bDebugPauseExecution = true; }
                }
            }

            // Check TRANSITION breakpoints — any transition from old→new state
            for (auto Node : _Graph->Nodes)
            {
                if (auto* TransNode = Cast<UCkSmDebugNode_Transition>(Node))
                {
                    if (NOT TransNode->Get_HasBreakpoint()) { continue; }
                    auto* Src = TransNode->GetSourceNode();
                    auto* Dst = TransNode->GetTargetNode();
                    if (Src && Dst
                        && Src->Get_StateIndex() == _LastCurrentStateIdx
                        && Dst->Get_StateIndex() == CurrentStateIdx)
                    {
                        if (GEditor && GEditor->PlayWorld)
                        { GEditor->PlayWorld->bDebugPauseExecution = true; }
                    }
                }
            }
        }
        _LastCurrentStateIdx = CurrentStateIdx;
    }

    // Detail panel: swap content when selection signature changes.
    RefreshDetailContent();
}

auto SCkSmDebuggerWindow::TargetEntity(const FCk_Handle& InEntity) -> void
{
    if (ck::IsValid(InEntity)) { _PendingTarget = InEntity.Get_Entity(); }
}

// --------------------------------------------------------------------------------------------------------------------
// Key input — F to focus graph / timeline
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    OnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent)
    -> FReply
{
    if (InKeyEvent.GetKey() == EKeys::F)
    {
        // Zoom graph to fit all nodes
        if (_GraphEditor.IsValid())
        { _GraphEditor->ZoomToFit(false); }

        // Re-centre the timeline window on the scrub cursor / "now"
        FocusTimelineOnCursor();

        return FReply::Handled();
    }

    // Arrow keys step the scrub needle by transition while in Scrub mode.
    if (_ViewModel.IsValid() && _ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
    {
        // Bookmarks: Ctrl+B adds at the current scrub position; Shift+B removes
        // the closest one; F2 / F3 cycle backward / forward.
        if (InKeyEvent.GetKey() == EKeys::B && InKeyEvent.IsControlDown())
        {
            _ViewModel->AddBookmark(_ViewModel->Get_ScrubState().ScrubTime);
            return FReply::Handled();
        }
        if (InKeyEvent.GetKey() == EKeys::B && InKeyEvent.IsShiftDown())
        {
            _ViewModel->RemoveNearestBookmark(_ViewModel->Get_ScrubState().ScrubTime);
            return FReply::Handled();
        }
        if (InKeyEvent.GetKey() == EKeys::F2)
        {
            auto Target = _ViewModel->FindNextBookmark(_ViewModel->Get_ScrubState().ScrubTime, -1);
            if (Target >= 0.0)
            {
                auto NS = _ViewModel->Get_ScrubState();
                NS.ScrubTime = Target;
                _ViewModel->Set_ScrubState(NS);
                FocusTimelineOnCursor();
            }
            return FReply::Handled();
        }
        if (InKeyEvent.GetKey() == EKeys::F3)
        {
            auto Target = _ViewModel->FindNextBookmark(_ViewModel->Get_ScrubState().ScrubTime, +1);
            if (Target >= 0.0)
            {
                auto NS = _ViewModel->Get_ScrubState();
                NS.ScrubTime = Target;
                _ViewModel->Set_ScrubState(NS);
                FocusTimelineOnCursor();
            }
            return FReply::Handled();
        }

        if (InKeyEvent.GetKey() == EKeys::Left)
        { StepScrubToTransition(-1); return FReply::Handled(); }
        if (InKeyEvent.GetKey() == EKeys::Right)
        { StepScrubToTransition(+1); return FReply::Handled(); }
        if (InKeyEvent.GetKey() == EKeys::Home)
        {
            auto NewScrubState = _ViewModel->Get_ScrubState();
            NewScrubState.ScrubTime = 0.0;
            _ViewModel->Set_ScrubState(NewScrubState);
            FocusTimelineOnCursor();
            return FReply::Handled();
        }
        if (InKeyEvent.GetKey() == EKeys::End)
        {
            auto* SmInfo = _ViewModel->Get_CurrentSmInfo();
            if (SmInfo)
            {
                auto NewScrubState = _ViewModel->Get_ScrubState();
                NewScrubState.ScrubTime = SmInfo->CurrentRun.Duration;
                _ViewModel->Set_ScrubState(NewScrubState);
                FocusTimelineOnCursor();
            }
            return FReply::Handled();
        }
    }

    return SCompoundWidget::OnKeyDown(InGeometry, InKeyEvent);
}

// --------------------------------------------------------------------------------------------------------------------
// Toolbar
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    BuildMenuActions()
    -> TSharedRef<SWidget>
{
    const auto DisplayActions = TArray<FCkDebug_IconToggleAction>{
        FCkDebug_IconToggleAction{
            TEXT("Tasks"),
            TEXT("StateMachine"),
            FText::FromString(TEXT("Tasks")),
            FText::FromString(TEXT("Show task nodes in the state machine graph.")),
            TAttribute<bool>::CreateLambda([this]() -> bool
            {
                return _Graph && _Graph->LayoutParams.ExpandTasks;
            }),
            FOnCkDebug_IconToggleChanged::CreateLambda([this](bool InIsOn)
            {
                if (_Graph)
                {
                    _Graph->LayoutParams.ExpandTasks = InIsOn;
                    _Graph->ForceRebuild();
                }
            }),
            TAttribute<bool>::CreateLambda([this]() -> bool { return _Graph != nullptr; })},
        FCkDebug_IconToggleAction{
            TEXT("CompactLayout"),
            TEXT("Grid"),
            FText::FromString(TEXT("Compact Layout")),
            FText::FromString(TEXT("Use a compact, undirected layout for the state machine graph.")),
            TAttribute<bool>::CreateLambda([this]() -> bool
            {
                return _Graph && _Graph->LayoutParams.UndirectedBFS;
            }),
            FOnCkDebug_IconToggleChanged::CreateLambda([this](bool InIsOn)
            {
                if (_Graph)
                {
                    _Graph->LayoutParams.UndirectedBFS = InIsOn;
                    _Graph->ForceRebuild();
                }
            }),
            TAttribute<bool>::CreateLambda([this]() -> bool { return _Graph != nullptr; })},
        FCkDebug_IconToggleAction{
            TEXT("ShowFrames"),
            TEXT("Calendar"),
            NSLOCTEXT("CkSmDebugger", "ShowFrames", "Show frames"),
            NSLOCTEXT("CkSmDebugger", "ShowFramesTooltip",
                "Show timeline labels as frame numbers (on) or seconds (off)."),
            TAttribute<bool>::CreateLambda([this]() -> bool
            {
                return _ViewModel.IsValid() && _ViewModel->Get_ScrubState().ShowFramesOnTimeline;
            }),
            FOnCkDebug_IconToggleChanged::CreateLambda([this](bool InIsOn)
            {
                if (NOT _ViewModel.IsValid()) { return; }
                auto NewState = _ViewModel->Get_ScrubState();
                NewState.ShowFramesOnTimeline = InIsOn;
                _ViewModel->Set_ScrubState(NewState);
            }),
            TAttribute<bool>::CreateLambda([this]() -> bool { return _ViewModel.IsValid(); })}};

    return SNew(SCkDebug_IconToolbar)
        .Actions(DisplayActions);
}

auto
    SCkSmDebuggerWindow::
    BuildToolbar()
    -> TSharedRef<SWidget>
{

    return SNew(SHorizontalBox)

        // ── World Selector (shared across all CK debuggers) ──────────────

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SCkDebug_WorldSelector, _WorldModel)
                    .ShowHeaderLabel(false)
            ]

        // ── SM Selector ──────────────────────────────────────────────────

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("SM:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(CkStyle::TextDim())
            ]
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(2.0f)
            [
                SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&_SmSelectorItems)
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
                    {
                        return SNew(STextBlock)
                            .Text(FText::FromString(*InItem))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9));
                    })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<FString> InItem, ESelectInfo::Type InType)
                    {
                        if (NOT InItem.IsValid() || NOT _ViewModel.IsValid())
                        { return; }

                        auto Index = _SmSelectorItems.IndexOfByKey(InItem);
                        if (Index >= 0 && Index < _SmSelectorHandles.Num())
                        { _ViewModel->Set_SelectedSmHandle(_SmSelectorHandles[Index]); }
                    })
                    [
                        SAssignNew(_SmSelectorLabel, STextBlock)
                            .Text_Lambda([this]()
                            {
                                if (NOT _ViewModel.IsValid())
                                { return FText::FromString(TEXT("(none)")); }

                                auto SmInfo = _ViewModel->Get_CurrentSmInfo();
                                if (SmInfo)
                                { return FText::FromString(SmInfo->DebugName); }

                                return FText::FromString(TEXT("(select SM)"));
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    ]
            ]

        // ── Separator ────────────────────────────────────────────────────

        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(6.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("|")))
                    .ColorAndOpacity(CkStyle::TextMute())
            ]

        // ── Owning entity (clickable → opens in CK ECS Debugger) ─────────
        // Sits next to the SM combo; that combo already shows the entity's
        // DebugName, so the pill renders just the canonical ID. Same pattern
        // as the GOAP and AStar toolbars (no leading "Entity:" label —
        // visually consistent across the four debuggers).
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 6.0f, 0.0f)
            [
                SNew(SCkDebug_EntityRef)
                    .Entity_Lambda([this]() -> FCk_Handle
                    {
                        if (NOT _ViewModel.IsValid())
                        { return FCk_Handle{}; }

                        const auto* SmInfo = _ViewModel->Get_CurrentSmInfo();
                        if (NOT SmInfo)
                        { return FCk_Handle{}; }

                        return SmInfo->GameEntity;
                    })
            ]

        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 6.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("|")))
                    .ColorAndOpacity(CkStyle::TextMute())
            ]

        // Name depth — the shared cycler widget; depth lives on the graph's
        // LayoutParams and a change forces a graph relayout.
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_NameDepthCycler)
                    .Depth_Lambda([this]() -> int32
                    {
                        return _Graph ? _Graph->LayoutParams.NameDepth : 1;
                    })
                    .MaxDepth_Lambda([this]() -> int32
                    {
                        return _Graph ? _Graph->Get_MaxNameDepth() : 1;
                    })
                    .OnDepthChanged(FOnCkDebug_NameDepthChanged::CreateLambda([this](int32 InNewDepth)
                    {
                        if (NOT _Graph) { return; }
                        _Graph->LayoutParams.NameDepth = InNewDepth;
                        _Graph->ForceRebuild();
                    }))
            ]

        // Relayout
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("Relayout")))
                    .OnClicked_Lambda([this]()
                    {
                        if (_Graph)
                        { _Graph->ForceRebuild(); }
                        if (_ViewModel.IsValid())
                        { _ViewModel->RequestRelayout(); }
                        return FReply::Handled();
                    })
            ]

        // ── Layout settings gear dropdown ────────────────────────────────

        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f, 0.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SComboButton)
                    .HasDownArrow(true)
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("\x2699")))  // ⚙
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                    ]
                    .MenuContent()
                    [
                        SNew(SVerticalBox)

                        // History style
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(4.0f, 4.0f, 4.0f, 2.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SBox)
                                            .MinDesiredWidth(80.0f)
                                            [
                                                SNew(STextBlock)
                                                    .Text(FText::FromString(TEXT("History")))
                                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                                    .ColorAndOpacity(CkStyle::TextDim())
                                            ]
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SButton)
                                            .Text(FText::FromString(TEXT("\x25C0")))
                                            .OnClicked_Lambda([this]()
                                            {
                                                if (_Graph)
                                                {
                                                    auto& S = _Graph->LayoutParams.HistoryStyle;
                                                    S = static_cast<ECkSmDebugger_HistoryStyle>(
                                                        (static_cast<int32>(S) + 2) % 3);
                                                }
                                                return FReply::Handled();
                                            })
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                            .Text_Lambda([this]()
                                            {
                                                if (NOT _Graph)
                                                { return FText::FromString(TEXT("Classic")); }

                                                switch (_Graph->LayoutParams.HistoryStyle)
                                                {
                                                case ECkSmDebugger_HistoryStyle::ArrowCards:
                                                    return FText::FromString(TEXT("Cards"));
                                                case ECkSmDebugger_HistoryStyle::ClassicArrows:
                                                    return FText::FromString(TEXT("Classic"));
                                                case ECkSmDebugger_HistoryStyle::CompactBlocks:
                                                    return FText::FromString(TEXT("Compact"));
                                                default:
                                                    return FText::FromString(TEXT("Classic"));
                                                }
                                            })
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                            .Justification(ETextJustify::Center)
                                            .MinDesiredWidth(52.0f)
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SButton)
                                            .Text(FText::FromString(TEXT("\x25B6")))
                                            .OnClicked_Lambda([this]()
                                            {
                                                if (_Graph)
                                                {
                                                    auto& S = _Graph->LayoutParams.HistoryStyle;
                                                    S = static_cast<ECkSmDebugger_HistoryStyle>(
                                                        (static_cast<int32>(S) + 1) % 3);
                                                }
                                                return FReply::Handled();
                                            })
                                    ]
                            ]

                        // H Spacing
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(4.0f, 2.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SBox)
                                            .MinDesiredWidth(80.0f)
                                            [
                                                SNew(STextBlock)
                                                    .Text(FText::FromString(TEXT("H Spacing")))
                                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                                    .ColorAndOpacity(CkStyle::TextDim())
                                            ]
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SButton)
                                            .Text(FText::FromString(TEXT("-")))
                                            .OnClicked_Lambda([this]()
                                            {
                                                if (_Graph)
                                                {
                                                    _Graph->LayoutParams.SpacingX = FMath::Max(100, _Graph->LayoutParams.SpacingX - 50);
                                                    _Graph->ForceRebuild();
                                                }
                                                return FReply::Handled();
                                            })
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                            .Text_Lambda([this]()
                                            {
                                                if (NOT _Graph) { return FText::FromString(TEXT("350")); }
                                                return FText::FromString(FString::Printf(TEXT("%d"), _Graph->LayoutParams.SpacingX));
                                            })
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                            .Justification(ETextJustify::Center)
                                            .MinDesiredWidth(32.0f)
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SButton)
                                            .Text(FText::FromString(TEXT("+")))
                                            .OnClicked_Lambda([this]()
                                            {
                                                if (_Graph)
                                                {
                                                    _Graph->LayoutParams.SpacingX = FMath::Min(800, _Graph->LayoutParams.SpacingX + 50);
                                                    _Graph->ForceRebuild();
                                                }
                                                return FReply::Handled();
                                            })
                                    ]
                            ]

                        // V Spacing
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(4.0f, 2.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SBox)
                                            .MinDesiredWidth(80.0f)
                                            [
                                                SNew(STextBlock)
                                                    .Text(FText::FromString(TEXT("V Spacing")))
                                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                                    .ColorAndOpacity(CkStyle::TextDim())
                                            ]
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SButton)
                                            .Text(FText::FromString(TEXT("-")))
                                            .OnClicked_Lambda([this]()
                                            {
                                                if (_Graph)
                                                {
                                                    _Graph->LayoutParams.SpacingY = FMath::Max(40, _Graph->LayoutParams.SpacingY - 20);
                                                    _Graph->ForceRebuild();
                                                }
                                                return FReply::Handled();
                                            })
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                            .Text_Lambda([this]()
                                            {
                                                if (NOT _Graph) { return FText::FromString(TEXT("120")); }
                                                return FText::FromString(FString::Printf(TEXT("%d"), _Graph->LayoutParams.SpacingY));
                                            })
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                            .Justification(ETextJustify::Center)
                                            .MinDesiredWidth(32.0f)
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SButton)
                                            .Text(FText::FromString(TEXT("+")))
                                            .OnClicked_Lambda([this]()
                                            {
                                                if (_Graph)
                                                {
                                                    _Graph->LayoutParams.SpacingY = FMath::Min(400, _Graph->LayoutParams.SpacingY + 20);
                                                    _Graph->ForceRebuild();
                                                }
                                                return FReply::Handled();
                                            })
                                    ]
                            ]

                        // Badge Spread
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(4.0f, 2.0f, 4.0f, 4.0f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SBox)
                                            .MinDesiredWidth(80.0f)
                                            [
                                                SNew(STextBlock)
                                                    .Text(FText::FromString(TEXT("Badge Gap")))
                                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                                    .ColorAndOpacity(CkStyle::TextDim())
                                            ]
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SButton)
                                            .Text(FText::FromString(TEXT("-")))
                                            .OnClicked_Lambda([this]()
                                            {
                                                if (_Graph)
                                                {
                                                    _Graph->LayoutParams.BadgeSpread = FMath::Max(0.0f, _Graph->LayoutParams.BadgeSpread - 5.0f);
                                                }
                                                return FReply::Handled();
                                            })
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                            .Text_Lambda([this]()
                                            {
                                                if (NOT _Graph) { return FText::FromString(TEXT("20")); }
                                                return FText::FromString(FString::Printf(TEXT("%.0f"), _Graph->LayoutParams.BadgeSpread));
                                            })
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                            .Justification(ETextJustify::Center)
                                            .MinDesiredWidth(32.0f)
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SButton)
                                            .Text(FText::FromString(TEXT("+")))
                                            .OnClicked_Lambda([this]()
                                            {
                                                if (_Graph)
                                                {
                                                    _Graph->LayoutParams.BadgeSpread = FMath::Min(80.0f, _Graph->LayoutParams.BadgeSpread + 5.0f);
                                                }
                                                return FReply::Handled();
                                            })
                                    ]
                            ]
                    ]
            ]

        // ── Separator ────────────────────────────────────────────────────

        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(6.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("|")))
                    .ColorAndOpacity(CkStyle::TextMute())
            ]

        // ── Playback: mode indicator + buttons ───────────────────────────

        // View mode (LIVE / SCRUB / TEST)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        if (_IsTestMode)
                        { return FText::FromString(TEXT("TEST")); }

                        if (NOT _ViewModel.IsValid())
                        { return FText::GetEmpty(); }

                        return (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Live)
                            ? FText::FromString(TEXT("LIVE"))
                            : FText::FromString(TEXT("SCRUB"));
                    })
                    .ColorAndOpacity_Lambda([this]()
                    {
                        if (_IsTestMode)
                        { return FSlateColor(CkStyle::Warn()); }

                        if (NOT _ViewModel.IsValid())
                        { return FSlateColor(CkStyle::TextMute()); }

                        return (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Live)
                            ? FSlateColor(CkStyle::Ok())
                            : FSlateColor(CkStyle::Warn());
                    })
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            ]

        // (Scrub navigation moved to the dedicated timeline toolbar, just above the
        // timeline widget — closer to where the user is actually scrubbing.)

        // Pause / Resume
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text_Lambda([]()
                    {
                        if (GEditor && GEditor->PlayWorld && GEditor->PlayWorld->bDebugPauseExecution)
                        { return FText::FromString(TEXT("\x25B6 Resume")); }
                        // ASCII bars, not U+23F8 ⏸ — that codepoint misses the whole
                        // editor font chain and forces a synchronous LastResort.ttf
                        // load mid-paint (log-confirmed; also the prime suspect for a
                        // font-cache "array changed during ranged-for" ensure).
                        return FText::FromString(TEXT("|| Pause"));
                    })
                    .OnClicked_Lambda([this]()
                    {
                        if (GEditor && GEditor->PlayWorld)
                        {
                            GEditor->PlayWorld->bDebugPauseExecution =
                                !GEditor->PlayWorld->bDebugPauseExecution;
                        }
                        return FReply::Handled();
                    })
            ]

        // Preview — toggles a right-side pane for statically previewing any SM asset
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text_Lambda([this]()
                    {
                        return _IsPreviewOpen
                            ? FText::FromString(TEXT("Close Preview"))
                            : FText::FromString(TEXT("Preview"));
                    })
                    .OnClicked_Lambda([this]()
                    {
                        _IsPreviewOpen = NOT _IsPreviewOpen;

                        if (NOT _RootSplitter.IsValid())
                        { return FReply::Handled(); }

                        if (_IsPreviewOpen && _PreviewPane.IsValid())
                        {
                            // Even 50/50 split with the live view on the left
                            _RootSplitter->AddSlot()
                                .Value(1.0f)
                                [
                                    _PreviewPane.ToSharedRef()
                                ];
                        }
                        else if (_RootSplitter->GetChildren()->Num() > 1)
                        {
                            _RootSplitter->RemoveAt(_RootSplitter->GetChildren()->Num() - 1);
                        }

                        return FReply::Handled();
                    })
            ]

        // Test
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text_Lambda([this]()
                    {
                        return _IsTestMode
                            ? FText::FromString(TEXT("Exit Test"))
                            : FText::FromString(TEXT("Test"));
                    })
                    .OnClicked_Lambda([this]()
                    {
                        _IsTestMode = !_IsTestMode;

                        if (_IsTestMode)
                        {
                            _Graph->SetSuppressNotifications(true);
                            _Graph->BuildMockup();
                            _Graph->SetSuppressNotifications(false);
                            _Graph->NotifyGraphChanged();
                        }
                        else
                        {
                            _Graph->ForceRebuild();
                        }

                        return FReply::Handled();
                    })
            ]

        // ── Spacer ───────────────────────────────────────────────────────

        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNullWidget::NullWidget
            ]

        // ── Preview picker (inline, only visible when the preview pane is open) ──
        // Sharing the main toolbar row keeps the preview graph top aligned with
        // the live graph top on the left. Occupies the right half via FillWidth(1).
        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(2.0f)
            [
                SNew(SBox)
                    .Visibility_Lambda([this]()
                    {
                        return _IsPreviewOpen ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    [
                        _PreviewPane->BuildPickerRow()
                    ]
            ]

        // ── Breakpoint status (right-aligned) ───────────────────────────

        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        if (NOT _ViewModel.IsValid())
                        { return FText::GetEmpty(); }

                        auto SmInfo = _ViewModel->Get_CurrentSmInfo();
                        if (SmInfo && SmInfo->HasBreakpointHit)
                        { return FText::FromString(SmInfo->BreakpointHitDescription); }

                        return FText::GetEmpty();
                    })
                    .ColorAndOpacity(CkStyle::Err())
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            ]

        // ── Per-window refresh controls ─────────────────────────────────

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(12.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SCkDebugger_RefreshControls)
                    .WindowId(SCkSmDebuggerWindow::WindowId)
            ];
}

// --------------------------------------------------------------------------------------------------------------------
// SM Selector
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    RefreshSmSelector()
    -> void
{
    if (NOT _ViewModel.IsValid())
    { return; }

    auto& AllSms = _ViewModel->Get_AllStateMachines();

    // Identity diff — count-based diff missed cross-PIE handle churn where Num() happened
    // to stay equal. Compare handle-by-handle so a new PIE session always triggers refresh.
    auto NeedsRepopulate = _SmSelectorHandles.Num() != AllSms.Num();
    if (NOT NeedsRepopulate)
    {
        for (auto i = 0; i < AllSms.Num(); ++i)
        {
            if (_SmSelectorHandles[i] != AllSms[i].Handle)
            {
                NeedsRepopulate = true;
                break;
            }
        }
    }

    if (NeedsRepopulate)
    {
        _SmSelectorItems.Empty(AllSms.Num());
        _SmSelectorHandles.Empty(AllSms.Num());

        for (auto& SmInfo : AllSms)
        {
            _SmSelectorItems.Add(MakeShared<FString>(SmInfo.DebugName));
            _SmSelectorHandles.Add(SmInfo.Handle);
        }

        // Auto-select the first SM if nothing is selected
        if (NOT _ViewModel->Has_SelectedSm() && _SmSelectorHandles.Num() > 0)
        { _ViewModel->Set_SelectedSmHandle(_SmSelectorHandles[0]); }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Timeline toolbar — scrub navigation kept close to the timeline so the eye
// doesn't have to bounce between the top of the window and the timeline row.
// Visible only while scrubbing; the top toolbar still owns the SM picker, run
// selector, Pause, Preview, Test and the frames toggle (less timeline-specific).
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    BuildTimelineToolbar()
    -> TSharedRef<SWidget>
{
    // Used only by the Back-to-Live button: hide it (but reserve the layout space)
    // while in Live mode so toggling Scrub doesn't pop the toolbar height.
    auto BackToLiveVis = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(
        [this]()
        {
            if (NOT _ViewModel.IsValid()) { return EVisibility::Hidden; }
            return (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                ? EVisibility::Visible : EVisibility::Hidden;
        }));

    // Returns the run-relative time the navigation buttons should treat as the
    // "current cursor position" when entering Scrub from Live. In Live mode
    // ScrubTime is meaningless (defaults to 0), so we use the run's duration —
    // i.e. "now". In Scrub mode we use the existing ScrubTime.
    auto GetCursorTime = [this]() -> double
    {
        if (NOT _ViewModel.IsValid()) { return 0.0; }
        if (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
        { return _ViewModel->Get_ScrubState().ScrubTime; }
        auto* Info = _ViewModel->Get_CurrentSmInfo();
        return Info ? Info->CurrentRun.Duration : 0.0;
    };

    auto EnsureScrubMode = [this]()
    {
        if (_ViewModel.IsValid() && _ViewModel->Get_ViewMode() != ECkSmDebugger_ViewMode::Scrub)
        { _ViewModel->Set_ViewMode(ECkSmDebugger_ViewMode::Scrub); }
    };

    // Scrub frame as a live-bound text — when not focused, displays the current
    // scrub-needle frame; when committed, jumps the needle to that frame.
    auto FrameAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
        [this]() -> FText
        {
            if (NOT _ViewModel.IsValid()) { return FText::GetEmpty(); }
            auto* Info = _ViewModel->Get_CurrentSmInfo();
            if (NOT Info) { return FText::GetEmpty(); }
            auto& SS = _ViewModel->Get_ScrubState();
            auto RIdx = SS.SelectedRunIndex;
            auto& R = (RIdx < 0)
                ? Info->CurrentRun
                : (RIdx < Info->CompletedRuns.Num() ? Info->CompletedRuns[RIdx] : Info->CurrentRun);
            if (R.FrameSegments.IsEmpty()) { return FText::GetEmpty(); }

            auto T = SS.ScrubTime;
            int64 Frame = static_cast<int64>(R.FrameSegments[0].StartFrame);
            if (T <= R.FrameSegments[0].StartTime) { Frame = static_cast<int64>(R.FrameSegments[0].StartFrame); }
            else if (T >= R.FrameSegments.Last().EndTime) { Frame = static_cast<int64>(R.FrameSegments.Last().EndFrame); }
            else
            {
                for (auto& Seg : R.FrameSegments)
                {
                    if (T >= Seg.StartTime && T <= Seg.EndTime)
                    {
                        auto Span = Seg.EndTime - Seg.StartTime;
                        auto Frac = (Span > 0.0) ? (T - Seg.StartTime) / Span : 0.0;
                        auto FrameSpan = static_cast<int64>(Seg.EndFrame) - static_cast<int64>(Seg.StartFrame);
                        Frame = static_cast<int64>(Seg.StartFrame) + FMath::FloorToInt64(Frac * FrameSpan);
                        break;
                    }
                }
            }
            return FText::FromString(FString::Printf(TEXT("%lld"), Frame));
        }));

    return SNew(SHorizontalBox)

            // To Start — works in either mode; auto-enters Scrub.
            + SHorizontalBox::Slot()
                .AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("|\x25C0")))
                        .ToolTipText(NSLOCTEXT("CkSmDebugger", "ToStartTooltip",
                            "Jump the scrub needle to the start of the run (Home)."))
                        .OnClicked_Lambda([this, EnsureScrubMode]()
                        {
                            if (_ViewModel.IsValid())
                            {
                                EnsureScrubMode();
                                auto NS = _ViewModel->Get_ScrubState();
                                NS.ScrubTime = 0.0;
                                _ViewModel->Set_ScrubState(NS);
                                FocusTimelineOnCursor();
                            }
                            return FReply::Handled();
                        })
                ]

            // Step Prev — auto-enters Scrub at the previous transition relative to
            // current "cursor" (ScrubTime in Scrub, RunDuration in Live).
            + SHorizontalBox::Slot()
                .AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("\x25C0")))
                        .ToolTipText(NSLOCTEXT("CkSmDebugger", "StepBackTooltip",
                            "Jump scrub to the previous transition (Left arrow)."))
                        .OnClicked_Lambda([this, EnsureScrubMode, GetCursorTime]()
                        {
                            if (NOT _ViewModel.IsValid()) { return FReply::Handled(); }
                            // Seed ScrubTime to the cursor before stepping so Live mode
                            // walks back from "now" rather than from time 0.
                            auto NS = _ViewModel->Get_ScrubState();
                            NS.ScrubTime = GetCursorTime();
                            _ViewModel->Set_ScrubState(NS);
                            EnsureScrubMode();
                            StepScrubToTransition(-1);
                            return FReply::Handled();
                        })
                ]

            // "Frame: <live value>" — shows the scrub frame in Scrub mode and the
            // live frame in Live mode. Editing + Enter jumps and enters Scrub.
            + SHorizontalBox::Slot()
                .AutoWidth().Padding(8.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(NSLOCTEXT("CkSmDebugger", "JumpToFrameLabel", "Frame:"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                ]
            + SHorizontalBox::Slot()
                .AutoWidth().Padding(0.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(80.0f)
                    [
                        SNew(SEditableTextBox)
                            .Text(FrameAttr)
                            .ToolTipText(NSLOCTEXT("CkSmDebugger", "JumpTooltip",
                                "Current frame. Type a frame number and press Enter to jump the scrub needle there."))
                            .SelectAllTextWhenFocused(true)
                            .RevertTextOnEscape(true)
                            .OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType)
                            {
                                if (InCommitType != ETextCommit::OnEnter) { return; }
                                auto Str = InText.ToString().TrimStartAndEnd();
                                if (Str.IsEmpty()) { return; }
                                int64 Frame = 0;
                                LexFromString(Frame, *Str);
                                JumpScrubToFrame(Frame);
                            })
                    ]
                ]

            // Step Next — only meaningful in Scrub mode (Live is already at "now").
            // Enabled in Scrub, disabled (greyed) in Live.
            + SHorizontalBox::Slot()
                .AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("\x25B6")))
                        .ToolTipText(NSLOCTEXT("CkSmDebugger", "StepFwdTooltip",
                            "Jump scrub to the next transition (Right arrow). Disabled in Live mode."))
                        .IsEnabled_Lambda([this]()
                        {
                            return _ViewModel.IsValid()
                                && _ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub;
                        })
                        .OnClicked_Lambda([this]() { StepScrubToTransition(+1); return FReply::Handled(); })
                ]

            // Bookmark add — works in both modes (bookmarks the current cursor position).
            + SHorizontalBox::Slot()
                .AutoWidth().Padding(8.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("\x2606")))
                        .ToolTipText(NSLOCTEXT("CkSmDebugger", "AddBookmarkTooltip",
                            "Bookmark the current cursor position (Ctrl+B). Shift+B removes the nearest one."))
                        .OnClicked_Lambda([this, GetCursorTime]()
                        {
                            if (_ViewModel.IsValid())
                            { _ViewModel->AddBookmark(GetCursorTime()); }
                            return FReply::Handled();
                        })
                ]

            // Prev bookmark — auto-enters Scrub.
            + SHorizontalBox::Slot()
                .AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("\x25C0\x2606")))
                        .ToolTipText(NSLOCTEXT("CkSmDebugger", "PrevBookmarkTooltip",
                            "Jump to the previous bookmark (F2)."))
                        .OnClicked_Lambda([this, EnsureScrubMode, GetCursorTime]()
                        {
                            if (NOT _ViewModel.IsValid()) { return FReply::Handled(); }
                            auto Target = _ViewModel->FindNextBookmark(GetCursorTime(), -1);
                            if (Target >= 0.0)
                            {
                                EnsureScrubMode();
                                auto NS = _ViewModel->Get_ScrubState();
                                NS.ScrubTime = Target;
                                _ViewModel->Set_ScrubState(NS);
                                FocusTimelineOnCursor();
                            }
                            return FReply::Handled();
                        })
                ]

            // Next bookmark — auto-enters Scrub.
            + SHorizontalBox::Slot()
                .AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("\x2606\x25B6")))
                        .ToolTipText(NSLOCTEXT("CkSmDebugger", "NextBookmarkTooltip",
                            "Jump to the next bookmark (F3)."))
                        .OnClicked_Lambda([this, EnsureScrubMode, GetCursorTime]()
                        {
                            if (NOT _ViewModel.IsValid()) { return FReply::Handled(); }
                            auto Target = _ViewModel->FindNextBookmark(GetCursorTime(), +1);
                            if (Target >= 0.0)
                            {
                                EnsureScrubMode();
                                auto NS = _ViewModel->Get_ScrubState();
                                NS.ScrubTime = Target;
                                _ViewModel->Set_ScrubState(NS);
                                FocusTimelineOnCursor();
                            }
                            return FReply::Handled();
                        })
                ]

            // Back to Live — visible in Scrub, Hidden (space reserved) in Live so
            // the toolbar height never changes between modes.
            + SHorizontalBox::Slot()
                .AutoWidth().Padding(16.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(SBox).Visibility(BackToLiveVis)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Back to Live")))
                            .ToolTipText(NSLOCTEXT("CkSmDebugger", "BackToLiveTooltip",
                                "Resume tracking the live SM (exits Scrub mode)."))
                            .OnClicked_Lambda([this]()
                            {
                                if (_ViewModel.IsValid())
                                {
                                    _ViewModel->Set_ViewMode(ECkSmDebugger_ViewMode::Live);
                                    _ViewModel->ClearScrubTransitionHighlight();
                                    // Re-attach the widget's live-follow — the user may have
                                    // panned away while scrubbing.
                                    FocusTimelineOnCursor();
                                    _SelectedHistoryEntry.Reset();
                                }
                                return FReply::Handled();
                            })
                    ]
                ]

            // Trailing spacer.
            + SHorizontalBox::Slot().FillWidth(1.0f)
                [ SNullWidget::NullWidget ]
        ;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    JumpScrubToFrame(
        int64 InFrame)
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }
    auto* SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (NOT SmInfo) { return; }

    auto& ScrubState = _ViewModel->Get_ScrubState();
    auto RIdx = ScrubState.SelectedRunIndex;
    auto& Run = (RIdx < 0)
        ? SmInfo->CurrentRun
        : (RIdx < SmInfo->CompletedRuns.Num() ? SmInfo->CompletedRuns[RIdx] : SmInfo->CurrentRun);
    if (Run.FrameSegments.IsEmpty()) { return; }

    // Convert target frame → run-relative time using the FrameSegments table.
    auto F = static_cast<uint64>(FMath::Max<int64>(InFrame, 0));
    auto Target = 0.0;
    if (F <= Run.FrameSegments[0].StartFrame) { Target = 0.0; }
    else if (F >= Run.FrameSegments.Last().EndFrame) { Target = Run.Duration; }
    else
    {
        for (auto& Seg : Run.FrameSegments)
        {
            if (F >= Seg.StartFrame && F <= Seg.EndFrame)
            {
                auto FrameSpan = static_cast<double>(Seg.EndFrame - Seg.StartFrame);
                auto Frac = (FrameSpan > 0.0) ? static_cast<double>(F - Seg.StartFrame) / FrameSpan : 0.0;
                Target = Seg.StartTime + Frac * (Seg.EndTime - Seg.StartTime);
                break;
            }
        }
    }

    auto NewScrubState = ScrubState;
    NewScrubState.ScrubTime = FMath::Clamp(Target, 0.0, Run.Duration);
    _ViewModel->Set_ScrubState(NewScrubState);
    _ViewModel->Set_ViewMode(ECkSmDebugger_ViewMode::Scrub);
    FocusTimelineOnCursor();
}

// --------------------------------------------------------------------------------------------------------------------
// Detail panel — structured widget-based layout.
// Content is rebuilt only when the selection signature changes; live values
// (dwell time, yes/no flags, task results) update inside via attribute lambdas.
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    BuildDetailPanel()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderBackgroundColor(CkStyle::Bg1())
        .Padding(FMargin(8.0f))
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
                .Padding(2.0f)
                [
                    SAssignNew(_DetailContentBox, SBox)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("No selection")))
                            .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                            .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    ]
                ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    RefreshDetailContent()
    -> void
{
    if (NOT _DetailContentBox.IsValid())
    { return; }

    // Compute current selection signature
    auto Sig = FDetailSignature{};
    if (_ViewModel.IsValid())
    {
        auto* SmInfo = _ViewModel->Get_CurrentSmInfo();
        Sig.SmInfo = static_cast<const void*>(SmInfo);
        Sig.ScrubMode = (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub) ? 1 : 0;
        // In scrub mode the panel layout doesn't depend on which history row
        // (if any) was clicked — Case 1 is gated to live mode only — so don't
        // let history-click churn force panel rebuilds during scrubbing.
        Sig.HistoryEntry = Sig.ScrubMode
            ? nullptr
            : static_cast<const void*>(_SelectedHistoryEntry.Get());
        Sig.NodeIdx = _ViewModel->Get_SelectedNodeIndex();
        Sig.TransitionIdx = _SelectedTransitionIndex;
        auto& ScrubSnap = _ViewModel->Get_ScrubSnapshot();
        Sig.ScrubHistoryIdx = ScrubSnap.HistoryIndex;
        Sig.ScrubActiveStateIdx = ScrubSnap.ActiveStateIndex;

        // ScrubAlignsWithSelectedEntry is no longer consulted in scrub mode
        // (Case 1 only fires in live mode) — leave it at 1 in scrub so it
        // doesn't flip 0/1 as the needle crosses history rows and force a
        // pointless rebuild. In live mode it tracks the click target normally.
        Sig.ScrubAlignsWithSelectedEntry = 1;
        if (NOT Sig.ScrubMode && SmInfo && _SelectedHistoryEntry.IsValid())
        {
            auto& SS = _ViewModel->Get_ScrubState();
            auto RIdx = SS.SelectedRunIndex;
            auto& R = (RIdx < 0)
                ? SmInfo->CurrentRun
                : (RIdx < SmInfo->CompletedRuns.Num() ? SmInfo->CompletedRuns[RIdx] : SmInfo->CurrentRun);
            auto EntryRunRel = _SelectedHistoryEntry->RealTimeSeconds - R.StartTime;
            constexpr auto Epsilon = 0.05;
            Sig.ScrubAlignsWithSelectedEntry = (FMath::Abs(EntryRunRel - SS.ScrubTime) <= Epsilon) ? 1 : 0;
        }

        if (SmInfo && Sig.NodeIdx >= 0 && Sig.NodeIdx < SmInfo->States.Num())
        {
            Sig.TaskCount = SmInfo->States[Sig.NodeIdx].Tasks.Num();
            for (auto& Tr : SmInfo->Transitions)
            {
                if (Tr.SourceStateIndex == Sig.NodeIdx)
                { Sig.TransitionCount++; }
            }
        }
    }

    if (Sig == _LastDetailSig)
    { return; }

    _LastDetailSig = Sig;
    _DetailContentBox->SetContent(BuildDetailContent());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    BuildDetailContent()
    -> TSharedRef<SWidget>
{
    auto MakeNoSelection = []()
    {
        return StaticCastSharedRef<SWidget>(
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("No selection")))
                .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9)));
    };

    if (NOT _ViewModel.IsValid())
    { return MakeNoSelection(); }

    auto* SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (NOT SmInfo)
    { return MakeNoSelection(); }

    auto Depth = _Graph ? _Graph->LayoutParams.NameDepth : 1;
    auto Root = SNew(SVerticalBox);

    // ───────────────────────────────────────────────────────────────────
    // Case 1: history entry selected (live mode only)
    //
    // In scrub mode the unified scrub view (Case 4 prefix + Case 3 body)
    // always wins — clicking a history row updates the scrub time but the
    // panel keeps showing STATE / TASKS / TRANSITIONS for the segment
    // under the needle, so the layout doesn't flip between two completely
    // different shapes as you drag past transition frames.
    // ───────────────────────────────────────────────────────────────────
    auto IsScrubbingForCase1 = _ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub;
    if (NOT IsScrubbingForCase1 && _SelectedHistoryEntry.IsValid())
    {
        auto& Entry = *_SelectedHistoryEntry;
        auto ToName   = SCkDebug_NameLabel::Get_ShortName(Entry.ToStateName,   Depth);
        auto FromName = SCkDebug_NameLabel::Get_ShortName(Entry.FromStateName, Depth);

        // Find state indices
        auto ToIdx = -1;
        auto FromIdx = -1;
        for (auto i = 0; i < SmInfo->States.Num(); ++i)
        {
            if (SmInfo->States[i].StateName == Entry.ToStateName)   { ToIdx = i; }
            if (SmInfo->States[i].StateName == Entry.FromStateName) { FromIdx = i; }
        }

        // --- ARRIVED AT ---
        Root->AddSlot().AutoHeight() [ ck_sm_debugger_window::MakeSectionHeader(TEXT("ARRIVED AT")) ];
        Root->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("\x25CF")))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Bullet()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(ToName))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
        ];

        if (ToIdx >= 0)
        {
            auto& ToState = SmInfo->States[ToIdx];
            for (auto& Task : ToState.Tasks)
            {
                auto TName = SCkDebug_NameLabel::Get_ShortName(Task.ClassName, Depth);
                auto IconChar = FString{TEXT("\x25CF")};
                switch (Task.LastResult)
                {
                case ECk_SmTaskResult::Succeeded: IconChar = TEXT("\x2713"); break;
                case ECk_SmTaskResult::Failed:    IconChar = TEXT("\x2717"); break;
                default: break;
                }
                const auto IconColor = CkSmDebugger::GetTaskResultColor(Task.LastResult);
                Root->AddSlot().AutoHeight().Padding(16.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(IconChar))
                                .ColorAndOpacity(FSlateColor(IconColor))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TName))
                                .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        ]
                ];
            }
        }

        // --- CAME FROM ---
        Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ ck_sm_debugger_window::MakeSectionHeader(TEXT("CAME FROM")) ];
        Root->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("\x25CB")))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FromName))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
        ];

        if (FromIdx >= 0)
        {
            auto& FromState = SmInfo->States[FromIdx];
            for (auto& Task : FromState.Tasks)
            {
                auto TName = SCkDebug_NameLabel::Get_ShortName(Task.ClassName, Depth);
                Root->AddSlot().AutoHeight().Padding(16.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TName))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                ];
            }
        }

        // --- TASKS AT TRANSITION ---
        if (Entry.TaskSnapshots.Num() > 0)
        {
            Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ ck_sm_debugger_window::MakeSectionHeader(TEXT("TASKS AT TRANSITION")) ];
            for (auto& Snap : Entry.TaskSnapshots)
            {
                auto TName = SCkDebug_NameLabel::Get_ShortName(Snap.TaskName, Depth);
                auto Label = FString{TEXT("RUNNING")};
                switch (Snap.Result)
                {
                case ECk_SmTaskResult::Succeeded: Label = TEXT("SUCCEEDED"); break;
                case ECk_SmTaskResult::Failed:    Label = TEXT("FAILED");    break;
                default: break;
                }
                const auto Color = CkSmDebugger::GetTaskResultColor(Snap.Result);
                Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TName))
                                .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                        [
                            ck_sm_debugger_window::MakePill(Label, Color)
                        ]
                ];
            }
        }

        // --- TRANSITION conditions ---
        Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ ck_sm_debugger_window::MakeSectionHeader(TEXT("TRANSITION")) ];
        if (Entry.ConditionNames.Num() > 0)
        {
            for (auto& CondName : Entry.ConditionNames)
            {
                auto CName = SCkDebug_NameLabel::Get_ShortName(CondName, Depth);
                Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                        [
                            ck_sm_debugger_window::MakeConditionPill(ECk_SmConditionResult::Pass)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(CName))
                                .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        ]
                ];
            }
        }
        else
        {
            Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(unconditional)")))
                    .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
            ];
        }

        Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("Frame [%llu]"), Entry.FrameNumber)))
                .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
        ];

        return Root;
    }

    // ───────────────────────────────────────────────────────────────────
    // Case 2: transition selected in graph
    // ───────────────────────────────────────────────────────────────────
    if (_SelectedTransitionIndex >= 0 && _SelectedTransitionIndex < SmInfo->Transitions.Num())
    {
        auto TransIdx = _SelectedTransitionIndex;

        Root->AddSlot().AutoHeight() [ ck_sm_debugger_window::MakeSectionHeader(TEXT("TRANSITION")) ];

        auto SrcName = SCkDebug_NameLabel::Get_ShortName(SmInfo->Transitions[TransIdx].SourceStateName, Depth);
        auto DstName = SCkDebug_NameLabel::Get_ShortName(SmInfo->Transitions[TransIdx].TargetStateName, Depth);

        Root->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(SrcName))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("\x2500\x25B6")))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Arrow()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(DstName))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
        ];

        // Live count + result pill
        auto CountAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
            [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel), TransIdx]()
            {
                auto VM = ViewModelWeak.Pin();
                if (NOT VM.IsValid()) { return FText::GetEmpty(); }
                auto* Info = VM->Get_CurrentSmInfo();
                if (NOT Info || TransIdx >= Info->Transitions.Num()) { return FText::GetEmpty(); }
                auto& T = Info->Transitions[TransIdx];
                return FText::FromString(FString::Printf(TEXT("%d/%d conditions satisfied"), T.SatisfiedCount, T.TotalCount));
            }));

        auto TransSelHasPolled = SmInfo->Transitions[TransIdx].Conditions.ContainsByPredicate(
            [](const FCkSmDebugger_ConditionInfo& C){ return C.Mode != ECk_SmConditionMode::EventDriven; });
        auto TransSelModeLabel = TransSelHasPolled ? FString{TEXT("POLLED")} : FString{TEXT("EVENT-DRIVEN")};
        auto TransSelModeColor = TransSelHasPolled
            ? CkStyle::TextDim()
            : CkStyle::Warn();

        Root->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(CountAttr)
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
                [
                    ck_sm_debugger_window::MakePill(TransSelModeLabel, TransSelModeColor)
                ]
        ];

        Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ ck_sm_debugger_window::MakeSectionHeader(TEXT("CONDITIONS")) ];

        auto& Trans = SmInfo->Transitions[TransIdx];
        for (auto i = 0; i < Trans.Conditions.Num(); ++i)
        {
            auto& Cond = Trans.Conditions[i];
            auto CName = SCkDebug_NameLabel::Get_ShortName(Cond.ClassName, Depth);
            auto ClassStr = IsValid(Cond.ScriptClass) ? Cond.ScriptClass->GetName() : FString(TEXT("(unknown)"));

            auto CondIdx = i;
            auto ResultAttr = TAttribute<ECk_SmConditionResult>::Create(TAttribute<ECk_SmConditionResult>::FGetter::CreateLambda(
                [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel), TransIdx, CondIdx]()
                {
                    auto VM = ViewModelWeak.Pin();
                    if (NOT VM.IsValid()) { return ECk_SmConditionResult::Undetermined; }
                    auto* Info = VM->Get_CurrentSmInfo();
                    if (NOT Info || TransIdx >= Info->Transitions.Num()) { return ECk_SmConditionResult::Undetermined; }
                    auto& Tr = Info->Transitions[TransIdx];
                    if (CondIdx >= Tr.Conditions.Num()) { return ECk_SmConditionResult::Undetermined; }
                    return Tr.Conditions[CondIdx].Result;
                }));

            auto CondIsEventDriven = (Cond.Mode == ECk_SmConditionMode::EventDriven);
            auto CondModeLabel = CondIsEventDriven ? FString{TEXT("E")} : FString{TEXT("P")};
            auto CondModeColor = CondIsEventDriven
                ? CkStyle::Warn()
                : CkStyle::TextDim();

            Root->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                            [
                                ck_sm_debugger_window::MakeConditionPillLive(ResultAttr)
                            ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                            [
                                ck_sm_debugger_window::MakePill(CondModeLabel, CondModeColor)
                            ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(CName))
                                    .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(44.0f, 1.0f, 0.0f, 0.0f)
                    [
                        ck_sm_debugger_window::MakeClassName(ClassStr)
                    ]
            ];
        }

        return Root;
    }

    // ───────────────────────────────────────────────────────────────────
    // Scrub prefix (renders before the state body when scrubbing): adds SCRUB AT
    // / CAME FROM / NEXT TRANSITION blocks at the top so the user sees both the
    // needle's position context AND the live state details in one panel. The
    // state body itself (Case 3 below) renders the active state's tasks and
    // transitions — the window's Tick auto-mirrors SelectedNodeIndex to the
    // scrubbed-active state, so the same code path serves both modes.
    // ───────────────────────────────────────────────────────────────────
    auto IsScrubbing = _ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub;
    if (IsScrubbing)
    {
        auto& ScrubState = _ViewModel->Get_ScrubState();
        auto SRunIdx = ScrubState.SelectedRunIndex;
        auto& SRun = (SRunIdx < 0)
            ? SmInfo->CurrentRun
            : (SRunIdx < SmInfo->CompletedRuns.Num() ? SmInfo->CompletedRuns[SRunIdx] : SmInfo->CurrentRun);

        auto ActiveSegIdx = -1;
        auto ActiveSegStart = 0.0;
        auto ActiveSegStartFrame = uint64{0};
        for (auto i = 0; i < SRun.Segments.Num(); ++i)
        {
            auto& Seg = SRun.Segments[i];
            if (ScrubState.ScrubTime >= Seg.StartTime && ScrubState.ScrubTime <= Seg.EndTime)
            {
                ActiveSegIdx = i;
                ActiveSegStart = Seg.StartTime;
                ActiveSegStartFrame = Seg.StartFrame;
                break;
            }
        }

        if (ActiveSegIdx >= 0)
        {
            auto ScrubFrameAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
                [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel)]()
                {
                    auto VM = ViewModelWeak.Pin();
                    if (NOT VM.IsValid()) { return FText::GetEmpty(); }
                    auto* Info = VM->Get_CurrentSmInfo();
                    if (NOT Info) { return FText::GetEmpty(); }
                    auto& SS = VM->Get_ScrubState();
                    auto RIdx = SS.SelectedRunIndex;
                    auto& R = (RIdx < 0)
                        ? Info->CurrentRun
                        : (RIdx < Info->CompletedRuns.Num() ? Info->CompletedRuns[RIdx] : Info->CurrentRun);

                    auto T = SS.ScrubTime;
                    int64 Frame = R.FrameSegments.IsEmpty()
                        ? 0
                        : static_cast<int64>(R.FrameSegments[0].StartFrame);
                    if (NOT R.FrameSegments.IsEmpty())
                    {
                        if (T <= R.FrameSegments[0].StartTime) { Frame = static_cast<int64>(R.FrameSegments[0].StartFrame); }
                        else if (T >= R.FrameSegments.Last().EndTime) { Frame = static_cast<int64>(R.FrameSegments.Last().EndFrame); }
                        else
                        {
                            for (auto& Seg : R.FrameSegments)
                            {
                                if (T >= Seg.StartTime && T <= Seg.EndTime)
                                {
                                    auto Span = Seg.EndTime - Seg.StartTime;
                                    auto Frac = (Span > 0.0) ? (T - Seg.StartTime) / Span : 0.0;
                                    auto FrameSpan = static_cast<int64>(Seg.EndFrame) - static_cast<int64>(Seg.StartFrame);
                                    Frame = static_cast<int64>(Seg.StartFrame) + FMath::FloorToInt64(Frac * FrameSpan);
                                    break;
                                }
                            }
                        }
                    }
                    return FText::FromString(FString::Printf(TEXT("Frame [%lld] (%03lld)  \x2022  %.2fs"), Frame, Frame % 1000, T));
                }));

            Root->AddSlot().AutoHeight() [ ck_sm_debugger_window::MakeSectionHeader(TEXT("SCRUB AT")) ];
            Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(ScrubFrameAttr)
                    .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                    .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
            ];

            // CAME FROM (the transition that entered this segment)
            auto CameFromHistIdx = ActiveSegIdx > 0 ? ActiveSegIdx - 1 : -1;
            if (CameFromHistIdx >= 0 && CameFromHistIdx < SRun.History.Num())
            {
                auto& PrevEntry = SRun.History[CameFromHistIdx];
                auto FromName = SCkDebug_NameLabel::Get_ShortName(PrevEntry.FromStateName, Depth);

                Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ ck_sm_debugger_window::MakeSectionHeader(TEXT("CAME FROM")) ];
                Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("%s  (frame %llu)"), *FromName, PrevEntry.FrameNumber)))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                ];
            }

            // NEXT TRANSITION (upcoming history entry, if any). Mirrors Case 1's
            // TRANSITION block so the same PASS feedback shows whether the user
            // got here by clicking a history row or by pure timeline scrubbing.
            auto NextHistIdx = (ActiveSegIdx >= 0 && ActiveSegIdx < SRun.History.Num()) ? ActiveSegIdx : -1;
            if (NextHistIdx >= 0 && NextHistIdx < SRun.History.Num())
            {
                auto& NextEntry = SRun.History[NextHistIdx];
                auto NextName = SCkDebug_NameLabel::Get_ShortName(NextEntry.ToStateName, Depth);
                auto FramesAhead = static_cast<int64>(NextEntry.FrameNumber) - static_cast<int64>(ActiveSegStartFrame);

                Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ ck_sm_debugger_window::MakeSectionHeader(TEXT("NEXT TRANSITION")) ];
                Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(
                            TEXT("\x2500\x25B6 %s  (in %lld frames, frame %llu)"),
                            *NextName, FramesAhead, NextEntry.FrameNumber)))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                ];

                // Conditions that fired the transition (PASS pills). Falls back to
                // "(unconditional)" when ConditionNames is empty — same as Case 1.
                if (NextEntry.ConditionNames.Num() > 0)
                {
                    for (auto& CondName : NextEntry.ConditionNames)
                    {
                        auto CName = SCkDebug_NameLabel::Get_ShortName(CondName, Depth);
                        Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                        [
                            SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                                [
                                    ck_sm_debugger_window::MakeConditionPill(ECk_SmConditionResult::Pass)
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(CName))
                                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                ]
                        ];
                    }
                }
                else
                {
                    Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("(unconditional)")))
                            .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                            .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    ];
                }
            }
            else
            {
                Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("(currently the latest state in this run)")))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                ];
            }

            // Visual divider between scrub-navigation and the state body that follows.
            Root->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f) [ SNullWidget::NullWidget ];
        }
    }

    // ───────────────────────────────────────────────────────────────────
    // Case 3: state body (live or scrubbed). The window's Tick auto-mirrors
    // SelectedNodeIndex to the scrub-active state, so the same code path
    // renders state + tasks + transitions in either mode.
    // ───────────────────────────────────────────────────────────────────
    auto SelectedIdx = _ViewModel->Get_SelectedNodeIndex();
    if (SelectedIdx >= 0 && SelectedIdx < SmInfo->States.Num())
    {
        auto& State = SmInfo->States[SelectedIdx];
        auto DisplayName = SCkDebug_NameLabel::Get_ShortName(State.StateName, Depth);
        auto ClassStr = IsValid(State.ScriptClass) ? State.ScriptClass->GetName() : FString(TEXT("(unknown)"));
        auto HasOverride = IsValid(State.RequestedScriptClass) && State.RequestedScriptClass != State.ScriptClass;
        auto OverrideStr = HasOverride ? State.RequestedScriptClass->GetName() : FString{};

        Root->AddSlot().AutoHeight() [ ck_sm_debugger_window::MakeSectionHeader(TEXT("STATE")) ];

        // Name row — bullet + name + ACTIVE pill
        {
            auto NameRow = SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("\x25CF")))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Bullet()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(DisplayName))
                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                ];

            auto StateIdx = SelectedIdx;
            auto VisibilityAttr = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(
                [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel), StateIdx]()
                {
                    auto VM = ViewModelWeak.Pin();
                    if (NOT VM.IsValid()) { return EVisibility::Collapsed; }
                    auto* Info = VM->Get_CurrentSmInfo();
                    if (NOT Info || StateIdx >= Info->States.Num()) { return EVisibility::Collapsed; }
                    if (VM->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                    {
                        return VM->Get_ScrubSnapshot().ActiveStateIndex == StateIdx
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                    }
                    return Info->States[StateIdx].IsCurrentState ? EVisibility::Visible : EVisibility::Collapsed;
                }));

            NameRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                    .Visibility(VisibilityAttr)
                    [
                        ck_sm_debugger_window::MakePill(TEXT("ACTIVE"), CkStyle::Warn())
                    ]
            ];

            // HISTORICAL pill: shown when this is a sub-SM state restored from the
            // persistent cache (live entity is gone but we kept the structure so
            // the inspector still works while scrubbing into that segment).
            if (State.IsHistoricalSubSm)
            {
                NameRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                [
                    ck_sm_debugger_window::MakePill(TEXT("HISTORICAL"), CkStyle::TextDim())
                ];
            }

            // Event-driven / tick / polled summary pills — only meaningful when the
            // runtime has cached real data for this state (ScriptClass is valid).
            // Unvisited target states get placeholder entries with no tasks/conditions,
            // which would otherwise read as vacuously "event-driven".
            auto HasCompleteData = IsValid(State.ScriptClass);
            auto HasAnyTickTask = State.Tasks.ContainsByPredicate(
                [](const FCkSmDebugger_TaskInfo& T){ return T.Mode == ECk_SmTaskMode::Tick; });
            auto HasAnyPolledCondition = false;
            for (const auto& Trans : SmInfo->Transitions)
            {
                if (Trans.SourceStateIndex != SelectedIdx) { continue; }
                for (const auto& Cond : Trans.Conditions)
                {
                    if (Cond.Mode != ECk_SmConditionMode::EventDriven)
                    { HasAnyPolledCondition = true; break; }
                }
                if (HasAnyPolledCondition) { break; }
            }
            auto IsFullyEventDriven = HasCompleteData && NOT HasAnyTickTask && NOT HasAnyPolledCondition;

            if (IsFullyEventDriven)
            {
                NameRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                [
                    ck_sm_debugger_window::MakePill(TEXT("EVENT-DRIVEN"), CkStyle::Warn())
                ];
            }
            else if (HasCompleteData)
            {
                if (HasAnyTickTask)
                {
                    NameRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                    [
                        ck_sm_debugger_window::MakePill(TEXT("TICKS"), CkStyle::Warn())
                    ];
                }
                if (HasAnyPolledCondition)
                {
                    NameRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                    [
                        ck_sm_debugger_window::MakePill(TEXT("POLLED"), CkStyle::TextDim())
                    ];
                }
            }

            Root->AddSlot().AutoHeight() [ NameRow ];
        }

        // Class name + override info
        Root->AddSlot().AutoHeight().Padding(20.0f, 2.0f, 0.0f, 0.0f) [ ck_sm_debugger_window::MakeClassName(ClassStr) ];

        if (HasOverride)
        {
            Root->AddSlot().AutoHeight().Padding(20.0f, 2.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                    [
                        ck_sm_debugger_window::MakePill(TEXT("OVERRIDE"), FCkSmDebuggerStyle::Color_Sm_Override)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        ck_sm_debugger_window::MakeClassName(OverrideStr)
                    ]
            ];
        }

        Root->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            ck_sm_debugger_window::MakeKeyValue(TEXT("Dwell"), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
                [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel), SelectedIdx]()
                {
                    auto VM = ViewModelWeak.Pin();
                    if (NOT VM.IsValid()) { return FText::GetEmpty(); }
                    auto* Info = VM->Get_CurrentSmInfo();
                    if (NOT Info || SelectedIdx >= Info->States.Num()) { return FText::GetEmpty(); }

                    // While scrubbing, report dwell relative to the scrub needle: time spent
                    // in the active segment up to ScrubTime. For the live state with no exit
                    // yet this matches the live DwellTimeSeconds; for completed segments it
                    // shows the duration the SM was in this state at the scrubbed moment.
                    if (VM->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                    {
                        auto& SS = VM->Get_ScrubState();
                        auto RIdx = SS.SelectedRunIndex;
                        auto& R = (RIdx < 0)
                            ? Info->CurrentRun
                            : (RIdx < Info->CompletedRuns.Num() ? Info->CompletedRuns[RIdx] : Info->CurrentRun);
                        for (auto& Seg : R.Segments)
                        {
                            if (SS.ScrubTime >= Seg.StartTime && SS.ScrubTime <= Seg.EndTime)
                            { return FText::FromString(FString::Printf(TEXT("%.2fs"), FMath::Max(0.0, SS.ScrubTime - Seg.StartTime))); }
                        }
                    }

                    return FText::FromString(FString::Printf(TEXT("%.2fs"), Info->States[SelectedIdx].DwellTimeSeconds));
                })))
        ];
        Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [
            ck_sm_debugger_window::MakeKeyValue(TEXT("Visited"), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
                [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel), SelectedIdx]()
                {
                    auto VM = ViewModelWeak.Pin();
                    if (NOT VM.IsValid()) { return FText::GetEmpty(); }
                    auto* Info = VM->Get_CurrentSmInfo();
                    if (NOT Info || SelectedIdx >= Info->States.Num()) { return FText::GetEmpty(); }
                    return FText::FromString(Info->States[SelectedIdx].HasBeenVisited ? TEXT("Yes") : TEXT("No"));
                })))
        ];

        // --- TASKS ---
        if (State.Tasks.Num() > 0)
        {
            Root->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f) [ ck_sm_debugger_window::MakeSectionHeader(TEXT("TASKS")) ];
            for (auto TaskIdx = 0; TaskIdx < State.Tasks.Num(); ++TaskIdx)
            {
                auto& Task = State.Tasks[TaskIdx];
                auto TName = SCkDebug_NameLabel::Get_ShortName(Task.ClassName, Depth);
                auto TaskClassStr = IsValid(Task.ScriptClass) ? Task.ScriptClass->GetName() : FString(TEXT("(unknown)"));

                auto TaskNameCapture = Task.ClassName;
                auto ResultAttr = TAttribute<ECk_SmTaskResult>::Create(TAttribute<ECk_SmTaskResult>::FGetter::CreateLambda(
                    [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel), SelectedIdx, TaskIdx, TaskNameCapture]()
                    {
                        auto VM = ViewModelWeak.Pin();
                        if (NOT VM.IsValid()) { return ECk_SmTaskResult::Running; }
                        auto* Info = VM->Get_CurrentSmInfo();
                        if (NOT Info || SelectedIdx >= Info->States.Num()) { return ECk_SmTaskResult::Running; }
                        auto& St = Info->States[SelectedIdx];
                        if (TaskIdx >= St.Tasks.Num()) { return ECk_SmTaskResult::Running; }

                        // While scrubbing on a completed state segment, return the task's
                        // recorded result from the next transition's TaskSnapshots — that's
                        // the value at the moment the SM left this state. For the currently-
                        // active live state (no exit yet), fall through to the live value.
                        if (VM->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                        {
                            auto& SS = VM->Get_ScrubState();
                            auto RIdx = SS.SelectedRunIndex;
                            auto& R = (RIdx < 0)
                                ? Info->CurrentRun
                                : (RIdx < Info->CompletedRuns.Num() ? Info->CompletedRuns[RIdx] : Info->CurrentRun);
                            auto ActiveSegIdx = -1;
                            for (auto i = 0; i < R.Segments.Num(); ++i)
                            {
                                if (SS.ScrubTime >= R.Segments[i].StartTime && SS.ScrubTime <= R.Segments[i].EndTime)
                                { ActiveSegIdx = i; break; }
                            }
                            if (ActiveSegIdx >= 0 && ActiveSegIdx < R.History.Num())
                            {
                                auto& NextEntry = R.History[ActiveSegIdx];
                                for (auto& Snap : NextEntry.TaskSnapshots)
                                {
                                    if (Snap.TaskName == TaskNameCapture)
                                    { return Snap.Result; }
                                }
                            }
                        }

                        return St.Tasks[TaskIdx].LastResult;
                    }));

                auto IsTickMode = (Task.Mode == ECk_SmTaskMode::Tick);
                auto ModeLabel = IsTickMode ? FString{TEXT("TICK")} : FString{TEXT("ENTER/EXIT")};
                auto ModeColor = IsTickMode ? CkStyle::Warn() : CkStyle::TextDim();

                Root->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TName))
                                        .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                                [
                                    ck_sm_debugger_window::MakePill(ModeLabel, ModeColor)
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                                [
                                    ck_sm_debugger_window::MakeTaskResultPill(ResultAttr)
                                ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
                        [
                            ck_sm_debugger_window::MakeClassName(TaskClassStr)
                        ]
                ];
            }
        }

        // --- TRANSITIONS (outgoing) ---
        auto HasAnyOutgoing = false;
        for (auto& Transition : SmInfo->Transitions)
        {
            if (Transition.SourceStateIndex == SelectedIdx) { HasAnyOutgoing = true; break; }
        }
        if (HasAnyOutgoing)
        {
            Root->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f) [ ck_sm_debugger_window::MakeSectionHeader(TEXT("TRANSITIONS")) ];

            for (auto TrIdx = 0; TrIdx < SmInfo->Transitions.Num(); ++TrIdx)
            {
                auto& Transition = SmInfo->Transitions[TrIdx];
                if (Transition.SourceStateIndex != SelectedIdx) { continue; }

                auto DstName = SCkDebug_NameLabel::Get_ShortName(Transition.TargetStateName, Depth);

                auto TargetIdx = TrIdx;
                auto CountAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
                    [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel), TargetIdx]()
                    {
                        auto VM = ViewModelWeak.Pin();
                        if (NOT VM.IsValid()) { return FText::GetEmpty(); }
                        auto* Info = VM->Get_CurrentSmInfo();
                        if (NOT Info || TargetIdx >= Info->Transitions.Num()) { return FText::GetEmpty(); }
                        auto& T = Info->Transitions[TargetIdx];

                        // While scrubbing, look up the recorded SatisfiedCount/TotalCount
                        // from the per-frame snapshot history at the scrub time. Falls
                        // back to live counts only when no snapshot is available (e.g.
                        // completed-run snapshots not yet populated).
                        if (VM->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                        {
                            auto& SS = VM->Get_ScrubState();
                            auto RIdx = SS.SelectedRunIndex;
                            auto& R = (RIdx < 0)
                                ? Info->CurrentRun
                                : (RIdx < Info->CompletedRuns.Num() ? Info->CompletedRuns[RIdx] : Info->CurrentRun);
                            const auto AbsoluteTime = R.StartTime + SS.ScrubTime;

                            if (auto* Snap = CkSmDebugger::FindTransitionSnapshot(R, T.SourceStateClass, T.TargetStateClass, T.Order))
                            {
                                if (auto* Sample = CkSmDebugger::FindSampleAtTime(Snap->ResultHistory, AbsoluteTime))
                                {
                                    return FText::FromString(FString::Printf(TEXT("%d/%d"),
                                        Sample->SatisfiedCount, Sample->TotalCount));
                                }
                                // Pre-first-sample: nothing has been satisfied yet.
                                return FText::FromString(FString::Printf(TEXT("0/%d"), T.TotalCount));
                            }
                        }

                        return FText::FromString(FString::Printf(TEXT("%d/%d"), T.SatisfiedCount, T.TotalCount));
                    }));

                auto TransHasPolled = Transition.Conditions.ContainsByPredicate(
                    [](const FCkSmDebugger_ConditionInfo& C){ return C.Mode != ECk_SmConditionMode::EventDriven; });
                auto TransModeLabel = TransHasPolled ? FString{TEXT("POLLED")} : FString{TEXT("EVENT-DRIVEN")};
                auto TransModeColor = TransHasPolled
                    ? CkStyle::TextDim()
                    : CkStyle::Warn();

                Root->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("\x2500\x25B6")))
                                .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Arrow()))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(DstName))
                                .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(CountAttr)
                                .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Label()))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
                        [
                            ck_sm_debugger_window::MakePill(TransModeLabel, TransModeColor)
                        ]
                ];

                for (auto CondIdx = 0; CondIdx < Transition.Conditions.Num(); ++CondIdx)
                {
                    auto& Cond = Transition.Conditions[CondIdx];
                    auto CName = SCkDebug_NameLabel::Get_ShortName(Cond.ClassName, Depth);
                    auto CondClassStr = IsValid(Cond.ScriptClass) ? Cond.ScriptClass->GetName() : FString(TEXT("(unknown)"));

                    auto CondNameCapture = Cond.ClassName;
                    auto CondResultAttr = TAttribute<ECk_SmConditionResult>::Create(TAttribute<ECk_SmConditionResult>::FGetter::CreateLambda(
                        [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel), TargetIdx, CondIdx, CondNameCapture]()
                        {
                            auto VM = ViewModelWeak.Pin();
                            if (NOT VM.IsValid()) { return ECk_SmConditionResult::Undetermined; }
                            auto* Info = VM->Get_CurrentSmInfo();
                            if (NOT Info || TargetIdx >= Info->Transitions.Num()) { return ECk_SmConditionResult::Undetermined; }
                            auto& Tr = Info->Transitions[TargetIdx];
                            if (CondIdx >= Tr.Conditions.Num()) { return ECk_SmConditionResult::Undetermined; }

                            // While scrubbing, look up the recorded condition result
                            // from the per-frame snapshot history at the scrub time.
                            // Falls back to the live result only when no snapshot
                            // exists (e.g. completed-run snapshots not yet populated).
                            if (VM->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                            {
                                auto& SS = VM->Get_ScrubState();
                                auto RIdx = SS.SelectedRunIndex;
                                auto& R = (RIdx < 0)
                                    ? Info->CurrentRun
                                    : (RIdx < Info->CompletedRuns.Num() ? Info->CompletedRuns[RIdx] : Info->CurrentRun);
                                const auto AbsoluteTime = R.StartTime + SS.ScrubTime;

                                if (auto* Snap = CkSmDebugger::FindTransitionSnapshot(R, Tr.SourceStateClass, Tr.TargetStateClass, Tr.Order))
                                {
                                    if (CondIdx < Snap->ConditionResultHistory.Num())
                                    {
                                        const auto& CondHist = Snap->ConditionResultHistory[CondIdx];
                                        if (auto* Sample = CkSmDebugger::FindSampleAtTime(CondHist, AbsoluteTime))
                                        { return Sample->Result; }
                                    }
                                    // Pre-first-sample: condition hasn't reported yet.
                                    return ECk_SmConditionResult::Undetermined;
                                }
                            }

                            return Tr.Conditions[CondIdx].Result;
                        }));

                    auto CondIsEventDriven = (Cond.Mode == ECk_SmConditionMode::EventDriven);
                    auto CondModeLabel = CondIsEventDriven ? FString{TEXT("E")} : FString{TEXT("P")};
                    auto CondModeColor = CondIsEventDriven
                        ? CkStyle::Warn()
                        : CkStyle::TextDim();

                    Root->AddSlot().AutoHeight().Padding(20.0f, 2.0f, 0.0f, 0.0f)
                    [
                        SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                                    [
                                        ck_sm_debugger_window::MakeConditionPillLive(CondResultAttr)
                                    ]
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                                    [
                                        ck_sm_debugger_window::MakePill(CondModeLabel, CondModeColor)
                                    ]
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(CName))
                                            .ColorAndOpacity(FSlateColor(ck_sm_debugger_window::Color_Detail_Value()))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                    ]
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(44.0f, 1.0f, 0.0f, 0.0f)
                            [
                                ck_sm_debugger_window::MakeClassName(CondClassStr)
                            ]
                    ];
                }
            }
        }

        return Root;
    }

    return MakeNoSelection();
}

// --------------------------------------------------------------------------------------------------------------------
