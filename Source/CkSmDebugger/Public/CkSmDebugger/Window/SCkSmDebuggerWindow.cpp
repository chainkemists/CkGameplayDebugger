#include "CkSmDebugger/Window/SCkSmDebuggerWindow.h"
#include "CkSmDebugger/ViewModel/CkSmDebugger_ViewModel.h"
#include "CkSmDebugger/Data/CkSmDebugger_DataCollector.h"
#include "CkSmDebugger/Window/SCkSmDebugger_HistoryList.h"
#include "CkSmDebugger/Window/SCkSmDebugger_Timeline.h"
#include "CkSmDebugger/Preview/SCkSmDebugger_PreviewPane.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"

#include "CkSmDebugger/Graph/CkSmDebugGraph.h"
#include "CkSmDebugger/Graph/CkSmDebugGraphSchema.h"
#include "CkSmDebugger/Graph/CkSmDebugNode_State.h"
#include "CkSmDebugger/Graph/CkSmDebugNode_Transition.h"

#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "GraphEditor.h"
#include "Editor.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"

// --------------------------------------------------------------------------------------------------------------------
// Detail panel — small widget helpers
// --------------------------------------------------------------------------------------------------------------------

namespace
{
    inline const FLinearColor Color_Detail_SectionHeader = FLinearColor(0.82f, 0.86f, 0.96f);
    inline const FLinearColor Color_Detail_Label         = FLinearColor(0.58f, 0.60f, 0.67f);
    inline const FLinearColor Color_Detail_Value         = FLinearColor(0.88f, 0.90f, 0.92f);
    inline const FLinearColor Color_Detail_ClassName     = FLinearColor(0.56f, 0.64f, 0.78f);
    inline const FLinearColor Color_Detail_Separator     = FLinearColor(0.18f, 0.20f, 0.27f, 1.0f);
    inline const FLinearColor Color_Detail_Bullet        = FLinearColor(0.85f, 0.55f, 0.25f);
    inline const FLinearColor Color_Detail_Arrow         = FLinearColor(0.56f, 0.64f, 0.78f);

    // -----------------------------------------------------------------------------------------------------------------
    // Section header: bold title + 1px underline.
    // -----------------------------------------------------------------------------------------------------------------
    auto MakeSectionHeader(const FString& InText) -> TSharedRef<SWidget>
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(InText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                    .ColorAndOpacity(FSlateColor(Color_Detail_SectionHeader))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(SBox).HeightOverride(1.0f)
                [
                    SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                        .BorderBackgroundColor(Color_Detail_Separator)
                ]
            ];
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Colored pill used for status chips (ACTIVE, RUNNING, SATISFIED, etc).
    // Background is a 22%-alpha fill of the accent color; text is the full accent.
    // -----------------------------------------------------------------------------------------------------------------
    auto MakePill(const FString& InText, const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(FLinearColor(InColor.R, InColor.G, InColor.B, 0.22f))
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
            .ColorAndOpacity(FSlateColor(Color_Detail_ClassName));
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
                        .ColorAndOpacity(FSlateColor(Color_Detail_Label))
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(InValue)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(Color_Detail_Value))
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
            switch (InResult.Get())
            {
            case ECk_SmTaskResult::Succeeded: return FSlateColor(FCkSmDebuggerStyle::Color_Sm_TaskSucceeded);
            case ECk_SmTaskResult::Failed:    return FSlateColor(FCkSmDebuggerStyle::Color_Sm_TaskFailed);
            default:                          return FSlateColor(FCkSmDebuggerStyle::Color_Sm_TaskRunning);
            }
        }));

        auto BgColorAttr = TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([InResult]()
        {
            auto Base = FCkSmDebuggerStyle::Color_Sm_TaskRunning;
            switch (InResult.Get())
            {
            case ECk_SmTaskResult::Succeeded: Base = FCkSmDebuggerStyle::Color_Sm_TaskSucceeded; break;
            case ECk_SmTaskResult::Failed:    Base = FCkSmDebuggerStyle::Color_Sm_TaskFailed;    break;
            default: break;
            }
            return FSlateColor(FLinearColor(Base.R, Base.G, Base.B, 0.22f));
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
    auto MakeConditionPill(ECk_SmConditionResult InResult) -> TSharedRef<SWidget>
    {
        auto Label   = FString{TEXT("?")};
        auto Color   = FCkSmDebuggerStyle::Color_Sm_ConditionUnknown;
        switch (InResult)
        {
        case ECk_SmConditionResult::Pass:         Label = TEXT("PASS"); Color = FCkSmDebuggerStyle::Color_Sm_ConditionSatisfied;   break;
        case ECk_SmConditionResult::Fail:         Label = TEXT("FAIL"); Color = FCkSmDebuggerStyle::Color_Sm_ConditionUnsatisfied; break;
        default: break;
        }
        return MakePill(Label, Color);
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

        auto ResolveColor = [InResult]() -> FLinearColor
        {
            switch (InResult.Get())
            {
            case ECk_SmConditionResult::Pass: return FCkSmDebuggerStyle::Color_Sm_ConditionSatisfied;
            case ECk_SmConditionResult::Fail: return FCkSmDebuggerStyle::Color_Sm_ConditionUnsatisfied;
            default:                          return FCkSmDebuggerStyle::Color_Sm_ConditionUnknown;
            }
        };

        auto FgAttr = TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([ResolveColor]()
        {
            return FSlateColor(ResolveColor());
        }));
        auto BgAttr = TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([ResolveColor]()
        {
            auto C = ResolveColor();
            return FSlateColor(FLinearColor(C.R, C.G, C.B, 0.22f));
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
    if (_OnEndPieHandle.IsValid())
    { FEditorDelegates::EndPIE.Remove(_OnEndPieHandle); }

    if (_OnBeginPieHandle.IsValid())
    { FEditorDelegates::BeginPIE.Remove(_OnBeginPieHandle); }

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

    // Create the preview pane eagerly so its picker can live in the main toolbar
    // (keeps the preview graph top aligned with the live graph top on the left).
    _PreviewPane = SNew(SCkSmDebugger_PreviewPane);

    // PIE lifecycle — tear down cached world/graph/history on stop so the next PIE session
    // re-links cleanly without requiring the user to close and reopen the debugger.
    _OnEndPieHandle = FEditorDelegates::EndPIE.AddLambda([this](bool /*bIsSimulating*/)
    {
        HandleWorldTornDown();
    });

    _OnBeginPieHandle = FEditorDelegates::BeginPIE.AddLambda([this](bool /*bIsSimulating*/)
    {
        // Ensure we start the new session from an empty slate — Tick will repopulate.
        HandleWorldTornDown();
    });

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
                                + SVerticalBox::Slot()
                                    .AutoHeight()
                                    [
                                        SAssignNew(_Timeline, SCkSmDebugger_Timeline, _ViewModel, _Graph)
                                            .DesiredHeight(56.0f)
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
    ];

    // Bind history selection → detail panel
    _HistoryList->OnEntrySelected.BindLambda([this](TSharedPtr<FCkSmDebugger_HistoryEntry> InEntry)
    {
        _SelectedHistoryEntry = InEntry;
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
    { return; }

    // Per-window refresh gate — short-circuits when hidden / paused / rate-capped.
    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    // Find PIE world — re-validate each tick since PIE can end at any time
    {
        auto FoundWorld = static_cast<UWorld*>(nullptr);

        for (auto It = GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
        {
            if (It->WorldType == EWorldType::PIE && ck::IsValid(It->World()) && It->World()->HasBegunPlay())
            {
                FoundWorld = It->World();
                break;
            }
        }

        _CachedWorld = FoundWorld;
    }

    // Keep the preview pane in sync with the current PIE world so its walker can
    // spawn a throwaway SM entity to discover the graph when a class is picked.
    if (_PreviewPane.IsValid())
    { _PreviewPane->Set_WorldContext(_CachedWorld); }

    auto* World = _CachedWorld.Get();
    if (ck::Is_NOT_Valid(World))
    { return; }

    // Collect data from ECS
    _DataCollector->Collect(World);

    // Tick ViewModel — broadcasts delegates to sub-widgets
    _ViewModel->Tick(World, InDeltaTime);

    // Auto-select the current active state when the user hasn't explicitly
    // picked something — saves them a click to populate the detail panel.
    if (_AutoSelectActiveState
        && NOT _SelectedHistoryEntry.IsValid()
        && _SelectedTransitionIndex < 0)
    {
        if (auto SmInfo = _ViewModel->Get_CurrentSmInfo())
        {
            auto ActiveIdx = -1;
            for (auto i = 0; i < SmInfo->States.Num(); ++i)
            {
                if (SmInfo->States[i].IsCurrentState)
                { ActiveIdx = i; break; }
            }

            if (ActiveIdx >= 0 && _ViewModel->Get_SelectedNodeIndex() != ActiveIdx)
            { _ViewModel->Set_SelectedNodeIndex(ActiveIdx); }
        }
    }

    // Refresh SM selector combo
    RefreshSmSelector();

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

        // Reset timeline scroll to center on scrub cursor / "now"
        if (_ViewModel.IsValid())
        {
            auto NewScrubState = _ViewModel->Get_ScrubState();
            NewScrubState.TimelineScrollX = 0.0f;
            _ViewModel->Set_ScrubState(NewScrubState);
        }

        return FReply::Handled();
    }

    // Arrow keys step the scrub needle by transition while in Scrub mode.
    if (_ViewModel.IsValid() && _ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
    {
        if (InKeyEvent.GetKey() == EKeys::Left)
        { StepScrubToTransition(-1); return FReply::Handled(); }
        if (InKeyEvent.GetKey() == EKeys::Right)
        { StepScrubToTransition(+1); return FReply::Handled(); }
        if (InKeyEvent.GetKey() == EKeys::Home)
        {
            auto NewScrubState = _ViewModel->Get_ScrubState();
            NewScrubState.ScrubTime = 0.0;
            NewScrubState.TimelineScrollX = 0.0f;
            _ViewModel->Set_ScrubState(NewScrubState);
            return FReply::Handled();
        }
        if (InKeyEvent.GetKey() == EKeys::End)
        {
            auto* SmInfo = _ViewModel->Get_CurrentSmInfo();
            if (SmInfo)
            {
                auto NewScrubState = _ViewModel->Get_ScrubState();
                NewScrubState.ScrubTime = SmInfo->CurrentRun.Duration;
                NewScrubState.TimelineScrollX = 0.0f;
                _ViewModel->Set_ScrubState(NewScrubState);
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
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        // ── SM Selector ──────────────────────────────────────────────────

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("SM:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
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
                    .ColorAndOpacity(FLinearColor(0.35f, 0.35f, 0.4f))
            ]

        // ── Display: Tasks toggle, Name depth ────────────────────────────

        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCheckBox)
                    .IsChecked_Lambda([this]()
                    {
                        return (_Graph && _Graph->LayoutParams.bExpandTasks)
                            ? ECheckBoxState::Checked
                            : ECheckBoxState::Unchecked;
                    })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                    {
                        if (_Graph)
                        {
                            _Graph->LayoutParams.bExpandTasks = (InState == ECheckBoxState::Checked);
                            _Graph->ForceRebuild();
                        }
                    })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Tasks")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    ]
            ]

        // Name depth:  [<]  value  [>]
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Name")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
            ]
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("\x25C0")))  // ◀
                    .OnClicked_Lambda([this]()
                    {
                        if (_Graph)
                        {
                            auto& Depth = _Graph->LayoutParams.NameDepth;
                            const auto MaxDepth = _Graph->Get_MaxNameDepth();
                            // Cycle: Full(0) → MaxDepth → … → 2 → 1 → Full(0)
                            Depth = (Depth == 0) ? MaxDepth : (Depth - 1);
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
                        if (NOT _Graph) { return FText::FromString(TEXT("1")); }
                        auto D = _Graph->LayoutParams.NameDepth;
                        return FText::FromString(D == 0 ? TEXT("Full") : FString::Printf(TEXT("%d"), D));
                    })
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))
                    .Justification(ETextJustify::Center)
                    .MinDesiredWidth(24.0f)
            ]
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("\x25B6")))  // ▶
                    .OnClicked_Lambda([this]()
                    {
                        if (_Graph)
                        {
                            auto& Depth = _Graph->LayoutParams.NameDepth;
                            const auto MaxDepth = _Graph->Get_MaxNameDepth();
                            // Cycle: Full(0) → 1 → 2 → … → MaxDepth → Full(0)
                            Depth = (Depth >= MaxDepth) ? 0 : (Depth + 1);
                            _Graph->ForceRebuild();
                        }
                        return FReply::Handled();
                    })
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
                                                    .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
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

                        // Compact toggle
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(4.0f, 4.0f, 4.0f, 2.0f)
                            [
                                SNew(SCheckBox)
                                    .IsChecked_Lambda([this]()
                                    {
                                        return (_Graph && _Graph->LayoutParams.bUndirectedBFS)
                                            ? ECheckBoxState::Checked
                                            : ECheckBoxState::Unchecked;
                                    })
                                    .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                                    {
                                        if (_Graph)
                                        {
                                            _Graph->LayoutParams.bUndirectedBFS = (InState == ECheckBoxState::Checked);
                                            _Graph->ForceRebuild();
                                        }
                                    })
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("Compact Layout")))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
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
                                                    .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
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
                                                    .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
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
                                                    .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
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
                    .ColorAndOpacity(FLinearColor(0.35f, 0.35f, 0.4f))
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
                        { return FSlateColor(FLinearColor(0.85f, 0.55f, 0.25f)); }

                        if (NOT _ViewModel.IsValid())
                        { return FSlateColor(FLinearColor(0.35f, 0.35f, 0.4f)); }

                        return (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Live)
                            ? FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f))
                            : FSlateColor(FLinearColor(0.9f, 0.7f, 0.1f));
                    })
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            ]

        // To Start (scrub-mode only)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("\x23EE To Start")))
                    .ToolTipText(NSLOCTEXT("CkSmDebugger", "ToStartTooltip",
                        "Jump the scrub needle to the beginning of the run (frame 0)."))
                    .Visibility_Lambda([this]()
                    {
                        if (NOT _ViewModel.IsValid()) { return EVisibility::Collapsed; }
                        return (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                            ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    .OnClicked_Lambda([this]()
                    {
                        if (_ViewModel.IsValid())
                        {
                            auto NewScrubState = _ViewModel->Get_ScrubState();
                            NewScrubState.ScrubTime = 0.0;
                            NewScrubState.TimelineScrollX = 0.0f;
                            _ViewModel->Set_ScrubState(NewScrubState);
                        }
                        return FReply::Handled();
                    })
            ]

        // Step backward 1 frame (scrub-mode only)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("\x25C0")))
                    .ToolTipText(NSLOCTEXT("CkSmDebugger", "StepBackTooltip",
                        "Step the scrub needle to the previous transition."))
                    .Visibility_Lambda([this]()
                    {
                        if (NOT _ViewModel.IsValid()) { return EVisibility::Collapsed; }
                        return (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                            ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    .OnClicked_Lambda([this]()
                    {
                        StepScrubToTransition(-1);
                        return FReply::Handled();
                    })
            ]

        // Step forward 1 transition (scrub-mode only)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("\x25B6")))
                    .ToolTipText(NSLOCTEXT("CkSmDebugger", "StepFwdTooltip",
                        "Step the scrub needle to the next transition."))
                    .Visibility_Lambda([this]()
                    {
                        if (NOT _ViewModel.IsValid()) { return EVisibility::Collapsed; }
                        return (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                            ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    .OnClicked_Lambda([this]()
                    {
                        StepScrubToTransition(+1);
                        return FReply::Handled();
                    })
            ]

        // Back to Live (scrub-mode only)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("Back to Live \x23ED")))
                    .Visibility_Lambda([this]()
                    {
                        if (NOT _ViewModel.IsValid())
                        { return EVisibility::Collapsed; }

                        return (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                    })
                    .OnClicked_Lambda([this]()
                    {
                        if (_ViewModel.IsValid())
                        {
                            // Reset the timeline scroll offset so Live mode anchors back to "now".
                            // Without this, the scroll offset accumulated during scrubbing would
                            // pull the live view far into the past.
                            auto NewScrubState = _ViewModel->Get_ScrubState();
                            NewScrubState.TimelineScrollX = 0.0f;
                            _ViewModel->Set_ScrubState(NewScrubState);

                            _ViewModel->Set_ViewMode(ECkSmDebugger_ViewMode::Live);
                            _ViewModel->ClearScrubTransitionHighlight();
                        }
                        return FReply::Handled();
                    })
            ]

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
                        return FText::FromString(TEXT("\x23F8 Pause"));
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

        // Show frames (timeline label format)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8.0f, 2.0f, 2.0f, 2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCheckBox)
                    .IsChecked_Lambda([this]() -> ECheckBoxState
                    {
                        return _ViewModel.IsValid() && _ViewModel->Get_ScrubState().ShowFramesOnTimeline
                            ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                    })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState InNewState)
                    {
                        if (NOT _ViewModel.IsValid()) { return; }
                        auto NewScrubState = _ViewModel->Get_ScrubState();
                        NewScrubState.ShowFramesOnTimeline = (InNewState == ECheckBoxState::Checked);
                        _ViewModel->Set_ScrubState(NewScrubState);
                    })
                    .ToolTipText(NSLOCTEXT("CkSmDebugger", "ShowFramesTooltip",
                        "Show timeline labels as frame numbers (checked) or seconds (unchecked)."))
                    [
                        SNew(STextBlock)
                            .Text(NSLOCTEXT("CkSmDebugger", "ShowFrames", "Show frames"))
                    ]
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
                    .ColorAndOpacity(FLinearColor(0.937f, 0.325f, 0.314f))
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
        .BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.12f))
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
                            .ColorAndOpacity(FSlateColor(Color_Detail_Label))
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
        Sig.HistoryEntry = static_cast<const void*>(_SelectedHistoryEntry.Get());
        Sig.NodeIdx = _ViewModel->Get_SelectedNodeIndex();
        Sig.TransitionIdx = _SelectedTransitionIndex;
        Sig.ScrubMode = (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub) ? 1 : 0;
        auto& ScrubSnap = _ViewModel->Get_ScrubSnapshot();
        Sig.ScrubHistoryIdx = ScrubSnap.HistoryIndex;
        Sig.ScrubActiveStateIdx = ScrubSnap.ActiveStateIndex;

        Sig.ScrubAlignsWithSelectedEntry = 1;
        if (Sig.ScrubMode && SmInfo && _SelectedHistoryEntry.IsValid())
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
                .ColorAndOpacity(FSlateColor(Color_Detail_Label))
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
    // Case 1: history entry selected
    //
    // Skipped when scrubbing AND the needle has moved away from the entry's
    // time — that means the user clicked a row earlier but is now actively
    // dragging the needle; the live scrub snapshot (Case 4) wins. Without
    // this guard, the panel would stay stuck on the old entry forever.
    // ───────────────────────────────────────────────────────────────────
    auto IsScrubbingForCase1 = _ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub;
    auto Case1NeedleAlignsWithEntry = true;
    if (IsScrubbingForCase1 && _SelectedHistoryEntry.IsValid())
    {
        auto& SS = _ViewModel->Get_ScrubState();
        auto RIdx = SS.SelectedRunIndex;
        auto& R = (RIdx < 0)
            ? SmInfo->CurrentRun
            : (RIdx < SmInfo->CompletedRuns.Num() ? SmInfo->CompletedRuns[RIdx] : SmInfo->CurrentRun);
        auto EntryRunRel = _SelectedHistoryEntry->RealTimeSeconds - R.StartTime;
        constexpr auto Epsilon = 0.05;  // 50 ms — tolerance for "still on the clicked row"
        Case1NeedleAlignsWithEntry = FMath::Abs(EntryRunRel - SS.ScrubTime) <= Epsilon;
    }

    if (_SelectedHistoryEntry.IsValid() && Case1NeedleAlignsWithEntry)
    {
        auto& Entry = *_SelectedHistoryEntry;
        auto ToName   = FCkSmLayoutParams::ComputeDisplayName(Entry.ToStateName,   Depth);
        auto FromName = FCkSmLayoutParams::ComputeDisplayName(Entry.FromStateName, Depth);

        // Find state indices
        auto ToIdx = -1;
        auto FromIdx = -1;
        for (auto i = 0; i < SmInfo->States.Num(); ++i)
        {
            if (SmInfo->States[i].StateName == Entry.ToStateName)   { ToIdx = i; }
            if (SmInfo->States[i].StateName == Entry.FromStateName) { FromIdx = i; }
        }

        // --- ARRIVED AT ---
        Root->AddSlot().AutoHeight() [ MakeSectionHeader(TEXT("ARRIVED AT")) ];
        Root->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("\x25CF")))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Bullet))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(ToName))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
        ];

        if (ToIdx >= 0)
        {
            auto& ToState = SmInfo->States[ToIdx];
            for (auto& Task : ToState.Tasks)
            {
                auto TName = FCkSmLayoutParams::ComputeDisplayName(Task.ClassName, Depth);
                auto IconChar = FString{TEXT("\x25CF")};
                auto IconColor = FCkSmDebuggerStyle::Color_Sm_TaskRunning;
                switch (Task.LastResult)
                {
                case ECk_SmTaskResult::Succeeded: IconChar = TEXT("\x2713"); IconColor = FCkSmDebuggerStyle::Color_Sm_TaskSucceeded; break;
                case ECk_SmTaskResult::Failed:    IconChar = TEXT("\x2717"); IconColor = FCkSmDebuggerStyle::Color_Sm_TaskFailed;    break;
                default: break;
                }
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
                                .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        ]
                ];
            }
        }

        // --- CAME FROM ---
        Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ MakeSectionHeader(TEXT("CAME FROM")) ];
        Root->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("\x25CB")))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Label))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FromName))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
        ];

        if (FromIdx >= 0)
        {
            auto& FromState = SmInfo->States[FromIdx];
            for (auto& Task : FromState.Tasks)
            {
                auto TName = FCkSmLayoutParams::ComputeDisplayName(Task.ClassName, Depth);
                Root->AddSlot().AutoHeight().Padding(16.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TName))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Label))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                ];
            }
        }

        // --- TASKS AT TRANSITION ---
        if (Entry.TaskSnapshots.Num() > 0)
        {
            Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ MakeSectionHeader(TEXT("TASKS AT TRANSITION")) ];
            for (auto& Snap : Entry.TaskSnapshots)
            {
                auto TName = FCkSmLayoutParams::ComputeDisplayName(Snap.TaskName, Depth);
                auto Label = FString{TEXT("RUNNING")};
                auto Color = FCkSmDebuggerStyle::Color_Sm_TaskRunning;
                switch (Snap.Result)
                {
                case ECk_SmTaskResult::Succeeded: Label = TEXT("SUCCEEDED"); Color = FCkSmDebuggerStyle::Color_Sm_TaskSucceeded; break;
                case ECk_SmTaskResult::Failed:    Label = TEXT("FAILED");    Color = FCkSmDebuggerStyle::Color_Sm_TaskFailed;    break;
                default: break;
                }
                Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TName))
                                .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                        [
                            MakePill(Label, Color)
                        ]
                ];
            }
        }

        // --- TRANSITION conditions ---
        Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ MakeSectionHeader(TEXT("TRANSITION")) ];
        if (Entry.ConditionNames.Num() > 0)
        {
            for (auto& CondName : Entry.ConditionNames)
            {
                auto CName = FCkSmLayoutParams::ComputeDisplayName(CondName, Depth);
                Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                        [
                            MakeConditionPill(ECk_SmConditionResult::Pass)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(CName))
                                .ColorAndOpacity(FSlateColor(Color_Detail_Value))
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
                    .ColorAndOpacity(FSlateColor(Color_Detail_Label))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
            ];
        }

        Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("Frame [%llu]"), Entry.FrameNumber)))
                .ColorAndOpacity(FSlateColor(Color_Detail_Label))
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

        Root->AddSlot().AutoHeight() [ MakeSectionHeader(TEXT("TRANSITION")) ];

        auto SrcName = FCkSmLayoutParams::ComputeDisplayName(SmInfo->Transitions[TransIdx].SourceStateName, Depth);
        auto DstName = FCkSmLayoutParams::ComputeDisplayName(SmInfo->Transitions[TransIdx].TargetStateName, Depth);

        Root->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(SrcName))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("\x2500\x25B6")))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Arrow))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(DstName))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Value))
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
            ? FCkSmDebuggerStyle::Color_Sm_Polled
            : FCkSmDebuggerStyle::Color_Sm_EventDriven;

        Root->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(CountAttr)
                        .ColorAndOpacity(FSlateColor(Color_Detail_Label))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
                [
                    MakePill(TransSelModeLabel, TransSelModeColor)
                ]
        ];

        Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ MakeSectionHeader(TEXT("CONDITIONS")) ];

        auto& Trans = SmInfo->Transitions[TransIdx];
        for (auto i = 0; i < Trans.Conditions.Num(); ++i)
        {
            auto& Cond = Trans.Conditions[i];
            auto CName = FCkSmLayoutParams::ComputeDisplayName(Cond.ClassName, Depth);
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
                ? FCkSmDebuggerStyle::Color_Sm_EventDriven
                : FCkSmDebuggerStyle::Color_Sm_Polled;

            Root->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                            [
                                MakeConditionPillLive(ResultAttr)
                            ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                            [
                                MakePill(CondModeLabel, CondModeColor)
                            ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(CName))
                                    .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(44.0f, 1.0f, 0.0f, 0.0f)
                    [
                        MakeClassName(ClassStr)
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

            Root->AddSlot().AutoHeight() [ MakeSectionHeader(TEXT("SCRUB AT")) ];
            Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(ScrubFrameAttr)
                    .ColorAndOpacity(FSlateColor(Color_Detail_Label))
                    .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
            ];

            // CAME FROM (the transition that entered this segment)
            auto CameFromHistIdx = ActiveSegIdx > 0 ? ActiveSegIdx - 1 : -1;
            if (CameFromHistIdx >= 0 && CameFromHistIdx < SRun.History.Num())
            {
                auto& PrevEntry = SRun.History[CameFromHistIdx];
                auto FromName = FCkSmLayoutParams::ComputeDisplayName(PrevEntry.FromStateName, Depth);

                Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ MakeSectionHeader(TEXT("CAME FROM")) ];
                Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("%s  (frame %llu)"), *FromName, PrevEntry.FrameNumber)))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                ];
            }

            // NEXT TRANSITION (upcoming history entry, if any)
            auto NextHistIdx = (ActiveSegIdx >= 0 && ActiveSegIdx < SRun.History.Num()) ? ActiveSegIdx : -1;
            if (NextHistIdx >= 0 && NextHistIdx < SRun.History.Num())
            {
                auto& NextEntry = SRun.History[NextHistIdx];
                auto NextName = FCkSmLayoutParams::ComputeDisplayName(NextEntry.ToStateName, Depth);
                auto FramesAhead = static_cast<int64>(NextEntry.FrameNumber) - static_cast<int64>(ActiveSegStartFrame);

                Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f) [ MakeSectionHeader(TEXT("NEXT TRANSITION")) ];
                Root->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(
                            TEXT("\x2500\x25B6 %s  (in %lld frames, frame %llu)"),
                            *NextName, FramesAhead, NextEntry.FrameNumber)))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                ];
            }
            else
            {
                Root->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("(currently the latest state in this run)")))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Label))
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
        auto DisplayName = FCkSmLayoutParams::ComputeDisplayName(State.StateName, Depth);
        auto ClassStr = IsValid(State.ScriptClass) ? State.ScriptClass->GetName() : FString(TEXT("(unknown)"));
        auto HasOverride = IsValid(State.RequestedScriptClass) && State.RequestedScriptClass != State.ScriptClass;
        auto OverrideStr = HasOverride ? State.RequestedScriptClass->GetName() : FString{};

        Root->AddSlot().AutoHeight() [ MakeSectionHeader(TEXT("STATE")) ];

        // Name row — bullet + name + ACTIVE pill
        {
            auto NameRow = SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("\x25CF")))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Bullet))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(DisplayName))
                        .ColorAndOpacity(FSlateColor(Color_Detail_Value))
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
                    return Info->States[StateIdx].IsCurrentState ? EVisibility::Visible : EVisibility::Collapsed;
                }));

            NameRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                    .Visibility(VisibilityAttr)
                    [
                        MakePill(TEXT("ACTIVE"), FCkSmDebuggerStyle::Color_Sm_ActiveStateBorder)
                    ]
            ];

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
                    MakePill(TEXT("EVENT-DRIVEN"), FCkSmDebuggerStyle::Color_Sm_EventDriven)
                ];
            }
            else if (HasCompleteData)
            {
                if (HasAnyTickTask)
                {
                    NameRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                    [
                        MakePill(TEXT("TICKS"), FCkSmDebuggerStyle::Color_Sm_TaskTick)
                    ];
                }
                if (HasAnyPolledCondition)
                {
                    NameRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                    [
                        MakePill(TEXT("POLLED"), FCkSmDebuggerStyle::Color_Sm_Polled)
                    ];
                }
            }

            Root->AddSlot().AutoHeight() [ NameRow ];
        }

        // Class name + override info
        Root->AddSlot().AutoHeight().Padding(20.0f, 2.0f, 0.0f, 0.0f) [ MakeClassName(ClassStr) ];

        if (HasOverride)
        {
            Root->AddSlot().AutoHeight().Padding(20.0f, 2.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                    [
                        MakePill(TEXT("OVERRIDE"), FCkSmDebuggerStyle::Color_Sm_Override)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        MakeClassName(OverrideStr)
                    ]
            ];
        }

        Root->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            MakeKeyValue(TEXT("Dwell"), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
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
            MakeKeyValue(TEXT("Visited"), TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
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
            Root->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f) [ MakeSectionHeader(TEXT("TASKS")) ];
            for (auto TaskIdx = 0; TaskIdx < State.Tasks.Num(); ++TaskIdx)
            {
                auto& Task = State.Tasks[TaskIdx];
                auto TName = FCkSmLayoutParams::ComputeDisplayName(Task.ClassName, Depth);
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
                auto ModeColor = IsTickMode ? FCkSmDebuggerStyle::Color_Sm_TaskTick : FCkSmDebuggerStyle::Color_Sm_Polled;

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
                                        .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                                [
                                    MakePill(ModeLabel, ModeColor)
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
                                [
                                    MakeTaskResultPill(ResultAttr)
                                ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
                        [
                            MakeClassName(TaskClassStr)
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
            Root->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f) [ MakeSectionHeader(TEXT("TRANSITIONS")) ];

            for (auto TrIdx = 0; TrIdx < SmInfo->Transitions.Num(); ++TrIdx)
            {
                auto& Transition = SmInfo->Transitions[TrIdx];
                if (Transition.SourceStateIndex != SelectedIdx) { continue; }

                auto DstName = FCkSmLayoutParams::ComputeDisplayName(Transition.TargetStateName, Depth);

                auto TargetIdx = TrIdx;
                auto CountAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
                    [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel), TargetIdx]()
                    {
                        auto VM = ViewModelWeak.Pin();
                        if (NOT VM.IsValid()) { return FText::GetEmpty(); }
                        auto* Info = VM->Get_CurrentSmInfo();
                        if (NOT Info || TargetIdx >= Info->Transitions.Num()) { return FText::GetEmpty(); }
                        auto& T = Info->Transitions[TargetIdx];
                        return FText::FromString(FString::Printf(TEXT("%d/%d"), T.SatisfiedCount, T.TotalCount));
                    }));

                auto TransHasPolled = Transition.Conditions.ContainsByPredicate(
                    [](const FCkSmDebugger_ConditionInfo& C){ return C.Mode != ECk_SmConditionMode::EventDriven; });
                auto TransModeLabel = TransHasPolled ? FString{TEXT("POLLED")} : FString{TEXT("EVENT-DRIVEN")};
                auto TransModeColor = TransHasPolled
                    ? FCkSmDebuggerStyle::Color_Sm_Polled
                    : FCkSmDebuggerStyle::Color_Sm_EventDriven;

                Root->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("\x2500\x25B6")))
                                .ColorAndOpacity(FSlateColor(Color_Detail_Arrow))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(DstName))
                                .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(CountAttr)
                                .ColorAndOpacity(FSlateColor(Color_Detail_Label))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
                        [
                            MakePill(TransModeLabel, TransModeColor)
                        ]
                ];

                for (auto CondIdx = 0; CondIdx < Transition.Conditions.Num(); ++CondIdx)
                {
                    auto& Cond = Transition.Conditions[CondIdx];
                    auto CName = FCkSmLayoutParams::ComputeDisplayName(Cond.ClassName, Depth);
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

                            // While scrubbing on a completed segment, return Pass for the
                            // conditions named in the next transition's ConditionNames (those
                            // are the ones that actually fired the exit) and Fail for the
                            // rest. Live polling values are only meaningful for the current
                            // live state.
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
                                    if (Tr.SourceStateName == NextEntry.FromStateName
                                        && Tr.TargetStateName == NextEntry.ToStateName)
                                    {
                                        return NextEntry.ConditionNames.Contains(CondNameCapture)
                                            ? ECk_SmConditionResult::Pass
                                            : ECk_SmConditionResult::Fail;
                                    }
                                    // Different transition (didn't fire) — return Undetermined
                                    return ECk_SmConditionResult::Undetermined;
                                }
                            }

                            return Tr.Conditions[CondIdx].Result;
                        }));

                    auto CondIsEventDriven = (Cond.Mode == ECk_SmConditionMode::EventDriven);
                    auto CondModeLabel = CondIsEventDriven ? FString{TEXT("E")} : FString{TEXT("P")};
                    auto CondModeColor = CondIsEventDriven
                        ? FCkSmDebuggerStyle::Color_Sm_EventDriven
                        : FCkSmDebuggerStyle::Color_Sm_Polled;

                    Root->AddSlot().AutoHeight().Padding(20.0f, 2.0f, 0.0f, 0.0f)
                    [
                        SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                                    [
                                        MakeConditionPillLive(CondResultAttr)
                                    ]
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                                    [
                                        MakePill(CondModeLabel, CondModeColor)
                                    ]
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(CName))
                                            .ColorAndOpacity(FSlateColor(Color_Detail_Value))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                    ]
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(44.0f, 1.0f, 0.0f, 0.0f)
                            [
                                MakeClassName(CondClassStr)
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
