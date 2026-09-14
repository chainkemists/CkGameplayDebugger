#include "CkAStarDebugger/Window/SCkAStarDebugger_SearchHistory.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_HistoryRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_RailContainer.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

namespace ck_astar_debugger_search_history
{
    auto TextField(const FString& InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)}; }

    auto ColorField(const FLinearColor& InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Color, .Color = InValue}; }

    auto MetaText(const FCkAStarDebugger_HistoryEntry& InEntry) -> FString
    {
        if (InEntry.FinalStatus == ECk_AStarSearchStatus::Complete)
        {
            return ck::Format_UE(TEXT("{:.1f} cost \u00B7 {} iter \u00B7 {}us"),
                InEntry.TotalCost, InEntry.TotalIterations, InEntry.TotalTimeMicroseconds);
        }
        if (InEntry.FinalStatus == ECk_AStarSearchStatus::Failed)
        {
            return ck::Format_UE(TEXT("no path \u00B7 {} iter \u00B7 {}us"),
                InEntry.TotalIterations, InEntry.TotalTimeMicroseconds);
        }
        return ck::Format_UE(TEXT("{} iter \u00B7 {}us"), InEntry.TotalIterations, InEntry.TotalTimeMicroseconds);
    }

    auto CopyText(const FCkAStarDebugger_HistoryEntry& InEntry) -> FString
    {
        return ck::Format_UE(
            TEXT("A* search [{}]\n")
            TEXT("  frame:      {}\n")
            TEXT("  iterations: {}\n")
            TEXT("  time:       {}us\n")
            TEXT("  cost:       {}\n")
            TEXT("  path len:   {}"),
            CkAStarDebugger::GetStatusString(InEntry.FinalStatus), InEntry.FrameNumber,
            InEntry.TotalIterations, InEntry.TotalTimeMicroseconds, InEntry.TotalCost, InEntry.PathLength);
    }

    auto Tokens() -> FCkUiView::FTokens
    {
        return {
            {TEXT("--astar-history-body-font"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeBody()))},
            {TEXT("--astar-history-small-font"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeMicro()))},
            {TEXT("--astar-history-heading-font"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::PaneHeadingFontSize()))},
            {TEXT("--astar-history-text"), TEXT("#") + CkStyle::TextDim().ToFColorSRGB().ToHex()},
            {TEXT("--astar-history-muted"), TEXT("#") + CkStyle::TextMute().ToFColorSRGB().ToHex()},
            {TEXT("--astar-history-heading"), TEXT("#") + CkStyle::PaneHeadingColor().ToFColorSRGB().ToHex()},
            {TEXT("--astar-history-background"), TEXT("#") + CkStyle::Bg1().ToFColorSRGB().ToHex()},
            {TEXT("--astar-history-row-padding"), FString::SanitizeFloat(ck::debug_axes::Apply_RowDensity(FMargin(CkStyle::SpaceM)).Top)}
        };
    }
}

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

    SAssignNew(_Rail, SCkDebug_RailContainer)
        .Title(FText::FromString(TEXT("Search History")))
        .CountText(_CountText);
    ChildSlot[SAssignNew(_ContentHost, SBox)[_Rail.ToSharedRef()]];

    const auto SchemaResult = FCkUiCollection::TryCreate({
        {TEXT("status"), ECkUiFieldKind::Text}, {TEXT("frame"), ECkUiFieldKind::Text},
        {TEXT("meta"), ECkUiFieldKind::Text}, {TEXT("copy"), ECkUiFieldKind::Text},
        {TEXT("tone"), ECkUiFieldKind::Color}, {TEXT("background"), ECkUiFieldKind::Color}}, _AuthoredCollection);
    if (NOT SchemaResult.Succeeded)
    { _AuthoredLoadFailure = FString::Join(SchemaResult.Errors, TEXT("\n")); }
    if (_ViewModel.IsValid())
    {
        _SelectionChangedHandle = _ViewModel->OnSelectedEntityChanged.AddSP(
            this, &SCkAStarDebugger_SearchHistory::OnSelectedEntityChanged);
    }
    RefreshFromViewModel();
    TryActivateAuthoredView();
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

    if (_Released)
    { return; }

    RefreshFromViewModel();
    if (NOT _AuthoredView.IsValid() || InCurrentTime < _NextAuthoredPollSeconds)
    { return; }
    _NextAuthoredPollSeconds = InCurrentTime + 0.5;
    _AuthoredView->PollFiles(ck_astar_debugger_search_history::Tokens());
    if (_AuthoredView->GetLastResult().Succeeded)
    {
        _AuthoredLoadFailure.Reset();
        if (NOT _AuthoredMounted)
        {
            _ContentHost->SetContent(_AuthoredView->GetRegion(TEXT("main")));
            _AuthoredMounted = true;
        }
    }
    else
    { _AuthoredLoadFailure = FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n")); }
}

auto
    SCkAStarDebugger_SearchHistory::
    Rebuild_ForStyleChange()
    -> void
{
    _LastProjectionSignature.Reset();
    RefreshFromViewModel();
    if (_AuthoredView.IsValid())
    { _AuthoredView->PollFiles(ck_astar_debugger_search_history::Tokens()); }
}

SCkAStarDebugger_SearchHistory::~SCkAStarDebugger_SearchHistory()
{ Release_AuthoredView(); }

auto
    SCkAStarDebugger_SearchHistory::
    Release_AuthoredView()
    -> void
{
    _Released = true;
    _ProjectionCurrent = false;
    if (_ViewModel.IsValid() && _SelectionChangedHandle.IsValid())
    { _ViewModel->OnSelectedEntityChanged.Remove(_SelectionChangedHandle); }
    _SelectionChangedHandle.Reset();
    _AuthoredView.Reset();
    _AuthoredCollection.Reset();
    _ViewModel.Reset();
    if (_ContentHost.IsValid())
    { _ContentHost->SetContent(SNullWidget::NullWidget); }
    _Rail.Reset();
    _AuthoredMounted = false;
}

auto
    SCkAStarDebugger_SearchHistory::
    OnSelectedEntityChanged(FCk_Handle InEntity)
    -> void
{
    ++_SelectionRevision;
    RefreshFromViewModel();
}

auto
    SCkAStarDebugger_SearchHistory::
    RefreshFromViewModel()
    -> void
{
    using namespace ck_astar_debugger_search_history;
    if (_Released)
    { return; }

    const auto Selected = _ViewModel.IsValid() && _ViewModel->Has_SelectedEntity()
        ? _ViewModel->Get_SelectedEntityHandle() : FCk_Handle{};
    const auto HasSelection = ck::IsValid(Selected);
    const auto* History = HasSelection ? _ViewModel->Get_SearchHistory(Selected) : nullptr;
    const auto Count = History != nullptr ? History->Num() : 0;
    const auto SelectionKey = HasSelection
        ? FString::Printf(TEXT("astar-history:%llu:%u:%d:%d"), static_cast<unsigned long long>(_SelectionRevision),
            GetTypeHash(Selected), static_cast<int32>(Selected.Get_Entity().Get_EntityNumber()),
            static_cast<int32>(Selected.Get_Entity().Get_VersionNumber()))
        : FString{TEXT("astar-history:none")};
    auto Signature = SelectionKey;
    for (auto Index = Count - 1; Index >= 0; --Index)
    { Signature += TEXT("\n") + CopyText((*History)[Index]); }
    if (Signature == _LastProjectionSignature)
    { return; }

    _ProjectionCurrent = false;
    auto Records = TArray<FCkUiRecordData>{};
    for (auto Index = Count - 1; Index >= 0; --Index)
    {
        const auto& Entry = (*History)[Index];
        auto Record = FCkUiRecordData{};
        Record.Key = SelectionKey + TEXT(":") + FString::FromInt(Index);
        Record.Fields.Add(TEXT("status"), TextField(CkAStarDebugger::GetStatusString(Entry.FinalStatus)));
        Record.Fields.Add(TEXT("frame"), TextField(ck::Format_UE(TEXT("F#{}"), Entry.FrameNumber)));
        Record.Fields.Add(TEXT("meta"), TextField(MetaText(Entry)));
        Record.Fields.Add(TEXT("copy"), TextField(CopyText(Entry)));
        Record.Fields.Add(TEXT("tone"), ColorField(CkAStarDebugger::GetStatusColor(Entry.FinalStatus)));
        Record.Fields.Add(TEXT("background"), ColorField(CkStyle::GetToneDimColor(CkAStarDebugger::GetStatusTone(Entry.FinalStatus))));
        Records.Add(MoveTemp(Record));
    }

    if (_AuthoredCollection.IsValid())
    {
        const auto Result = _AuthoredCollection->TrySetRecords(MoveTemp(Records));
        if (NOT Result.Succeeded)
        {
            _AuthoredLoadFailure = FString::Join(Result.Errors, TEXT("\n"));
            return;
        }
    }
    _LastProjectionSignature = MoveTemp(Signature);
    _CountText = FText::FromString(FString::FromInt(Count));
    _EmptyText = FText::FromString(HasSelection ? TEXT("No search history yet") : TEXT("Select a search entity to see its history"));
    _ProjectionCurrent = true;
    if (NOT _AuthoredMounted)
    { RebuildList(); }
}

auto
    SCkAStarDebugger_SearchHistory::
    TryActivateAuthoredView()
    -> void
{
    if (_Released || _AuthoredView.IsValid() || NOT _AuthoredCollection.IsValid())
    { return; }
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const auto Result = FCkDebug_UiRegistry::TryCreate(Registry);
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Result.Succeeded || NOT Plugin.IsValid())
    {
        _AuthoredLoadFailure = Result.Succeeded ? TEXT("CkDebugger plugin is unavailable.") : FString::Join(Result.Errors, TEXT("\n"));
        return;
    }
    const auto WeakPanel = TWeakPtr<SCkAStarDebugger_SearchHistory>{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakPanel]()
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() && NOT Panel->_Released && Panel->_ProjectionCurrent;
    });
    Data.Text.Add(TEXT("astar-history-count"), TAttribute<FText>::CreateLambda([WeakPanel]()
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() ? Panel->_CountText : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("astar-history-empty"), TAttribute<FText>::CreateLambda([WeakPanel]()
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() ? Panel->_EmptyText : FText::GetEmpty();
    }));
    Data.Visibility.Add(TEXT("astar-history-is-empty"), TAttribute<bool>::CreateLambda([WeakPanel]()
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->_AuthoredCollection.IsValid()
            && Panel->_AuthoredCollection->GetRecords().IsEmpty();
    }));
    Data.Collections.Add(TEXT("astar-history-records"), _AuthoredCollection);
    Data.ItemActions.Add(TEXT("astar-history-copy"), FCkUiOnItemAction::CreateLambda([WeakPanel](const FString& InKey)
    {
        const auto Panel = WeakPanel.Pin();
        if (Panel.IsValid())
        { Panel->CopyAuthoredEntry(InKey); }
    }));
    _AuthoredView = FCkUiView::Create({}, {}, ck_astar_debugger_search_history::Tokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const auto Main = _AuthoredView->GetRegion(TEXT("main"));
    const auto Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _AuthoredView->SetFiles(FPaths::Combine(Directory, TEXT("AStarDebuggerSearchHistory.ui.html")),
        FPaths::Combine(Directory, TEXT("AStarDebuggerSearchHistory.ui.css")));
    _AuthoredView->PollFiles();
    if (_AuthoredView->GetLastResult().Succeeded)
    {
        _AuthoredMounted = true;
        _ContentHost->SetContent(Main);
    }
    else
    { _AuthoredLoadFailure = FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n")); }
}

auto
    SCkAStarDebugger_SearchHistory::
    CopyAuthoredEntry(const FString& InKey)
    -> void
{
    if (_Released || NOT _AuthoredCollection.IsValid())
    { return; }
    RefreshFromViewModel();
    if (NOT _ProjectionCurrent)
    { return; }
    const auto Record = _AuthoredCollection->FindRecord(InKey);
    const auto* Copy = Record.IsValid() ? Record->FindField(TEXT("copy")) : nullptr;
    if (Copy == nullptr || Copy->Kind != ECkUiFieldKind::Text)
    { return; }
    FPlatformApplicationMisc::ClipboardCopy(*Copy->Text.ToString());
}

// ====================================================================================================================
// Rebuild list
// ====================================================================================================================

auto
    SCkAStarDebugger_SearchHistory::
    RebuildList()
    -> void
{
    if (NOT _Rail.IsValid())
    { return; }

    _Rail->ClearChildren();

    const auto Selected = _ViewModel.IsValid() && _ViewModel->Has_SelectedEntity()
        ? _ViewModel->Get_SelectedEntityHandle() : FCk_Handle{};
    const auto* History = ck::IsValid(Selected) ? _ViewModel->Get_SearchHistory(Selected) : nullptr;

    if (NOT History || History->IsEmpty())
    {
        _Rail->Set_CountText(FText::FromString(TEXT("0")));
        _Rail->AddChild(
            SNew(SCkDebug_SelectableLabel)
                .Text(_EmptyText)
                .Font(ck::debug_axes::ScaledFont("Italic", CkStyle::FontSizeBody()))
                .ColorAndOpacity(CkStyle::TextMute()));
        return;
    }

    _Rail->Set_CountText(FText::FromString(ck::Format_UE(TEXT("{}"), History->Num())));

    for (auto Idx = History->Num() - 1; Idx >= 0; --Idx)
    {
        _Rail->AddChild(BuildHistoryEntry((*History)[Idx]));
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
    return SNew(SCkDebug_HistoryRow)
        .Tone(CkAStarDebugger::GetStatusTone(InEntry.FinalStatus))
        .TitleText(FText::FromString(CkAStarDebugger::GetStatusString(InEntry.FinalStatus)))
        .RightText(FText::FromString(ck::Format_UE(TEXT("F#{}"), InEntry.FrameNumber)))
        .SubtitleText(FText::FromString(ck_astar_debugger_search_history::MetaText(InEntry)))
        .CopyText(ck_astar_debugger_search_history::CopyText(InEntry));
}

// ====================================================================================================================
