#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/CkFlexText.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_debug_event_log_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    { InSlate.PumpMessages(); InSlate.Tick(); InSlate.Tick(); }

    auto Find(const TSharedRef<SWidget>& InRoot, FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        auto* Children = InRoot->GetChildren();
        for (auto Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (const auto Found = Find(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag)) { return Found; } }
        return {};
    }

    auto Text(const TSharedRef<SWidget>& InRoot, const TCHAR* InTag) -> FString
    {
        const auto Widget = Find(InRoot, FName{InTag});
        return Widget.IsValid() && Widget->GetTypeAsString() == TEXT("SCkFlexText")
            ? StaticCastSharedPtr<SCkFlexText>(Widget)->GetText().ToString() : FString{};
    }

    auto FindText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString() == InText) { return InRoot; }
        auto* Children = InRoot->GetChildren();
        for (auto Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (const auto Found = FindText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return Found; } }
        return {};
    }

    auto FindMenuEntry(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTypeAsString() == TEXT("SMenuEntryButton") && FindText(InRoot, InText).IsValid())
        { return InRoot; }
        auto* Children = InRoot->GetChildren();
        for (auto Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (const auto Found = FindMenuEntry(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return Found; } }
        return {};
    }

    auto FocusPathContains(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const auto Focused = InSlate.GetUserFocusedWidget(0);
        if (NOT Focused.IsValid()) { return false; }
        auto Path = FWidgetPath{};
        if (NOT InSlate.GeneratePathToWidgetUnchecked(Focused.ToSharedRef(), Path)) { return false; }
        for (auto Index = 0; Index < Path.Widgets.Num(); ++Index)
        { if (Path.Widgets[Index].Widget == InWidget) { return true; } }
        return false;
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget,
        FKey InButton = EKeys::LeftMouseButton, bool InControl = false) -> bool
    {
        const auto Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid()) { return false; }
        Window->BringToFront(true);
        Tick(InSlate);
        InSlate.CloseToolTip();
        InSlate.ReleaseAllPointerCapture(0);
        const auto Geometry = InWidget->GetCachedGeometry();
        const auto Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const FModifierKeysState Modifiers(false, false, InControl, false, false, false, false, false, false);
        const FPointerEvent Move(0, FSlateApplication::CursorPointerIndex, Position, Position,
            TSet<FKey>{}, EKeys::Invalid, 0.0f, Modifiers);
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(Move, true);
        const auto Path = InSlate.LocateWindowUnderMouse(Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        auto Found = false;
        for (auto Index = 0; Index < Path.Widgets.Num(); ++Index) { Found |= Path.Widgets[Index].Widget == InWidget; }
        if (NOT Found) { return false; }
        const FPointerEvent Down(0, FSlateApplication::CursorPointerIndex, Position, Position,
            TSet<FKey>{InButton}, InButton, 0.0f, Modifiers);
        const FPointerEvent Up(0, FSlateApplication::CursorPointerIndex, Position, Position,
            TSet<FKey>{}, InButton, 0.0f, Modifiers);
        const auto Handled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), Down);
        const auto UpHandled = InSlate.ProcessMouseButtonUpEvent(Up);
        Tick(InSlate);
        return Handled || UpHandled;
    }

    auto Entry(const TCHAR* InMessage, int32 InId, const TCHAR* InCategory = TEXT("STATE")) -> FCkDebug_EventLogEntry
    {
        auto Result = FCkDebug_EventLogEntry{};
        Result.Message = InMessage;
        Result.Category = InCategory;
        Result.Tone = ECk_Tone::Ok;
        Result.TimeSeconds = 61.125;
        Result.SelectionId = InId;
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugEventLog_AuthoredPresentation,
    "Ck.UiAuthoring.Debugger.EventLog.AuthoredPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkDebugEventLog_AuthoredPresentation::RunTest(const FString&) -> bool
{
    using namespace ck_debug_event_log_tests;
    if (NOT TestTrue(TEXT("Slate is initialized"), FSlateApplication::IsInitialized())) { return false; }
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT TestTrue(TEXT("debugger resource root exists"), Plugin.IsValid())) { return false; }
    const auto Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    auto Sources = TArray<FString>{};
    for (const auto* File : {TEXT("DebuggerEventLog.ui.html"), TEXT("DebuggerEventLog.ui.css"),
        TEXT("DebuggerEventLogRow.ui.html"), TEXT("DebuggerEventLogRow.ui.css")})
    {
        auto Source = FString{};
        if (NOT TestTrue(TEXT("installed EventLog source is readable"), FFileHelper::LoadFileToString(Source, *FPaths::Combine(Directory, File)))) { return false; }
        Sources.Add(MoveTemp(Source));
    }
    auto& Slate = FSlateApplication::Get();
    auto* StyleSettings = UCkDebuggerStyleSettings::Get_Mutable();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), StyleSettings)) { return false; }
    const auto SavedSelection = StyleSettings->Selection;
    auto OriginalClipboard = FString{};
    FPlatformApplicationMisc::ClipboardPaste(OriginalClipboard);
    TSharedPtr<SWindow> Window;
    ON_SCOPE_EXIT
    {
        Slate.DismissAllMenus();
        FPlatformApplicationMisc::ClipboardCopy(*OriginalClipboard);
        StyleSettings->Selection = SavedSelection;
        StyleSettings->NotifyChanged();
        if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); }
    };
    StyleSettings->Selection.ChipStyle = ECkDebugAxis_ChipStyle::Tint;
    StyleSettings->NotifyChanged();

    auto Native = SNew(SCkDebug_EventLog).MaxEntries(2).ShowTimestamps(false);
    Native->Add_Entries({Entry(TEXT("native one"), 1), Entry(TEXT("native two"), 2), Entry(TEXT("native three"), 3)});
    TestTrue(TEXT("native-default consumer retains cap and presentation"), NOT Native->Get_UsesAuthoredPresentation()
        && NOT Native->Get_AuthoredView().IsValid() && Native->Get_EntryCount() == 2);

    int32 SelectedId = INDEX_NONE;
    TSharedPtr<SCkDebug_EventLog> Log = SNew(SCkDebug_EventLog).MaxEntries(3).UseAuthoredPresentation(true)
        .OnEntrySelected_Lambda([&SelectedId](int32 InId) { SelectedId = InId; });
    auto FocusTarget = SNew(SEditableTextBox);
    Window = SNew(SWindow).ClientSize(FVector2D(640.0f, 260.0f)).CreateTitleBar(false).HasCloseButton(false)
        [SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[FocusTarget]
            + SVerticalBox::Slot().FillHeight(1.0f)[Log.ToSharedRef()]];
    Slate.AddWindow(Window.ToSharedRef(), true);
    Tick(Slate);
    if (NOT TestTrue(TEXT("shared EventLog admits its authored shell and row schema"), Log->Get_UsesAuthoredPresentation()))
    { AddError(Log->Get_AuthoredFailure()); return false; }
    const auto Shell = Log->Get_AuthoredView();
    const auto List = Log->_ListView;
    TestEqual(TEXT("empty state is authored"), Text(Shell->GetRegion(TEXT("main")), TEXT("event-log-empty")), FString(TEXT("No events yet.")));
    Slate.SetUserFocus(0, FocusTarget, EFocusCause::SetDirectly);
    Log->Add_Entries({Entry(TEXT("first"), 1), Entry(TEXT("second"), 2), Entry(TEXT("third"), 3, TEXT(""))});
    Tick(Slate);
    TestTrue(TEXT("append autoscroll preserves keyboard focus"), FocusPathContains(Slate, FocusTarget));
    const auto First = Log->_Entries[0];
    const auto Second = Log->_Entries[1];
    const auto FirstRow = List->WidgetFromItem(First);
    const auto SecondRow = List->WidgetFromItem(Second);
    if (NOT TestTrue(TEXT("native virtualization creates authored row contents"), FirstRow.IsValid() && SecondRow.IsValid())) { return false; }
    const auto HeldFirst = FirstRow->AsWidget();
    TestTrue(TEXT("row source owns timestamp and message"), Text(HeldFirst, TEXT("event-log-timestamp")) == TEXT("01:01.125")
        && Text(HeldFirst, TEXT("event-log-message")) == TEXT("first") && Find(HeldFirst, TEXT("event-log-category")).IsValid());
    const auto CategoryChip = Find(HeldFirst, TEXT("event-log-category"));
    if (NOT TestTrue(TEXT("authored row mounts the native axis-aware category chip"), CategoryChip.IsValid())) { return false; }
    CategoryChip->SlatePrepass();
    const auto TintSize = CategoryChip->GetDesiredSize();
    StyleSettings->Selection.ChipStyle = ECkDebugAxis_ChipStyle::TextOnly;
    StyleSettings->NotifyChanged();
    CategoryChip->SlatePrepass();
    const auto TextOnlySize = CategoryChip->GetDesiredSize();
    StyleSettings->Selection.ChipStyle = ECkDebugAxis_ChipStyle::Solid;
    StyleSettings->NotifyChanged();
    CategoryChip->SlatePrepass();
    TestTrue(TEXT("retained category follows live TextOnly and Solid chip axes"),
        TextOnlySize.X < TintSize.X && TextOnlySize.Y < TintSize.Y
            && CategoryChip->GetDesiredSize().X > TextOnlySize.X && CategoryChip->GetDesiredSize().Y > TextOnlySize.Y);
    if (NOT TestTrue(TEXT("passive authored content routes physical selection to native STableRow"), Click(Slate, HeldFirst)
        && SelectedId == 1 && List->GetSelectedItems().Contains(First))) { return false; }
    if (NOT TestTrue(TEXT("native multi-selection survives authored row content"), Click(Slate, SecondRow->AsWidget(), EKeys::LeftMouseButton, true)
        && List->GetSelectedItems().Num() == 2)) { return false; }
    if (NOT TestTrue(TEXT("physical right-click opens the multi-selection copy menu"), Click(Slate, SecondRow->AsWidget(), EKeys::RightMouseButton))) { return false; }
    TSharedPtr<SWidget> Copy;
    auto PopupWindows = TArray<TSharedRef<SWindow>>{};
    Slate.GetAllVisibleWindowsOrdered(PopupWindows);
    for (const auto& Popup : PopupWindows)
    { if (const auto Found = FindMenuEntry(Popup, TEXT("Copy Events"))) { Copy = Found; break; } }
    if (NOT TestTrue(TEXT("native copy command is present in the child-window menu"), Copy.IsValid())) { return false; }
    if (NOT TestTrue(TEXT("native copy command is physically reachable"), Click(Slate, Copy.ToSharedRef()))) { return false; }
    auto Copied = FString{};
    FPlatformApplicationMisc::ClipboardPaste(Copied);
    TestTrue(TEXT("copy preserves timestamps, categories and selected messages"), Copied.Contains(TEXT("[01:01.125] STATE  first"))
        && Copied.Contains(TEXT("[01:01.125] STATE  second")) && NOT Copied.Contains(TEXT("third")));
    Slate.DismissAllMenus();

    const auto Revision = Shell->GetRevision();
    auto Candidate = Sources;
    Candidate[3] += TEXT("\n.event-log-row { padding: 2px 0; }\n");
    TestTrue(TEXT("compatible shared reload is atomic and retains native list and row identity"), Log->TryApply_AuthoredSources(Candidate)
        && Shell->GetRevision() > Revision && Log->_ListView == List && List->WidgetFromItem(Second) == SecondRow
        && List->GetSelectedItems().Num() == 2);
    const auto AcceptedRevision = Shell->GetRevision();
    Candidate[2] = Candidate[2].Replace(TEXT("color-bind=\"event-foreground\""), TEXT("color-bind=\"missing-color\""));
    TestTrue(TEXT("invalid row candidate rejects shell and every row together"), NOT Log->TryApply_AuthoredSources(Candidate)
        && Shell->GetRevision() == AcceptedRevision && Log->Get_UsesAuthoredPresentation()
        && Text(HeldFirst, TEXT("event-log-message")) == TEXT("first"));
    Candidate = Sources;
    Candidate[2] = TEXT("<ui version=\"1\"><region name=\"main\"><search id=\"trap\" /></region></ui>");
    TestTrue(TEXT("click-consuming row content cannot enter the selection surface"), NOT Log->TryApply_AuthoredSources(Candidate)
        && Shell->GetRevision() == AcceptedRevision);
    Log->_ShowTimestamps = false;
    Tick(Slate);
    const auto TimestampWrap = Find(HeldFirst, TEXT("event-log-timestamp-wrap"));
    TestTrue(TEXT("timestamp preference updates retained authored rows"), TimestampWrap.IsValid()
        && TimestampWrap->GetVisibility() == EVisibility::Collapsed);
    Log->_TimestampColumnWidth = 94.0f;
    TestTrue(TEXT("token change reloads accepted row presentation"), Log->TryApply_AuthoredSources(Sources));
    Window->Resize(FVector2D(300.0f, 180.0f));
    Tick(Slate);
    TestTrue(TEXT("narrow Events retains a finite, reachable native list"), List->GetCachedGeometry().GetLocalSize().X > 0.0f
        && List->GetCachedGeometry().GetLocalSize().X <= 300.0f && List->GetCachedGeometry().GetLocalSize().Y > 0.0f);
    Log->Add_Entry(Entry(TEXT("fourth"), 4));
    Tick(Slate);
    TestTrue(TEXT("front eviction retains surviving record pointers and selection"), Log->Get_EntryCount() == 3
        && Log->_Entries[0] == Second && List->GetSelectedItems().Contains(Second) && NOT Log->Get_ContainsEntry(First));
    SelectedId = INDEX_NONE;
    Log->Handle_SelectionChanged(First, ESelectInfo::OnMouseClick);
    TestTrue(TEXT("evicted held rows reject selection and stop exposing old text"), SelectedId == INDEX_NONE
        && Text(HeldFirst, TEXT("event-log-message")).IsEmpty());
    Log->Clear_Entries();
    Tick(Slate);
    const auto EmptyWrap = Find(Shell->GetRegion(TEXT("main")), TEXT("event-log-empty-wrap"));
    TestTrue(TEXT("clear invalidates retained authored history"), Log->Get_EntryCount() == 0 && List->GetSelectedItems().IsEmpty()
        && EmptyWrap.IsValid() && EmptyWrap->GetVisibility().IsVisible());

    auto Many = SNew(SCkDebug_EventLog).MaxEntries(100).UseAuthoredPresentation(true);
    auto ManyEntries = TArray<FCkDebug_EventLogEntry>{};
    for (auto Index = 0; Index < 100; ++Index) { ManyEntries.Add(Entry(TEXT("virtualized event"), Index)); }
    Many->Add_Entries(MoveTemp(ManyEntries));
    Window->SetContent(Many);
    Tick(Slate);
    auto LiveRows = 0;
    for (const auto& Item : Many->_Entries) { LiveRows += Many->_ListView->WidgetFromItem(Item).IsValid() ? 1 : 0; }
    TestTrue(TEXT("long authored logs retain native virtualization and tail scrolling"), Many->Get_EntryCount() == 100
        && LiveRows > 0 && LiveRows < 100 && Many->_ListView->WidgetFromItem(Many->_Entries.Last()).IsValid());
    const auto ScrollOffset = Many->_ListView->GetScrollOffset();
    TestTrue(TEXT("populated authored reload keeps native scroll position"), Many->TryApply_AuthoredSources(Sources)
        && FMath::IsNearlyEqual(Many->_ListView->GetScrollOffset(), ScrollOffset));

    // A malformed installed row must also fail while the log is empty; the prototype participates in admission.
    const auto RowPath = FPaths::Combine(Directory, TEXT("DebuggerEventLogRow.ui.html"));
    const auto RowSource = Sources[2];
    ON_SCOPE_EXIT { FFileHelper::SaveStringToFile(RowSource, *RowPath); };
    if (NOT TestTrue(TEXT("startup fallback fixture writes a malformed row resource"), FFileHelper::SaveStringToFile(TEXT("<ui>"), *RowPath))) { return false; }
    TSharedPtr<SCkDebug_EventLog> Fallback = SNew(SCkDebug_EventLog).UseAuthoredPresentation(true);
    Fallback->Add_Entry(Entry(TEXT("fallback history"), 5));
    Window->SetContent(Fallback.ToSharedRef());
    Tick(Slate);
    const auto FallbackList = Fallback->_ListView;
    const auto FallbackEntry = Fallback->_Entries[0];
    const auto FallbackRow = FallbackList->WidgetFromItem(FallbackEntry);
    TestFalse(TEXT("invalid shared startup resource keeps complete native presentation"), Fallback->Get_UsesAuthoredPresentation());
    if (NOT TestTrue(TEXT("fallback fixture restores the installed row source"), FFileHelper::SaveStringToFile(RowSource, *RowPath))) { return false; }
    Fallback->Poll_AuthoredPresentation();
    Tick(Slate);
    TestTrue(TEXT("valid resource recovery retains native list and history"), Fallback->Get_UsesAuthoredPresentation()
        && Fallback->_ListView == FallbackList && Fallback->_Entries[0] == FallbackEntry && FallbackRow.IsValid()
        && FallbackList->WidgetFromItem(FallbackEntry) == FallbackRow
        && Text(FallbackRow->AsWidget(), TEXT("event-log-message")) == TEXT("fallback history"));
    const TWeakPtr<SCkDebug_EventLog> WeakLog = Log;
    Window->SetContent(SNullWidget::NullWidget);
    Log.Reset();
    TestTrue(TEXT("held rows and native list do not retain the released log or read destroyed list source"), NOT WeakLog.IsValid()
        && Text(HeldFirst, TEXT("event-log-message")).IsEmpty());
    return true;
}

#endif
