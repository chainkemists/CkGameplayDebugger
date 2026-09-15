#include "CkIntentDebugger/Window/SCkIntentDebugger_ResolutionPanel.h"

#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/IMenu.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_resolution
{
    constexpr auto PadS = 4.0f;

    // Every key a terminal button resolves to, joined for display — primary first, exactly as the row
    // carries them. Empty reads as the same "<unbound>" a single unresolved key always has.
    auto
        Get_KeysLabel(
            const TArray<FKey>& InKeys)
        -> FString
    {
        if (InKeys.IsEmpty())
        { return TEXT("<unbound>"); }

        auto Parts = TArray<FString>{};
        Parts.Reserve(InKeys.Num());
        for (const auto& Key : InKeys)
        { Parts.Add(Key.ToString()); }

        return FString::Join(Parts, TEXT(", "));
    }

    auto
        Get_DeferralLabel(
            const FCkIntentDebugger_ResolutionRow& InRow)
        -> FString
    {
        if (NOT InRow.Get_IsDeferred())
        { return TEXT("immediate"); }

        auto Parts = TArray<FString>{};

        if (InRow.HoldSiblingFrames > 0)
        { Parts.Add(ck::Format_UE(TEXT("hold sibling {}f"), InRow.HoldSiblingFrames)); }

        if (InRow.ChordMemberFrames > 0)
        { Parts.Add(ck::Format_UE(TEXT("chord member {}f"), InRow.ChordMemberFrames)); }

        return FString::Join(Parts, TEXT(" + "));
    }

    auto
        Get_CopyText(
            const FCkIntentDebugger_ResolutionRow& InRow)
        -> FString
    {
        auto Names = TArray<FString>{};
        for (const auto& Name : InRow.IntentNames)
        { Names.Add(Name.ToString()); }

        return ck::Format_UE(TEXT("{}\t{}\t{}\t{}"),
            InRow.TerminalLabel,
            Get_KeysLabel(InRow.ResolvedKeys),
            FString::Join(Names, TEXT(" > ")),
            Get_DeferralLabel(InRow));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_ResolutionPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;
    const TWeakPtr<SCkIntentDebugger_ResolutionPanel> WeakPanel = SharedThis(this);

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
                .Label(FText::FromString(TEXT("Resolution table")))
                .SubText(FText::FromString(TEXT("terminal button → candidates, most dominant first · deferral verdict")))
        ]

        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SAssignNew(_ListView, SListView<TSharedPtr<FCkIntentDebugger_ResolutionRow>>)
                .ListItemsSource(&_Rows)
                .SelectionMode(ESelectionMode::Multi)
                .OnGenerateRow(this, &SCkIntentDebugger_ResolutionPanel::OnGenerateRow)
                .OnContextMenuOpening_Lambda([WeakPanel]() -> TSharedPtr<SWidget>
                {
                    if (const TSharedPtr<SCkIntentDebugger_ResolutionPanel> Panel = WeakPanel.Pin()) { return Panel->OpenContextMenu(); }
                    return nullptr;
                })
        ]
    ];
}

auto SCkIntentDebugger_ResolutionPanel::ReleaseContextMenu() -> void
{
    const TSharedPtr<IMenu> Menu = MoveTemp(_ContextMenu);
    if (Menu.IsValid() && FSlateApplication::IsInitialized()) { FSlateApplication::Get().DismissMenu(Menu); }
}

auto SCkIntentDebugger_ResolutionPanel::OpenContextMenu() -> TSharedPtr<SWidget>
{
    if (NOT _ListView.IsValid() || NOT FSlateApplication::IsInitialized()) { return nullptr; }
    auto Lines = TArray<FString>{};
    for (const TSharedPtr<FCkIntentDebugger_ResolutionRow>& Selected : _ListView->GetSelectedItems())
    { if (Selected.IsValid()) { Lines.Add(ck_intent_debugger_resolution::Get_CopyText(*Selected)); } }
    if (Lines.IsEmpty()) { return nullptr; }
    auto Builder = FMenuBuilder{true, nullptr};
    ck::DebugCopyMenu::AddCopyEntry(Builder, FText::FromString(TEXT("Copy Row(s)")), FText::FromString(TEXT("Copy the selected resolution rows")), FString::Join(Lines, TEXT("\n")));
    ReleaseContextMenu();
    _ContextMenu = FSlateApplication::Get().PushMenu(_ListView.ToSharedRef(), FWidgetPath{}, Builder.MakeWidget(), FVector2f{FSlateApplication::Get().GetCursorPos()}, FPopupTransitionEffect{FPopupTransitionEffect::ContextMenu});
    if (NOT _ContextMenu.IsValid()) { return nullptr; }
    const TWeakPtr<SCkIntentDebugger_ResolutionPanel> WeakPanel = SharedThis(this);
    _ContextMenu->GetOnMenuDismissed().AddLambda([WeakPanel](const TSharedRef<IMenu>& InDismissedMenu)
    { if (const TSharedPtr<SCkIntentDebugger_ResolutionPanel> Panel = WeakPanel.Pin(); Panel.IsValid() && Panel->_ContextMenu == InDismissedMenu) { Panel->_ContextMenu.Reset(); } });
    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_ResolutionPanel::
    Reset_ForWorldChange()
    -> void
{
    _Rows.Reset();

    if (_ListView.IsValid())
    { _ListView->RequestListRefresh(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_ResolutionPanel::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid() || NOT _ListView.IsValid())
    { return; }

    const auto* Layer = _ViewModel->TryGet_SelectedLayer();

    auto Existing = TMap<FString, TSharedPtr<FCkIntentDebugger_ResolutionRow>>{};
    for (const auto& Row : _Rows)
    {
        if (Row.IsValid())
        { Existing.Add(Row->TerminalLabel, Row); }
    }

    auto NewRows = TArray<TSharedPtr<FCkIntentDebugger_ResolutionRow>>{};
    auto SetChanged = false;

    if (Layer != nullptr)
    {
        NewRows.Reserve(Layer->Resolutions.Num());

        for (const auto& Source : Layer->Resolutions)
        {
            if (auto* Found = Existing.Find(Source.TerminalLabel))
            {
                **Found = Source;
                NewRows.Add(*Found);
                Existing.Remove(Source.TerminalLabel);
                continue;
            }

            NewRows.Add(MakeShared<FCkIntentDebugger_ResolutionRow>(Source));
            SetChanged = true;
        }
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    _Rows = MoveTemp(NewRows);

    if (SetChanged)
    { _ListView->RequestListRefresh(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_ResolutionPanel::
    OnGenerateRow(
        TSharedPtr<FCkIntentDebugger_ResolutionRow> InRow,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    const auto WeakRow = TWeakPtr<FCkIntentDebugger_ResolutionRow>(InRow);

    return SNew(STableRow<TSharedPtr<FCkIntentDebugger_ResolutionRow>>, InOwnerTable)
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().FillWidth(0.22f).Padding(ck_intent_debugger_resolution::PadS, 0.0f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(CkStyle::Text())
                    .Text_Lambda([WeakRow]()
                    {
                        const auto Row = WeakRow.Pin();
                        return Row.IsValid() ? FText::FromString(Row->TerminalLabel) : FText::GetEmpty();
                    })
            ]

            + SHorizontalBox::Slot().FillWidth(0.18f).Padding(ck_intent_debugger_resolution::PadS, 0.0f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .Text_Lambda([WeakRow]()
                    {
                        const auto Row = WeakRow.Pin();
                        if (NOT Row.IsValid())
                        { return FText::GetEmpty(); }

                        return FText::FromString(ck_intent_debugger_resolution::Get_KeysLabel(Row->ResolvedKeys));
                    })
                    .ColorAndOpacity_Lambda([WeakRow]()
                    {
                        const auto Row = WeakRow.Pin();
                        const auto IsBound = Row.IsValid() && NOT Row->ResolvedKeys.IsEmpty();
                        return FSlateColor{IsBound ? CkStyle::TextDim() : CkStyle::Err()};
                    })
            ]

            + SHorizontalBox::Slot().FillWidth(0.38f).Padding(ck_intent_debugger_resolution::PadS, 0.0f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(CkStyle::Text())
                    .Text_Lambda([WeakRow]()
                    {
                        const auto Row = WeakRow.Pin();
                        if (NOT Row.IsValid())
                        { return FText::GetEmpty(); }

                        auto Names = TArray<FString>{};
                        for (const auto& Name : Row->IntentNames)
                        { Names.Add(Name.ToString()); }

                        return FText::FromString(FString::Join(Names, TEXT(" > ")));
                    })
            ]

            + SHorizontalBox::Slot().FillWidth(0.22f).Padding(ck_intent_debugger_resolution::PadS, 0.0f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .Text_Lambda([WeakRow]()
                    {
                        const auto Row = WeakRow.Pin();
                        return Row.IsValid()
                            ? FText::FromString(ck_intent_debugger_resolution::Get_DeferralLabel(*Row))
                            : FText::GetEmpty();
                    })
                    .ColorAndOpacity_Lambda([WeakRow]()
                    {
                        const auto Row = WeakRow.Pin();
                        const auto IsDeferred = Row.IsValid() && Row->Get_IsDeferred();
                        return FSlateColor{IsDeferred ? CkStyle::Warn() : CkStyle::TextMute()};
                    })
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------
