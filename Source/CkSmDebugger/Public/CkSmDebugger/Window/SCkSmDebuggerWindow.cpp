#include "CkSmDebugger/Window/SCkSmDebuggerWindow.h"
#include "CkSmDebugger/ViewModel/CkSmDebugger_ViewModel.h"
#include "CkSmDebugger/Data/CkSmDebugger_DataCollector.h"
#include "CkSmDebugger/Window/SCkSmDebugger_HistoryList.h"
#include "CkSmDebugger/Window/SCkSmDebugger_Timeline.h"

#include "CkSmDebugger/Graph/CkSmDebugGraph.h"
#include "CkSmDebugger/Graph/CkSmDebugGraphSchema.h"
#include "CkSmDebugger/Graph/CkSmDebugNode_State.h"
#include "CkSmDebugger/Graph/CkSmDebugNode_Transition.h"

#include "CkCore/Macros/CkMacros.h"

#include "GraphEditor.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

SCkSmDebuggerWindow::~SCkSmDebuggerWindow()
{
    if (_Graph)
    {
        _Graph->RemoveFromRoot();
        _Graph = nullptr;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = MakeShared<FCkSmDebugger_ViewModel>();
    _DataCollector = MakeShared<FCkSmDebugger_DataCollector>();

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
                    return;
                }

                if (auto* TransNode = Cast<UCkSmDebugNode_Transition>(Obj))
                {
                    _SelectedTransitionIndex = TransNode->Get_TransitionIndex();
                    _ViewModel->Set_SelectedNodeIndex(-1);
                    return;
                }
            }

            _ViewModel->Set_SelectedNodeIndex(-1);
            _SelectedTransitionIndex = -1;
        });

    _GraphEditor = SNew(SGraphEditor)
        .GraphToEdit(_Graph)
        .IsEditable(true)
        .GraphEvents(GraphEvents);

    // Build the full layout
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

            // Main content area: vertical split — top: graph+detail, bottom: history
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SSplitter)
                        .Orientation(Orient_Vertical)

                        // Top area (75%): graph + detail side-by-side
                        + SSplitter::Slot()
                            .Value(0.75f)
                            [
                                SNew(SSplitter)
                                    .Orientation(Orient_Horizontal)

                                    // Graph view (left, 70%)
                                    + SSplitter::Slot()
                                        .Value(0.7f)
                                        [
                                            SNew(SVerticalBox)

                                            // Graph canvas
                                            + SVerticalBox::Slot()
                                                .FillHeight(1.0f)
                                                [
                                                    _GraphEditor.ToSharedRef()
                                                ]

                                            // Timeline bar
                                            + SVerticalBox::Slot()
                                                .AutoHeight()
                                                [
                                                    SAssignNew(_Timeline, SCkSmDebugger_Timeline, _ViewModel)
                                                        .DesiredHeight(40.0f)
                                                ]
                                        ]

                                    // Detail panel (right, 30%)
                                    + SSplitter::Slot()
                                        .Value(0.3f)
                                        [
                                            BuildDetailPanel()
                                        ]
                            ]

                        // Bottom area (25%): history list (full width)
                        + SSplitter::Slot()
                            .Value(0.25f)
                            [
                                SNew(SBox)
                                    .Padding(FMargin(2.0f))
                                    [
                                        SAssignNew(_HistoryList, SCkSmDebugger_HistoryList, _ViewModel)
                                    ]
                            ]
                ]
    ];
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

    // Find PIE world if we don't have one
    if (NOT IsValid(_CachedWorld))
    {
        for (auto It = GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
        {
            if (It->WorldType == EWorldType::PIE && IsValid(It->World()))
            {
                _CachedWorld = It->World();
                break;
            }
        }
    }

    if (NOT IsValid(_CachedWorld))
    { return; }

    // Collect data from ECS
    _DataCollector->Collect(_CachedWorld);

    // Tick ViewModel — broadcasts delegates to sub-widgets
    _ViewModel->Tick(_CachedWorld, InDeltaTime);

    // Refresh SM selector combo
    RefreshSmSelector();

    // Update graph from SmInfo
    auto SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (SmInfo && _Graph)
    {
        // Safety: ensure notifications are never stuck off (e.g. from context menu rebuild race)
        _Graph->SetSuppressNotifications(false);
        _Graph->UpdateFromSmInfo(*SmInfo);
    }
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

        // SM Selector label
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

        // SM Selector combo
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

        // Separator
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8.0f, 0.0f, 2.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("|")))
                    .ColorAndOpacity(FLinearColor(0.35f, 0.35f, 0.4f))
            ]

        // Expand all toggle
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCheckBox)
                    .IsChecked_Lambda([this]()
                    {
                        return (_ViewModel.IsValid() && _ViewModel->Get_ExpandAllNodes())
                            ? ECheckBoxState::Checked
                            : ECheckBoxState::Unchecked;
                    })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                    {
                        if (_ViewModel.IsValid())
                        {
                            _ViewModel->Set_ExpandAllNodes(InState == ECheckBoxState::Checked);
                            _ViewModel->RequestRelayout();
                        }
                    })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Expand Tasks")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    ]
            ]

        // Relayout button
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("Relayout")))
                    .OnClicked_Lambda([this]()
                    {
                        if (_Graph)
                        {
                            _Graph->ForceRebuild();
                        }
                        if (_ViewModel.IsValid())
                        { _ViewModel->RequestRelayout(); }
                        return FReply::Handled();
                    })
            ]

        // Separator — layout controls
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8.0f, 0.0f, 2.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("|")))
                    .ColorAndOpacity(FLinearColor(0.35f, 0.35f, 0.4f))
            ]

        // Compact layout toggle (undirected BFS)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
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
                            .Text(FText::FromString(TEXT("Compact")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    ]
            ]

        // Spacing X −/+
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("H:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
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

        // Spacing Y −/+
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("V:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
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

        // View mode indicator (LIVE / SCRUB / TEST)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8.0f, 0.0f, 2.0f, 0.0f)
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

        // Back to Live button (only visible in scrub mode)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("Back to Live")))
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
                            _ViewModel->Set_ViewMode(ECkSmDebugger_ViewMode::Live);
                            _ViewModel->ClearScrubTransitionHighlight();
                        }
                        return FReply::Handled();
                    })
            ]

        // Pause/Resume button — reacts to breakpoints
        + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text_Lambda([this]()
                    {
                        if (NOT _ViewModel.IsValid())
                        { return FText::FromString(TEXT("Pause")); }

                        auto SmInfo = _ViewModel->Get_CurrentSmInfo();
                        if (SmInfo && SmInfo->IsPieDebugPaused)
                        { return FText::FromString(TEXT("Resume")); }

                        return FText::FromString(TEXT("Pause"));
                    })
                    .OnClicked_Lambda([this]()
                    {
                        if (NOT _ViewModel.IsValid())
                        { return FReply::Handled(); }

                        auto SmInfo = _ViewModel->Get_CurrentSmInfo();
                        if (SmInfo && SmInfo->IsPieDebugPaused)
                        {
                            auto Cmd = FCkSmDebugger_Command{};
                            Cmd.Type = FCkSmDebugger_Command::EType::ResumeFromBreakpoint;
                            _ViewModel->IssueCommand(Cmd);
                        }
                        else
                        {
                            auto Cmd = FCkSmDebugger_Command{};
                            Cmd.Type = FCkSmDebugger_Command::EType::PauseExecution;
                            _ViewModel->IssueCommand(Cmd);
                        }

                        return FReply::Handled();
                    })
            ]

        // Test mockup toggle button
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

        // Spacer
        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNullWidget::NullWidget
            ]

        // Breakpoint status
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

    if (_SmSelectorItems.Num() != AllSms.Num())
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
// Detail panel
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebuggerWindow::
    BuildDetailPanel()
    -> TSharedRef<SWidget>
{
    return SNew(SScrollBox)
        + SScrollBox::Slot()
            .Padding(8.0f)
            [
                SNew(SVerticalBox)

                // Header — changes between "Node Details" and "Transition Details"
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([this]()
                            {
                                if (_SelectedTransitionIndex >= 0)
                                { return FText::FromString(TEXT("Transition Details")); }

                                return FText::FromString(TEXT("Node Details"));
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                            .ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
                    ]

                // Content — state info OR transition info depending on selection
                + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                            .Text_Lambda([this]()
                            {
                                if (NOT _ViewModel.IsValid())
                                { return FText::FromString(TEXT("No selection")); }

                                auto SmInfo = _ViewModel->Get_CurrentSmInfo();
                                if (NOT SmInfo)
                                { return FText::FromString(TEXT("No selection")); }

                                // ----- Transition selected -----
                                if (_SelectedTransitionIndex >= 0 && _SelectedTransitionIndex < SmInfo->Transitions.Num())
                                {
                                    auto& ClickedTrans = SmInfo->Transitions[_SelectedTransitionIndex];
                                    auto SrcIdx = ClickedTrans.SourceStateIndex;
                                    auto DstIdx = ClickedTrans.TargetStateIndex;

                                    auto Info = FString{};

                                    // Show ALL transitions between this source↔target pair
                                    for (auto i = 0; i < SmInfo->Transitions.Num(); ++i)
                                    {
                                        auto& Trans = SmInfo->Transitions[i];

                                        auto bSameEdge =
                                            (Trans.SourceStateIndex == SrcIdx && Trans.TargetStateIndex == DstIdx) ||
                                            (Trans.SourceStateIndex == DstIdx && Trans.TargetStateIndex == SrcIdx);

                                        if (NOT bSameEdge) { continue; }

                                        Info += FString::Printf(TEXT("%s -> %s  [%d/%d]"),
                                            *Trans.SourceStateName,
                                            *Trans.TargetStateName,
                                            Trans.SatisfiedCount,
                                            Trans.TotalCount);

                                        if (Trans.AreAllConditionsSatisfied)
                                        { Info += TEXT("  READY"); }

                                        Info += TEXT("\n");

                                        for (auto& Cond : Trans.Conditions)
                                        {
                                            Info += FString::Printf(TEXT("  %s %s\n"),
                                                Cond.IsSatisfied ? TEXT("[+]") : TEXT("[-]"),
                                                *Cond.ClassName);
                                        }

                                        Info += TEXT("\n");
                                    }

                                    if (Info.IsEmpty())
                                    { return FText::FromString(TEXT("No transition data")); }

                                    return FText::FromString(Info);
                                }

                                // ----- State selected -----
                                auto SelectedIdx = _ViewModel->Get_SelectedNodeIndex();

                                if (SelectedIdx < 0 || SelectedIdx >= SmInfo->States.Num())
                                { return FText::FromString(TEXT("No selection")); }

                                auto& State = SmInfo->States[SelectedIdx];
                                auto Info = FString::Printf(TEXT("State: %s\nCurrent: %s\nDwell: %.2fs\nVisited: %s\nTasks: %d"),
                                    *State.StateName,
                                    State.IsCurrentState ? TEXT("Yes") : TEXT("No"),
                                    State.DwellTimeSeconds,
                                    State.HasBeenVisited ? TEXT("Yes") : TEXT("No"),
                                    State.Tasks.Num());

                                return FText::FromString(Info);
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))
                            .AutoWrapText(true)
                    ]

                // Task details (only for state selection)
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 8.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([this]()
                            {
                                if (NOT _ViewModel.IsValid() || _SelectedTransitionIndex >= 0)
                                { return FText::GetEmpty(); }

                                auto SelectedIdx = _ViewModel->Get_SelectedNodeIndex();
                                auto SmInfo = _ViewModel->Get_CurrentSmInfo();

                                if (NOT SmInfo || SelectedIdx < 0 || SelectedIdx >= SmInfo->States.Num())
                                { return FText::GetEmpty(); }

                                auto& State = SmInfo->States[SelectedIdx];
                                if (State.Tasks.Num() == 0)
                                { return FText::GetEmpty(); }

                                auto TaskInfo = FString(TEXT("Tasks:\n"));
                                for (auto& Task : State.Tasks)
                                {
                                    auto ResultStr = TEXT("Unknown");
                                    switch (Task.LastResult)
                                    {
                                    case ECk_SmTaskResult::Running:   ResultStr = TEXT("Running"); break;
                                    case ECk_SmTaskResult::Succeeded: ResultStr = TEXT("Succeeded"); break;
                                    case ECk_SmTaskResult::Failed:    ResultStr = TEXT("Failed"); break;
                                    default: break;
                                    }

                                    TaskInfo += FString::Printf(TEXT("  %s [%s]\n"), *Task.ClassName, ResultStr);
                                }

                                return FText::FromString(TaskInfo);
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                            .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
                            .AutoWrapText(true)
                    ]

                // Outgoing transitions (only for state selection)
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 8.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([this]()
                            {
                                if (NOT _ViewModel.IsValid() || _SelectedTransitionIndex >= 0)
                                { return FText::GetEmpty(); }

                                auto SelectedIdx = _ViewModel->Get_SelectedNodeIndex();
                                auto SmInfo = _ViewModel->Get_CurrentSmInfo();

                                if (NOT SmInfo || SelectedIdx < 0)
                                { return FText::GetEmpty(); }

                                auto TransInfo = FString(TEXT("Transitions:\n"));
                                auto HasAny = false;

                                for (auto& Transition : SmInfo->Transitions)
                                {
                                    if (Transition.SourceStateIndex != SelectedIdx)
                                    { continue; }

                                    HasAny = true;
                                    TransInfo += FString::Printf(TEXT("  -> %s [%d/%d]"),
                                        *Transition.TargetStateName,
                                        Transition.SatisfiedCount,
                                        Transition.TotalCount);

                                    if (Transition.AreAllConditionsSatisfied)
                                    { TransInfo += TEXT(" READY"); }

                                    TransInfo += TEXT("\n");

                                    for (auto& Cond : Transition.Conditions)
                                    {
                                        TransInfo += FString::Printf(TEXT("    %s %s: %s\n"),
                                            Cond.IsSatisfied ? TEXT("[+]") : TEXT("[-]"),
                                            *Cond.ClassName,
                                            Cond.IsSatisfied ? TEXT("true") : TEXT("false"));
                                    }
                                }

                                if (NOT HasAny)
                                { return FText::GetEmpty(); }

                                return FText::FromString(TransInfo);
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                            .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
                            .AutoWrapText(true)
                    ]
            ];
}

// --------------------------------------------------------------------------------------------------------------------
