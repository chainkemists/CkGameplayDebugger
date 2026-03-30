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

// --------------------------------------------------------------------------------------------------------------------

SCkSmDebuggerWindow::~SCkSmDebuggerWindow()
{
    if (_Graph && UObjectInitialized())
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

            // Main content area
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
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
                                            .DesiredHeight(40.0f)
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

    // Find PIE world — re-validate each tick since PIE can end at any time
    {
        auto FoundWorld = static_cast<UWorld*>(nullptr);

        for (auto It = GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
        {
            if (It->WorldType == EWorldType::PIE && IsValid(It->World()))
            {
                FoundWorld = It->World();
                break;
            }
        }

        _CachedWorld = FoundWorld;
    }

    if (NOT _CachedWorld)
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
            _Graph->TickLiveFlash(InDeltaTime, _LastCurrentStateIdx, SmInfo->CurrentStateIndex);
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
                            if (Depth > 1) { --Depth; }
                            else { Depth = 0; }
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
                            Depth = (Depth == 0) ? 0 : Depth + 1;
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

        // Back to Live (conditional)
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
    auto DimColor = FLinearColor(0.5f, 0.5f, 0.55f);
    auto SepColor = FLinearColor(0.25f, 0.25f, 0.33f, 0.5f);

    return SNew(SBorder)
        .BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.12f))
        .Padding(FMargin(6.0f))
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            .Padding(4.0f)
            [
                SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        if (NOT _ViewModel.IsValid())
                        { return FText::FromString(TEXT("No selection")); }

                        auto SmInfo = _ViewModel->Get_CurrentSmInfo();
                        if (NOT SmInfo)
                        { return FText::FromString(TEXT("No selection")); }

                        auto Depth = _Graph ? _Graph->LayoutParams.NameDepth : 1;
                        auto Info = FString{};

                        // ── History entry selected ──────────────────────
                        if (_SelectedHistoryEntry.IsValid())
                        {
                            auto& Entry = *_SelectedHistoryEntry;
                            auto ToName = FCkSmLayoutParams::ComputeDisplayName(Entry.ToStateName, Depth);
                            auto FromName = FCkSmLayoutParams::ComputeDisplayName(Entry.FromStateName, Depth);

                            // Find state indices
                            auto ToIdx = -1;
                            auto FromIdx = -1;
                            for (auto i = 0; i < SmInfo->States.Num(); ++i)
                            {
                                if (SmInfo->States[i].StateName == Entry.ToStateName) { ToIdx = i; }
                                if (SmInfo->States[i].StateName == Entry.FromStateName) { FromIdx = i; }
                            }

                            // --- Arrived At ---
                            Info += FString::Printf(TEXT("ARRIVED AT\n"));
                            Info += FString::Printf(TEXT("\x25CF %s\n"), *ToName);
                            if (ToIdx >= 0 && ToIdx < SmInfo->States.Num())
                            {
                                auto& ToState = SmInfo->States[ToIdx];
                                for (auto& Task : ToState.Tasks)
                                {
                                    auto TName = FCkSmLayoutParams::ComputeDisplayName(Task.ClassName, Depth);
                                    auto Icon = TEXT("\x25CF");
                                    switch (Task.LastResult)
                                    {
                                    case ECk_SmTaskResult::Succeeded: Icon = TEXT("\x2713"); break;
                                    case ECk_SmTaskResult::Failed:    Icon = TEXT("\x2717"); break;
                                    default: break;
                                    }
                                    Info += FString::Printf(TEXT("    %s %s\n"), *TName, Icon);
                                }
                            }

                            Info += TEXT("\n");

                            // --- Came From ---
                            Info += FString::Printf(TEXT("CAME FROM\n"));
                            Info += FString::Printf(TEXT("\x25CB %s\n"), *FromName);
                            if (FromIdx >= 0 && FromIdx < SmInfo->States.Num())
                            {
                                auto& FromState = SmInfo->States[FromIdx];
                                for (auto& Task : FromState.Tasks)
                                {
                                    auto TName = FCkSmLayoutParams::ComputeDisplayName(Task.ClassName, Depth);
                                    Info += FString::Printf(TEXT("    %s\n"), *TName);
                                }
                            }

                            // --- Task snapshots at transition time ---
                            if (Entry.TaskSnapshots.Num() > 0)
                            {
                                Info += TEXT("\nTASKS AT TRANSITION\n");
                                for (auto& Snap : Entry.TaskSnapshots)
                                {
                                    auto TName = FCkSmLayoutParams::ComputeDisplayName(Snap.TaskName, Depth);
                                    auto ResultStr = TEXT("Running");
                                    switch (Snap.Result)
                                    {
                                    case ECk_SmTaskResult::Succeeded: ResultStr = TEXT("Succeeded"); break;
                                    case ECk_SmTaskResult::Failed:    ResultStr = TEXT("Failed"); break;
                                    default: break;
                                    }
                                    Info += FString::Printf(TEXT("    %s  %s\n"), *TName, ResultStr);
                                }
                            }

                            Info += TEXT("\n");

                            // --- Transition conditions ---
                            Info += TEXT("TRANSITION\n");
                            if (Entry.ConditionNames.Num() > 0)
                            {
                                for (auto& CondName : Entry.ConditionNames)
                                {
                                    auto CName = FCkSmLayoutParams::ComputeDisplayName(CondName, Depth);
                                    Info += FString::Printf(TEXT("    [+] %s\n"), *CName);
                                }
                            }
                            else
                            {
                                Info += TEXT("    (unconditional)\n");
                            }

                            Info += FString::Printf(TEXT("\nFrame [%llu]\n"), Entry.FrameNumber);

                            return FText::FromString(Info);
                        }

                        // ── Transition selected in graph ───────────────
                        if (_SelectedTransitionIndex >= 0 && _SelectedTransitionIndex < SmInfo->Transitions.Num())
                        {
                            auto& Trans = SmInfo->Transitions[_SelectedTransitionIndex];
                            auto SrcName = FCkSmLayoutParams::ComputeDisplayName(Trans.SourceStateName, Depth);
                            auto DstName = FCkSmLayoutParams::ComputeDisplayName(Trans.TargetStateName, Depth);

                            Info += FString::Printf(TEXT("TRANSITION\n%s \x2500\x25B6 %s\n\n"), *SrcName, *DstName);
                            Info += FString::Printf(TEXT("Conditions [%d/%d]%s\n"),
                                Trans.SatisfiedCount, Trans.TotalCount,
                                Trans.AreAllConditionsSatisfied ? TEXT("  READY") : TEXT(""));

                            for (auto& Cond : Trans.Conditions)
                            {
                                auto CName = FCkSmLayoutParams::ComputeDisplayName(Cond.ClassName, Depth);
                                Info += FString::Printf(TEXT("  %s %s\n"),
                                    Cond.IsSatisfied ? TEXT("[+]") : TEXT("[-]"), *CName);
                            }

                            return FText::FromString(Info);
                        }

                        // ── State selected in graph ────────────────────
                        auto SelectedIdx = _ViewModel->Get_SelectedNodeIndex();
                        if (SelectedIdx >= 0 && SelectedIdx < SmInfo->States.Num())
                        {
                            auto& State = SmInfo->States[SelectedIdx];
                            auto DisplayName = FCkSmLayoutParams::ComputeDisplayName(State.StateName, Depth);

                            Info += FString::Printf(TEXT("STATE\n\x25CF %s\n\n"), *DisplayName);
                            Info += FString::Printf(TEXT("Current: %s\n"), State.IsCurrentState ? TEXT("Yes") : TEXT("No"));
                            Info += FString::Printf(TEXT("Dwell: %.2fs\n"), State.DwellTimeSeconds);
                            Info += FString::Printf(TEXT("Visited: %s\n\n"), State.HasBeenVisited ? TEXT("Yes") : TEXT("No"));

                            if (State.Tasks.Num() > 0)
                            {
                                Info += TEXT("TASKS\n");
                                for (auto& Task : State.Tasks)
                                {
                                    auto TName = FCkSmLayoutParams::ComputeDisplayName(Task.ClassName, Depth);
                                    auto ResultStr = TEXT("Running");
                                    switch (Task.LastResult)
                                    {
                                    case ECk_SmTaskResult::Succeeded: ResultStr = TEXT("Succeeded"); break;
                                    case ECk_SmTaskResult::Failed:    ResultStr = TEXT("Failed"); break;
                                    default: break;
                                    }
                                    Info += FString::Printf(TEXT("    %s  %s\n"), *TName, ResultStr);
                                }
                                Info += TEXT("\n");
                            }

                            // Outgoing transitions
                            auto HasTrans = false;
                            for (auto& Transition : SmInfo->Transitions)
                            {
                                if (Transition.SourceStateIndex != SelectedIdx) { continue; }
                                if (NOT HasTrans) { Info += TEXT("TRANSITIONS\n"); HasTrans = true; }

                                auto DstName = FCkSmLayoutParams::ComputeDisplayName(Transition.TargetStateName, Depth);
                                Info += FString::Printf(TEXT("  \x2500\x25B6 %s  [%d/%d]%s\n"),
                                    *DstName,
                                    Transition.SatisfiedCount,
                                    Transition.TotalCount,
                                    Transition.AreAllConditionsSatisfied ? TEXT(" READY") : TEXT(""));

                                for (auto& Cond : Transition.Conditions)
                                {
                                    auto CName = FCkSmLayoutParams::ComputeDisplayName(Cond.ClassName, Depth);
                                    Info += FString::Printf(TEXT("      %s %s\n"),
                                        Cond.IsSatisfied ? TEXT("[+]") : TEXT("[-]"), *CName);
                                }
                            }

                            return FText::FromString(Info);
                        }

                        return FText::FromString(TEXT("No selection"));
                    })
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.78f))
                    .AutoWrapText(true)
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------
