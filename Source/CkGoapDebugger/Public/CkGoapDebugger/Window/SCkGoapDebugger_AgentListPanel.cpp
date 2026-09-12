#include "CkGoapDebugger/Window/SCkGoapDebugger_AgentListPanel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Focus.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/CkGoapDebugger_Axes.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/SCkUiTable.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

namespace ck_goap_debugger_agentlist_internal
{
    auto RowName(const FString &InName) -> FString
    {
        return InName.IsEmpty() ? TEXT("(unnamed)") : ck::DebugNameClean::Get_CleanName(InName);
    }
    auto TextField(const FString &Value) -> FCkUiFieldValue
    {
        return {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Value)};
    }
    auto ColorField(const FLinearColor &Value) -> FCkUiFieldValue
    {
        return {.Kind = ECkUiFieldKind::Color, .Color = Value};
    }
    auto Schema() -> TArray<FCkUiFieldSchema>
    {
        return {{TEXT("agent-entity-id"), ECkUiFieldKind::Text},
                {TEXT("agent-name"), ECkUiFieldKind::Text},
                {TEXT("agent-name-color"), ECkUiFieldKind::Color},
                {TEXT("agent-planners"), ECkUiFieldKind::Text}};
    }
    auto Tokens() -> FCkUiView::FTokens
    {
        return {{TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS)},
                {TEXT("--space-l"), FString::SanitizeFloat(CkStyle::SpaceL)},
                {TEXT("--goap-agent-list-row-font-size"),
                 FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeBody()))},
                {TEXT("--goap-agent-list-micro-font-size"),
                 FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeMicro()))},
                {TEXT("--goap-agent-list-text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex()},
                {TEXT("--goap-agent-list-text-mute"), TEXT("#") + CkStyle::TextMute().ToFColorSRGB().ToHex()}};
    }
} // namespace ck_goap_debugger_agentlist_internal

auto SCkGoapDebugger_AgentListPanel::Construct(const FArguments &InArgs) -> void
{
    _ViewModel = InArgs._ViewModel;
    _NativeContent =
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
            .Padding(0.0f)
                [SNew(SVerticalBox) +
                 SVerticalBox::Slot().AutoHeight().Padding(FCkGoapDebuggerStyle::Padding_Medium,
                                                           FCkGoapDebuggerStyle::Padding_Small)
                     [SNew(STextBlock)
                          .Text(FText::FromString(TEXT("AGENTS")))
                          .Font_Lambda([] { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); })
                          .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))] +
                 SVerticalBox::Slot().AutoHeight().Padding(
                     FCkGoapDebuggerStyle::Padding_Small, 0.0f, FCkGoapDebuggerStyle::Padding_Small,
                     FCkGoapDebuggerStyle::Padding_Small)[SNew(SCkDebug_DualSearchBar)
                                                              .FilterHintText(FText::FromString(TEXT("Filter agents…")))
                                                              .HighlightHintText(FText::FromString(TEXT("Highlight…")))
                                                              .OnFilterTextChanged_Lambda(
                                                                  [this](const FString &Text)
                                                                  {
                                                                      if (_FilterString != Text)
                                                                      {
                                                                          _FilterString = Text;
                                                                          ApplySearchAndRefresh();
                                                                      }
                                                                  })
                                                              .OnHighlightTextChanged_Lambda(
                                                                  [this](const FString &Text)
                                                                  {
                                                                      if (_HighlightString != Text)
                                                                      {
                                                                          _HighlightString = Text;
                                                                          ApplySearchAndRefresh();
                                                                      }
                                                                  })] +
                 SVerticalBox::Slot().FillHeight(
                     1.0f)[SAssignNew(_ListView, SListView<ItemPtr>)
                               .ListItemsSource(&_Visible)
                               .OnGenerateRow(this, &SCkGoapDebugger_AgentListPanel::OnGenerateRow)
                               .OnSelectionChanged(this, &SCkGoapDebugger_AgentListPanel::OnSelectionChanged)
                               .OnContextMenuOpening(this, &SCkGoapDebugger_AgentListPanel::OnContextMenuOpening)
                               .SelectionMode(ESelectionMode::Single)]];
    ChildSlot[SAssignNew(_ContentHost, SBox)[_NativeContent.ToSharedRef()]];

    const FCkUiLoadResult Result =
        FCkUiCollection::TryCreate(ck_goap_debugger_agentlist_internal::Schema(), _AuthoredCollection);
    _AuthoredProjectionReady = Result.Succeeded;
    if (!Result.Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(Result.Errors, TEXT("\n"));
    }
    TryActivateAuthoredView();
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

auto SCkGoapDebugger_AgentListPanel::MakeUiKey(const FCk_Handle &Handle) const -> FString
{
    const FCk_Entity &Entity = Handle.Get_Entity();
    return FString::Printf(TEXT("goap-agent:%d:%d"), static_cast<int32>(Entity.Get_EntityNumber()),
                           static_cast<int32>(Entity.Get_VersionNumber()));
}
auto SCkGoapDebugger_AgentListPanel::FindItemByUiKey(const FString &Key) const -> ItemPtr
{
    const TWeakPtr<FAgentRow> *Found = _AuthoredRows.Find(Key);
    return Found != nullptr ? Found->Pin() : nullptr;
}
auto SCkGoapDebugger_AgentListPanel::GetAuthoredTable() const -> TSharedPtr<SCkUiTable>
{
    return _AuthoredView.IsValid() ? _AuthoredView->GetTable(TEXT("goap-agent-list-table")) : nullptr;
}

auto SCkGoapDebugger_AgentListPanel::Reset_ForWorldChange() -> void
{
    _SyncFlashItem.Reset();
    _SyncFlashEndSeconds = 0.0;
    _AllItems.Empty();
    _Visible.Empty();
    _AuthoredRows.Empty();
    if (_AuthoredCollection.IsValid())
    {
        _AuthoredCollection->TrySetRecords({});
    }
    if (_ListView.IsValid())
    {
        _ListView->RequestListRefresh();
    }
    if (const TSharedPtr<SCkUiTable> Table = GetAuthoredTable(); Table.IsValid())
    {
        Table->TrySelectKey({}, false);
        Table->TryRefresh();
    }
}

auto SCkGoapDebugger_AgentListPanel::RefreshFromViewModel() -> void
{
    if (!_ViewModel.IsValid())
    {
        return;
    }
    const auto &Roster = _ViewModel->Get_Roster();
    TMap<FCk_Handle, ItemPtr> Existing;
    for (const ItemPtr &Item : _AllItems)
    {
        if (Item.IsValid())
        {
            Existing.Add(Item->Handle, Item);
        }
    }
    TArray<ItemPtr> NewItems;
    NewItems.Reserve(Roster.Num());
    bool SetChanged = false;
    for (const auto &Entry : Roster)
    {
        ItemPtr Item = Existing.FindRef(Entry.EntityHandle);
        if (Item.IsValid())
        {
            Existing.Remove(Entry.EntityHandle);
        }
        else
        {
            Item = MakeShared<FAgentRow>();
            Item->Handle = Entry.EntityHandle;
            SetChanged = true;
        }
        Item->Name = ck_goap_debugger_agentlist_internal::RowName(Entry.DebugName);
        Item->PlannerCount = Entry.Planners.Num();
        NewItems.Add(Item);
    }
    SetChanged |= Existing.Num() > 0;
    _AllItems = MoveTemp(NewItems);
    if (SetChanged)
    {
        ApplySearchAndRefresh();
    }
    else
    {
        PublishAuthoredRows();
    }
    if (_AuthoredView.IsValid())
    {
        _AuthoredView->PollFiles(ck_goap_debugger_agentlist_internal::Tokens());
        if (!_AuthoredView->GetLastResult().Succeeded)
        {
            ActivateNativeFallback();
        }
    }
}

auto SCkGoapDebugger_AgentListPanel::ApplySearchAndRefresh() -> void
{
    _Visible.Reset(_AllItems.Num());
    for (const ItemPtr &Item : _AllItems)
    {
        if (!Item.IsValid())
        {
            continue;
        }
        if (!_FilterString.IsEmpty() && !Item->Name.Contains(_FilterString))
        {
            continue;
        } // legacy: name-only, case-sensitive
        Item->IsHighlightMatch = _HighlightString.IsEmpty() || Item->Name.Contains(_HighlightString);
        _Visible.Add(Item);
    }
    if (_ListView.IsValid())
    {
        _ListView->RequestListRefresh();
    }
    PublishAuthoredRows();
    RestoreSelectionFromViewModel();
}

auto SCkGoapDebugger_AgentListPanel::PublishAuthoredRows() -> void
{
    if (!_AuthoredCollection.IsValid())
    {
        return;
    }
    TArray<FCkUiRecordData> Records;
    TMap<FString, TWeakPtr<FAgentRow>> Rows;
    for (const ItemPtr &Item : _Visible)
    {
        if (!Item.IsValid())
        {
            continue;
        }
        FCkUiRecordData Record;
        Record.Key = MakeUiKey(Item->Handle);
        Record.Fields.Add(TEXT("agent-entity-id"),
                          ck_goap_debugger_agentlist_internal::TextField(ck::Format_UE(TEXT("{}"), Item->Handle)));
        Record.Fields.Add(TEXT("agent-name"), ck_goap_debugger_agentlist_internal::TextField(Item->Name));
        const double RemainingFlash =
            _SyncFlashItem.Pin() == Item ? _SyncFlashEndSeconds - FSlateApplication::Get().GetCurrentTime() : 0.0;
        const FLinearColor NameColor =
            RemainingFlash > 0.0
                ? FMath::Lerp(Item->IsHighlightMatch ? CkStyle::Text() : CkStyle::OverlayOf(CkStyle::TextMute(), 0.55f),
                              CkStyle::Info(), 0.75f * FMath::Clamp(static_cast<float>(RemainingFlash), 0.0f, 1.0f))
                : (Item->IsHighlightMatch ? CkStyle::Text() : CkStyle::OverlayOf(CkStyle::TextMute(), 0.55f));
        Record.Fields.Add(TEXT("agent-name-color"), ck_goap_debugger_agentlist_internal::ColorField(NameColor));
        Record.Fields.Add(TEXT("agent-planners"), ck_goap_debugger_agentlist_internal::TextField(
                                                      FString::Printf(TEXT("%d planner%s"), Item->PlannerCount,
                                                                      Item->PlannerCount == 1 ? TEXT("") : TEXT("s"))));
        Rows.Add(Record.Key, Item);
        Records.Add(MoveTemp(Record));
    }
    const FCkUiLoadResult Result = _AuthoredCollection->TrySetRecords(MoveTemp(Records));
    if (!Result.Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(Result.Errors, TEXT("\n"));
        ActivateNativeFallback();
        return;
    }
    _AuthoredRows = MoveTemp(Rows);
    if (const TSharedPtr<SCkUiTable> Table = GetAuthoredTable(); Table.IsValid())
    {
        Table->TryRefresh();
    }
}

auto SCkGoapDebugger_AgentListPanel::RestoreSelectionFromViewModel() -> void
{
    if (!_ViewModel.IsValid())
    {
        return;
    }
    const FCk_Handle Selected = _ViewModel->GetSelectedEntity();
    if (ck::Is_NOT_Valid(Selected))
    {
        return;
    }
    for (const ItemPtr &Item : _Visible)
    {
        if (!Item.IsValid() || !(Item->Handle == Selected))
        {
            continue;
        }
        if (_ListView.IsValid())
        {
            const auto Current = _ListView->GetSelectedItems();
            if (!(Current.Num() == 1 && Current[0] == Item))
            {
                _ListView->SetSelection(Item, ESelectInfo::Direct);
            }
        }
        if (const TSharedPtr<SCkUiTable> Table = GetAuthoredTable(); Table.IsValid())
        {
            Table->TrySelectKey(MakeUiKey(Item->Handle), false);
        }
        return;
    }
}

auto SCkGoapDebugger_AgentListPanel::OnGenerateRow(ItemPtr Item, const TSharedRef<STableViewBase> &Table)
    -> TSharedRef<ITableRow>
{
    if (!Item.IsValid())
    {
        return SNew(STableRow<ItemPtr>, Table)[SNew(STextBlock).Text(FText::FromString(TEXT("(invalid)")))];
    }
    const TWeakPtr<FAgentRow> WeakItem{Item};
    const TWeakPtr<SCkGoapDebugger_AgentListPanel> WeakPanel{SharedThis(this)};
    return SNew(STableRow<ItemPtr>, Table)
        .Padding(ck_goap_debugger_axes::Live_RowDensity(FMargin{8.0f, 3.0f}))
        .ShowSelection(true)
            [SNew(SBorder)
                 .BorderImage(CkStyle::GetFilledBrush())
                 .Padding(0.0f)
                 .BorderBackgroundColor_Lambda(
                     [WeakPanel, WeakItem]() -> FSlateColor
                     {
                         const auto Panel = WeakPanel.Pin();
                         const auto Pinned = WeakItem.Pin();
                         if (!Panel.IsValid() || !Pinned.IsValid() || Panel->_SyncFlashItem.Pin() != Pinned)
                         {
                             return FSlateColor(FLinearColor::Transparent);
                         }
                         const double Remaining =
                             Panel->_SyncFlashEndSeconds - FSlateApplication::Get().GetCurrentTime();
                         FLinearColor Tint = CkStyle::Info();
                         Tint.A =
                             Remaining > 0.0 ? 0.35f * FMath::Clamp(static_cast<float>(Remaining), 0.0f, 1.0f) : 0.0f;
                         return FSlateColor(Tint);
                     })[SNew(SHorizontalBox) +
                        SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(0, 0, 6, 0)[SNew(SCkDebug_EntityRef)
                                                     .Entity_Lambda(
                                                         [WeakItem]
                                                         {
                                                             const auto Pinned = WeakItem.Pin();
                                                             return Pinned.IsValid() ? Pinned->Handle : FCk_Handle{};
                                                         })] +
                        SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                            [SNew(STextBlock)
                                 .Text_Lambda(
                                     [WeakItem]
                                     {
                                         const auto Pinned = WeakItem.Pin();
                                         return FText::FromString(Pinned.IsValid() ? Pinned->Name : FString{});
                                     })
                                 .Font_Lambda(
                                     [] { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); })
                                 .ColorAndOpacity_Lambda(
                                     [WeakItem]
                                     {
                                         const auto Pinned = WeakItem.Pin();
                                         return FSlateColor(Pinned.IsValid() && !Pinned->IsHighlightMatch
                                                                ? CkStyle::OverlayOf(CkStyle::TextMute(), 0.55f)
                                                                : CkStyle::Text());
                                     })] +
                        SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(6, 0, 0, 0)
                                [SNew(STextBlock)
                                     .Text_Lambda(
                                         [WeakItem]
                                         {
                                             const auto Pinned = WeakItem.Pin();
                                             return Pinned.IsValid()
                                                        ? FText::FromString(FString::Printf(
                                                              TEXT("%d planner%s"), Pinned->PlannerCount,
                                                              Pinned->PlannerCount == 1 ? TEXT("") : TEXT("s")))
                                                        : FText::GetEmpty();
                                         })
                                     .Font_Lambda(
                                         [] { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); })
                                     .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))]]];
}

auto SCkGoapDebugger_AgentListPanel::OnSelectionChanged(ItemPtr Item, ESelectInfo::Type Type) -> void
{
    if (Type == ESelectInfo::Direct || !_ViewModel.IsValid() || !Item.IsValid())
    {
        return;
    }
    _ViewModel->SetSelectedEntity(Item->Handle);
    ck::DebugSelectionSync::Broadcast(Item->Handle, TEXT("GoapDebugger"));
}
auto SCkGoapDebugger_AgentListPanel::OnAuthoredSelectionChanged(TOptional<FString> Key, ESelectInfo::Type Type) -> void
{
    if (Type == ESelectInfo::Direct || !Key.IsSet() || !_ViewModel.IsValid())
    {
        return;
    }
    if (const ItemPtr Item = FindItemByUiKey(Key.GetValue()); Item.IsValid())
    {
        _ViewModel->SetSelectedEntity(Item->Handle);
        ck::DebugSelectionSync::Broadcast(Item->Handle, TEXT("GoapDebugger"));
    }
}

auto SCkGoapDebugger_AgentListPanel::OnContextMenuOpening() -> TSharedPtr<SWidget>
{
    ItemPtr Item;
    if (const auto Table = GetAuthoredTable(); Table.IsValid())
    {
        // Right-click context menus keep the exact row key separately. Do not select it here:
        // opening a menu on a non-selected row must not alter the user's selection.
        if (const TOptional<FString> ContextMenuKey = Table->GetContextMenuKey(); ContextMenuKey.IsSet())
        {
            Item = FindItemByUiKey(ContextMenuKey.GetValue());
        }
        // Keyboard invocation has no pointer target, so it intentionally uses the selected row.
        if (!Item.IsValid())
        {
            if (const TOptional<FString> SelectedKey = Table->GetSelectedKey(); SelectedKey.IsSet())
            {
                Item = FindItemByUiKey(SelectedKey.GetValue());
            }
        }
    }
    if (!Item.IsValid() && _ListView.IsValid())
    {
        const auto Selected = _ListView->GetSelectedItems();
        if (Selected.Num() > 0)
        {
            Item = Selected[0];
        }
    }
    if (!Item.IsValid())
    {
        return nullptr;
    }
    FMenuBuilder Menu{true, nullptr};
    ck::DebugCopyMenu::AddCopyEntry(
        Menu, NSLOCTEXT("CkGoapAgentList", "CopyName", "Copy Name"),
        NSLOCTEXT("CkGoapAgentList", "CopyNameTip", "Copy the agent's display name to the clipboard."), Item->Name);
    ck::DebugCopyMenu::AddCopyEntry(
        Menu, NSLOCTEXT("CkGoapAgentList", "CopyEntity", "Copy Entity"),
        NSLOCTEXT("CkGoapAgentList", "CopyEntityTip",
                  "Copy the full entity handle (ID|Version + debug name) to the clipboard."),
        ck::Format_UE(TEXT("{}"), Item->Handle));
    if (ck::DebugFocus::Get_CanFocus() && ck::IsValid(Item->Handle))
    {
        const FCk_Handle Focus = Item->Handle;
        Menu.AddMenuEntry(NSLOCTEXT("CkGoapAgentList", "Focus", "Focus in Viewport (F)"),
                           NSLOCTEXT("CkGoapAgentList", "FocusTip",
                                     "Glide the editor camera to frame this agent (auto-ejects while possessed)."),
                           FSlateIcon(),
                           FUIAction(FExecuteAction::CreateLambda(
                               [Focus]()
                               {
                                   ck::DebugFocus::Focus_Entity(Focus);
                               })));
    }
    return Menu.MakeWidget();
}
auto SCkGoapDebugger_AgentListPanel::OnAuthoredContextMenuOpening() -> TSharedPtr<SWidget>
{
    return OnContextMenuOpening();
}
auto SCkGoapDebugger_AgentListPanel::OnAuthoredEntityNavigate(const FString &Key) -> void
{
    if (const ItemPtr Item = FindItemByUiKey(Key); Item.IsValid() && ck::IsValid(Item->Handle))
    {
        ck::DebugNav::Goto_Entity(Item->Handle);
    }
}

auto SCkGoapDebugger_AgentListPanel::OnKeyDown(const FGeometry &Geometry, const FKeyEvent &Event) -> FReply
{
    if (Event.GetKey() == EKeys::F && _ViewModel.IsValid())
    {
        const FCk_Handle Selected = _ViewModel->GetSelectedEntity();
        if (ck::IsValid(Selected) && ck::DebugFocus::Focus_Entity(Selected))
        {
            return FReply::Handled();
        }
    }
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

auto SCkGoapDebugger_AgentListPanel::OnGlobalSelectionSync(const FCk_Handle &Selected, FName Source) -> void
{
    if (Source == TEXT("GoapDebugger") || !_ViewModel.IsValid())
    {
        return;
    }
    for (const ItemPtr &Item : _AllItems)
    {
        if (!Item.IsValid() || !ck::DebugSelectionSync::Is_SameLineage(Item->Handle, Selected))
        {
            continue;
        }
        const ck::DebugSelectionSync::FApplyGuard Guard;
        _ViewModel->SetSelectedEntity(Item->Handle);
        if (_ListView.IsValid())
        {
            _ListView->SetSelection(Item, ESelectInfo::Direct);
            _ListView->RequestScrollIntoView(Item);
        }
        if (const auto Table = GetAuthoredTable(); Table.IsValid())
        {
            const FString Key = MakeUiKey(Item->Handle);
            Table->TrySelectKey(Key, false);
            if (const auto List = Table->GetList(); List.IsValid())
            {
                if (const SCkUiTable::FRecord Record =
                        _AuthoredCollection.IsValid() ? _AuthoredCollection->FindRecord(Key) : nullptr;
                    Record.IsValid())
                {
                    List->RequestScrollIntoView(Record);
                }
            }
        }
        _SyncFlashItem = Item;
        _SyncFlashEndSeconds = FSlateApplication::Get().GetCurrentTime() + 1.2;
        // The native fallback has an attribute-bound row tint; the authored table repaints its
        // retained name cell on the gated refreshes during the same 1.2s window.
        PublishAuthoredRows();
        return;
    }
}

auto SCkGoapDebugger_AgentListPanel::TryActivateAuthoredView() -> void
{
    if (_AuthoredView.IsValid() || !_ContentHost.IsValid() || !_AuthoredProjectionReady ||
        !_AuthoredCollection.IsValid())
    {
        return;
    }
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (!RegistryResult.Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(RegistryResult.Errors, TEXT("\n"));
        ActivateNativeFallback();
        return;
    }
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid())
    {
        _AuthoredLoadFailure = TEXT("CkDebugger plugin is unavailable.");
        ActivateNativeFallback();
        return;
    }
    const TWeakPtr<SCkGoapDebugger_AgentListPanel> WeakPanel{SharedThis(this)};
    FCkUiView::FDataBindings Data;
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda(
        [WeakPanel]()
        {
            return WeakPanel.IsValid();
        });
    Data.Text.Add(TEXT("goap-agent-list-filter"),
                  TAttribute<FText>::CreateLambda(
                      [WeakPanel]
                      {
                          const auto Panel = WeakPanel.Pin();
                          return Panel.IsValid() ? FText::FromString(Panel->_FilterString) : FText::GetEmpty();
                      }));
    Data.Text.Add(TEXT("goap-agent-list-highlight"),
                  TAttribute<FText>::CreateLambda(
                      [WeakPanel]
                      {
                          const auto Panel = WeakPanel.Pin();
                          return Panel.IsValid() ? FText::FromString(Panel->_HighlightString) : FText::GetEmpty();
                      }));
    Data.Text.Add(TEXT("goap-agent-list-empty"),
                  TAttribute<FText>::CreateLambda(
                      [WeakPanel]
                      {
                          const auto Panel = WeakPanel.Pin();
                          return !Panel.IsValid() || !Panel->_Visible.IsEmpty()
                                     ? FText::GetEmpty()
                                     : FText::FromString(Panel->_AllItems.IsEmpty()
                                                             ? TEXT("No GOAP agents in the active world.")
                                                             : TEXT("No agents match the current filter."));
                      }));
    Data.Visibility.Add(TEXT("goap-agent-list-empty-visible"), TAttribute<bool>::CreateLambda(
                                                                   [WeakPanel]
                                                                   {
                                                                       const auto Panel = WeakPanel.Pin();
                                                                       return Panel.IsValid() &&
                                                                              Panel->_Visible.IsEmpty();
                                                                   }));
    Data.TextChanged.Add(TEXT("goap-agent-list-filter"),
                         FOnTextChanged::CreateLambda(
                             [WeakPanel](const FText &Text)
                             {
                                 if (const auto Panel = WeakPanel.Pin();
                                     Panel.IsValid() && Panel->_FilterString != Text.ToString())
                                 {
                                     Panel->_FilterString = Text.ToString();
                                     Panel->ApplySearchAndRefresh();
                                 }
                             }));
    Data.TextChanged.Add(TEXT("goap-agent-list-highlight"),
                         FOnTextChanged::CreateLambda(
                             [WeakPanel](const FText &Text)
                             {
                                 if (const auto Panel = WeakPanel.Pin();
                                     Panel.IsValid() && Panel->_HighlightString != Text.ToString())
                                 {
                                     Panel->_HighlightString = Text.ToString();
                                     Panel->ApplySearchAndRefresh();
                                 }
                             }));
    Data.Collections.Add(TEXT("goap-agent-list-records"), _AuthoredCollection);
    Data.TableSelectionChanged.Add(
        TEXT("select-goap-agent"),
        FOnCkUiTableSelectionChanged::CreateSP(this, &SCkGoapDebugger_AgentListPanel::OnAuthoredSelectionChanged));
    Data.TableContextMenus.Add(
        TEXT("goap-agent-context"),
        FOnContextMenuOpening::CreateSP(this, &SCkGoapDebugger_AgentListPanel::OnAuthoredContextMenuOpening));
    Data.ItemActions.Add(TEXT("goap-agent-navigate"), FCkUiOnItemAction::CreateLambda(
                                                          [WeakPanel](const FString &Key)
                                                          {
                                                              if (const auto Panel = WeakPanel.Pin())
                                                              {
                                                                  Panel->OnAuthoredEntityNavigate(Key);
                                                              }
                                                          }));
    const TSharedRef<FCkUiView> Candidate =
        FCkUiView::Create({}, {}, ck_goap_debugger_agentlist_internal::Tokens(),
                          CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = Candidate->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Directory, TEXT("GoapDebuggerAgentList.ui.html")),
                        FPaths::Combine(Directory, TEXT("GoapDebuggerAgentList.ui.css")));
    Candidate->PollFiles();
    if (!Candidate->GetLastResult().Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        ActivateNativeFallback();
        return;
    }
    _AuthoredView = Candidate;
    _AuthoredLoadFailure.Reset();
    _ContentHost->SetContent(Main);
}
auto SCkGoapDebugger_AgentListPanel::ActivateNativeFallback() -> void
{
    _AuthoredView.Reset();
    if (_ContentHost.IsValid() && _NativeContent.IsValid())
    {
        _ContentHost->SetContent(_NativeContent.ToSharedRef());
    }
}
