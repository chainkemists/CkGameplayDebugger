#include "CkEqsDebugger/Window/SCkEqsDebugger_CandidatePanel.h"

#include "CkEqsDebugger/ViewModel/CkEqsDebugger_ViewModel.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CategoryDot.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkEqsDebugger_CandidatePanel"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_eqs_debugger_candidate_panel
{
    auto Tone_FromCandidate(const FCkEqsDebugger_CandidateInfo& InItem) -> ECk_Tone
    {
        if (InItem.IsBestPick) { return ECk_Tone::Accent; }
        if (NOT InItem.Passed) { return ECk_Tone::Err; }
        return ECk_Tone::Ok;
    }

}


// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_CandidatePanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    if (_ViewModel.IsValid())
    {
        _OnQueryListChangedHandle     = _ViewModel->OnQueryListChanged.AddSP(this, &SCkEqsDebugger_CandidatePanel::OnQueryListChanged);
        _OnSelectedQueryChangedHandle = _ViewModel->OnSelectedQueryChanged.AddSP(this, &SCkEqsDebugger_CandidatePanel::OnSelectedQueryChanged);
    }

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 0.0f, 0.0f, 4.0f})
        [
            SAssignNew(_HeaderLabel, SCkDebug_SelectableLabel)
            .Text(FText::FromString(TEXT("Candidates (no query selected)")))
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SAssignNew(_ListView, SListView<TSharedPtr<FCkEqsDebugger_CandidateInfo>>)
            .ListItemsSource(&_Items)
            .OnGenerateRow(this, &SCkEqsDebugger_CandidatePanel::OnGenerateRow)
            .OnSelectionChanged(this, &SCkEqsDebugger_CandidatePanel::OnSelectionChanged)
            .OnContextMenuOpening(this, &SCkEqsDebugger_CandidatePanel::OnContextMenuOpening)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];
}

SCkEqsDebugger_CandidatePanel::~SCkEqsDebugger_CandidatePanel()
{
    if (_ViewModel.IsValid())
    {
        if (_OnQueryListChangedHandle.IsValid())
        { _ViewModel->OnQueryListChanged.Remove(_OnQueryListChangedHandle); }
        if (_OnSelectedQueryChangedHandle.IsValid())
        { _ViewModel->OnSelectedQueryChanged.Remove(_OnSelectedQueryChangedHandle); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_CandidatePanel::
    OnQueryListChanged(
        const TArray<FCkEqsDebugger_QueryInfo>& /*InQueries*/)
    -> void
{
    Refresh_FromCurrentQuery();
}

auto
    SCkEqsDebugger_CandidatePanel::
    OnSelectedQueryChanged(
        FCk_Handle_EqsQuery /*InHandle*/)
    -> void
{
    Refresh_FromCurrentQuery();
}

auto
    SCkEqsDebugger_CandidatePanel::
    Refresh_FromCurrentQuery()
    -> void
{
    auto HeaderStr = FString{TEXT("Candidates (no query selected)")};

    // Reuse existing TSharedPtr instances by ResultIndex when possible — keeps SListView selection identity
    // stable across the per-tick refresh. Same fix as SCkEqsDebugger_QueryList::ApplyFilterPipeline; without
    // this, selecting a candidate would be impossible because the items would be reallocated 30x/sec.
    auto Existing = TMap<int32, TSharedPtr<FCkEqsDebugger_CandidateInfo>>{};
    Existing.Reserve(_Items.Num());
    for (const auto& I : _Items)
    {
        if (I.IsValid())
        { Existing.Add(I->ResultIndex, I); }
    }

    auto NewItems = TArray<TSharedPtr<FCkEqsDebugger_CandidateInfo>>{};
    auto SetChanged = false;
    auto NewSelection = TSharedPtr<FCkEqsDebugger_CandidateInfo>{};
    const auto SelectedIndex = _ViewModel.IsValid() ? _ViewModel->Get_SelectedCandidateIndex() : -1;

    if (_ViewModel.IsValid())
    {
        const auto* QueryInfo = _ViewModel->Get_CurrentQueryInfo();
        if (QueryInfo)
        {
            HeaderStr = ck::Format_UE(TEXT("Candidates ({})"), QueryInfo->Candidates.Num());
            NewItems.Reserve(QueryInfo->Candidates.Num());
            for (const auto& C : QueryInfo->Candidates)
            {
                auto Item = TSharedPtr<FCkEqsDebugger_CandidateInfo>{};
                if (auto* Found = Existing.Find(C.ResultIndex))
                {
                    Item = *Found;       // stable pointer
                    *Item = C;           // refresh fields in place
                    Existing.Remove(C.ResultIndex);
                }
                else
                {
                    Item = MakeShared<FCkEqsDebugger_CandidateInfo>(C);
                    SetChanged = true;
                }

                if (Item->ResultIndex == SelectedIndex)
                { NewSelection = Item; }

                NewItems.Add(MoveTemp(Item));
            }
        }
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    _Items = MoveTemp(NewItems);

    if (_HeaderLabel.IsValid())
    { _HeaderLabel->SetText(FText::FromString(HeaderStr)); }

    if (_ListView.IsValid())
    {
        if (SetChanged)
        { _ListView->RequestListRefresh(); }

        if (NewSelection.IsValid())
        {
            const auto Cur = _ListView->GetSelectedItems();
            const auto AlreadySelected = (Cur.Num() == 1 && Cur[0] == NewSelection);
            if (NOT AlreadySelected)
            { _ListView->SetItemSelection(NewSelection, true, ESelectInfo::Direct); }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_CandidatePanel::
    OnGenerateRow(
        TSharedPtr<FCkEqsDebugger_CandidateInfo> InItem,
        const TSharedRef<STableViewBase>&        InOwner)
    -> TSharedRef<ITableRow>
{
    const auto& C = *InItem;
    const auto Tone      = ck_eqs_debugger_candidate_panel::Tone_FromCandidate(C);
    const auto ToneColor = CkStyle::GetToneColor(Tone);

    const auto TitleText = C.IsBestPick
        ? ck::Format_UE(TEXT("[BEST] #{}    score {:.4f}"), C.ResultIndex, C.FinalScore)
        : ck::Format_UE(TEXT("#{}    score {:.4f}"), C.ResultIndex, C.FinalScore);

    const auto MetaText = ck::Format_UE(TEXT("({:.0f}, {:.0f}, {:.0f})"),
        C.Location.X, C.Location.Y, C.Location.Z);

    const auto SubtitleText = ck::IsValid(C.EntityHandle)
        ? ck::Format_UE(TEXT("entity: {}"), C.EntityHandle)
        : (C.Passed ? FString{} : FString{TEXT("(filter-rejected)")});

    // Custom row body — STextBlock + the click-passive SCkDebug_CategoryDot don't trap clicks the way
    // SCkDebug_HistoryRow's internal SButton does. STableRow.ShowSelection(true) gives the selection visual.
    auto Body = SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TitleText))
            .Font(C.IsBestPick
                ? CkStyle::BoldFont(CkStyle::FontSizeBody())
                : CkStyle::RegularFont(CkStyle::FontSizeBody()))
            .ColorAndOpacity(FSlateColor{CkStyle::Text()})
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(MetaText))
            .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
        ];

    if (NOT SubtitleText.IsEmpty())
    {
        Body->AddSlot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(SubtitleText))
            .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
        ];
    }

    return SNew(STableRow<TSharedPtr<FCkEqsDebugger_CandidateInfo>>, InOwner)
        .Padding(ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 1.0f}))
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth().VAlign(VAlign_Center)
            .Padding(FMargin{6.0f, 0.0f, 8.0f, 0.0f})
            [
                SNew(SCkDebug_CategoryDot)
                .Color(ToneColor)
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f).VAlign(VAlign_Center)
            .Padding(FMargin{0.0f, 2.0f})
            [
                Body
            ]
        ];
}

auto
    SCkEqsDebugger_CandidatePanel::
    OnSelectionChanged(
        TSharedPtr<FCkEqsDebugger_CandidateInfo> InItem,
        ESelectInfo::Type                        InSelectInfo)
    -> void
{
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    if (NOT _ViewModel.IsValid())
    { return; }

    _ViewModel->Set_SelectedCandidateIndex(InItem.IsValid() ? InItem->ResultIndex : -1);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_CandidatePanel::
    OnContextMenuOpening()
    -> TSharedPtr<SWidget>
{
    if (NOT _ListView.IsValid())
    { return nullptr; }

    auto Selected = _ListView->GetSelectedItems();
    if (Selected.IsEmpty() || NOT Selected[0].IsValid())
    { return nullptr; }

    const auto& C = *Selected[0];
    auto MenuBuilder = FMenuBuilder(true, nullptr);

    ck::DebugCopyMenu::AddCopyEntry(
        MenuBuilder,
        FText::FromString(TEXT("Copy Location")),
        FText::FromString(TEXT("Copy this candidate's world location as (x, y, z)")),
        ck::Format_UE(TEXT("({:.4f}, {:.4f}, {:.4f})"), C.Location.X, C.Location.Y, C.Location.Z));

    ck::DebugCopyMenu::AddCopyEntry(
        MenuBuilder,
        FText::FromString(TEXT("Copy Score")),
        FText::FromString(TEXT("Copy this candidate's final accumulated score")),
        ck::Format_UE(TEXT("{}"), C.FinalScore));

    ck::DebugCopyMenu::AddCopyEntry(
        MenuBuilder,
        FText::FromString(TEXT("Copy All")),
        FText::FromString(TEXT("Copy a multi-line summary of this candidate")),
        BuildCopyTextForItem(C));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_CandidatePanel::
    BuildCopyTextForItem(
        const FCkEqsDebugger_CandidateInfo& InC) const
    -> FString
{
    return ck::Format_UE(
        TEXT("Candidate #{}{}\n")
        TEXT("  location: ({:.4f}, {:.4f}, {:.4f})\n")
        TEXT("  score:    {}\n")
        TEXT("  passed:   {}\n")
        TEXT("  entity:   {}"),
        InC.ResultIndex, InC.IsBestPick ? TEXT("  [BEST]") : TEXT(""),
        InC.Location.X, InC.Location.Y, InC.Location.Z,
        InC.FinalScore,
        InC.Passed ? TEXT("true") : TEXT("false"),
        ck::IsValid(InC.EntityHandle) ? ck::Format_UE(TEXT("{}"), InC.EntityHandle) : FString{TEXT("(none)")});
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------
