#include "CkAStarDebugger/Window/SCkAStarDebuggerWindow.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkAStarDebugger/GridView/SCkAStarDebugger_GridView.h"
#include "CkAStarDebugger/Window/SCkAStarDebugger_StatsPanel.h"
#include "CkAStarDebugger/Window/SCkAStarDebugger_SearchHistory.h"
#include "CkAStarDebugger/CkAStarDebuggerStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

// ====================================================================================================================
// Construction
// ====================================================================================================================

auto
    SCkAStarDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = MakeShared<FCkAStarDebugger_ViewModel>();

    // Layout:
    //   Toolbar (auto-height)
    //   ├─ GridView (left, ~70%)
    //   └─ Right panel (~30%)
    //      ├─ StatsPanel (top, ~60%)
    //      └─ SearchHistory (bottom, ~40%)

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

            // Main content
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SSplitter)
                        .Orientation(Orient_Horizontal)

                        // Grid view (left, ~70%)
                        + SSplitter::Slot()
                            .Value(0.7f)
                            [
                                SAssignNew(_GridView, SCkAStarDebugger_GridView, _ViewModel)
                            ]

                        // Right panel (~30%)
                        + SSplitter::Slot()
                            .Value(0.3f)
                            [
                                SNew(SSplitter)
                                    .Orientation(Orient_Vertical)

                                    // Stats panel (top, ~60%)
                                    + SSplitter::Slot()
                                        .Value(0.6f)
                                        [
                                            SAssignNew(_StatsPanel, SCkAStarDebugger_StatsPanel, _ViewModel)
                                        ]

                                    // Search history (bottom, ~40%)
                                    + SSplitter::Slot()
                                        .Value(0.4f)
                                        [
                                            SAssignNew(_SearchHistory, SCkAStarDebugger_SearchHistory, _ViewModel)
                                        ]
                            ]
                ]
    ];
}

// ====================================================================================================================
// Tick — find PIE world, collect data, feed grid view
// ====================================================================================================================

auto
    SCkAStarDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

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

    _ViewModel->Tick(_CachedWorld, InDeltaTime);
    RefreshEntitySelector();

    auto* Info = _ViewModel->Get_CurrentSearchInfo();

    if (Info)
    {
        _GridView->SetSearchInfo(*Info);

        if (_StatusBadgeText.IsValid())
        {
            auto StatusStr = CkAStarDebugger::GetStatusString(Info->SearchStatus);
            auto StatusColor = CkAStarDebugger::GetStatusColor(Info->SearchStatus);
            _StatusBadgeText->SetText(FText::FromString(StatusStr));
            _StatusBadgeText->SetColorAndOpacity(StatusColor);
        }
    }
}

// ====================================================================================================================
// Toolbar
// ====================================================================================================================

auto
    SCkAStarDebuggerWindow::
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        // "Entity:" label
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Entity:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Secondary)
            ]

        // Entity selector combo box
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f)
            [
                SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&_EntitySelectorItems)
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) -> TSharedRef<SWidget>
                    {
                        return SNew(STextBlock)
                            .Text(FText::FromString(*InItem))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9));
                    })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<FString> InItem, ESelectInfo::Type)
                    {
                        if (NOT InItem.IsValid()) { return; }

                        auto Idx = _EntitySelectorItems.IndexOfByKey(InItem);

                        if (Idx != INDEX_NONE && Idx < _EntitySelectorHandles.Num())
                        {
                            _ViewModel->Set_SelectedEntityHandle(_EntitySelectorHandles[Idx]);
                        }
                    })
                    [
                        SAssignNew(_EntitySelectorLabel, STextBlock)
                            .Text(FText::FromString(TEXT("(no entities)")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    ]
            ]

        // Status badge
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(8.0f, 0.0f)
            [
                SAssignNew(_StatusBadgeText, STextBlock)
                    .Text(FText::FromString(TEXT("Idle")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Status_Idle)
            ]

        // Spacer
        + SHorizontalBox::Slot()
            .FillWidth(1.0f)

        // Pause button
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f)
            [
                SNew(SButton)
                    .Text_Lambda([this]() -> FText
                    {
                        auto IsPaused = _ViewModel.IsValid() && _ViewModel->Get_Paused();
                        return FText::FromString(IsPaused ? TEXT("Resume") : TEXT("Pause"));
                    })
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (_ViewModel.IsValid())
                        {
                            _ViewModel->Set_Paused(NOT _ViewModel->Get_Paused());
                        }
                        return FReply::Handled();
                    })
            ];
}

// ====================================================================================================================
// Entity selector refresh
// ====================================================================================================================

auto
    SCkAStarDebuggerWindow::
    RefreshEntitySelector()
    -> void
{
    if (NOT _ViewModel.IsValid())
    { return; }

    const auto& Entities = _ViewModel->Get_AllSearchEntities();

    if (Entities.Num() == _EntitySelectorItems.Num())
    { return; }

    _EntitySelectorItems.Reset();
    _EntitySelectorHandles.Reset();

    for (const auto& Info : Entities)
    {
        auto Label = FString::Printf(TEXT("%s"), *Info.DebugName);

        if (Info.GridWidth > 0 && Info.GridHeight > 0)
        {
            Label += FString::Printf(TEXT(" \u2014 %dx%d Grid"), Info.GridWidth, Info.GridHeight);
        }

        _EntitySelectorItems.Add(MakeShared<FString>(Label));
        _EntitySelectorHandles.Add(Info.EntityHandle);
    }

    if (_EntitySelectorLabel.IsValid() && _EntitySelectorItems.Num() > 0)
    {
        auto SelectedHandle = _ViewModel->Get_SelectedEntityHandle();
        auto SelectedIdx = _EntitySelectorHandles.IndexOfByKey(SelectedHandle);

        if (SelectedIdx != INDEX_NONE)
        {
            _EntitySelectorLabel->SetText(FText::FromString(*_EntitySelectorItems[SelectedIdx]));
        }
        else
        {
            _EntitySelectorLabel->SetText(FText::FromString(*_EntitySelectorItems[0]));
        }
    }
    else if (_EntitySelectorLabel.IsValid())
    {
        _EntitySelectorLabel->SetText(FText::FromString(TEXT("(no entities)")));
    }
}

// ====================================================================================================================
