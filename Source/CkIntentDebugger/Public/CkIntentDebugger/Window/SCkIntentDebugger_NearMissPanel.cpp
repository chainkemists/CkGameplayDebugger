#include "CkIntentDebugger/Window/SCkIntentDebugger_NearMissPanel.h"

#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_nearmiss
{
    constexpr auto PadS = 4.0f;
    constexpr auto PadM = 8.0f;

    auto
        Get_Summary(
            const FCk_Intent_ScanDiagnostic& InDiagnostic)
        -> FString
    {
        if (InDiagnostic.Get_Outcome() == ECk_Intent_ScanOutcome::Matched)
        { return ck::Format_UE(TEXT("matched on frame {}"), InDiagnostic.Get_TerminalFrame()); }

        const auto& Steps = InDiagnostic.Get_Steps();
        if (Steps.IsEmpty())
        { return TEXT("failed — no steps walked"); }

        const auto& Last = Steps.Last();

        return ck::Format_UE(TEXT("failed at step {} · {} · {} frame(s) examined"),
            InDiagnostic.Get_FailedStepIndex(),
            ck::intent_debugger::Get_Label(Last.Get_Outcome()),
            Last.Get_FramesExamined());
    }

    auto
        Get_CopyText(
            const FCk_Intent_ScanDiagnostic& InDiagnostic)
        -> FString
    {
        auto Lines = TArray<FString>{};
        Lines.Add(ck::Format_UE(TEXT("{}\tterminal frame {}\t{}"),
            InDiagnostic.Get_IntentName(), InDiagnostic.Get_TerminalFrame(), Get_Summary(InDiagnostic)));

        for (const auto& Step : InDiagnostic.Get_Steps())
        {
            Lines.Add(ck::Format_UE(TEXT("\tstep {}\t{}\tmatched at {}\t{} frame(s) examined"),
                Step.Get_StepIndex(),
                ck::intent_debugger::Get_Label(Step.Get_Outcome()),
                Step.Get_MatchedAtFrame(),
                Step.Get_FramesExamined()));
        }

        return FString::Join(Lines, TEXT("\n"));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_NearMissRow::
    Make_Key(
        const FCk_Intent_ScanDiagnostic& InDiagnostic)
    -> FString
{
    const auto& Steps = InDiagnostic.Get_Steps();
    const auto FramesExamined = Steps.IsEmpty() ? 0 : Steps.Last().Get_FramesExamined();

    return ck::Format_UE(TEXT("{}|{}|{}|{}|{}"),
        InDiagnostic.Get_IntentName(),
        InDiagnostic.Get_TerminalFrame(),
        static_cast<int32>(InDiagnostic.Get_Outcome()),
        InDiagnostic.Get_FailedStepIndex(),
        FramesExamined);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_NearMissPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    const auto WeakPanel = TWeakPtr<SCkIntentDebugger_NearMissPanel>(SharedThis(this));

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
                .Label(FText::FromString(TEXT("Near misses")))
                .SubText(FText::FromString(TEXT("newest first · the only record a failed scan leaves")))
        ]

        + SVerticalBox::Slot().AutoHeight().Padding(ck_intent_debugger_nearmiss::PadS)
        [
            SNew(STextBlock)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(CkStyle::Warn())
                .AutoWrapText(true)
                .Visibility_Lambda([WeakPanel]()
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid())
                    { return EVisibility::Collapsed; }

                    return Panel->_ViewModel->Get_Snapshot().ScanDiagnosticsEnabled
                        ? EVisibility::Collapsed
                        : EVisibility::Visible;
                })
                .Text(FText::FromString(TEXT(
                    "Scan diagnostics are OFF. An empty list here means nothing was recorded, not that nothing was "
                    "scanned. Enable with: ck.Intent.RecordScanDiagnostics 1")))
        ]

        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SAssignNew(_ListView, SListView<TSharedPtr<FCkIntentDebugger_NearMissRow>>)
                .ListItemsSource(&_Rows)
                .SelectionMode(ESelectionMode::Single)
                .OnGenerateRow(this, &SCkIntentDebugger_NearMissPanel::OnGenerateRow)
                .OnSelectionChanged(this, &SCkIntentDebugger_NearMissPanel::OnSelectionChanged)
                .OnContextMenuOpening_Lambda([this]() -> TSharedPtr<SWidget>
                {
                    auto Lines = TArray<FString>{};
                    for (const auto& Selected : _ListView->GetSelectedItems())
                    {
                        if (NOT Selected.IsValid())
                        { continue; }

                        Lines.Add(ck_intent_debugger_nearmiss::Get_CopyText(Selected->Diagnostic));
                    }

                    if (Lines.IsEmpty())
                    { return nullptr; }

                    auto MenuBuilder = FMenuBuilder{true, nullptr};
                    ck::DebugCopyMenu::AddCopyEntry(
                        MenuBuilder,
                        FText::FromString(TEXT("Copy Scan(s)")),
                        FText::FromString(TEXT("Copy the selected scan with every walked step")),
                        FString::Join(Lines, TEXT("\n")));

                    return MenuBuilder.MakeWidget();
                })
        ]

        + SVerticalBox::Slot().AutoHeight().MaxHeight(240.0f)
        [
            SNew(SScrollBox)

            + SScrollBox::Slot()
            [
                SAssignNew(_StepDetail, SVerticalBox)
            ]
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_NearMissPanel::
    Reset_ForWorldChange()
    -> void
{
    _Rows.Reset();
    _SelectedKey.Reset();
    _StepDetailHash = 0;

    if (_StepDetail.IsValid())
    { _StepDetail->ClearChildren(); }

    if (_ListView.IsValid())
    { _ListView->RequestListRefresh(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_NearMissPanel::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid() || NOT _ListView.IsValid())
    { return; }

    const auto* Layer = _ViewModel->TryGet_SelectedLayer();

    auto Existing = TMap<FString, TSharedPtr<FCkIntentDebugger_NearMissRow>>{};
    for (const auto& Row : _Rows)
    {
        if (Row.IsValid())
        { Existing.Add(Row->Key, Row); }
    }

    auto NewRows = TArray<TSharedPtr<FCkIntentDebugger_NearMissRow>>{};
    auto SetChanged = false;

    if (Layer != nullptr)
    {
        const auto& Diagnostics = Layer->ScanDiagnostics;
        NewRows.Reserve(Diagnostics.Num());

        for (const auto& Diagnostic : Diagnostics)
        {
            const auto Key = FCkIntentDebugger_NearMissRow::Make_Key(Diagnostic);

            if (auto* Found = Existing.Find(Key))
            {
                (*Found)->Diagnostic = Diagnostic;
                NewRows.Add(*Found);
                Existing.Remove(Key);
                continue;
            }

            auto Row = MakeShared<FCkIntentDebugger_NearMissRow>();
            Row->Key = Key;
            Row->Diagnostic = Diagnostic;

            NewRows.Add(MoveTemp(Row));
            SetChanged = true;
        }
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    _Rows = MoveTemp(NewRows);

    if (SetChanged)
    { _ListView->RequestListRefresh(); }

    DoRebuild_StepDetail();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_NearMissPanel::
    OnGenerateRow(
        TSharedPtr<FCkIntentDebugger_NearMissRow> InRow,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    const auto WeakRow = TWeakPtr<FCkIntentDebugger_NearMissRow>(InRow);

    return SNew(STableRow<TSharedPtr<FCkIntentDebugger_NearMissRow>>, InOwnerTable)
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().FillWidth(0.3f).Padding(ck_intent_debugger_nearmiss::PadS, 0.0f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(CkStyle::Text())
                    .Text_Lambda([WeakRow]()
                    {
                        const auto Row = WeakRow.Pin();
                        return Row.IsValid()
                            ? FText::FromName(Row->Diagnostic.Get_IntentName())
                            : FText::GetEmpty();
                    })
            ]

            + SHorizontalBox::Slot().FillWidth(0.7f).Padding(ck_intent_debugger_nearmiss::PadS, 0.0f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .Text_Lambda([WeakRow]()
                    {
                        const auto Row = WeakRow.Pin();
                        return Row.IsValid()
                            ? FText::FromString(ck_intent_debugger_nearmiss::Get_Summary(Row->Diagnostic))
                            : FText::GetEmpty();
                    })
                    .ColorAndOpacity_Lambda([WeakRow]()
                    {
                        const auto Row = WeakRow.Pin();
                        if (NOT Row.IsValid())
                        { return FSlateColor{CkStyle::TextMute()}; }

                        return FSlateColor{Row->Diagnostic.Get_Outcome() == ECk_Intent_ScanOutcome::Matched
                            ? CkStyle::Ok()
                            : CkStyle::Err()};
                    })
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_NearMissPanel::
    OnSelectionChanged(
        TSharedPtr<FCkIntentDebugger_NearMissRow> InRow,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    _SelectedKey = InRow.IsValid() ? InRow->Key : FString{};

    if (_ViewModel.IsValid() && InRow.IsValid())
    { _ViewModel->Set_ScrubFrame(InRow->Diagnostic.Get_TerminalFrame()); }

    DoRebuild_StepDetail();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_NearMissPanel::
    DoRebuild_StepDetail()
    -> void
{
    if (NOT _StepDetail.IsValid())
    { return; }

    const FCkIntentDebugger_NearMissRow* Selected = nullptr;
    for (const auto& Row : _Rows)
    {
        if (Row.IsValid() && Row->Key == _SelectedKey)
        {
            Selected = Row.Get();
            break;
        }
    }

    const auto Hash = Selected != nullptr
        ? HashCombine(GetTypeHash(Selected->Key), GetTypeHash(Selected->Diagnostic.Get_Steps().Num()))
        : 0u;

    if (Hash == _StepDetailHash)
    { return; }

    _StepDetailHash = Hash;
    _StepDetail->ClearChildren();

    if (Selected == nullptr)
    { return; }

    const auto& Diagnostic = Selected->Diagnostic;

    _StepDetail->AddSlot().AutoHeight().Padding(ck_intent_debugger_nearmiss::PadM, ck_intent_debugger_nearmiss::PadS)
    [
        SNew(STextBlock)
            .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(CkStyle::TextStrong())
            .Text(FText::FromString(ck::Format_UE(TEXT("{} · terminal frame {} · steps in WALK order"),
                Diagnostic.Get_IntentName(), Diagnostic.Get_TerminalFrame())))
    ];

    for (const auto& Step : Diagnostic.Get_Steps())
    {
        const auto Outcome = Step.Get_Outcome();

        _StepDetail->AddSlot().AutoHeight().Padding(ck_intent_debugger_nearmiss::PadM, 1.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().FillWidth(0.2f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(CkStyle::TextDim())
                    .Text(FText::FromString(ck::Format_UE(TEXT("step {}"), Step.Get_StepIndex())))
            ]

            + SHorizontalBox::Slot().FillWidth(0.35f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(ck::intent_debugger::Get_StepOutcomeColor(Outcome))
                    .Text(FText::FromString(ck::intent_debugger::Get_Label(Outcome)))
            ]

            + SHorizontalBox::Slot().FillWidth(0.45f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(CkStyle::TextDim())
                    .Text(FText::FromString(ck::Format_UE(TEXT("matched at {} · {} frame(s) examined"),
                        Step.Get_MatchedAtFrame(), Step.Get_FramesExamined())))
            ]
        ];
    }
}

// --------------------------------------------------------------------------------------------------------------------
