#include "CkGoapDebugger/Window/SCkGoapDebugger_WorldStateRail.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================
// Internal helpers
// ====================================================================================================================

namespace ck_goap_debugger_wsrail_internal
{
    // Cap displayed key text so very long tags don't blow out the 300px rail.
    constexpr int32 MaxKeyChars = 28;

    auto TruncateKey(const FString& InKey) -> FString
    {
        if (InKey.Len() <= MaxKeyChars) { return InKey; }
        // Show the leaf bit by preferring the right side of the tag, since
        // GameplayTag tail-segments tend to be the discriminating identifier.
        const auto Tail = InKey.Right(MaxKeyChars - 1);
        return FString(TEXT("…")) + Tail;
    }
}

// ====================================================================================================================
// CONSTRUCT / DESTRUCT
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    // ---- Header (pane-head) ----------------------------------
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SAssignNew(_HeaderHost, SBox)
                        ]

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SSeparator)
                                .Thickness(1.0f)
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Border_Subtle))
                        ]

                    // ---- Body (scrollable key list) --------------------------
                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNew(SScrollBox)
                                .Orientation(Orient_Vertical)
                                + SScrollBox::Slot()
                                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                                 FCkGoapDebuggerStyle::Padding_Small))
                                [
                                    SAssignNew(_Body, SVerticalBox)
                                ]
                        ]

                    // ---- Footer ----------------------------------------------
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SAssignNew(_FooterHost, SBox)
                        ]
            ]
    ];

    RefreshFromViewModel();
}

SCkGoapDebugger_WorldStateRail::~SCkGoapDebugger_WorldStateRail() = default;

// ====================================================================================================================
// REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    Resolve_DisplayedWorldState(
        const TArray<FCkGoapDebugger_WorldStateEntry>*& OutEntries,
        FString& OutLabel) const
    -> bool
{
    OutEntries = nullptr;
    OutLabel.Reset();

    if (NOT _ViewModel.IsValid()) { return false; }

    const auto* AsInfo = _ViewModel->GetSelectedActionSetInfo();
    if (AsInfo == nullptr) { return false; }

    // Prefer the selected Action's WS source label when set; the per-ActionSet
    // WS array remains the source of resolved values (we don't yet collect a
    // per-Action override on FCkGoapDebugger_ActionInfo).
    const auto* SelAction = _ViewModel->GetSelectedActionInfo();
    if (SelAction != nullptr && NOT SelAction->WorldStateSourceLabel.IsEmpty())
    {
        OutLabel = SelAction->WorldStateSourceLabel;
    }
    else
    {
        OutLabel = AsInfo->WorldStateSourceLabel;
    }

    OutEntries = &AsInfo->WorldState;
    return true;
}

auto
    SCkGoapDebugger_WorldStateRail::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    const TArray<FCkGoapDebugger_WorldStateEntry>* Entries = nullptr;
    auto Label = FString{};
    const auto HasSelection = Resolve_DisplayedWorldState(Entries, Label);

    // ---- Empty state ----------------------------------------------------------
    if (NOT HasSelection || Entries == nullptr || Entries->Num() == 0)
    {
        if (_HeaderHost.IsValid())
        {
            _HeaderHost->SetContent(
                SNew(SBorder)
                    .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
                    .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                     FCkGoapDebuggerStyle::Padding_Medium))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("WORLDSTATE")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]);
        }

        if (_Body.IsValid())
        {
            _Body->ClearChildren();
            _Body->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium))
                [
                    BuildEmptyState()
                ];
        }

        if (_FooterHost.IsValid())
        { _FooterHost->SetContent(SNew(SSpacer)); }

        return;
    }

    // ---- Header (label + key count) ------------------------------------------
    const auto KeyCount = Entries->Num();
    auto RecentCount    = 0;
    for (const auto& E : *Entries)
    { if (E.RecentlyChanged) { ++RecentCount; } }

    if (_HeaderHost.IsValid())
    {
        const auto HeaderText = FString::Printf(TEXT("WORLDSTATE · %s"),
            Label.IsEmpty() ? TEXT("(unresolved)") : *Label);
        const auto CountText = FString::Printf(TEXT("%d keys"), KeyCount);

        _HeaderHost->SetContent(
            SNew(SBorder)
                .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                 FCkGoapDebuggerStyle::Padding_Medium))
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(HeaderText))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                            ]

                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(CountText))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Planning))
                            ]
                ]);
    }

    // ---- Body (rows) ----------------------------------------------------------
    if (_Body.IsValid())
    {
        _Body->ClearChildren();
        for (const auto& Entry : *Entries)
        {
            _Body->AddSlot()
                .AutoHeight()
                [
                    BuildKeyRow(Entry)
                ];
        }
    }

    // ---- Footer ---------------------------------------------------------------
    if (_FooterHost.IsValid())
    {
        const auto FooterText = FString::Printf(
            TEXT("★ = recently changed · %d of %d keys"), RecentCount, KeyCount);

        _FooterHost->SetContent(
            SNew(SBorder)
                .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Border.Subtle")))
                .Padding(FMargin(0.0f, 1.0f, 0.0f, 0.0f))   // top-only divider line
                [
                    SNew(SBorder)
                        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
                        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                         FCkGoapDebuggerStyle::Padding_Small))
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(FooterText))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                        ]
                ]);
    }
}

// ====================================================================================================================
// BUILD — EMPTY STATE
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    BuildEmptyState()
    -> TSharedRef<SWidget>
{
    auto Message = FString(TEXT("No entity selected"));
    if (_ViewModel.IsValid())
    {
        if (ck::IsValid(_ViewModel->GetSelectedEntity()))
        {
            if (_ViewModel->GetSelectedActionSetInfo() == nullptr)
            { Message = TEXT("No ActionSet selected"); }
            else
            { Message = TEXT("(WorldState empty)"); }
        }
    }

    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large))
        [
            SNew(STextBlock)
                .Text(FText::FromString(Message))
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
        ];
}

// ====================================================================================================================
// BUILD — KEY ROW
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    BuildKeyRow(
        const FCkGoapDebugger_WorldStateEntry& InEntry)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_wsrail_internal;

    const auto KeyText  = TruncateKey(InEntry.Key.IsValid() ? InEntry.Key.ToString() : FString(TEXT("(invalid)")));
    const auto ValueStr = InEntry.Value ? FString(TEXT("TRUE")) : FString(TEXT("false"));
    const auto ValueCol = InEntry.Value
        ? FCkGoapDebuggerStyle::Color_Status_PlanFound
        : FCkGoapDebuggerStyle::Color_Text_Dim;
    const auto KeyCol = FCkGoapDebuggerStyle::Color_Text_Secondary;

    auto RowContent = SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(STextBlock)
                    .Visibility(InEntry.RecentlyChanged ? EVisibility::Visible : EVisibility::Collapsed)
                    .Text(FText::FromString(TEXT("★")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
            ]
        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(KeyText))
                    .ToolTipText(FText::FromString(InEntry.Key.IsValid() ? InEntry.Key.ToString() : FString{}))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(KeyCol))
            ]
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(ValueStr))
                    .Font(InEntry.Value
                        ? FCoreStyle::GetDefaultFontStyle("Bold", 9)
                        : FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(ValueCol))
            ];

    if (InEntry.RecentlyChanged)
    {
        return SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.WS.RowRecent")))
            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 1.0f))
            [
                RowContent
            ];
    }

    return SNew(SBox)
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 1.0f))
        [
            RowContent
        ];
}

// ====================================================================================================================
