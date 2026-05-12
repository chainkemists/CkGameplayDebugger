#include "CkEqsDebugger/Window/SCkEqsDebugger_TestBreakdownPanel.h"

#include "CkEqsDebugger/CkEqsDebuggerStyle.h"
#include "CkEqsDebugger/ViewModel/CkEqsDebugger_ViewModel.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"   // ECkDebug_Tone full definition (Err / Accent / Ok)

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkEqsDebugger_TestBreakdownPanel"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    auto Tone_FromTest(const FCkEqsDebugger_PerTestInfo& InTest) -> ECkDebug_Tone
    {
        if (NOT InTest.PassedThisTest) { return ECkDebug_Tone::Err; }
        if (InTest.TestPurpose == ECk_Eqs_TestPurpose::Score) { return ECkDebug_Tone::Accent; }
        return ECkDebug_Tone::Ok;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_TestBreakdownPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    if (_ViewModel.IsValid())
    {
        _OnQueryListChangedHandle         = _ViewModel->OnQueryListChanged.AddSP(this, &SCkEqsDebugger_TestBreakdownPanel::OnQueryListChanged);
        _OnSelectedCandidateChangedHandle = _ViewModel->OnSelectedCandidateChanged.AddSP(this, &SCkEqsDebugger_TestBreakdownPanel::OnSelectedCandidateChanged);
    }

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 0.0f, 0.0f, 4.0f})
        [
            SAssignNew(_HeaderLabel, SCkDebug_SelectableLabel)
            .Text(FText::FromString(TEXT("Test breakdown (no candidate selected)")))
            .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Secondary})
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SAssignNew(_ListView, SListView<TSharedPtr<FCkEqsDebugger_PerTestInfo>>)
            .ListItemsSource(&_Items)
            .OnGenerateRow(this, &SCkEqsDebugger_TestBreakdownPanel::OnGenerateRow)
            .OnContextMenuOpening(this, &SCkEqsDebugger_TestBreakdownPanel::OnContextMenuOpening)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];
}

SCkEqsDebugger_TestBreakdownPanel::~SCkEqsDebugger_TestBreakdownPanel()
{
    if (_ViewModel.IsValid())
    {
        if (_OnQueryListChangedHandle.IsValid())
        { _ViewModel->OnQueryListChanged.Remove(_OnQueryListChangedHandle); }
        if (_OnSelectedCandidateChangedHandle.IsValid())
        { _ViewModel->OnSelectedCandidateChanged.Remove(_OnSelectedCandidateChangedHandle); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_TestBreakdownPanel::
    OnQueryListChanged(
        const TArray<FCkEqsDebugger_QueryInfo>& /*InQueries*/)
    -> void
{
    Refresh_FromCurrentCandidate();
}

auto
    SCkEqsDebugger_TestBreakdownPanel::
    OnSelectedCandidateChanged(
        int32 /*InIndex*/)
    -> void
{
    Refresh_FromCurrentCandidate();
}

auto
    SCkEqsDebugger_TestBreakdownPanel::
    Refresh_FromCurrentCandidate()
    -> void
{
    auto HeaderStr = FString{TEXT("Test breakdown (no candidate selected)")};

    // Stable-pointer pattern keyed by TestIndex (same shape as QueryList / CandidatePanel) — keeps the
    // SListView selection identity stable across the per-tick refresh.
    auto Existing = TMap<int32, TSharedPtr<FCkEqsDebugger_PerTestInfo>>{};
    Existing.Reserve(_Items.Num());
    for (const auto& I : _Items)
    {
        if (I.IsValid())
        { Existing.Add(I->TestIndex, I); }
    }

    auto NewItems = TArray<TSharedPtr<FCkEqsDebugger_PerTestInfo>>{};
    auto SetChanged = false;

    if (_ViewModel.IsValid())
    {
        const auto* CandInfo = _ViewModel->Get_CurrentCandidateInfo();
        if (CandInfo)
        {
            HeaderStr = ck::Format_UE(TEXT("Test breakdown (candidate #{}, score {:.4f})"),
                CandInfo->ResultIndex, CandInfo->FinalScore);
            NewItems.Reserve(CandInfo->PerTest.Num());
            for (const auto& T : CandInfo->PerTest)
            {
                auto Item = TSharedPtr<FCkEqsDebugger_PerTestInfo>{};
                if (auto* Found = Existing.Find(T.TestIndex))
                {
                    Item = *Found;   // stable pointer
                    *Item = T;       // refresh fields in place
                    Existing.Remove(T.TestIndex);
                }
                else
                {
                    Item = MakeShared<FCkEqsDebugger_PerTestInfo>(T);
                    SetChanged = true;
                }
                NewItems.Add(MoveTemp(Item));
            }
        }
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    _Items = MoveTemp(NewItems);

    if (_HeaderLabel.IsValid())
    { _HeaderLabel->SetText(FText::FromString(HeaderStr)); }

    if (_ListView.IsValid() && SetChanged)
    { _ListView->RequestListRefresh(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_TestBreakdownPanel::
    OnGenerateRow(
        TSharedPtr<FCkEqsDebugger_PerTestInfo> InItem,
        const TSharedRef<STableViewBase>&      InOwner)
    -> TSharedRef<ITableRow>
{
    const auto& T        = *InItem;
    const auto Tone      = Tone_FromTest(T);
    const auto ToneColor = CkDebugStyle::GetToneColor(Tone);

    const auto TitleText = ck::Format_UE(TEXT("[{}] {}    ({}, w={:.2f})"),
        T.TestIndex,
        CkEqsDebugger::GetTestTypeString(T.TestType),
        CkEqsDebugger::GetTestPurposeString(T.TestPurpose),
        T.Weight);

    const auto MetaText = ck::Format_UE(TEXT("raw {:.4f}    norm {:.4f}    contrib {:.4f}"),
        T.RawValue, T.NormalizedScore, T.WeightedContribution);

    const auto RightText = T.PassedThisTest ? FString{TEXT("PASS")} : FString{TEXT("FAIL")};

    // See SCkEqsDebugger_QueryList::OnGenerateRow for the rationale on not using SCkDebug_HistoryRow:
    // its internal SButton consumes selection clicks. Build the row body from STextBlock + SBox + SImage.
    return SNew(STableRow<TSharedPtr<FCkEqsDebugger_PerTestInfo>>, InOwner)
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth().VAlign(VAlign_Center)
            .Padding(FMargin{6.0f, 0.0f, 8.0f, 0.0f})
            [
                SNew(SBox)
                .WidthOverride(8.0f).HeightOverride(8.0f)
                [
                    SNew(SImage)
                    .Image(CkDebugStyle::GetFilledBrush())
                    .ColorAndOpacity(FSlateColor{ToneColor})
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f).VAlign(VAlign_Center)
            .Padding(FMargin{0.0f, 2.0f})
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TitleText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Primary})
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(MetaText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Secondary})
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth().VAlign(VAlign_Center)
            .Padding(FMargin{8.0f, 0.0f, 6.0f, 0.0f})
            [
                SNew(STextBlock)
                .Text(FText::FromString(RightText))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                .ColorAndOpacity(FSlateColor{ToneColor})
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_TestBreakdownPanel::
    OnContextMenuOpening()
    -> TSharedPtr<SWidget>
{
    if (NOT _ListView.IsValid())
    { return nullptr; }

    auto Selected = _ListView->GetSelectedItems();
    if (Selected.IsEmpty() || NOT Selected[0].IsValid())
    { return nullptr; }

    const auto& T = *Selected[0];
    auto MenuBuilder = FMenuBuilder(true, nullptr);

    ck::DebugCopyMenu::AddCopyEntry(
        MenuBuilder,
        FText::FromString(TEXT("Copy Raw Value")),
        FText::FromString(TEXT("Copy this test's raw value for the selected candidate")),
        ck::Format_UE(TEXT("{}"), T.RawValue));

    ck::DebugCopyMenu::AddCopyEntry(
        MenuBuilder,
        FText::FromString(TEXT("Copy All")),
        FText::FromString(TEXT("Copy a multi-line summary of this test's contribution")),
        BuildCopyTextForItem(T));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_TestBreakdownPanel::
    BuildCopyTextForItem(
        const FCkEqsDebugger_PerTestInfo& InT) const
    -> FString
{
    return ck::Format_UE(
        TEXT("Test [{}] {}\n")
        TEXT("  purpose:    {}\n")
        TEXT("  weight:     {}\n")
        TEXT("  raw value:  {}\n")
        TEXT("  normalized: {}\n")
        TEXT("  contrib:    {}\n")
        TEXT("  result:     {}"),
        InT.TestIndex, CkEqsDebugger::GetTestTypeString(InT.TestType),
        CkEqsDebugger::GetTestPurposeString(InT.TestPurpose),
        InT.Weight,
        InT.RawValue,
        InT.NormalizedScore,
        InT.WeightedContribution,
        InT.PassedThisTest ? TEXT("PASS") : TEXT("FAIL"));
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------
