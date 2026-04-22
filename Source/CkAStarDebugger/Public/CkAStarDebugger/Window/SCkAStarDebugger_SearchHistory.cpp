#include "CkAStarDebugger/Window/SCkAStarDebugger_SearchHistory.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkAStarDebugger/CkAStarDebuggerStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "Widgets/Images/SImage.h"

// ====================================================================================================================
// Construction
// ====================================================================================================================

auto
    SCkAStarDebugger_SearchHistory::
    Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkAStarDebugger_ViewModel> InViewModel)
    -> void
{
    _ViewModel = InViewModel;

    ChildSlot
    [
        SNew(SVerticalBox)
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(12.0f, 8.0f, 12.0f, 4.0f)
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(TEXT("SEARCH HISTORY")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                        .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Muted)
                ]
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(12.0f, 0.0f, 12.0f, 8.0f)
                [
                    SNew(SScrollBox)
                        + SScrollBox::Slot()
                            [
                                SAssignNew(_EntryListBox, SVerticalBox)
                            ]
                ]
    ];
}

// ====================================================================================================================
// Tick — rebuild list when history changes
// ====================================================================================================================

auto
    SCkAStarDebugger_SearchHistory::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT _ViewModel.IsValid() || NOT _ViewModel->Has_SelectedEntity())
    { return; }

    auto* History = _ViewModel->Get_SearchHistory(_ViewModel->Get_SelectedEntityHandle());
    auto CurrentCount = History ? History->Num() : 0;

    if (CurrentCount != _LastEntryCount)
    {
        _LastEntryCount = CurrentCount;
        RebuildList();
    }
}

// ====================================================================================================================
// Rebuild list
// ====================================================================================================================

auto
    SCkAStarDebugger_SearchHistory::
    RebuildList()
    -> void
{
    _EntryListBox->ClearChildren();

    if (NOT _ViewModel.IsValid() || NOT _ViewModel->Has_SelectedEntity())
    { return; }

    auto* History = _ViewModel->Get_SearchHistory(_ViewModel->Get_SelectedEntityHandle());

    if (NOT History || History->IsEmpty())
    {
        _EntryListBox->AddSlot()
            .AutoHeight()
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(TEXT("No search history yet")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Muted)
            ];
        return;
    }

    for (int32 Idx = History->Num() - 1; Idx >= 0; --Idx)
    {
        _EntryListBox->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 3.0f)
            [
                BuildHistoryEntry((*History)[Idx])
            ];
    }
}

// ====================================================================================================================
// Build a single history entry widget
// ====================================================================================================================

auto
    SCkAStarDebugger_SearchHistory::
    BuildHistoryEntry(
        const FCkAStarDebugger_HistoryEntry& InEntry)
    -> TSharedRef<SWidget>
{
    auto StatusStr = CkAStarDebugger::GetStatusString(InEntry.FinalStatus);
    auto StatusColor = CkAStarDebugger::GetStatusColor(InEntry.FinalStatus);

    auto MetaStr = FString::Printf(TEXT("F#%llu"), InEntry.FrameNumber);

    if (InEntry.FinalStatus == ECk_AStarSearchStatus::Complete)
    {
        MetaStr += FString::Printf(TEXT(" \u00B7 %.1f cost \u00B7 %d iter \u00B7 %lldus"),
            InEntry.TotalCost,
            InEntry.TotalIterations,
            InEntry.TotalTimeMicroseconds);
    }
    else if (InEntry.FinalStatus == ECk_AStarSearchStatus::Failed)
    {
        MetaStr += FString::Printf(TEXT(" \u00B7 no path \u00B7 %d iter \u00B7 %lldus"),
            InEntry.TotalIterations,
            InEntry.TotalTimeMicroseconds);
    }
    else
    {
        MetaStr += FString::Printf(TEXT(" \u00B7 %d iter \u00B7 %lldus"),
            InEntry.TotalIterations,
            InEntry.TotalTimeMicroseconds);
    }

    return SNew(SHorizontalBox)
        .Clipping(EWidgetClipping::ClipToBounds)

        // Status dot
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SBox)
                    .WidthOverride(FCkAStarDebuggerStyle::HistoryDot_Size)
                    .HeightOverride(FCkAStarDebuggerStyle::HistoryDot_Size)
                    [
                        SNew(SImage)
                            .Image(FAppStyle::GetBrush("WhiteBrush"))
                            .ColorAndOpacity(StatusColor)
                    ]
            ]

        // Status text + meta
        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(StatusStr))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                .ColorAndOpacity(StatusColor)
                        ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(MetaStr))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Muted)
                        ]
            ];
}

// ====================================================================================================================
