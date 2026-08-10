#include "CkGoapDebugger/Window/SCkGoapDebugger_AgentListPanel.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkDebuggerCommon/Navigation/CkDebug_Focus.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

// ====================================================================================================================

namespace ck_goap_debugger_agentlist_internal
{
    auto RowName(const FString& InDebugName) -> FString
    {
        if (InDebugName.IsEmpty())
        { return FString(TEXT("(unnamed)")); }
        // Shared cleanup + settings-driven strip patterns (Project Settings →
        // Ck → Debugger → Entity Name Strip Patterns).
        return ck::DebugNameClean::Get_CleanName(InDebugName);
    }
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentListPanel::
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

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("AGENTS")))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                        ]

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f, FCkGoapDebuggerStyle::Padding_Small, FCkGoapDebuggerStyle::Padding_Small))
                        [
                            SNew(SCkDebug_DualSearchBar)
                                .FilterHintText(FText::FromString(TEXT("Filter agents…")))
                                .HighlightHintText(FText::FromString(TEXT("Highlight…")))
                                .OnFilterTextChanged_Lambda([this](const FString& InText)
                                {
                                    if (_FilterString == InText) { return; }
                                    _FilterString = InText;
                                    ApplySearchAndRefresh();
                                })
                                .OnHighlightTextChanged_Lambda([this](const FString& InText)
                                {
                                    if (_HighlightString == InText) { return; }
                                    _HighlightString = InText;
                                    ApplySearchAndRefresh();
                                })
                        ]

                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SAssignNew(_ListView, SListView<ItemPtr>)
                                .ListItemsSource(&_Visible)
                                .OnGenerateRow(this, &SCkGoapDebugger_AgentListPanel::OnGenerateRow)
                                .OnSelectionChanged(this, &SCkGoapDebugger_AgentListPanel::OnSelectionChanged)
                                .OnContextMenuOpening(this, &SCkGoapDebugger_AgentListPanel::OnContextMenuOpening)
                                .SelectionMode(ESelectionMode::Single)
                        ]
            ]
    ];

    _OnSelectionSyncHandle = ck::DebugSelectionSync::Get_OnSelection().AddSP(
        SharedThis(this), &SCkGoapDebugger_AgentListPanel::OnGlobalSelectionSync);
}

SCkGoapDebugger_AgentListPanel::~SCkGoapDebugger_AgentListPanel()
{
    if (_OnSelectionSyncHandle.IsValid())
    {
        ck::DebugSelectionSync::Get_OnSelection().Remove(_OnSelectionSyncHandle);
    }
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentListPanel::
    Reset_ForWorldChange()
    -> void
{
    _AllItems.Empty();
    _Visible.Empty();
    if (_ListView.IsValid())
    { _ListView->RequestListRefresh(); }
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentListPanel::
    RefreshFromViewModel()
    -> void
{
    using namespace ck_goap_debugger_agentlist_internal;

    if (NOT _ViewModel.IsValid()) { return; }

    const auto& Roster = _ViewModel->Get_Roster();

    // Stable-pointer protocol: reuse the existing row for a matching handle,
    // update its fields in place; allocate only for genuinely new entities.
    auto Existing = TMap<FCk_Handle, ItemPtr>{};
    for (const auto& Item : _AllItems)
    {
        if (Item.IsValid()) { Existing.Add(Item->Handle, Item); }
    }

    auto NewItems = TArray<ItemPtr>{};
    NewItems.Reserve(Roster.Num());
    auto SetChanged = false;

    for (const auto& Entry : Roster)
    {
        auto Item = ItemPtr{};
        if (auto* Found = Existing.Find(Entry.EntityHandle))
        {
            Item = *Found;
            Existing.Remove(Entry.EntityHandle);
        }
        else
        {
            Item = MakeShared<FAgentRow>();
            Item->Handle = Entry.EntityHandle;
            SetChanged = true;
        }
        Item->Name = RowName(Entry.DebugName);
        Item->PlannerCount = Entry.Planners.Num();
        NewItems.Add(MoveTemp(Item));
    }
    if (Existing.Num() > 0) { SetChanged = true; }   // vanished entities

    _AllItems = MoveTemp(NewItems);

    // Re-stamp selection ONLY when the set rebuilt. Stamping every tick kills
    // in-flight clicks: SListView highlights on mouse-down but commits the
    // selection on mouse-up, so a per-tick restore between the two snaps the
    // highlight back to the old row before the click ever lands (same trap
    // documented in the Crowd agent list's OnAgentListChanged).
    if (SetChanged)
    {
        ApplySearchAndRefresh();
    }
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentListPanel::
    ApplySearchAndRefresh()
    -> void
{
    _Visible.Reset(_AllItems.Num());
    for (const auto& Item : _AllItems)
    {
        if (NOT Item.IsValid()) { continue; }
        if (NOT _FilterString.IsEmpty() && NOT Item->Name.Contains(_FilterString))
        { continue; }
        Item->IsHighlightMatch = _HighlightString.IsEmpty() || Item->Name.Contains(_HighlightString);
        _Visible.Add(Item);
    }

    if (NOT _ListView.IsValid()) { return; }
    _ListView->RequestListRefresh();

    RestoreSelectionFromViewModel();
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentListPanel::
    RestoreSelectionFromViewModel()
    -> void
{
    if (NOT _ListView.IsValid() || NOT _ViewModel.IsValid()) { return; }

    const auto Selected = _ViewModel->GetSelectedEntity();
    if (ck::Is_NOT_Valid(Selected)) { return; }

    for (const auto& Item : _Visible)
    {
        if (NOT (Item->Handle == Selected)) { continue; }

        // Compare first so the programmatic re-stamp doesn't fire a spurious
        // OnSelectionChanged every tick.
        const auto Cur = _ListView->GetSelectedItems();
        if (NOT (Cur.Num() == 1 && Cur[0] == Item))
        { _ListView->SetSelection(Item, ESelectInfo::Direct); }
        break;
    }
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentListPanel::
    OnGenerateRow(
        ItemPtr InItem,
        const TSharedRef<STableViewBase>& InTable)
    -> TSharedRef<ITableRow>
{
    if (NOT InItem.IsValid())
    {
        return SNew(STableRow<ItemPtr>, InTable)
            [ SNew(STextBlock).Text(FText::FromString(TEXT("(invalid)"))) ];
    }

    const auto WeakItem = TWeakPtr<FAgentRow>(InItem);
    const auto WeakPanel = TWeakPtr<SCkGoapDebugger_AgentListPanel>(SharedThis(this));

    return SNew(STableRow<ItemPtr>, InTable)
        .Padding(ck_goap_debugger_axes::Live_RowDensity(FMargin{8.0f, 3.0f}))
        .ShowSelection(true)
        [
            SNew(SBorder)
                .BorderImage(CkStyle::GetFilledBrush())
                .Padding(FMargin(0.0f))
                .BorderBackgroundColor_Lambda([WeakPanel, WeakItem]() -> FSlateColor
                {
                    const auto Panel = WeakPanel.Pin();
                    const auto Item  = WeakItem.Pin();
                    if (NOT Panel.IsValid() || NOT Item.IsValid() || Panel->_SyncFlashItem.Pin() != Item)
                    { return FSlateColor(FLinearColor::Transparent); }

                    const auto Remaining = Panel->_SyncFlashEndSeconds - FSlateApplication::Get().GetCurrentTime();
                    if (Remaining <= 0.0)
                    { return FSlateColor(FLinearColor::Transparent); }

                    auto Tint = CkStyle::Info();
                    Tint.A = 0.35f * FMath::Clamp(static_cast<float>(Remaining), 0.0f, 1.0f);
                    return FSlateColor(Tint);
                })
                [
            SNew(SHorizontalBox)

                // Canonical entity identity — click navigates to the ECS debugger.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                    [
                        SNew(SCkDebug_EntityRef)
                            .Entity_Lambda([WeakItem]() -> FCk_Handle
                            {
                                const auto Pinned = WeakItem.Pin();
                                return Pinned.IsValid() ? Pinned->Handle : FCk_Handle{};
                            })
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([WeakItem]() -> FText
                            {
                                const auto Pinned = WeakItem.Pin();
                                return FText::FromString(Pinned.IsValid() ? Pinned->Name : FString{});
                            })
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); })
                            .ColorAndOpacity_Lambda([WeakItem]() -> FSlateColor
                            {
                                const auto Pinned = WeakItem.Pin();
                                const auto Dimmed = Pinned.IsValid() && NOT Pinned->IsHighlightMatch;
                                return FSlateColor(Dimmed
                                    ? CkStyle::OverlayOf(CkStyle::TextMute(), 0.55f)
                                    : CkStyle::Text());
                            })
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(6.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([WeakItem]() -> FText
                            {
                                const auto Pinned = WeakItem.Pin();
                                if (NOT Pinned.IsValid()) { return FText::GetEmpty(); }
                                return FText::FromString(FString::Printf(TEXT("%d planner%s"),
                                    Pinned->PlannerCount, Pinned->PlannerCount == 1 ? TEXT("") : TEXT("s")));
                            })
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                    ]
                ]
        ];
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentListPanel::
    OnSelectionChanged(
        ItemPtr InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }
    if (NOT InItem.IsValid())     { return; }   // ignore deselects — GOAP always shows something

    _ViewModel->SetSelectedEntity(InItem->Handle);

    // User-driven selection → sync the other debuggers. Direct = programmatic
    // (selection restore / sync apply); never re-broadcast those.
    if (InSelectInfo != ESelectInfo::Direct)
    {
        ck::DebugSelectionSync::Broadcast(InItem->Handle, TEXT("GoapDebugger"));
    }
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentListPanel::
    OnContextMenuOpening()
    -> TSharedPtr<SWidget>
{
    if (NOT _ListView.IsValid()) { return nullptr; }

    const auto Selected = _ListView->GetSelectedItems();
    if (Selected.Num() == 0 || NOT Selected[0].IsValid()) { return nullptr; }

    const auto& Item = *Selected[0];

    constexpr auto CloseAfterSelection = true;
    auto MenuBuilder = FMenuBuilder{CloseAfterSelection, nullptr};

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        NSLOCTEXT("CkGoapAgentList", "CopyName", "Copy Name"),
        NSLOCTEXT("CkGoapAgentList", "CopyNameTip", "Copy the agent's display name to the clipboard."),
        Item.Name);

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        NSLOCTEXT("CkGoapAgentList", "CopyEntity", "Copy Entity"),
        NSLOCTEXT("CkGoapAgentList", "CopyEntityTip", "Copy the full entity handle (ID|Version + debug name) to the clipboard."),
        ck::Format_UE(TEXT("{}"), Item.Handle));

    if (ck::DebugFocus::Get_CanFocus() && ck::IsValid(Item.Handle))
    {
        const auto FocusTarget = Item.Handle;
        MenuBuilder.AddMenuEntry(
            NSLOCTEXT("CkGoapAgentList", "FocusInViewport", "Focus in Viewport (F)"),
            NSLOCTEXT("CkGoapAgentList", "FocusInViewportTip", "Glide the editor camera to frame this agent (auto-ejects while possessed)."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([FocusTarget]()
            {
                ck::DebugFocus::Focus_Entity(FocusTarget);
            })));
    }

    return MenuBuilder.MakeWidget();
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentListPanel::
    OnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent)
    -> FReply
{
    if (InKeyEvent.GetKey() == EKeys::F && _ViewModel.IsValid())
    {
        const auto Selected = _ViewModel->GetSelectedEntity();
        if (ck::IsValid(Selected) && ck::DebugFocus::Focus_Entity(Selected))
        { return FReply::Handled(); }
    }

    return SCompoundWidget::OnKeyDown(InGeometry, InKeyEvent);
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentListPanel::
    OnGlobalSelectionSync(
        const FCk_Handle& InSelected,
        FName InSource)
    -> void
{
    if (InSource == TEXT("GoapDebugger")) { return; }
    if (NOT _ViewModel.IsValid())         { return; }

    for (const auto& Item : _AllItems)
    {
        if (NOT Item.IsValid()) { continue; }
        if (NOT ck::DebugSelectionSync::Is_SameLineage(Item->Handle, InSelected)) { continue; }

        const auto Guard = ck::DebugSelectionSync::FApplyGuard{};
        _ViewModel->SetSelectedEntity(Item->Handle);
        if (_ListView.IsValid())
        {
            _ListView->SetSelection(Item, ESelectInfo::Direct);
            _ListView->RequestScrollIntoView(Item);
        }

        // Make the resolve visible — fading row flash.
        _SyncFlashItem       = Item;
        _SyncFlashEndSeconds = FSlateApplication::Get().GetCurrentTime() + 1.2;
        return;
    }
    // No lineage match — broadcast entity has no GOAP root; keep current selection.
}

// ====================================================================================================================
