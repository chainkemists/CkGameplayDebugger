#include "CkAudioDebugger/Window/SCkAudioDebuggerWindow.h"
#include "CkAudioDebugger/Window/SCkAudioDebugger_FalloffCurve.h"
#include "CkAudioDebugger/Window/SCkAudioDebugger_Radar.h"
#include "../../CkAudioDebugger_Module.h"

#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_UnderlineTabs.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiRepeat.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/IToolTip.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_audio_debugger_authored_shell_tests
{
    struct FScopedStyleSelection
    {
        FScopedStyleSelection()
        {
            if (const auto* Settings = UCkDebuggerStyleSettings::Get())
            {
                Selection = Settings->Selection;
                ProfileName = Settings->ActiveProfileName;
            }
        }

        ~FScopedStyleSelection()
        {
            if (auto* Settings = UCkDebuggerStyleSettings::Get_Mutable())
            {
                Settings->Selection = Selection;
                Settings->ActiveProfileName = ProfileName;
                Settings->NotifyChanged();
            }
        }

        FCkDebuggerStyleSelection Selection;
        FString ProfileName;
    };

    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindEditableText(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableText>
    {
        if (InRoot->GetTypeAsString() == TEXT("SEditableText"))
        { return StaticCastSharedRef<SEditableText>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableText> Found = FindEditableText(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindTaggedUnderlineTabs(const TSharedRef<SWidget>& InRoot, const FName InTag)
        -> TSharedPtr<SCkDebug_UnderlineTabs>
    {
        if (InRoot->GetTag() == InTag && InRoot->GetTypeAsString() == TEXT("SCkDebug_UnderlineTabs"))
        { return StaticCastSharedRef<SCkDebug_UnderlineTabs>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCkDebug_UnderlineTabs> Found = FindTaggedUnderlineTabs(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto TaggedText(const TSharedRef<SWidget>& InRoot, const TCHAR* InTag) -> FString
    {
        const TSharedPtr<SWidget> Widget = FindTaggedWidget(InRoot, FName{InTag});
        return Widget.IsValid() && Widget->GetTypeAsString() == TEXT("SCkFlexText")
            ? StaticCastSharedPtr<SCkFlexText>(Widget)->GetText().ToString() : FString{};
    }

    auto FindTaggedTextBlock(const TSharedRef<SWidget>& InRoot, FName InTag) -> TSharedPtr<STextBlock>
    {
        if (InRoot->GetTag() == InTag && InRoot->GetTypeAsString() == TEXT("STextBlock"))
        { return StaticCastSharedRef<STextBlock>(InRoot); }
        auto* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const auto Found = FindTaggedTextBlock(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag))
            { return Found; }
        }
        return {};
    }

    auto FindEntityRef(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCkDebug_EntityRef>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_EntityRef"))
        { return StaticCastSharedRef<SCkDebug_EntityRef>(InRoot); }
        auto* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const auto Found = FindEntityRef(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))))
            { return Found; }
        }
        return {};
    }

    auto FindWidgetType(const TSharedRef<SWidget>& InRoot, const FString& InType) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTypeAsString() == InType) { return InRoot; }
        auto* Children = InRoot->GetChildren();
        for (auto Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const auto Found = FindWidgetType(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType))
            { return Found; }
        }
        return {};
    }

    auto CountTrackRecords(const TSharedPtr<FCkUiCollection>& InRecords) -> int32
    {
        auto Count = 0;
        if (InRecords.IsValid())
        {
            for (const auto& Record : InRecords->GetRecords())
            { if (const auto* Field = Record->FindField(TEXT("is-track")); Field != nullptr && Field->Bool) { ++Count; } }
        }
        return Count;
    }

    auto ProbeRetainedEntityRef(const TSharedRef<SCkDebug_EntityRef>& InRef) -> void
    {
        const auto Position = InRef->GetCachedGeometry().GetAbsolutePosition();
        const FPointerEvent Event(0, FSlateApplication::CursorPointerIndex, Position, Position,
            TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InRef->OnMouseButtonDown(InRef->GetCachedGeometry(), Event);
    }

    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto ContainsWidget(const TSharedRef<SWidget>& InRoot, const TSharedRef<SWidget>& InTarget) -> bool
    {
        if (InRoot == InTarget) { return true; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTarget))
            { return true; }
        }
        return false;
    }

    auto SubtreeHasText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString() == InText)
        { return true; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (SubtreeHasText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText))
            { return true; }
        }
        return false;
    }

    auto FindButtonWithText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && SubtreeHasText(InRoot, InText))
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButtonWithText(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindCheckBoxWithText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SCheckBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCheckBox") && SubtreeHasText(InRoot, InText))
        { return StaticCastSharedRef<SCheckBox>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCheckBox> Found = FindCheckBoxWithText(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto WidgetPathContains(const FWidgetPath& InPath, const TSharedRef<SWidget>& InWidget) -> bool
    {
        for (int32 Index = 0; Index < InPath.Widgets.Num(); ++Index)
        { if (InPath.Widgets[Index].Widget == InWidget) { return true; } }
        return false;
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid()) { return false; }
        Window->BringToFront(true);
        TickSlate(InSlate);
        InSlate.ReleaseAllPointerCapture(0);
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftDown{EKeys::LeftMouseButton};
        const FPointerEvent MoveEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            NoButtons, EKeys::Invalid, 0.0f, FModifierKeysState{});
        const FPointerEvent DownEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            LeftDown, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        const FPointerEvent UpEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(MoveEvent, true);
        const FWidgetPath Path = InSlate.LocateWindowUnderMouse(
            Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        if (NOT WidgetPathContains(Path, InWidget)) { return false; }
        const bool DownHandled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), DownEvent);
        InSlate.ProcessMouseButtonUpEvent(UpEvent);
        TickSlate(InSlate);
        return DownHandled;
    }

    auto IsInHitPath(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid()) { return false; }
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        if (Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f) { return false; }
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const FWidgetPath Path = InSlate.LocateWindowUnderMouse(
            Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        return WidgetPathContains(Path, InWidget);
    }

    auto ReplaceSearchText(FSlateApplication& InSlate, const TSharedRef<SEditableText>& InEditable,
        const FString& InText) -> bool
    {
        if (NOT Click(InSlate, InEditable) || InSlate.GetUserFocusedWidget(0) != InEditable)
        { return false; }
        const FModifierKeysState Control{false, false, true, false, false, false, false, false, false};
        if (NOT InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::A, Control, 0, false, 0, 0}))
        { return false; }
        InSlate.ProcessKeyUpEvent(FKeyEvent{EKeys::A, Control, 0, false, 0, 0});
        if (NOT InEditable->GetText().IsEmpty())
        {
            if (NOT InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::BackSpace, FModifierKeysState{}, 0, false, 0, 0}))
            { return false; }
            InSlate.ProcessKeyUpEvent(FKeyEvent{EKeys::BackSpace, FModifierKeysState{}, 0, false, 0, 0});
        }
        for (const TCHAR Character : InText)
        {
            if (NOT InSlate.ProcessKeyCharEvent(FCharacterEvent{Character, FModifierKeysState{}, 0, false}))
            { return false; }
        }
        // Enter runs SCkDebug_SearchBar's existing immediate commit path; do not wait out or change its debounce.
        const bool Committed = InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
        InSlate.ProcessKeyUpEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
        return Committed && InEditable->GetText().ToString() == InText;
    }

    // Keep Tracks code generation out of RunTest while its closure retains the fixture's friend access.
    template <typename TFixture>
    static FORCENOINLINE auto RunFixturePhase(TFixture&& InFixture) -> bool
    {
        return InFixture();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAudioDebugger_AuthoredShell,
    "Ck.AudioDebugger.AuthoredShell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkAudioDebugger_AuthoredShell::RunTest(const FString&) -> bool
{
    using namespace ck_audio_debugger_authored_shell_tests;

    const auto StyleGuard = FScopedStyleSelection{};
    auto* StyleSettings = UCkDebuggerStyleSettings::Get_Mutable();
    if (NOT TestNotNull(TEXT("Audio authored-shell test requires debugger style settings"), StyleSettings))
    { return false; }
    StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Normal;
    StyleSettings->Selection.CornerStyle = ECkDebugAxis_CornerStyle::Rounded;
    StyleSettings->Selection.SurfaceElevation = ECkDebugAxis_SurfaceElevation::Layered;
    StyleSettings->NotifyChanged();

    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Audio authored-shell test requires Slate."));
        return false;
    }

    auto& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    TSharedPtr<SCkAudioDebuggerWindow> DebuggerWindow = SNew(SCkAudioDebuggerWindow);
    const auto GetTabs = [&DebuggerWindow]() -> TSharedPtr<SCkDebug_UnderlineTabs>
    {
        if (DebuggerWindow->_UsingNativeFallback) { return DebuggerWindow->_Tabs; }
        return FindTaggedUnderlineTabs(DebuggerWindow->_AuthoredShellView->GetRegion(TEXT("main")),
            TEXT("audio-shell-tabs"));
    };
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1100.0f, 720.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [DebuggerWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);

    TSharedPtr<FCkUiView> View = DebuggerWindow->_AuthoredShellView;
    if (NOT TestTrue(TEXT("production Audio window admits its authored shell"),
        View.IsValid() && View->GetLastResult().Succeeded))
    {
        if (View.IsValid()) { AddError(FString::Join(View->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }
    TSharedPtr<FCkUiView> CrossfadeView = DebuggerWindow->_AuthoredCrossfadeView;
    if (NOT TestTrue(TEXT("production Audio window admits its authored Crossfade page"),
        CrossfadeView.IsValid() && CrossfadeView->GetLastResult().Succeeded))
    {
        if (CrossfadeView.IsValid()) { AddError(FString::Join(CrossfadeView->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }
    TSharedPtr<FCkUiView> AttenuationView = DebuggerWindow->_AuthoredAttenuationView;
    if (NOT TestTrue(TEXT("production Audio window admits its authored attenuation block"),
        AttenuationView.IsValid() && AttenuationView->GetLastResult().Succeeded))
    {
        if (AttenuationView.IsValid())
        { AddError(FString::Join(AttenuationView->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }

    TSharedPtr<FCkUiView> EventsView = DebuggerWindow->_AuthoredEventsToolbarView;
    TSharedPtr<FCkUiView> DirectorsView = DebuggerWindow->_AuthoredDirectorsView;
    TSharedPtr<FCkUiView> TracksView = DebuggerWindow->_AuthoredTracksView;
    TSharedPtr<FCkUiView> SpatialPageView = DebuggerWindow->_AuthoredSpatialView;
    if (NOT TestTrue(TEXT("production Audio admits the Spatial page with its exact retained Radar"),
        SpatialPageView.IsValid() && SpatialPageView->GetLastResult().Succeeded
            && NOT DebuggerWindow->_UsingNativeSpatialFallback
            && SpatialPageView->GetRepeat(TEXT("audio-spatial-tracks")).IsValid()
            && SpatialPageView->GetScroll(TEXT("audio-spatial-selector-scroll")).IsValid()
            && ContainsWidget(SpatialPageView->GetRegion(TEXT("main")), DebuggerWindow->_Radar.ToSharedRef())))
    {
        if (SpatialPageView.IsValid()) { AddError(FString::Join(SpatialPageView->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }
    if (NOT TestTrue(TEXT("production Audio window admits its authored Tracks page and compact plot"),
        TracksView.IsValid() && TracksView->GetLastResult().Succeeded
            && NOT DebuggerWindow->_UsingNativeTracksFallback
            && TracksView->GetRepeat(TEXT("audio-tracks")).IsValid()
            && TracksView->GetScroll(TEXT("audio-tracks-scroll")).IsValid()
            && ContainsWidget(TracksView->GetRegion(TEXT("main")), DebuggerWindow->_CompactCrossfadePlot.ToSharedRef())))
    {
        if (TracksView.IsValid()) { AddError(FString::Join(TracksView->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }
    if (NOT TestTrue(TEXT("production Audio window admits its authored Directors page"),
        DirectorsView.IsValid() && DirectorsView->GetLastResult().Succeeded
            && NOT DebuggerWindow->_UsingNativeDirectorsFallback
            && DirectorsView->GetRepeat(TEXT("audio-directors")).IsValid()
            && DirectorsView->GetScroll(TEXT("audio-directors-scroll")).IsValid()))
    {
        if (DirectorsView.IsValid()) { AddError(FString::Join(DirectorsView->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }
    if (NOT TestTrue(TEXT("production Audio window admits its authored Events toolbar"),
        EventsView.IsValid() && EventsView->GetLastResult().Succeeded))
    {
        if (EventsView.IsValid())
        { AddError(FString::Join(EventsView->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }
    const TSharedRef<SWidget> EventsMain = EventsView->GetRegion(TEXT("main"));
    const TSharedPtr<SCkDebug_EventLog> OriginalEventLog = DebuggerWindow->_EventLog;
    const TArray<TSharedPtr<SCkDebug_ToggleSurface>> OriginalEventToggles{
        DebuggerWindow->_EventsStateToggle, DebuggerWindow->_EventsFadesToggle,
        DebuggerWindow->_EventsVirtualizationToggle, DebuggerWindow->_EventsLifecycleToggle};
    for (const auto& Toggle : OriginalEventToggles)
    {
        TestTrue(TEXT("authored Events toolbar mounts each exact native toggle surface"),
            Toggle.IsValid() && ContainsWidget(EventsMain, Toggle.ToSharedRef()));
    }
    TestEqual(TEXT("Events toolbar authors the sampling caveat"),
        TaggedText(EventsMain, TEXT("audio-events-caveat")),
        FString{TEXT("sampled at the refresh rate — sub-tick transitions are not captured")});
    TestFalse(TEXT("authored Events toolbar does not own the native event log"),
        ContainsWidget(EventsMain, OriginalEventLog.ToSharedRef()));

    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    TestTrue(TEXT("authored shell retains production tabs and pages and authors filter composition and live summary cards"),
        GetTabs().IsValid()
            && DebuggerWindow->_StatCards.IsValid()
            && DebuggerWindow->_FilterSearchBar.IsValid()
            && DebuggerWindow->_PageSwitcher.IsValid()
            && ContainsWidget(Main, GetTabs().ToSharedRef())
            && NOT ContainsWidget(Main, DebuggerWindow->_StatCards.ToSharedRef())
            && ContainsWidget(Main, DebuggerWindow->_FilterSearchBar.ToSharedRef())
            && ContainsWidget(Main, DebuggerWindow->_PageSwitcher.ToSharedRef())
            && TaggedText(Main, TEXT("audio-stat-concurrency-label")) == TEXT("Active / max")
            && TaggedText(Main, TEXT("audio-stat-audible-label")) == TEXT("Audible")
            && TaggedText(Main, TEXT("audio-stat-fading-label")) == TEXT("Fading")
            && TaggedText(Main, TEXT("audio-stat-virtualized-label")) == TEXT("Virtualized")
            && TaggedText(Main, TEXT("audio-stat-concurrency")) == TEXT("0 / 0"));
    TestEqual(TEXT("production page switcher retains all six Audio pages"),
        DebuggerWindow->_PageSwitcher->GetNumWidgets(), 6);
    TestEqual(TEXT("production Audio shell preserves Tracks as its default page"),
        DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex(), 1);
    const auto OriginalTabs = GetTabs();
    if (NOT TestTrue(TEXT("authored tabs use the retained semantic adapter and leave the startup strip inert"),
        OriginalTabs.IsValid() && OriginalTabs != DebuggerWindow->_Tabs
            && NOT DebuggerWindow->_Tabs->GetCanDispatchEvents()
            && NOT ContainsWidget(Main, DebuggerWindow->_Tabs.ToSharedRef())))
    { return false; }
    TestTrue(TEXT("authored shell exposes horizontal overflow reachability"),
        View->GetScroll(TEXT("audio-shell-scroll")).IsValid());
    const TSharedPtr<SCkDebug_SearchBar> HeldFilterSearch = DebuggerWindow->_FilterSearchBar;
    const TArray<TSharedPtr<SCkDebug_ToggleSurface>> HeldFilterToggles{
        DebuggerWindow->_FilterPlayingToggle, DebuggerWindow->_FilterFadingToggle,
        DebuggerWindow->_FilterStoppedToggle, DebuggerWindow->_FilterGroupToggle};
    const TSharedPtr<SWidget> AuthoredFilters = FindTaggedWidget(Main, TEXT("audio-shell-filters"));
    if (NOT TestTrue(TEXT("authored filter row contains the exact native search bar"),
        AuthoredFilters.IsValid() && ContainsWidget(AuthoredFilters.ToSharedRef(), HeldFilterSearch.ToSharedRef())))
    { return false; }
    for (const auto& Toggle : HeldFilterToggles)
    { TestTrue(TEXT("authored filter row mounts each exact native toggle"), ContainsWidget(AuthoredFilters.ToSharedRef(), Toggle.ToSharedRef())); }
    const TSharedPtr<SEditableText> HeldFilterEditable = FindEditableText(HeldFilterSearch.ToSharedRef());
    if (NOT TestTrue(TEXT("physical search typing and Enter update both filter and highlight"),
        HeldFilterEditable.IsValid() && ReplaceSearchText(Slate, HeldFilterEditable.ToSharedRef(), TEXT("audio"))
            && HeldFilterSearch->Get_SearchText() == TEXT("audio")
            && DebuggerWindow->_FilterString == TEXT("audio") && DebuggerWindow->_HighlightString == TEXT("audio")
            && DebuggerWindow->_LastSignature.IsEmpty()))
    { return false; }
    const TSharedPtr<SCheckBox> HeldPlayingFilter = FindCheckBoxWithText(HeldFilterToggles[0].ToSharedRef(), TEXT("Playing"));
    const TSharedPtr<SCheckBox> HeldFadingFilter = FindCheckBoxWithText(HeldFilterToggles[1].ToSharedRef(), TEXT("Fading"));
    const TSharedPtr<SCheckBox> HeldStoppedFilter = FindCheckBoxWithText(HeldFilterToggles[2].ToSharedRef(), TEXT("Stopped"));
    const TSharedPtr<SCheckBox> HeldGroupFilter = FindCheckBoxWithText(HeldFilterToggles[3].ToSharedRef(), TEXT("Group by director"));
    if (NOT TestTrue(TEXT("all four retained filter controls route physical input independently"),
        HeldPlayingFilter.IsValid() && HeldFadingFilter.IsValid() && HeldStoppedFilter.IsValid() && HeldGroupFilter.IsValid()
            && Click(Slate, HeldPlayingFilter.ToSharedRef()) && NOT DebuggerWindow->_ShowPlaying && DebuggerWindow->_ShowFading
            && Click(Slate, HeldFadingFilter.ToSharedRef()) && NOT DebuggerWindow->_ShowFading && DebuggerWindow->_ShowStopped
            && Click(Slate, HeldStoppedFilter.ToSharedRef()) && NOT DebuggerWindow->_ShowStopped && DebuggerWindow->_GroupByDirector
            && Click(Slate, HeldGroupFilter.ToSharedRef()) && NOT DebuggerWindow->_GroupByDirector))
    { return false; }
    DebuggerWindow->_LastSignature = TEXT("filter callback probe");
    HeldPlayingFilter->ToggleCheckedState();
    TestTrue(TEXT("held filter toggle probe dispatches and invalidates the live structure signature"),
        DebuggerWindow->_ShowPlaying && DebuggerWindow->_LastSignature.IsEmpty());
    HeldPlayingFilter->ToggleCheckedState();
    const TSharedRef<SWidget> CrossfadeMain = CrossfadeView->GetRegion(TEXT("main"));
    TestTrue(TEXT("dedicated Crossfade page authors its ordinary presentation around the exact dual-series plot"),
        TaggedText(CrossfadeMain, TEXT("audio-crossfade-title")) == TEXT("Crossfade lane")
            && TaggedText(CrossfadeMain, TEXT("audio-crossfade-subtitle")) == TEXT("recent history of _CurrentVolume")
            && TaggedText(CrossfadeMain, TEXT("audio-crossfade-legend")) == TEXT("(nothing playing)")
            && ContainsWidget(CrossfadeMain, DebuggerWindow->_CrossfadePagePlot.ToSharedRef())
            && DebuggerWindow->_CrossfadePagePlot->Get_Samples() == DebuggerWindow->_CrossfadeSeriesA
            && DebuggerWindow->_CrossfadePagePlot->Get_BandSamples() == DebuggerWindow->_CrossfadeSeriesB
            && DebuggerWindow->_CrossfadePagePlot->Get_BandFillOpacity() == 0.0f
            && DebuggerWindow->_CrossfadePagePlot->Get_DesiredSize() == FVector2D{320.0f, 220.0f});
    const TSharedRef<SWidget> AttenuationMain = AttenuationView->GetRegion(TEXT("main"));
    TestTrue(TEXT("authored attenuation block owns six labels around the retained native curve"),
        ContainsWidget(AttenuationMain, DebuggerWindow->_AttenuationCurve.ToSharedRef())
            && TaggedText(AttenuationMain, TEXT("audio-attenuation-distance-label")) == TEXT("Distance")
            && TaggedText(AttenuationMain, TEXT("audio-attenuation-bearing-label")) == TEXT("Bearing")
            && TaggedText(AttenuationMain, TEXT("audio-attenuation-gain-label")) == TEXT("Attenuation gain")
            && TaggedText(AttenuationMain, TEXT("audio-attenuation-track-volume-label")) == TEXT("Track volume")
            && TaggedText(AttenuationMain, TEXT("audio-attenuation-audible-label")) == TEXT("Audible")
            && TaggedText(AttenuationMain, TEXT("audio-attenuation-asset-label")) == TEXT("Attenuation asset")
            && TaggedText(AttenuationMain, TEXT("audio-attenuation-heading")).IsEmpty());

    const TSharedPtr<SWidget> AuthoredValueWidget = FindTaggedWidget(Main, TEXT("audio-stat-concurrency"));
    const bool AuthoredValueIsText = AuthoredValueWidget.IsValid()
        && AuthoredValueWidget->GetTypeAsString() == TEXT("SCkFlexText");
    const float NormalValueFontSize = AuthoredValueIsText
        ? StaticCastSharedPtr<SCkFlexText>(AuthoredValueWidget)->GetFont().Size : 0.0f;
    const TSharedPtr<SWidget> LayeredWrap = FindTaggedWidget(Main, TEXT("audio-stat-concurrency-wrap"));
    const TSharedPtr<SWidget> RoundedCard = FindTaggedWidget(Main, TEXT("audio-stat-concurrency-card"));
    const float LayeredWrapHeight = LayeredWrap.IsValid() ? LayeredWrap->GetDesiredSize().Y : 0.0f;
    const bool RoundedCardIsBorder = RoundedCard.IsValid()
        && RoundedCard->GetTypeAsString() == TEXT("SBorder");
    const FSlateBrush* RoundedCardBrush = RoundedCardIsBorder
        ? StaticCastSharedPtr<SBorder>(RoundedCard)->GetBorderImage() : nullptr;
    TestTrue(TEXT("authored Audio card starts from native rounded-card geometry"),
        RoundedCardBrush != nullptr
            && RoundedCardBrush->DrawAs == ESlateBrushDrawType::RoundedBox
            && RoundedCardBrush->OutlineSettings.CornerRadii.X == CkStyle::RadiusL()
            && RoundedCardBrush->OutlineSettings.Width == CkStyle::RingWidth());

    const int64 RevisionBeforeGeometryChange = View->GetRevision();
    const int64 CrossfadeRevisionBeforeGeometryChange = CrossfadeView->GetRevision();
    const int64 AttenuationRevisionBeforeGeometryChange = AttenuationView->GetRevision();
    StyleSettings->Selection.CornerStyle = ECkDebugAxis_CornerStyle::Sharp;
    StyleSettings->Selection.SurfaceElevation = ECkDebugAxis_SurfaceElevation::Flat;
    StyleSettings->NotifyChanged();
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    const TSharedPtr<SWidget> FlatWrap = FindTaggedWidget(
        View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency-wrap"));
    const TSharedPtr<SWidget> SharpCard = FindTaggedWidget(
        View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency-card"));
    const bool SharpCardIsBorder = SharpCard.IsValid() && SharpCard->GetTypeAsString() == TEXT("SBorder");
    const FSlateBrush* SharpCardBrush = SharpCardIsBorder
        ? StaticCastSharedPtr<SBorder>(SharpCard)->GetBorderImage() : nullptr;
    TestTrue(TEXT("live corner and elevation axes republish authored Audio card geometry"),
        View->GetRevision() > RevisionBeforeGeometryChange
            && CrossfadeView->GetRevision() > CrossfadeRevisionBeforeGeometryChange
            && AttenuationView->GetRevision() > AttenuationRevisionBeforeGeometryChange
            && FlatWrap.IsValid() && FlatWrap->GetDesiredSize().Y < LayeredWrapHeight
            && SharpCardBrush != nullptr
            && SharpCardBrush->OutlineSettings.CornerRadii.X == 0.0f
            && SharpCardBrush->OutlineSettings.Width == CkStyle::RingWidth());

    const int64 RevisionBeforeTextChange = View->GetRevision();
    const int64 EventsRevisionBeforeTextChange = EventsView->GetRevision();
    const TSharedPtr<SWidget> NormalEventsCaveat = FindTaggedWidget(EventsMain, TEXT("audio-events-caveat"));
    const bool EventsCaveatIsText = NormalEventsCaveat.IsValid()
        && NormalEventsCaveat->GetTypeAsString() == TEXT("SCkFlexText");
    const int32 NormalEventsFontSize = EventsCaveatIsText
        ? StaticCastSharedPtr<SCkFlexText>(NormalEventsCaveat)->GetFont().Size : 0;
    const TSharedPtr<SWidget> NormalAttenuationValue = FindTaggedWidget(
        AttenuationView->GetRegion(TEXT("main")), TEXT("audio-attenuation-audible"));
    const bool AttenuationValueIsText = NormalAttenuationValue.IsValid()
        && NormalAttenuationValue->GetTypeAsString() == TEXT("SCkFlexText");
    const int32 NormalAttenuationFontSize = AttenuationValueIsText
        ? StaticCastSharedPtr<SCkFlexText>(NormalAttenuationValue)->GetFont().Size : 0;
    StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Large;
    StyleSettings->NotifyChanged();
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    const TSharedPtr<SWidget> RestyledValueWidget = FindTaggedWidget(
        View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency"));
    TestTrue(TEXT("live Style Lab revision republishes authored Audio card tokens without rebuilding the window"),
        AuthoredValueIsText
            && RestyledValueWidget.IsValid()
            && RestyledValueWidget->GetTypeAsString() == TEXT("SCkFlexText")
            && View->GetRevision() > RevisionBeforeTextChange
            && StaticCastSharedPtr<SCkFlexText>(RestyledValueWidget)->GetFont().Size > NormalValueFontSize);
    const TSharedPtr<SWidget> LargeAttenuationValue = FindTaggedWidget(
        AttenuationView->GetRegion(TEXT("main")), TEXT("audio-attenuation-audible"));
    TestTrue(TEXT("live Style Lab text scale updates the authored attenuation value font"),
        AttenuationValueIsText && LargeAttenuationValue.IsValid()
            && LargeAttenuationValue->GetTypeAsString() == TEXT("SCkFlexText")
            && StaticCastSharedPtr<SCkFlexText>(LargeAttenuationValue)->GetFont().Size > NormalAttenuationFontSize);

    const TSharedPtr<SWidget> LargeEventsCaveat = FindTaggedWidget(EventsMain, TEXT("audio-events-caveat"));
    TestTrue(TEXT("live style revision updates the authored Events caveat font"),
        EventsCaveatIsText && LargeEventsCaveat.IsValid()
            && LargeEventsCaveat->GetTypeAsString() == TEXT("SCkFlexText")
            && EventsView->GetRevision() > EventsRevisionBeforeTextChange
            && StaticCastSharedPtr<SCkFlexText>(LargeEventsCaveat)->GetFont().Size > NormalEventsFontSize);

    const TSharedPtr<SButton> EventsTab = FindButtonWithText(GetTabs().ToSharedRef(), TEXT("Events"));
    if (NOT TestTrue(TEXT("production Events tab is physically selectable without a world"),
        EventsTab.IsValid() && Click(Slate, EventsTab.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 4))
    { return false; }
    // SWidgetSwitcher::GetChildren exposes only its active slot; inspect the mounted page after physical selection.
    TestTrue(TEXT("physically selected Events page retains the original native event log"),
        ContainsWidget(DebuggerWindow->_PageSwitcher.ToSharedRef(), OriginalEventLog.ToSharedRef()));
    const TSharedPtr<SCheckBox> HeldEventsStateToggle = FindCheckBoxWithText(EventsMain, TEXT("State"));
    const TSharedPtr<SCheckBox> EventsFadesToggle = FindCheckBoxWithText(EventsMain, TEXT("Fades"));
    const TSharedPtr<SCheckBox> EventsVirtualizationToggle = FindCheckBoxWithText(EventsMain, TEXT("Virtualization"));
    const TSharedPtr<SCheckBox> EventsLifecycleToggle = FindCheckBoxWithText(EventsMain, TEXT("Lifecycle"));
    if (NOT TestTrue(TEXT("all four Events native controls remain physically usable without a world"),
        HeldEventsStateToggle.IsValid() && EventsFadesToggle.IsValid()
            && EventsVirtualizationToggle.IsValid() && EventsLifecycleToggle.IsValid()
            && Click(Slate, HeldEventsStateToggle.ToSharedRef())
            && NOT DebuggerWindow->_EventsShowStateChanges && DebuggerWindow->_EventsShowFades
            && Click(Slate, EventsFadesToggle.ToSharedRef())
            && NOT DebuggerWindow->_EventsShowFades && DebuggerWindow->_EventsShowVirtualization
            && Click(Slate, EventsVirtualizationToggle.ToSharedRef())
            && NOT DebuggerWindow->_EventsShowVirtualization && DebuggerWindow->_EventsShowLifecycle
            && Click(Slate, EventsLifecycleToggle.ToSharedRef())
            && NOT DebuggerWindow->_EventsShowLifecycle))
    { return false; }
    HeldEventsStateToggle->ToggleCheckedState();
    TestTrue(TEXT("held Events control dispatch probe changes a live owner preference"),
        DebuggerWindow->_EventsShowStateChanges && HeldEventsStateToggle->IsChecked());
    HeldEventsStateToggle->ToggleCheckedState();
    DebuggerWindow->HandleSessionInvalidated();
    TestTrue(TEXT("session invalidation preserves the four window recording preferences"),
        NOT DebuggerWindow->_EventsShowStateChanges && NOT DebuggerWindow->_EventsShowFades
            && NOT DebuggerWindow->_EventsShowVirtualization && NOT DebuggerWindow->_EventsShowLifecycle);

    const TSharedPtr<SButton> EmptySpatialTab = FindButtonWithText(GetTabs().ToSharedRef(), TEXT("Spatial"));
    if (NOT TestTrue(TEXT("production Spatial tab is physically selectable without a session"),
        EmptySpatialTab.IsValid() && Click(Slate, EmptySpatialTab.ToSharedRef())))
    { return false; }
    const TSharedPtr<SWidget> EmptySpatialPlots = FindTaggedWidget(
        DebuggerWindow->_PageSwitcher.ToSharedRef(), TEXT("audio-spatial-plots"));
    const TSharedPtr<SWidget> EmptySpatialMessage = FindTaggedWidget(
        DebuggerWindow->_PageSwitcher.ToSharedRef(), TEXT("audio-spatial-unavailable-wrap"));
    TestTrue(TEXT("unavailable Spatial state hides both plots and explains the missing track"),
        DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 3
            && EmptySpatialPlots.IsValid() && EmptySpatialPlots->GetVisibility() == EVisibility::Collapsed
            && EmptySpatialMessage.IsValid() && EmptySpatialMessage->GetVisibility().IsVisible()
            && TaggedText(SpatialPageView->GetRegion(TEXT("main")), TEXT("audio-spatial-unavailable")).Contains(TEXT("No track to inspect")));

    const TSharedPtr<SButton> CrossfadeTab = FindButtonWithText(
        GetTabs().ToSharedRef(), TEXT("Crossfade"));
    TestTrue(TEXT("physical nondefault tab selection routes through the production tab strip"),
        CrossfadeTab.IsValid() && Click(Slate, CrossfadeTab.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2
            && ContainsWidget(DebuggerWindow->_CrossfadePageHost.ToSharedRef(), CrossfadeView->GetRegion(TEXT("main"))));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Css;
    FString CrossfadeMarkup;
    FString CrossfadeCss;
    FString AttenuationMarkup;
    FString AttenuationCss;
    FString EventsMarkup;
    FString DirectorsMarkup;
    FString DirectorsCss;
    FString TracksMarkup;
    FString TracksCss;
    FString SpatialMarkup;
    FString SpatialCss;
    FString EventsCss;
    const FString Directory = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"))
        : FString{};
    if (NOT TestTrue(TEXT("installed Audio shell resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(Css, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.css")))
        && FFileHelper::LoadFileToString(CrossfadeMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.html")))
        && FFileHelper::LoadFileToString(CrossfadeCss, *FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.css")))
        && FFileHelper::LoadFileToString(AttenuationMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerAttenuation.ui.html")))
        && FFileHelper::LoadFileToString(AttenuationCss, *FPaths::Combine(Directory, TEXT("AudioDebuggerAttenuation.ui.css")))
        && FFileHelper::LoadFileToString(EventsMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerEventsToolbar.ui.html")))
        && FFileHelper::LoadFileToString(EventsCss, *FPaths::Combine(Directory, TEXT("AudioDebuggerEventsToolbar.ui.css")))
        && FFileHelper::LoadFileToString(DirectorsMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerDirectors.ui.html")))
        && FFileHelper::LoadFileToString(DirectorsCss, *FPaths::Combine(Directory, TEXT("AudioDebuggerDirectors.ui.css")))
        && FFileHelper::LoadFileToString(TracksMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerTracks.ui.html")))
        && FFileHelper::LoadFileToString(TracksCss, *FPaths::Combine(Directory, TEXT("AudioDebuggerTracks.ui.css")))
        && FFileHelper::LoadFileToString(SpatialMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerSpatial.ui.html")))
        && FFileHelper::LoadFileToString(SpatialCss, *FPaths::Combine(Directory, TEXT("AudioDebuggerSpatial.ui.css")))))
    { return false; }

    TestTrue(TEXT("Audio resource declares typed tabs without the old opaque native tab binding"),
        Markup.Contains(TEXT("<debug-tabs id=\"audio-shell-tabs\""))
            && NOT Markup.Contains(TEXT("<native id=\"audio-shell-tabs\"")));
    const auto Revision = View->GetRevision();
    const FCkUiLoadResult Reloaded = View->TryReload(Markup, Css, TEXT("Audio compatible shell candidate"));
    TickSlate(Slate);
    if (NOT Reloaded.Succeeded) { AddError(FString::Join(Reloaded.Errors, TEXT("\n"))); }
    TestTrue(TEXT("compatible reload retains native boundaries, authored summary ownership and page state"),
        Reloaded.Succeeded && View->GetRevision() > Revision && GetTabs() == OriginalTabs
            && ContainsWidget(View->GetRegion(TEXT("main")), GetTabs().ToSharedRef())
            && NOT ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_StatCards.ToSharedRef())
            && TaggedText(View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency-label")) == TEXT("Active / max")
            && ContainsWidget(View->GetRegion(TEXT("main")), HeldFilterSearch.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);
    const int64 TabsRevision = View->GetRevision();
    const auto TabsRejected = View->TryReload(
        Markup.Replace(TEXT("changed=\"audio-select-page\""), TEXT("changed=\"missing-page-route\"")), Css);
    TestTrue(TEXT("missing typed tab route rejects the entire shell without losing tab identity or page state"),
        NOT TabsRejected.Succeeded && View->GetRevision() == TabsRevision && GetTabs() == OriginalTabs
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);

    TestTrue(TEXT("compatible shell reload retains search text, highlight and all filter preferences"),
        DebuggerWindow->_FilterSearchBar == HeldFilterSearch && HeldFilterSearch->Get_SearchText() == TEXT("audio")
            && DebuggerWindow->_FilterString == TEXT("audio") && DebuggerWindow->_HighlightString == TEXT("audio")
            && NOT DebuggerWindow->_ShowPlaying && NOT DebuggerWindow->_ShowFading
            && NOT DebuggerWindow->_ShowStopped && NOT DebuggerWindow->_GroupByDirector);
    for (const auto& Toggle : HeldFilterToggles)
    { TestTrue(TEXT("compatible shell reload retains each filter control"), ContainsWidget(Main, Toggle.ToSharedRef())); }
    const TArray<FString> FilterPortNames{TEXT("search"), TEXT("playing"), TEXT("fading"), TEXT("stopped"), TEXT("group")};
    for (const FString& Port : FilterPortNames)
    {
        const FString Native = FString::Printf(TEXT("<native id=\"audio-filter-%s\" bind=\"audio-filter-%s\" />"), *Port, *Port);
        const FString MissingPortMarkup = Markup.Replace(*Native, TEXT(""));
        if (NOT TestTrue(TEXT("filter rejection fixture actually omits its required port"), MissingPortMarkup != Markup))
        { return false; }
        const int64 BeforeRejection = View->GetRevision();
        const FCkUiLoadResult FilterRejected = View->TryReload(MissingPortMarkup, Css, TEXT("Audio missing filter port"));
        TestTrue(TEXT("omitting any filter port rejects the entire shell without changing input state"),
            NOT FilterRejected.Succeeded && View->GetRevision() == BeforeRejection
                && FString::Join(FilterRejected.Errors, TEXT("\n")).Contains(TEXT("audio-filter-") + Port)
                && ContainsWidget(Main, HeldFilterSearch.ToSharedRef())
                && HeldFilterSearch->Get_SearchText() == TEXT("audio") && DebuggerWindow->_FilterString == TEXT("audio")
                && DebuggerWindow->_HighlightString == TEXT("audio") && NOT DebuggerWindow->_ShowPlaying
                && NOT DebuggerWindow->_ShowFading && NOT DebuggerWindow->_ShowStopped && NOT DebuggerWindow->_GroupByDirector);
        for (const auto& Toggle : HeldFilterToggles)
        { TestTrue(TEXT("rejected shell candidate retains all four filter controls"), ContainsWidget(Main, Toggle.ToSharedRef())); }
    }

    const int64 CrossfadeRevision = CrossfadeView->GetRevision();
    const TSharedPtr<TArray<float>> SeriesA = DebuggerWindow->_CrossfadeSeriesA;
    const TSharedPtr<TArray<float>> SeriesB = DebuggerWindow->_CrossfadeSeriesB;
    const FCkUiLoadResult CrossfadeReloaded = CrossfadeView->TryReload(
        CrossfadeMarkup, CrossfadeCss, TEXT("Audio compatible Crossfade candidate"));
    TickSlate(Slate);
    if (NOT CrossfadeReloaded.Succeeded) { AddError(FString::Join(CrossfadeReloaded.Errors, TEXT("\n"))); }
    TestTrue(TEXT("compatible Crossfade reload retains plot, series ownership and selected page"),
        CrossfadeReloaded.Succeeded && CrossfadeView->GetRevision() > CrossfadeRevision
            && DebuggerWindow->_CrossfadeSeriesA == SeriesA
            && DebuggerWindow->_CrossfadeSeriesB == SeriesB
            && ContainsWidget(CrossfadeView->GetRegion(TEXT("main")), DebuggerWindow->_CrossfadePagePlot.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);
    const int64 CrossfadeRevisionBeforeReject = CrossfadeView->GetRevision();
    const FCkUiLoadResult CrossfadeRejected = CrossfadeView->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-crossfade-plot\"/></region></ui>"),
        TEXT(""), TEXT("Audio rejected Crossfade candidate"));
    TestFalse(TEXT("missing Crossfade plot is rejected atomically"), CrossfadeRejected.Succeeded);
    TestTrue(TEXT("rejected Crossfade reload keeps the accepted page and exact plot"),
        CrossfadeView->GetRevision() == CrossfadeRevisionBeforeReject
            && ContainsWidget(CrossfadeView->GetRegion(TEXT("main")), DebuggerWindow->_CrossfadePagePlot.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);

    const auto RevisionBeforeReject = View->GetRevision();
    const FCkUiLoadResult Rejected = View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-audio-port\"/></region></ui>"),
        TEXT(""), TEXT("Audio rejected shell candidate"));
    TestFalse(TEXT("missing Audio native port is rejected atomically"), Rejected.Succeeded);
    TestTrue(TEXT("rejection reaches missing-binding admission"),
        FString::Join(Rejected.Errors, TEXT("\n")).Contains(TEXT("missing-audio-port")));
    TestTrue(TEXT("rejected reload leaves the committed shell and page state intact"),
        View->GetRevision() == RevisionBeforeReject
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);

    const int64 EventsRevision = EventsView->GetRevision();
    const FCkUiLoadResult EventsReloaded = EventsView->TryReload(
        EventsMarkup, EventsCss, TEXT("Audio compatible Events toolbar candidate"));
    TestTrue(TEXT("compatible Events reload retains the toolbar host, event log and recording preferences"),
        EventsReloaded.Succeeded && EventsView->GetRevision() > EventsRevision
            && DebuggerWindow->_EventLog == OriginalEventLog
            && ContainsWidget(DebuggerWindow->_EventsToolbarHost.ToSharedRef(), EventsMain)
            && NOT DebuggerWindow->_EventsShowStateChanges && NOT DebuggerWindow->_EventsShowFades
            && NOT DebuggerWindow->_EventsShowVirtualization && NOT DebuggerWindow->_EventsShowLifecycle);
    for (const auto& Toggle : OriginalEventToggles)
    { TestTrue(TEXT("compatible Events reload retains every native toggle"), ContainsWidget(EventsMain, Toggle.ToSharedRef())); }
    const TArray<FString> EventsPortNames{TEXT("state"), TEXT("fades"), TEXT("virtualization"), TEXT("lifecycle")};
    for (const FString& Port : EventsPortNames)
    {
        const FString Native = FString::Printf(TEXT("<native id=\"audio-events-%s\" bind=\"events-%s\" />"), *Port, *Port);
        const FString MissingPortMarkup = EventsMarkup.Replace(*Native, TEXT(""));
        if (NOT TestTrue(TEXT("missing Events port candidate actually removes one production port"), MissingPortMarkup != EventsMarkup))
        { return false; }
        const int64 BeforeRejection = EventsView->GetRevision();
        const FCkUiLoadResult PortRejected = EventsView->TryReload(
            MissingPortMarkup, EventsCss, TEXT("Audio missing Events toolbar port"));
        TestTrue(TEXT("omitting any Events port rejects the entire candidate before mutation"),
            NOT PortRejected.Succeeded && EventsView->GetRevision() == BeforeRejection
                && FString::Join(PortRejected.Errors, TEXT("\n")).Contains(TEXT("events-") + Port)
                && DebuggerWindow->_EventLog == OriginalEventLog);
        for (const auto& Toggle : OriginalEventToggles)
        { TestTrue(TEXT("rejected Events reload keeps all four committed controls"), ContainsWidget(EventsMain, Toggle.ToSharedRef())); }
    }

    HostWindow->Resize(FVector2D{360.0f, 480.0f});
    TickSlate(Slate);
    TestTrue(TEXT("actual narrow Audio shell keeps horizontal overflow reachable"),
        View->GetScroll(TEXT("audio-shell-scroll"))->GetScrollOffsetOfEnd() > 0.0f);
    const auto Overflow = StaticCastSharedPtr<SComboButton>(
        FindTaggedWidget(OriginalTabs.ToSharedRef(), TEXT("CkDebug.Tabs.Overflow")));
    if (NOT TestTrue(TEXT("narrow Audio tabs receive window width and expose the actual overflow control"),
        OriginalTabs->GetCachedGeometry().GetLocalSize().X < 400.0f && Overflow.IsValid()
            && Overflow->GetVisibility() == EVisibility::Visible && Click(Slate, Overflow.ToSharedRef()) && Overflow->IsOpen()))
    { return false; }
    const auto OverflowMenu = OriginalTabs->GetPopupFocusTarget();
    const auto OverflowOverlay = OverflowMenu.IsValid()
        ? FindButtonWithText(OverflowMenu.ToSharedRef(), TEXT("Overlay")) : nullptr;
    if (NOT TestTrue(TEXT("physical overflow selection reaches Overlay and makes it the visible active tab"),
        OverflowOverlay.IsValid() && Click(Slate, OverflowOverlay.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 5 && NOT Overflow->IsOpen()
            && FindButtonWithText(OriginalTabs.ToSharedRef(), TEXT("Overlay")).IsValid()))
    { return false; }

    HostWindow->Resize(FVector2D{1100.0f, 240.0f});
    TickSlate(Slate);
    TestTrue(TEXT("short Audio shell lets its page body shrink without vertical clipping pressure"),
        DebuggerWindow->_PageSwitcher->GetCachedGeometry().GetLocalSize().Y > 0.0f
            && DebuggerWindow->_PageSwitcher->GetCachedGeometry().GetLocalSize().Y < 320.0f);

    HostWindow->Resize(FVector2D{360.0f, 480.0f});
    TickSlate(Slate);
    if (NOT TestTrue(TEXT("Audio overflow reopens for retained reload and owner release"),
        Click(Slate, Overflow.ToSharedRef()) && Overflow->IsOpen())) { return false; }
    const auto HeldTabsMenu = OriginalTabs->GetPopupFocusTarget();
    const auto HeldMenuEvents = HeldTabsMenu.IsValid()
        ? FindButtonWithText(HeldTabsMenu.ToSharedRef(), TEXT("Events")) : nullptr;
    if (NOT TestTrue(TEXT("open tab menu survives a compatible production shell reload"),
        HeldMenuEvents.IsValid() && View->TryReload(Markup, Css).Succeeded
            && GetTabs() == OriginalTabs && Overflow->IsOpen()
            && OriginalTabs->GetPopupFocusTarget() == HeldTabsMenu
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 5)) { return false; }

    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    TickSlate(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    const TWeakPtr<FCkUiView> ReleasedCrossfadeView = CrossfadeView;
    const TWeakPtr<FCkUiView> ReleasedAttenuationView = AttenuationView;
    const TWeakPtr<FCkUiView> ReleasedEventsView = EventsView;
    const TWeakPtr<FCkUiView> ReleasedDirectorsView = DirectorsView;
    const TWeakPtr<FCkUiView> ReleasedTracksView = TracksView;
    const TWeakPtr<FCkUiView> ReleasedSpatialView = SpatialPageView;
    const TWeakPtr<SCkAudioDebuggerWindow> ReleasedEventsOwner = DebuggerWindow;
    DebuggerWindow.Reset();
    View.Reset();
    CrossfadeView.Reset();
    AttenuationView.Reset();
    EventsView.Reset();
    DirectorsView.Reset();
    TracksView.Reset();
    SpatialPageView.Reset();
    TestFalse(TEXT("authored Audio view releases with its production window"), ReleasedView.IsValid());
    TestFalse(TEXT("authored Crossfade view releases with its production window"), ReleasedCrossfadeView.IsValid());
    TestFalse(TEXT("authored attenuation view releases with its production window"), ReleasedAttenuationView.IsValid());
    TestFalse(TEXT("retained Events controls do not retain their production owner"), ReleasedEventsOwner.IsValid());
    TestFalse(TEXT("authored Events view releases with its production window"), ReleasedEventsView.IsValid());
    TestFalse(TEXT("authored Directors view releases with its production window"), ReleasedDirectorsView.IsValid());
    TestFalse(TEXT("authored Tracks view releases with its production window"), ReleasedTracksView.IsValid());
    TestFalse(TEXT("authored Spatial view releases with its production window"), ReleasedSpatialView.IsValid());
    TestTrue(TEXT("Audio owner release closes the owned tab popup and revokes retained tab dispatch"),
        NOT OriginalTabs->GetCanDispatchEvents() && NOT Overflow->IsOpen()
            && NOT OriginalTabs->GetPopupFocusTarget().IsValid());
    HostWindow = SNew(SWindow).ClientSize(FVector2D{1100.0f, 420.0f}).CreateTitleBar(false).HasCloseButton(false)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[OriginalTabs.ToSharedRef()]
            + SVerticalBox::Slot().AutoHeight()[HeldTabsMenu.ToSharedRef()]];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);
    Click(Slate, HeldMenuEvents.ToSharedRef());
    const auto HeldStripOverlay = FindTaggedWidget(OriginalTabs.ToSharedRef(), TEXT("CkDebug.Tab.Overlay"));
    if (HeldStripOverlay.IsValid()) { Click(Slate, HeldStripOverlay.ToSharedRef()); }
    TestTrue(TEXT("physically remounted held Audio strip and menu remain disabled without retaining their owner"),
        NOT ReleasedEventsOwner.IsValid() && NOT HeldMenuEvents->IsEnabled()
            && HeldStripOverlay.IsValid() && NOT HeldStripOverlay->IsEnabled());
    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    // IsEnabled reads a cached TSlateAttribute, unlike the checkbox's live TAttribute checked binding. This held
    // control is detached, so refresh its attributes explicitly rather than expecting a destroyed window to prepass it.
    HeldEventsStateToggle->Invalidate(EInvalidateWidgetReason::Prepass);
    HeldEventsStateToggle->SlatePrepass(1.0f);
    TestFalse(TEXT("released Events control becomes disabled when its Slate attributes update"),
        HeldEventsStateToggle->IsEnabled());
    HeldEventsStateToggle->ToggleCheckedState();
    TestFalse(TEXT("held Events dispatch probe remains unchecked after owner release"), HeldEventsStateToggle->IsChecked());
    HeldFilterSearch->Set_SearchText(TEXT("released filter probe"));
    HeldFilterSearch->Invalidate(EInvalidateWidgetReason::Prepass);
    HeldFilterSearch->SlatePrepass(1.0f);
    TestTrue(TEXT("held search callback cannot retain or dispatch to the released Audio owner"),
        NOT ReleasedEventsOwner.IsValid() && NOT ReleasedView.IsValid() && NOT HeldFilterSearch->IsEnabled());
    const TArray<TSharedPtr<SCheckBox>> HeldFilterChecks{
        HeldPlayingFilter, HeldFadingFilter, HeldStoppedFilter, HeldGroupFilter};
    for (const auto& Toggle : HeldFilterChecks)
    {
        Toggle->ToggleCheckedState();
        Toggle->Invalidate(EInvalidateWidgetReason::Prepass);
        Toggle->SlatePrepass(1.0f);
        TestTrue(TEXT("held filter toggle is disabled and cannot change state after owner release"),
            NOT Toggle->IsEnabled() && NOT Toggle->IsChecked());
    }

    const FString InvalidStartupMarkup =
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-audio-port\"/></region></ui>");
    const FString InvalidCrossfadeStartupMarkup =
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-crossfade-plot\"/></region></ui>");
    const FString InvalidAttenuationStartupMarkup =
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-attenuation-curve\"/></region></ui>");
    const FString InvalidEventsStartupMarkup = EventsMarkup.Replace(
        TEXT("<native id=\"audio-events-lifecycle\" bind=\"events-lifecycle\" />"), TEXT(""));
    const FString InvalidDirectorsStartupMarkup = DirectorsMarkup.Replace(
        TEXT("bind=\"audio-directors\""), TEXT("bind=\"missing-audio-directors\""));
    const FString InvalidTracksStartupMarkup = TracksMarkup.Replace(
        TEXT("bind=\"audio-tracks\""), TEXT("bind=\"missing-audio-tracks\""));
    const FString InvalidSpatialStartupMarkup = SpatialMarkup.Replace(
        TEXT("bind=\"audio-spatial-attenuation\""), TEXT("bind=\"missing-spatial-attenuation\""));
    if (NOT TestTrue(TEXT("Events startup fixture omits one required native port"), InvalidEventsStartupMarkup != EventsMarkup))
    { return false; }
    bool MarkupRestored = false;
    bool CrossfadeMarkupRestored = false;
    bool AttenuationMarkupRestored = false;
    bool EventsMarkupRestored = false;
    bool DirectorsMarkupRestored = false;
    bool TracksMarkupRestored = false;
    bool SpatialMarkupRestored = false;
    ON_SCOPE_EXIT
    {
        if (NOT MarkupRestored)
        { FFileHelper::SaveStringToFile(Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html"))); }
        if (NOT CrossfadeMarkupRestored)
        { FFileHelper::SaveStringToFile(CrossfadeMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.html"))); }
        if (NOT AttenuationMarkupRestored)
        { FFileHelper::SaveStringToFile(AttenuationMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerAttenuation.ui.html"))); }
        if (NOT EventsMarkupRestored)
        { FFileHelper::SaveStringToFile(EventsMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerEventsToolbar.ui.html"))); }
        if (NOT DirectorsMarkupRestored)
        { FFileHelper::SaveStringToFile(DirectorsMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerDirectors.ui.html"))); }
        if (NOT TracksMarkupRestored)
        { FFileHelper::SaveStringToFile(TracksMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerTracks.ui.html"))); }
        if (NOT SpatialMarkupRestored)
        { FFileHelper::SaveStringToFile(SpatialMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerSpatial.ui.html"))); }
    };
    if (NOT TestTrue(TEXT("Audio fixture installs its valid-but-unbound startup candidate"),
        FFileHelper::SaveStringToFile(
            InvalidStartupMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")))
            && FFileHelper::SaveStringToFile(InvalidCrossfadeStartupMarkup,
                *FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.html")))
            && FFileHelper::SaveStringToFile(InvalidAttenuationStartupMarkup,
                *FPaths::Combine(Directory, TEXT("AudioDebuggerAttenuation.ui.html")))
            && FFileHelper::SaveStringToFile(InvalidEventsStartupMarkup,
                *FPaths::Combine(Directory, TEXT("AudioDebuggerEventsToolbar.ui.html")))
            && FFileHelper::SaveStringToFile(InvalidDirectorsStartupMarkup,
                *FPaths::Combine(Directory, TEXT("AudioDebuggerDirectors.ui.html")))
            && FFileHelper::SaveStringToFile(InvalidTracksStartupMarkup,
                *FPaths::Combine(Directory, TEXT("AudioDebuggerTracks.ui.html")))
            && FFileHelper::SaveStringToFile(InvalidSpatialStartupMarkup,
                *FPaths::Combine(Directory, TEXT("AudioDebuggerSpatial.ui.html")))))
    { return false; }

    DebuggerWindow = SNew(SCkAudioDebuggerWindow);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1100.0f, 720.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [DebuggerWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);
    View = DebuggerWindow->_AuthoredShellView;
    const auto StartupTabs = GetTabs();
    TestTrue(TEXT("invalid startup resource mounts the complete native fallback"),
        View.IsValid() && NOT View->GetLastResult().Succeeded
            && DebuggerWindow->_UsingNativeFallback
            && ContainsWidget(DebuggerWindow->_AuthoredShellHost.ToSharedRef(),
                DebuggerWindow->_PageSwitcher.ToSharedRef())
            && NOT DebuggerWindow->_AuthoredAttenuationView.IsValid()
            && ContainsWidget(DebuggerWindow->_AttenuationPanelHost.ToSharedRef(),
                DebuggerWindow->_AttenuationCurve.ToSharedRef()));
    TestTrue(TEXT("outer startup fallback leaves Events controls mounted without competing child ownership"),
        NOT DebuggerWindow->_AuthoredEventsToolbarView.IsValid()
            && ContainsWidget(DebuggerWindow->_EventsToolbarHost.ToSharedRef(), DebuggerWindow->_EventsStateToggle.ToSharedRef())
            && ContainsWidget(DebuggerWindow->_EventsToolbarHost.ToSharedRef(), DebuggerWindow->_EventsFadesToggle.ToSharedRef())
            && ContainsWidget(DebuggerWindow->_EventsToolbarHost.ToSharedRef(), DebuggerWindow->_EventsVirtualizationToggle.ToSharedRef())
            && ContainsWidget(DebuggerWindow->_EventsToolbarHost.ToSharedRef(), DebuggerWindow->_EventsLifecycleToggle.ToSharedRef()));

    const TSharedPtr<SCkDebug_SearchBar> FallbackFilterSearch = DebuggerWindow->_FilterSearchBar;
    const TArray<TSharedPtr<SCkDebug_ToggleSurface>> FallbackFilterToggles{
        DebuggerWindow->_FilterPlayingToggle, DebuggerWindow->_FilterFadingToggle,
        DebuggerWindow->_FilterStoppedToggle, DebuggerWindow->_FilterGroupToggle};
    const TWeakPtr<SWidget> FallbackFilterParent = FallbackFilterSearch->GetParentWidget();
    TestTrue(TEXT("native startup fallback owns the original search control"),
        FallbackFilterParent.IsValid()
            && ContainsWidget(DebuggerWindow->_AuthoredShellHost.ToSharedRef(), FallbackFilterSearch.ToSharedRef()));
    for (const auto& Toggle : FallbackFilterToggles)
    { TestTrue(TEXT("native startup fallback owns every filter toggle"), ContainsWidget(DebuggerWindow->_AuthoredShellHost.ToSharedRef(), Toggle.ToSharedRef())); }
    const TSharedPtr<SEditableText> FallbackFilterEditable = FindEditableText(FallbackFilterSearch.ToSharedRef());
    const TSharedPtr<SCheckBox> FallbackPlayingFilter = FindCheckBoxWithText(FallbackFilterToggles[0].ToSharedRef(), TEXT("Playing"));
    if (NOT TestTrue(TEXT("native startup fallback preserves physical search and toggle behavior"),
        FallbackFilterEditable.IsValid() && FallbackPlayingFilter.IsValid()
            && ReplaceSearchText(Slate, FallbackFilterEditable.ToSharedRef(), TEXT("fallback"))
            && DebuggerWindow->_FilterString == TEXT("fallback") && DebuggerWindow->_HighlightString == TEXT("fallback")
            && Click(Slate, FallbackPlayingFilter.ToSharedRef()) && NOT DebuggerWindow->_ShowPlaying))
    { return false; }

    const auto StartupSpatial = FindButtonWithText(StartupTabs.ToSharedRef(), TEXT("Spatial"));
    if (NOT TestTrue(TEXT("native startup tab fallback physically selects a nondefault page"),
        StartupSpatial.IsValid() && Click(Slate, StartupSpatial.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 3)) { return false; }
    MarkupRestored = FFileHelper::SaveStringToFile(
        Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")));
    TestTrue(TEXT("Audio fixture restores the valid production resource"), MarkupRestored);
    StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Small;
    StyleSettings->NotifyChanged();
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    TestTrue(TEXT("normal style-revision tick recovers the outer shell and admits the still-invalid Crossfade fallback"),
        View->GetLastResult().Succeeded && NOT DebuggerWindow->_UsingNativeFallback
            && DebuggerWindow->_AuthoredCrossfadeView.IsValid()
            && NOT DebuggerWindow->_AuthoredCrossfadeView->GetLastResult().Succeeded
            && DebuggerWindow->_UsingNativeCrossfadeFallback
            && ContainsWidget(DebuggerWindow->_CrossfadePageHost.ToSharedRef(), DebuggerWindow->_CrossfadePagePlot.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), GetTabs().ToSharedRef())
            && NOT ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_StatCards.ToSharedRef())
            && TaggedText(View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency-label")) == TEXT("Active / max")
            && ContainsWidget(View->GetRegion(TEXT("main")), FallbackFilterSearch.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef()));
    TestTrue(TEXT("authored recovery takes over page state and revokes the detached native tab strip"),
        GetTabs() != StartupTabs && NOT StartupTabs->GetCanDispatchEvents()
            && NOT ContainsWidget(View->GetRegion(TEXT("main")), StartupTabs.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 3);
    TestTrue(TEXT("shell recovery releases the fallback-only parent and preserves filter identity and state"),
        NOT FallbackFilterParent.IsValid() && DebuggerWindow->_FilterSearchBar == FallbackFilterSearch
            && FallbackFilterSearch->Get_SearchText() == TEXT("fallback")
            && DebuggerWindow->_FilterString == TEXT("fallback") && DebuggerWindow->_HighlightString == TEXT("fallback")
            && NOT DebuggerWindow->_ShowPlaying);
    for (const auto& Toggle : FallbackFilterToggles)
    { TestTrue(TEXT("recovered authored shell owns all original filter controls"), ContainsWidget(View->GetRegion(TEXT("main")), Toggle.ToSharedRef())); }
    if (NOT TestTrue(TEXT("recovered authored controls physically clear search and restore Playing"),
        ReplaceSearchText(Slate, FallbackFilterEditable.ToSharedRef(), TEXT(""))
            && DebuggerWindow->_FilterString.IsEmpty() && DebuggerWindow->_HighlightString.IsEmpty()
            && Click(Slate, FallbackPlayingFilter.ToSharedRef()) && DebuggerWindow->_ShowPlaying))
    { return false; }
    AttenuationView = DebuggerWindow->_AuthoredAttenuationView;
    if (NOT TestTrue(TEXT("outer recovery independently leaves invalid attenuation in its native fallback"),
        AttenuationView.IsValid() && NOT AttenuationView->GetLastResult().Succeeded
            && DebuggerWindow->_UsingNativeAttenuationFallback
            && ContainsWidget(DebuggerWindow->_AttenuationPanelHost.ToSharedRef(),
                DebuggerWindow->_AttenuationCurve.ToSharedRef())))
    { return false; }
    CrossfadeMarkupRestored = FFileHelper::SaveStringToFile(
        CrossfadeMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.html")));
    TestTrue(TEXT("Audio fixture restores the valid Crossfade resource"), CrossfadeMarkupRestored);
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    CrossfadeView = DebuggerWindow->_AuthoredCrossfadeView;
    TestTrue(TEXT("bounded Crossfade polling recovers from native startup fallback with the exact retained plot"),
        CrossfadeView.IsValid() && CrossfadeView->GetLastResult().Succeeded
            && NOT DebuggerWindow->_UsingNativeCrossfadeFallback
            && ContainsWidget(CrossfadeView->GetRegion(TEXT("main")), DebuggerWindow->_CrossfadePagePlot.ToSharedRef())
            && TaggedText(CrossfadeView->GetRegion(TEXT("main")), TEXT("audio-crossfade-title")) == TEXT("Crossfade lane"));
    TestTrue(TEXT("Crossfade recovery does not admit the invalid attenuation sibling"),
        DebuggerWindow->_UsingNativeAttenuationFallback && NOT AttenuationView->GetLastResult().Succeeded);
    const TSharedPtr<SCkAudioDebugger_FalloffCurve> FallbackCurve = DebuggerWindow->_AttenuationCurve;
    AttenuationMarkupRestored = FFileHelper::SaveStringToFile(
        AttenuationMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerAttenuation.ui.html")));
    if (NOT TestTrue(TEXT("Audio fixture restores the valid attenuation resource"), AttenuationMarkupRestored))
    { return false; }
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    if (NOT TestTrue(TEXT("bounded attenuation polling recovers the exact native curve from startup fallback"),
        AttenuationView->GetLastResult().Succeeded && NOT DebuggerWindow->_UsingNativeAttenuationFallback
            && DebuggerWindow->_AttenuationCurve == FallbackCurve
            && ContainsWidget(AttenuationView->GetRegion(TEXT("main")), FallbackCurve.ToSharedRef())
            && TaggedText(AttenuationView->GetRegion(TEXT("main")), TEXT("audio-attenuation-audible-label")) == TEXT("Audible")))
    { return false; }

    EventsView = DebuggerWindow->_AuthoredEventsToolbarView;
    if (NOT TestTrue(TEXT("sibling recovery leaves the invalid Events toolbar in a complete native fallback"),
        EventsView.IsValid() && NOT EventsView->GetLastResult().Succeeded
            && DebuggerWindow->_UsingNativeEventsToolbarFallback
            && FString::Join(EventsView->GetLastResult().Errors, TEXT("\n")).Contains(TEXT("events-lifecycle"))))
    { return false; }
    const TArray<TSharedPtr<SCkDebug_ToggleSurface>> LiveEventToggles{
        DebuggerWindow->_EventsStateToggle, DebuggerWindow->_EventsFadesToggle,
        DebuggerWindow->_EventsVirtualizationToggle, DebuggerWindow->_EventsLifecycleToggle};
    const TSharedPtr<SCkDebug_EventLog> LiveEventLog = DebuggerWindow->_EventLog;
    const TSharedPtr<SButton> FallbackEventsTab = FindButtonWithText(GetTabs().ToSharedRef(), TEXT("Events"));
    const TSharedPtr<SCheckBox> LiveEventsStateToggle = FindCheckBoxWithText(
        DebuggerWindow->_EventsToolbarHost.ToSharedRef(), TEXT("State"));
    if (NOT TestTrue(TEXT("native Events startup fallback accepts physical preference input"),
        FallbackEventsTab.IsValid() && Click(Slate, FallbackEventsTab.ToSharedRef())
            && LiveEventsStateToggle.IsValid() && Click(Slate, LiveEventsStateToggle.ToSharedRef())
            && NOT DebuggerWindow->_EventsShowStateChanges))
    { return false; }
    for (const auto& Toggle : LiveEventToggles)
    { TestTrue(TEXT("Events fallback retains all four controls"), ContainsWidget(DebuggerWindow->_EventsToolbarHost.ToSharedRef(), Toggle.ToSharedRef())); }
    EventsMarkupRestored = FFileHelper::SaveStringToFile(
        EventsMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerEventsToolbar.ui.html")));
    if (NOT TestTrue(TEXT("Audio fixture restores the valid Events toolbar resource"), EventsMarkupRestored))
    { return false; }
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    if (NOT TestTrue(TEXT("Events polling recovers authored composition without replacing controls or preferences"),
        EventsView->GetLastResult().Succeeded && NOT DebuggerWindow->_UsingNativeEventsToolbarFallback
            && NOT DebuggerWindow->_EventsShowStateChanges && DebuggerWindow->_EventLog == LiveEventLog
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 4
            && TaggedText(EventsView->GetRegion(TEXT("main")), TEXT("audio-events-caveat"))
                == TEXT("sampled at the refresh rate — sub-tick transitions are not captured")))
    { return false; }
    for (const auto& Toggle : LiveEventToggles)
    { TestTrue(TEXT("recovered Events toolbar retains every exact native control"), ContainsWidget(EventsView->GetRegion(TEXT("main")), Toggle.ToSharedRef())); }

    DirectorsView = DebuggerWindow->_AuthoredDirectorsView;
    if (NOT TestTrue(TEXT("invalid Directors startup resource preserves the independent native page"),
        DirectorsView.IsValid() && NOT DirectorsView->GetLastResult().Succeeded
            && DebuggerWindow->_UsingNativeDirectorsFallback
            && ContainsWidget(DebuggerWindow->_DirectorsPageHost.ToSharedRef(), DebuggerWindow->_NativeDirectorsPage.ToSharedRef())))
    { return false; }
    DirectorsMarkupRestored = FFileHelper::SaveStringToFile(
        DirectorsMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerDirectors.ui.html")));
    if (NOT TestTrue(TEXT("Directors fixture restores its authored resource"), DirectorsMarkupRestored)) { return false; }
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    if (NOT TestTrue(TEXT("bounded Directors polling recovers and retires native page ownership"),
        DirectorsView->GetLastResult().Succeeded && NOT DebuggerWindow->_UsingNativeDirectorsFallback
            && NOT ContainsWidget(DebuggerWindow->_DirectorsPageHost.ToSharedRef(), DebuggerWindow->_NativeDirectorsPage.ToSharedRef())
            && DebuggerWindow->_DirectorPageSlots.IsEmpty()))
    { return false; }

    TracksView = DebuggerWindow->_AuthoredTracksView;
    if (NOT TestTrue(TEXT("invalid Tracks startup preserves the independent native page and exact compact plot"),
        TracksView.IsValid() && NOT TracksView->GetLastResult().Succeeded
            && DebuggerWindow->_UsingNativeTracksFallback
            && ContainsWidget(DebuggerWindow->_TracksPageHost.ToSharedRef(), DebuggerWindow->_NativeTracksPage.ToSharedRef())
            && ContainsWidget(DebuggerWindow->_CompactCrossfadeHost.ToSharedRef(), DebuggerWindow->_CompactCrossfadePlot.ToSharedRef())))
    { return false; }
    TracksMarkupRestored = FFileHelper::SaveStringToFile(TracksMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerTracks.ui.html")));
    if (NOT TestTrue(TEXT("Tracks fixture restores its resource"), TracksMarkupRestored)) { return false; }
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    if (NOT TestTrue(TEXT("bounded Tracks polling recovers and retires native rows"),
        TracksView->GetLastResult().Succeeded && NOT DebuggerWindow->_UsingNativeTracksFallback
            && NOT ContainsWidget(DebuggerWindow->_TracksPageHost.ToSharedRef(), DebuggerWindow->_NativeTracksPage.ToSharedRef())
            && ContainsWidget(TracksView->GetRegion(TEXT("main")), DebuggerWindow->_CompactCrossfadePlot.ToSharedRef())
            && DebuggerWindow->_TrackSlots.IsEmpty()))
    { return false; }

    if (NOT RunFixturePhase([&, this]() -> bool
    {
        SpatialPageView = DebuggerWindow->_AuthoredSpatialView;
        const auto Radar = DebuggerWindow->_Radar;
        const auto Model = DebuggerWindow->_SpatialView;
        if (NOT TestTrue(TEXT("invalid Spatial startup keeps the independent native page and exact plot mounts"),
            SpatialPageView.IsValid() && NOT SpatialPageView->GetLastResult().Succeeded
                && DebuggerWindow->_UsingNativeSpatialFallback
                && ContainsWidget(DebuggerWindow->_SpatialPageHost.ToSharedRef(), DebuggerWindow->_NativeSpatialPage.ToSharedRef())
                && ContainsWidget(DebuggerWindow->_NativeRadarHost.ToSharedRef(), Radar.ToSharedRef())
                && ContainsWidget(DebuggerWindow->_NativeSpatialAttenuationHost.ToSharedRef(), DebuggerWindow->_AttenuationPanelHost.ToSharedRef())))
        { return false; }
        SpatialMarkupRestored = FFileHelper::SaveStringToFile(SpatialMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerSpatial.ui.html")));
        if (NOT TestTrue(TEXT("Spatial fixture restores its authored source"), SpatialMarkupRestored)) { return false; }
        DebuggerWindow->OnStyleRevisionChanged();
        TickSlate(Slate);
        if (NOT TestTrue(TEXT("Spatial polling detaches fallback mounts before retaining the exact Radar and attenuation host"),
            SpatialPageView->GetLastResult().Succeeded && NOT DebuggerWindow->_UsingNativeSpatialFallback
                && DebuggerWindow->_Radar == Radar && DebuggerWindow->_SpatialView == Model
                && ContainsWidget(SpatialPageView->GetRegion(TEXT("main")), Radar.ToSharedRef())
                && ContainsWidget(SpatialPageView->GetRegion(TEXT("main")), DebuggerWindow->_AttenuationPanelHost.ToSharedRef())
                && NOT ContainsWidget(DebuggerWindow->_NativeRadarHost.ToSharedRef(), Radar.ToSharedRef())))
        {
            AddError(FString::Join(SpatialPageView->GetLastResult().Errors, TEXT("\n")));
            return false;
        }
        return true;
    })) { return false; }

    return RunFixturePhase([&, this]() -> bool
    {
    using namespace ck::registry_table;
    auto Registry = EnttRegistryType{};
    const auto RegistrySlot = Allocate(&Registry);
    ON_SCOPE_EXIT
    {
        if (DebuggerWindow.IsValid()) { DebuggerWindow->HandleSessionInvalidated(); }
        Free(RegistrySlot);
    };

    const auto DirectorEntity = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
    const auto OtherDirectorEntity = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
    auto TrackA = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
    auto TrackB = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
    TrackA.Add<ck::FFragment_AudioTrack_Params>();
    TrackA.Add<ck::FFragment_AudioTrack_Current>();
    TrackB.Add<ck::FFragment_AudioTrack_Params>();
    TrackB.Add<ck::FFragment_AudioTrack_Current>();

    auto Director = FCkAudioDebugger_DirectorInfo{};
    Director.DirectorEntity = DirectorEntity;
    Director.DirectorName = TEXT("Same director");
    Director.MaxConcurrentTracks = 4;

    auto& FixtureSnapshot = DebuggerWindow->_Collector._SnapshotOverrideForTests.Emplace();
    FixtureSnapshot.HasWorld = true;
    FixtureSnapshot.Directors.Add(Director);
    DebuggerWindow->_ObservedWorld = DebuggerWindow->DoGet_PieWorld();
    DebuggerWindow->_Collector.Collect(nullptr);
    const auto EmptyDirectorSignature = DebuggerWindow->DoBuild_Signature();
    DebuggerWindow->DoRebuild_Structure();
    DebuggerWindow->DoUpdate_LiveValues();
    const auto DirectorsRepeat = DirectorsView->GetRepeat(TEXT("audio-directors"));
    if (NOT TestTrue(TEXT("empty director publishes a keyed authored row"),
        DirectorsRepeat.IsValid() && DirectorsRepeat->TryRefresh() && DirectorsRepeat->GetItemCount() == 1
            && DebuggerWindow->_DirectorRecords->GetRecords().Num() == 1))
    { return false; }
    const auto DirectorKey = DebuggerWindow->_DirectorRecords->GetRecords()[0]->GetKey();
    const auto DirectorRow = DirectorsRepeat->GetItemWidget(DirectorKey);
    if (NOT TestTrue(TEXT("authored director repeat supplies its real row subtree"), DirectorRow.IsValid())) { return false; }
    TestTrue(TEXT("an empty director participates in the structure signature and dedicated page"),
        NOT EmptyDirectorSignature.IsEmpty()
            && DebuggerWindow->_DirectorSlots.IsEmpty()
            && DebuggerWindow->_DirectorPageSlots.IsEmpty()
            && TaggedText(DirectorRow.ToSharedRef(), TEXT("audio-director-active")) == TEXT("0 / 4 active"));

    auto TrackInfoA = FCkAudioDebugger_TrackInfo{};
    TrackInfoA.TrackEntity = TrackA;
    TrackInfoA.TrackName = TEXT("Same track");
    TrackInfoA.State = ECk_AudioTrack_State::Playing;
    TrackInfoA.CurrentVolume = 0.5f;
    FixtureSnapshot.Directors[0].Tracks.Add(TrackInfoA);
    DebuggerWindow->_Collector.Collect(nullptr);
    const auto TrackAStructureSignature = DebuggerWindow->DoBuild_Signature();
    const auto TrackAAllSignature = DebuggerWindow->DoBuild_AllTracksSignature();
    DebuggerWindow->DoRebuild_Structure();
    DebuggerWindow->DoUpdate_LiveValues();
    const auto TracksRepeat = TracksView->GetRepeat(TEXT("audio-tracks"));
    TracksRepeat->TryRefresh();
    if (NOT TestTrue(TEXT("Tracks projection creates one header and one track with no native positional rows"),
        DebuggerWindow->_TrackRecordsReady && TracksRepeat->GetItemCount() == 2
            && CountTrackRecords(DebuggerWindow->_TrackRecords) == 1
            && DebuggerWindow->_TrackSlots.IsEmpty() && DebuggerWindow->_DirectorSlots.IsEmpty()))
    { return false; }
    const auto TrackHeaderKey = DebuggerWindow->_TrackRecords->GetRecords()[0]->GetKey();
    const auto TrackRecordKey = DebuggerWindow->_TrackRecords->GetRecords()[1]->GetKey();
    const auto TrackHeaderRow = TracksRepeat->GetItemWidget(TrackHeaderKey);
    TestTrue(TEXT("both Audio pages receive the live director concurrency count"),
        DebuggerWindow->_DirectorPageSlots.IsEmpty()
            && TrackHeaderRow.IsValid()
            && TaggedText(TrackHeaderRow.ToSharedRef(), TEXT("audio-track-director-active")) == TEXT("1 / 4 active")
            && TaggedText(DirectorRow.ToSharedRef(), TEXT("audio-director-active")) == TEXT("1 / 4 active")
            && TaggedText(View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency")) == TEXT("1 / 4"));

    const auto DirectorName = FindTaggedTextBlock(DirectorRow.ToSharedRef(), TEXT("audio-director-name"));
    const auto DirectorEntityLink = FindTaggedWidget(DirectorRow.ToSharedRef(), TEXT("audio-director-entity"));
    if (NOT TestTrue(TEXT("authored director owns its highlighted native text leaf and shared entity link"),
        DirectorName.IsValid() && DirectorName->GetText().ToString() == TEXT("Same director")
            && DirectorEntityLink.IsValid()))
    { return false; }
    const auto DirectorSignatureBeforePolicy = DebuggerWindow->DoBuild_Signature();
    FixtureSnapshot.Directors[0].DefaultCrossfadeSeconds = 2.5f;
    FixtureSnapshot.Directors[0].SamePriorityBehavior = ECk_SamePriorityBehavior::Allow;
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();
    TestTrue(TEXT("policy-only changes publish without structural rebuild or replacing retained director leaves"),
        DebuggerWindow->DoBuild_Signature() == DirectorSignatureBeforePolicy
            && DirectorsRepeat->GetItemWidget(DirectorKey) == DirectorRow
            && FindTaggedTextBlock(DirectorRow.ToSharedRef(), TEXT("audio-director-name")) == DirectorName
            && FindTaggedWidget(DirectorRow.ToSharedRef(), TEXT("audio-director-entity")) == DirectorEntityLink
            && TaggedText(DirectorRow.ToSharedRef(), TEXT("audio-director-policy")) == TEXT("crossfade 2.5s  ·  same-priority: allow"));
    FixtureSnapshot.Directors[0].DefaultCrossfadeSeconds.Reset();
    FixtureSnapshot.Directors[0].MaxConcurrentTracks = 0;
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();
    TestTrue(TEXT("nonpositive director limit and absent crossfade retain native display semantics"),
        TaggedText(DirectorRow.ToSharedRef(), TEXT("audio-director-active")) == TEXT("1 active")
            && TaggedText(DirectorRow.ToSharedRef(), TEXT("audio-director-policy")) == TEXT("no default crossfade  ·  same-priority: allow"));
    FixtureSnapshot.Directors[0].MaxConcurrentTracks = 4;
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();

    DebuggerWindow->DoRebuild_OverlayList();
    DebuggerWindow->DoUpdate_DirectorRecords();
    TestTrue(TEXT("ordinary Overlay rebuild preserves the independent Directors session identity"),
        DebuggerWindow->_DirectorRecords->FindRecord(DirectorKey).IsValid()
            && DirectorsRepeat->GetItemWidget(DirectorKey) == DirectorRow
            && FindTaggedTextBlock(DirectorRow.ToSharedRef(), TEXT("audio-director-name")) == DirectorName);

    const auto DirectorsTab = FindButtonWithText(GetTabs().ToSharedRef(), TEXT("Directors"));
    if (NOT TestTrue(TEXT("physical Directors tab reaches authored cards without the native page subtree"),
        DirectorsTab.IsValid() && Click(Slate, DirectorsTab.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 0
            && ContainsWidget(DebuggerWindow->_PageSwitcher.ToSharedRef(), DirectorsView->GetRegion(TEXT("main")))
            && NOT ContainsWidget(DebuggerWindow->_PageSwitcher.ToSharedRef(), DebuggerWindow->_NativeDirectorsPage.ToSharedRef())))
    { return false; }
    const auto DirectorsStyleBefore = StyleSettings->Selection;
    StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Large;
    StyleSettings->NotifyChanged();
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    const auto LargeDirectorFont = DirectorName->GetFont().Size;
    StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Normal;
    StyleSettings->Selection.CornerStyle = ECkDebugAxis_CornerStyle::Rounded;
    StyleSettings->Selection.SurfaceElevation = ECkDebugAxis_SurfaceElevation::Layered;
    StyleSettings->NotifyChanged();
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    const auto RoundedDirectorCard = FindTaggedWidget(DirectorRow.ToSharedRef(), TEXT("audio-director-card"));
    const auto* RoundedDirectorBrush = RoundedDirectorCard.IsValid() && RoundedDirectorCard->GetTypeAsString() == TEXT("SBorder")
        ? StaticCastSharedPtr<SBorder>(RoundedDirectorCard)->GetBorderImage() : nullptr;
    TestTrue(TEXT("live Directors style revision preserves highlighted leaf identity"),
        FindTaggedTextBlock(DirectorRow.ToSharedRef(), TEXT("audio-director-name")) == DirectorName);
    TestTrue(TEXT("live Directors style revision applies the authored name font token"),
        DirectorName->GetFont().Size < LargeDirectorFont);
    TestTrue(TEXT("live Directors style revision applies the authored card tokens"),
        RoundedDirectorBrush != nullptr
            && RoundedDirectorBrush->DrawAs == ESlateBrushDrawType::RoundedBox
            && RoundedDirectorBrush->OutlineSettings.CornerRadii.X == CkStyle::RadiusL());
    StyleSettings->Selection = DirectorsStyleBefore;
    StyleSettings->NotifyChanged();
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    const auto DirectorScroll = DirectorsView->GetScroll(TEXT("audio-directors-scroll"));
    const auto DirectorRevision = DirectorsView->GetRevision();
    TestTrue(TEXT("compatible Directors reload preserves keyed row, highlighted name, page and scroll"),
        DirectorsView->TryReload(DirectorsMarkup, DirectorsCss, TEXT("Audio Directors compatible reload")).Succeeded
            && DirectorsView->GetRevision() > DirectorRevision
            && DirectorsView->GetScroll(TEXT("audio-directors-scroll")) == DirectorScroll
            && DirectorsRepeat->GetItemWidget(DirectorKey) == DirectorRow
            && FindTaggedTextBlock(DirectorRow.ToSharedRef(), TEXT("audio-director-name")) == DirectorName
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 0);
    const auto CommittedDirectorRevision = DirectorsView->GetRevision();
    TestTrue(TEXT("invalid Directors field rejects atomically and retains the prior row"),
        NOT DirectorsView->TryReload(DirectorsMarkup.Replace(TEXT("text-field=\"name\""),
            TEXT("text-field=\"missing-name\"")), DirectorsCss, TEXT("Audio Directors invalid field")).Succeeded
            && DirectorsView->GetRevision() == CommittedDirectorRevision
            && DirectorsRepeat->GetItemWidget(DirectorKey) == DirectorRow
            && FindTaggedTextBlock(DirectorRow.ToSharedRef(), TEXT("audio-director-name")) == DirectorName);
    TestTrue(TEXT("invalid Directors action rejects atomically"),
        NOT DirectorsView->TryReload(DirectorsMarkup.Replace(TEXT("item-action=\"audio-director-navigate\""),
            TEXT("item-action=\"missing-director-route\"")), DirectorsCss, TEXT("Audio Directors invalid action")).Succeeded
            && DirectorsView->GetRevision() == CommittedDirectorRevision);
    TestTrue(TEXT("Directors valid candidate remains available after rejection"),
        DirectorsView->TryReload(DirectorsMarkup, DirectorsCss).Succeeded);

    const auto NavigationResults = MakeShared<TArray<FCk_Entity>>();
    DebuggerWindow->_DirectorNavigationForTests = [NavigationResults](const FCk_Handle& InEntity)
    { NavigationResults->Add(InEntity.Get_Entity()); };
    const auto HeldDirectorRef = FindEntityRef(DirectorRow.ToSharedRef());
    if (NOT TestTrue(TEXT("physical shared director entity-link routes the exact live production entity"),
        HeldDirectorRef.IsValid() && Click(Slate, HeldDirectorRef.ToSharedRef())
            && NavigationResults->Num() == 1 && (*NavigationResults)[0] == DirectorEntity.Get_Entity()))
    { return false; }

    auto OtherDirector = Director;
    OtherDirector.DirectorEntity = OtherDirectorEntity;
    FixtureSnapshot.Directors.Add(OtherDirector);
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();
    if (NOT TestTrue(TEXT("equal-name directors retain distinct identities and rows"),
        DirectorsRepeat->TryRefresh() && DirectorsRepeat->GetItemCount() == 2
            && DebuggerWindow->_DirectorRecords->GetRecords().Num() == 2))
    { return false; }
    const auto OtherDirectorKey = DebuggerWindow->_DirectorRecords->GetRecords()[1]->GetKey();
    TestTrue(TEXT("director identity is independent of display name"), OtherDirectorKey != DirectorKey);
    const auto OtherDirectorRow = DirectorsRepeat->GetItemWidget(OtherDirectorKey);
    if (NOT TestTrue(TEXT("second same-name director supplies an independent row"), OtherDirectorRow.IsValid())) { return false; }
    const auto OtherDirectorRef = FindEntityRef(OtherDirectorRow.ToSharedRef());
    if (NOT TestTrue(TEXT("physical same-name director link routes its own exact entity"),
        OtherDirectorRef.IsValid() && Click(Slate, OtherDirectorRef.ToSharedRef())
            && NavigationResults->Num() == 2 && (*NavigationResults)[1] == OtherDirectorEntity.Get_Entity()))
    { return false; }
    FixtureSnapshot.Directors.RemoveAt(1);
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();
    TestTrue(TEXT("director removal retires its keyed row"), DirectorsRepeat->TryRefresh() && DirectorsRepeat->GetItemCount() == 1);
    ProbeRetainedEntityRef(OtherDirectorRef.ToSharedRef());
    TestEqual(TEXT("held removed director link cannot dispatch through the retired item scope"), NavigationResults->Num(), 2);

    const auto DuplicateDirector = FixtureSnapshot.Directors[0];
    FixtureSnapshot.Directors.Add(DuplicateDirector);
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();
    TestTrue(TEXT("duplicate director identity fails closed and clears the whole projection"),
        NOT DebuggerWindow->_DirectorRecordsReady && DebuggerWindow->_DirectorRecords->GetRecords().IsEmpty());
    ProbeRetainedEntityRef(HeldDirectorRef.ToSharedRef());
    TestEqual(TEXT("rejected director publication cannot leave old navigation active"), NavigationResults->Num(), 2);
    FixtureSnapshot.Directors.RemoveAt(1);
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();
    TestTrue(TEXT("valid director publication recovers after atomic rejection"),
        DebuggerWindow->_DirectorRecordsReady && DirectorsRepeat->TryRefresh() && DirectorsRepeat->GetItemCount() == 1);
    const auto ReplacementSourceRow = DirectorsRepeat->GetItemWidget(DirectorKey);
    const auto ReplacementSourceRef = ReplacementSourceRow.IsValid() ? FindEntityRef(ReplacementSourceRow.ToSharedRef()) : nullptr;
    if (NOT TestTrue(TEXT("replacement fixture holds the currently active director link"), ReplacementSourceRef.IsValid()))
    { return false; }
    FixtureSnapshot.Directors[0].DirectorEntity = OtherDirectorEntity;
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();
    if (NOT TestTrue(TEXT("same-name equal-count replacement changes the stable director identity"),
        DirectorsRepeat->TryRefresh() && DirectorsRepeat->GetItemCount() == 1
            && NOT DirectorsRepeat->GetItemWidget(DirectorKey).IsValid()
            && DirectorsRepeat->GetItemWidget(OtherDirectorKey).IsValid()))
    { return false; }
    ProbeRetainedEntityRef(ReplacementSourceRef.ToSharedRef());
    TestEqual(TEXT("same-name replacement cannot reactivate the held prior entity link"), NavigationResults->Num(), 2);
    const auto ReplacementRef = FindEntityRef(DirectorsRepeat->GetItemWidget(OtherDirectorKey).ToSharedRef());
    if (NOT TestTrue(TEXT("physical replacement director link resolves the replacement entity"),
        ReplacementRef.IsValid() && Click(Slate, ReplacementRef.ToSharedRef())
            && NavigationResults->Num() == 3 && (*NavigationResults)[2] == OtherDirectorEntity.Get_Entity()))
    { return false; }
    FixtureSnapshot.Directors[0].DirectorEntity = DirectorEntity;
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();
    DirectorsRepeat->TryRefresh();

    const auto LiveTabs = GetTabs();
    const auto TabCountText = [&LiveTabs](const TCHAR* InTag) -> FString
    {
        const auto Widget = FindTaggedWidget(LiveTabs.ToSharedRef(), FName{InTag});
        return Widget.IsValid() && Widget->GetTypeAsString() == TEXT("STextBlock")
            ? StaticCastSharedPtr<STextBlock>(Widget)->GetText().ToString() : FString{};
    };
    const auto TabWarningVisible = [&LiveTabs](const TCHAR* InTag) -> bool
    {
        const auto Widget = FindTaggedWidget(LiveTabs.ToSharedRef(), FName{InTag});
        if (NOT Widget.IsValid()) { return false; }
        Widget->Invalidate(EInvalidateWidgetReason::Prepass);
        Widget->SlatePrepass(1.0f);
        return Widget->GetVisibility() == EVisibility::SelfHitTestInvisible;
    };
    TestTrue(TEXT("typed tab model projects production director/track counts and empty Crossfade count"),
        TabCountText(TEXT("CkDebug.Tab.Count.Directors")) == TEXT("1")
            && TabCountText(TEXT("CkDebug.Tab.Count.Tracks")) == TEXT("1")
            && TabCountText(TEXT("CkDebug.Tab.Count.Crossfade")).IsEmpty());
    const auto LiveTracksHeader = FindTaggedWidget(LiveTabs.ToSharedRef(), TEXT("CkDebug.Tab.Tracks"));
    auto WarningTrack = TrackInfoA;
    WarningTrack.State = ECk_AudioTrack_State::FadingOut;
    WarningTrack.IsVirtualized = true;
    WarningTrack.HasSpatialData = true;
    WarningTrack.IsAttenuated = true;
    WarningTrack.MaxFalloffDistance = 100.0f;
    WarningTrack.DistanceToListener = 200.0f;
    FixtureSnapshot.Directors[0].Tracks = {WarningTrack};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();
    TestTrue(TEXT("production snapshot changes update fading count and both tab warnings without rebuilding headers"),
        TabCountText(TEXT("CkDebug.Tab.Count.Crossfade")) == TEXT("1")
            && TabWarningVisible(TEXT("CkDebug.Tab.Warning.Tracks"))
            && TabWarningVisible(TEXT("CkDebug.Tab.Warning.Spatial"))
            && GetTabs() == LiveTabs
            && FindTaggedWidget(LiveTabs.ToSharedRef(), TEXT("CkDebug.Tab.Tracks")) == LiveTracksHeader);
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_LiveValues();
    TestTrue(TEXT("cleared production conditions remove tab warnings and Crossfade count"),
        TabCountText(TEXT("CkDebug.Tab.Count.Crossfade")).IsEmpty()
            && NOT TabWarningVisible(TEXT("CkDebug.Tab.Warning.Tracks"))
            && NOT TabWarningVisible(TEXT("CkDebug.Tab.Warning.Spatial")));

    const auto TrackNavigationResults = MakeShared<TArray<FCk_Entity>>();
    if (NOT RunFixturePhase([&, this]() -> bool
    {
        const auto RefreshTracks = [&]()
        {
            DebuggerWindow->_Collector.Collect(nullptr);
            DebuggerWindow->DoUpdate_LiveValues();
            TracksRepeat->TryRefresh();
            TickSlate(Slate);
        };
        const auto TracksTab = FindButtonWithText(GetTabs().ToSharedRef(), TEXT("Tracks"));
        if (NOT TestTrue(TEXT("physical Tracks tab selects the authored page with one retained compact plot"),
            TracksTab.IsValid() && Click(Slate, TracksTab.ToSharedRef())
                && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 1
                && ContainsWidget(DebuggerWindow->_TracksPageHost.ToSharedRef(), TracksView->GetRegion(TEXT("main")))
                && NOT ContainsWidget(DebuggerWindow->_TracksPageHost.ToSharedRef(), DebuggerWindow->_NativeTracksPage.ToSharedRef())))
        { return false; }
        RefreshTracks();
        const auto TrackRow = TracksRepeat->GetItemWidget(TrackRecordKey);
        if (NOT TestTrue(TEXT("current Track identity resolves its authored row"), TrackRow.IsValid())) { return false; }
        const auto TrackMeter = StaticCastSharedPtr<SCkDebug_MeterBar>(FindWidgetType(TrackRow.ToSharedRef(), TEXT("SCkDebug_MeterBar")));
        const auto TrackEntityLink = FindTaggedWidget(TrackRow.ToSharedRef(), TEXT("audio-track-entity"));
        const auto TrackEntityRef = TrackEntityLink.IsValid() ? FindEntityRef(TrackEntityLink.ToSharedRef()) : nullptr;
        const auto TrackNameWrap = FindTaggedWidget(TrackRow.ToSharedRef(), TEXT("audio-track-name"));
        const auto TrackName = TrackNameWrap.IsValid()
            ? StaticCastSharedPtr<STextBlock>(FindWidgetType(TrackNameWrap.ToSharedRef(), TEXT("STextBlock"))) : nullptr;
        if (NOT TestTrue(TEXT("Tracks renders the real meter, exact chip, highlight leaf and entity reference"),
            TrackMeter.IsValid() && TrackEntityRef.IsValid() && TrackName.IsValid()
                && FindWidgetType(TrackRow.ToSharedRef(), TEXT("SCkDebug_Chip")).IsValid()
                && TrackMeter->Get_Fraction() == 0.5f && NOT TrackMeter->Get_TargetFraction().IsSet()))
        { return false; }
        DebuggerWindow->_TrackNavigationForTests = [TrackNavigationResults](const FCk_Handle& InEntity)
        { TrackNavigationResults->Add(InEntity.Get_Entity()); };
        if (NOT TestTrue(TEXT("physical authored track entity link reaches the exact production identity"),
            Click(Slate, TrackEntityRef.ToSharedRef()) && TrackNavigationResults->Num() == 1
                && (*TrackNavigationResults)[0] == TrackA.Get_Entity())) { return false; }

        auto LiveTrack = TrackInfoA;
        LiveTrack.State = ECk_AudioTrack_State::FadingIn;
        LiveTrack.CurrentVolume = 0.25f;
        LiveTrack.TargetVolume = 0.75f;
        LiveTrack.FadeSpeed = 0.5f;
        LiveTrack.Priority = 7;
        LiveTrack.LoopBehavior = ECk_LoopBehavior::PlayOnce;
        LiveTrack.SoundPath = TEXT("/Game/Audio/TrackCue.TrackCue");
        LiveTrack.OverrideBehavior = ECk_AudioTrack_OverrideBehavior::Queue;
        LiveTrack.IsVirtualized = true;
        FixtureSnapshot.Directors[0].Tracks = {LiveTrack};
        RefreshTracks();
        const auto TrackPriority = FindTaggedWidget(TrackRow.ToSharedRef(), TEXT("audio-track-priority"));
        const auto TrackOverride = FindTaggedWidget(TrackRow.ToSharedRef(), TEXT("audio-track-override"));
        const auto TrackLoop = FindTaggedWidget(TrackRow.ToSharedRef(), TEXT("audio-track-loop"));
        const auto TrackState = FindWidgetType(TrackRow.ToSharedRef(), TEXT("SCkDebug_StatusPill"));
        const auto TrackPriorityChip = TrackPriority.IsValid() ? FindWidgetType(TrackPriority.ToSharedRef(), TEXT("SCkDebug_Chip")) : nullptr;
        const auto TrackOverrideChip = TrackOverride.IsValid() ? FindWidgetType(TrackOverride.ToSharedRef(), TEXT("SCkDebug_Chip")) : nullptr;
        if (NOT TestTrue(TEXT("configuration retains exact shared chip leaves"), TrackPriorityChip.IsValid() && TrackOverrideChip.IsValid()))
        { return false; }
        TestTrue(TEXT("live Tracks state, current/target marker, alert and configuration update without replacing the row"),
            TracksRepeat->GetItemWidget(TrackRecordKey) == TrackRow
                && TrackMeter->Get_Fraction() == 0.25f
                && TrackMeter->Get_TargetFraction() == TOptional<float>{0.75f}
                && TrackMeter->Get_TargetColor() == CkStyle::Text()
                && TrackMeter->Get_FillColor() == CkStyle::Err()
                && TaggedText(TrackRow.ToSharedRef(), TEXT("audio-track-volume")) == TEXT("0.25 → 0.75")
                && TaggedText(TrackRow.ToSharedRef(), TEXT("audio-track-fade")) == DebuggerWindow->DoBuild_FadeText(LiveTrack)
                && TaggedText(TrackRow.ToSharedRef(), TEXT("audio-track-alert")) == DebuggerWindow->DoBuild_AlertText(LiveTrack)
                && TaggedText(TrackRow.ToSharedRef(), TEXT("audio-track-sound")) == TEXT("TrackCue")
                && TrackState.IsValid() && SubtreeHasText(TrackState.ToSharedRef(), TEXT("Fading in"))
                && TrackLoop.IsValid() && SubtreeHasText(TrackLoop.ToSharedRef(), TEXT("Play once"))
                && TrackPriority.IsValid() && SubtreeHasText(TrackPriority.ToSharedRef(), TEXT("p7"))
                && TrackOverride.IsValid() && SubtreeHasText(TrackOverride.ToSharedRef(), TEXT("Queue")));
        const auto SoundWidget = FindTaggedWidget(TrackRow.ToSharedRef(), TEXT("audio-track-sound"));
        if (SoundWidget.IsValid() && SoundWidget->GetToolTip().IsValid())
        { SoundWidget->GetToolTip()->GetContentWidget()->SlatePrepass(); }
        TestTrue(TEXT("sound leaf exposes the full asset path tooltip"), SoundWidget.IsValid()
            && SoundWidget->GetToolTip().IsValid()
            && SoundWidget->GetToolTip()->GetContentWidget()->GetAccessibleText().ToString() == LiveTrack.SoundPath);
        LiveTrack.State = ECk_AudioTrack_State::Playing;
        LiveTrack.IsVirtualized = false;
        LiveTrack.CurrentVolume = 0.75f;
        FixtureSnapshot.Directors[0].Tracks = {LiveTrack};
        RefreshTracks();
        TestTrue(TEXT("settled nonvirtualized Tracks remove the target and alert"),
            NOT TrackMeter->Get_TargetFraction().IsSet()
                && TrackMeter->Get_Fraction() == 0.75f
                && TaggedText(TrackRow.ToSharedRef(), TEXT("audio-track-volume")) == TEXT("0.75 = 0.75")
                && FindTaggedWidget(TrackRow.ToSharedRef(), TEXT("audio-track-alert-row"))->GetVisibility() == EVisibility::Collapsed);

        const auto CompactPlot = DebuggerWindow->_CompactCrossfadePlot;
        DebuggerWindow->DoRecord_VolumeHistory();
        TestTrue(TEXT("authored compact lane retains the exact shared pair, legend and 62px plot"),
            ContainsWidget(TracksView->GetRegion(TEXT("main")), CompactPlot.ToSharedRef())
                && CompactPlot->Get_Samples() == DebuggerWindow->_CrossfadeSeriesA
                && CompactPlot->Get_BandSamples() == DebuggerWindow->_CrossfadeSeriesB
                && CompactPlot->Get_DesiredSize().Y == 62.0f
                && CompactPlot->Get_BandFillOpacity() == 0.0f
                && TaggedText(TracksView->GetRegion(TEXT("main")), TEXT("audio-track-crossfade-legend")) == DebuggerWindow->_CrossfadeLegendText->ToString());
        const auto TrackScroll = TracksView->GetScroll(TEXT("audio-tracks-scroll"));
        TestTrue(TEXT("compatible Tracks reload retains rows, exact meter, chip, plot and scroll"),
            TracksView->TryReload(TracksMarkup, TracksCss, TEXT("Audio Tracks compatible reload")).Succeeded
                && TracksRepeat->GetItemWidget(TrackRecordKey) == TrackRow
                && FindWidgetType(TrackRow.ToSharedRef(), TEXT("SCkDebug_MeterBar")) == TrackMeter
                && ContainsWidget(TrackRow.ToSharedRef(), TrackPriorityChip.ToSharedRef())
                && TracksView->GetScroll(TEXT("audio-tracks-scroll")) == TrackScroll
                && ContainsWidget(TracksView->GetRegion(TEXT("main")), CompactPlot.ToSharedRef()));
        const auto TracksRevision = TracksView->GetRevision();
        TestTrue(TEXT("invalid Tracks field rejects without replacing accepted rows"),
            NOT TracksView->TryReload(TracksMarkup.Replace(TEXT("target-fraction-field=\"target-fraction\""),
                TEXT("target-fraction-field=\"missing-target\"")), TracksCss).Succeeded
                && TracksView->GetRevision() == TracksRevision && TracksRepeat->GetItemWidget(TrackRecordKey) == TrackRow);
        TestTrue(TEXT("invalid Tracks action rejects without replacing accepted rows"),
            NOT TracksView->TryReload(TracksMarkup.Replace(TEXT("item-action=\"audio-track-navigate\""),
                TEXT("item-action=\"missing-track-route\"")), TracksCss).Succeeded
                && TracksView->GetRevision() == TracksRevision);
        if (NOT TestTrue(TEXT("Tracks valid candidate remains available after rejection"), TracksView->TryReload(TracksMarkup, TracksCss).Succeeded))
        { return false; }
        DebuggerWindow->DoRebuild_OverlayList();
        RefreshTracks();
        TestTrue(TEXT("ordinary Overlay rebuild leaves Tracks lifecycle keys, rows and actions current"),
            TracksRepeat->GetItemWidget(TrackRecordKey) == TrackRow && Click(Slate, TrackEntityRef.ToSharedRef())
                && TrackNavigationResults->Num() == 2 && (*TrackNavigationResults)[1] == TrackA.Get_Entity());
        const auto TracksStyleBefore = StyleSettings->Selection;
        StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Normal;
        StyleSettings->Selection.ChipStyle = ECkDebugAxis_ChipStyle::Tint;
        StyleSettings->NotifyChanged();
        DebuggerWindow->OnStyleRevisionChanged();
        RefreshTracks();
        const auto NormalTrackFont = TrackName->GetFont().Size;
        const auto TrackChipBorder = StaticCastSharedPtr<SBorder>(FindWidgetType(TrackPriorityChip.ToSharedRef(), TEXT("SBorder")));
        StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Large;
        StyleSettings->Selection.ChipStyle = ECkDebugAxis_ChipStyle::TextOnly;
        StyleSettings->NotifyChanged();
        DebuggerWindow->OnStyleRevisionChanged();
        RefreshTracks();
        TestTrue(TEXT("Tracks style revision retains the row/name/chip and applies live TextScale and ChipStyle"),
            TracksRepeat->GetItemWidget(TrackRecordKey) == TrackRow && TrackName->GetFont().Size > NormalTrackFont
                && ContainsWidget(TrackRow.ToSharedRef(), TrackPriorityChip.ToSharedRef())
                && TrackChipBorder.IsValid() && TrackChipBorder->GetBorderBackgroundColor().GetSpecifiedColor().A == 0.0f);
        StyleSettings->Selection = TracksStyleBefore;
        StyleSettings->NotifyChanged();
        DebuggerWindow->OnStyleRevisionChanged();
        RefreshTracks();
        HostWindow->Resize(FVector2D{360.0f, 480.0f});
        TickSlate(Slate);
        const auto TracksOuterScroll = View->GetScroll(TEXT("audio-shell-scroll"));
        const auto TracksScrollEnd = TracksOuterScroll->GetScrollOffsetOfEnd();
        TracksOuterScroll->SetScrollOffset(TracksScrollEnd);
        TickSlate(Slate);
        const auto OverrideGeometry = TrackOverrideChip->GetCachedGeometry();
        const auto HostGeometry = DebuggerWindow->GetCachedGeometry();
        TestTrue(TEXT("narrow Tracks keeps the final configuration chip reachable through the shell scroll"),
            TracksScrollEnd > 0.0f && OverrideGeometry.GetLocalSize().X > 0.0f
                && OverrideGeometry.GetAbsolutePosition().X >= HostGeometry.GetAbsolutePosition().X - 1.0f
                && OverrideGeometry.GetAbsolutePosition().X + OverrideGeometry.GetAbsoluteSize().X
                    <= HostGeometry.GetAbsolutePosition().X + HostGeometry.GetAbsoluteSize().X + 1.0f);
        HostWindow->Resize(FVector2D{1100.0f, 720.0f});
        TracksOuterScroll->SetScrollOffset(0.0f);
        TickSlate(Slate);
        const auto GroupFilter = FindCheckBoxWithText(DebuggerWindow->_FilterGroupToggle.ToSharedRef(), TEXT("Group by director"));
        if (NOT TestTrue(TEXT("physical Group filter switches Tracks to flat rows"),
            GroupFilter.IsValid() && Click(Slate, GroupFilter.ToSharedRef()) && NOT DebuggerWindow->_GroupByDirector))
        { return false; }
        RefreshTracks();
        TestTrue(TEXT("ungrouping removes only headers and keeps the exact track row"),
            TracksRepeat->GetItemCount() == 1 && TracksRepeat->GetItemWidget(TrackRecordKey) == TrackRow
                && NOT TracksRepeat->GetItemWidget(TrackHeaderKey).IsValid());
        if (NOT TestTrue(TEXT("physical Group filter restores Tracks headers"), Click(Slate, GroupFilter.ToSharedRef()) && DebuggerWindow->_GroupByDirector))
        { return false; }
        RefreshTracks();
        TestTrue(TEXT("regrouping preserves existing track row identity"),
            TracksRepeat->GetItemCount() == 2 && TracksRepeat->GetItemWidget(TrackRecordKey) == TrackRow);
        const auto GroupedHeader = TracksRepeat->GetItemWidget(TrackHeaderKey);
        const auto GroupedDirectorLink = GroupedHeader.IsValid()
            ? FindTaggedWidget(GroupedHeader.ToSharedRef(), TEXT("audio-track-director-entity")) : nullptr;
        if (NOT TestTrue(TEXT("physical Tracks group-header navigation resolves its exact director"),
            GroupedDirectorLink.IsValid() && Click(Slate, GroupedDirectorLink.ToSharedRef())
                && TrackNavigationResults->Num() == 3 && (*TrackNavigationResults)[2] == DirectorEntity.Get_Entity()))
        { return false; }

        auto EqualNameTrack = LiveTrack;
        EqualNameTrack.TrackEntity = TrackB;
        FixtureSnapshot.Directors[0].Tracks = {LiveTrack, EqualNameTrack};
        RefreshTracks();
        TestTrue(TEXT("equal track names preserve separate full-identity records"),
            CountTrackRecords(DebuggerWindow->_TrackRecords) == 2 && TracksRepeat->GetItemCount() == 3
                && TracksRepeat->GetItemWidget(TrackRecordKey) == TrackRow);
        FixtureSnapshot.Directors[0].Tracks = {EqualNameTrack};
        RefreshTracks();
        ProbeRetainedEntityRef(TrackEntityRef.ToSharedRef());
        TestTrue(TEXT("same-name replacement retires the old Track identity and its held action"),
            NOT TracksRepeat->GetItemWidget(TrackRecordKey).IsValid() && CountTrackRecords(DebuggerWindow->_TrackRecords) == 1
                && TrackNavigationResults->Num() == 3);
        const auto TrackReplacementKey = DebuggerWindow->_TrackRecords->GetRecords()[1]->GetKey();
        const auto TrackReplacementRow = TracksRepeat->GetItemWidget(TrackReplacementKey);
        const auto ReplacementTrackLink = FindTaggedWidget(TrackReplacementRow.ToSharedRef(), TEXT("audio-track-entity"));
        const auto ReplacementTrackRef = ReplacementTrackLink.IsValid()
            ? FindEntityRef(ReplacementTrackLink.ToSharedRef()) : nullptr;
        if (NOT TestTrue(TEXT("replacement track exposes its exact retained entity action"), ReplacementTrackRef.IsValid()))
        { return false; }
        const bool ReplacementInitiallyHitTestable = IsInHitPath(Slate, ReplacementTrackRef.ToSharedRef());
        if (NOT ReplacementInitiallyHitTestable)
        {
            TrackScroll->ScrollDescendantIntoView(
                ReplacementTrackRef.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
            TickSlate(Slate);
        }
        const bool ReplacementArrangedIntoHitPath = IsInHitPath(Slate, ReplacementTrackRef.ToSharedRef());
        AddInfo(FString::Printf(TEXT("Replacement Track hit path: before=%s after=%s vertical-offset=%g end=%g."),
            ReplacementInitiallyHitTestable ? TEXT("true") : TEXT("false"),
            ReplacementArrangedIntoHitPath ? TEXT("true") : TEXT("false"),
            TrackScroll->GetScrollOffset(), TrackScroll->GetScrollOffsetOfEnd()));
        if (NOT TestTrue(TEXT("replacement track action is arranged into the current hit path"), ReplacementArrangedIntoHitPath))
        { return false; }
        const bool ReplacementClickHandled = Click(Slate, ReplacementTrackRef.ToSharedRef());
        const bool ReplacementCountAdvanced = TestEqual(TEXT("replacement track action dispatches exactly once"),
            TrackNavigationResults->Num(), 4);
        const bool ReplacementEntityMatches = TestTrue(TEXT("replacement track action resolves its exact entity"),
            TrackNavigationResults->Num() == 4 && (*TrackNavigationResults)[3] == TrackB.Get_Entity());
        if (NOT TestTrue(TEXT("replacement track physical action is handled"), ReplacementClickHandled)
            || NOT ReplacementCountAdvanced || NOT ReplacementEntityMatches)
        { return false; }
        FixtureSnapshot.Directors[0].Tracks = {EqualNameTrack, EqualNameTrack};
        RefreshTracks();
        TestTrue(TEXT("duplicate Track identities fail closed without a partially actionable publication"),
            NOT DebuggerWindow->_TrackRecordsReady && DebuggerWindow->_TrackRecords->GetRecords().IsEmpty()
                && TracksRepeat->GetItemCount() == 0);
        FixtureSnapshot.Directors[0].Tracks.Reset();
        RefreshTracks();
        TestTrue(TEXT("empty director emits neither Tracks rows nor an empty group header"),
            DebuggerWindow->_TrackRecordsReady && TracksRepeat->GetItemCount() == 0);
        FixtureSnapshot.Directors[0].Tracks = {TrackInfoA};
        RefreshTracks();
        return true;
    }))
    { return false; }

    // Exercise the same production structure/value pass against the controlled collector snapshot after routed input.
    const auto RefreshFilteredRows = [&]()
    {
        DebuggerWindow->_Collector.Collect(nullptr);
        DebuggerWindow->DoRebuild_Structure();
        DebuggerWindow->DoUpdate_LiveValues();
        TickSlate(Slate);
    };
    const TSharedPtr<SButton> FilterTracksTab = FindButtonWithText(GetTabs().ToSharedRef(), TEXT("Tracks"));
    if (NOT TestTrue(TEXT("filter fixture physically selects the production Tracks page"),
        FilterTracksTab.IsValid() && Click(Slate, FilterTracksTab.ToSharedRef())))
    { return false; }
    if (NOT TestTrue(TEXT("physical search supplies a nonmatching production query"),
        ReplaceSearchText(Slate, FallbackFilterEditable.ToSharedRef(), TEXT("absent-audio-track"))))
    { return false; }
    RefreshFilteredRows();
    TestTrue(TEXT("nonmatching search removes the controlled production track row"),
        DebuggerWindow->_TrackRecords->GetRecords().IsEmpty() && TracksRepeat->GetItemCount() == 0);
    DirectorsRepeat->TryRefresh();
    TestTrue(TEXT("nonmatching name filter removes authored Directors records"),
        DebuggerWindow->_DirectorRecords->GetRecords().IsEmpty()
            && DirectorsRepeat->GetItemCount() == 0);
    if (NOT TestTrue(TEXT("physical substring search retains the director and supplies its highlight binding"),
        ReplaceSearchText(Slate, FallbackFilterEditable.ToSharedRef(), TEXT("director"))))
    { return false; }
    RefreshFilteredRows();
    DirectorsRepeat->TryRefresh();
    TestTrue(TEXT("Director search is name-scoped and independent of a track name match"),
        DebuggerWindow->_HighlightString == TEXT("director")
            && DirectorsRepeat->GetItemCount() == 1
            && DebuggerWindow->_DirectorRecords->FindRecord(DirectorKey).IsValid());
    if (NOT TestTrue(TEXT("physical clear restores the production query"),
        ReplaceSearchText(Slate, FallbackFilterEditable.ToSharedRef(), TEXT(""))))
    { return false; }
    RefreshFilteredRows();
    TestEqual(TEXT("clearing search restores the controlled production track row"), CountTrackRecords(DebuggerWindow->_TrackRecords), 1);
    if (NOT TestTrue(TEXT("physical Playing filter hides playing tracks"), Click(Slate, FallbackPlayingFilter.ToSharedRef())))
    { return false; }
    RefreshFilteredRows();
    TestEqual(TEXT("Playing preference removes the controlled playing row"), CountTrackRecords(DebuggerWindow->_TrackRecords), 0);
    DirectorsRepeat->TryRefresh();
    TestTrue(TEXT("track-state preferences do not remove Directors records"),
        DebuggerWindow->_DirectorRecords->FindRecord(DirectorKey).IsValid()
            && DirectorsRepeat->GetItemCount() == 1);
    if (NOT TestTrue(TEXT("physical Playing filter restores playing tracks"), Click(Slate, FallbackPlayingFilter.ToSharedRef())))
    { return false; }
    RefreshFilteredRows();
    TestEqual(TEXT("re-enabling Playing restores the original track count"), CountTrackRecords(DebuggerWindow->_TrackRecords), 1);
    if (NOT TestTrue(TEXT("filter fixture physically returns to Events before recording its baseline"),
        Click(Slate, FallbackEventsTab.ToSharedRef())))
    { return false; }

    // Reset only after the physical Events selection: Click ticks Slate, so seeding before it could consume a diff.
    DebuggerWindow->_TrackWatch.Reset();
    DebuggerWindow->_HasWatchBaseline = false;
    LiveEventLog->Clear_Entries();
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoRecord_Events();
    TestEqual(TEXT("Events baseline never fabricates an appearance entry"), LiveEventLog->Get_EntryCount(), 0);
    TrackInfoA.State = ECk_AudioTrack_State::Paused;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoRecord_Events();
    TestEqual(TEXT("disabled State preference suppresses a future state transition"), LiveEventLog->Get_EntryCount(), 0);
    if (NOT TestTrue(TEXT("recovered authored Events composition physically re-enables state recording"),
        Click(Slate, LiveEventsStateToggle.ToSharedRef()) && DebuggerWindow->_EventsShowStateChanges))
    { return false; }
    TrackInfoA.State = ECk_AudioTrack_State::Playing;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoRecord_Events();
    TestEqual(TEXT("enabled State preference records exactly the next state transition"), LiveEventLog->Get_EntryCount(), 1);
    TickSlate(Slate);
    TestTrue(TEXT("recorded State event is physically rendered by the original native event log"),
        SubtreeHasText(LiveEventLog.ToSharedRef(), TEXT("Same track  Paused → Playing")));
    if (NOT TestTrue(TEXT("turning State recording off leaves existing events visible"),
        Click(Slate, LiveEventsStateToggle.ToSharedRef()) && NOT DebuggerWindow->_EventsShowStateChanges
            && LiveEventLog->Get_EntryCount() == 1
            && SubtreeHasText(LiveEventLog.ToSharedRef(), TEXT("Same track  Paused → Playing"))))
    { return false; }
    const int64 PopulatedEventsRevision = EventsView->GetRevision();
    const FCkUiLoadResult PopulatedEventsReload = EventsView->TryReload(
        EventsMarkup, EventsCss, TEXT("Audio populated Events toolbar reload"));
    TickSlate(Slate);
    TestTrue(TEXT("toolbar reload preserves the exact populated native log and recording preferences"),
        PopulatedEventsReload.Succeeded && EventsView->GetRevision() > PopulatedEventsRevision
            && DebuggerWindow->_EventLog == LiveEventLog && LiveEventLog->Get_EntryCount() == 1
            && SubtreeHasText(LiveEventLog.ToSharedRef(), TEXT("Same track  Paused → Playing"))
            && NOT DebuggerWindow->_EventsShowStateChanges && DebuggerWindow->_EventsShowFades
            && DebuggerWindow->_EventsShowVirtualization && DebuggerWindow->_EventsShowLifecycle);
    for (const auto& Toggle : LiveEventToggles)
    { TestTrue(TEXT("populated Events reload retains all four native controls"), ContainsWidget(EventsView->GetRegion(TEXT("main")), Toggle.ToSharedRef())); }
    const int64 PopulatedEventsRevisionBeforeReject = EventsView->GetRevision();
    const FCkUiLoadResult PopulatedEventsRejected = EventsView->TryReload(
        InvalidEventsStartupMarkup, EventsCss, TEXT("Audio populated Events toolbar rejection"));
    TestTrue(TEXT("rejected toolbar candidate cannot clear the populated log or alter preferences"),
        NOT PopulatedEventsRejected.Succeeded && EventsView->GetRevision() == PopulatedEventsRevisionBeforeReject
            && DebuggerWindow->_EventLog == LiveEventLog && LiveEventLog->Get_EntryCount() == 1
            && SubtreeHasText(LiveEventLog.ToSharedRef(), TEXT("Same track  Paused → Playing"))
            && NOT DebuggerWindow->_EventsShowStateChanges);

    const TSharedPtr<SButton> OverlayTab = FindButtonWithText(GetTabs().ToSharedRef(), TEXT("Overlay"));
    if (NOT TestTrue(TEXT("production Overlay tab is physically selectable"),
        OverlayTab.IsValid() && Click(Slate, OverlayTab.ToSharedRef())))
    { return false; }
    DebuggerWindow->Tick(DebuggerWindow->GetCachedGeometry(), FPlatformTime::Seconds(), 0.0f);
    const TSharedPtr<SCheckBox> HeldTrackAToggle = FindCheckBoxWithText(
        DebuggerWindow->_OverlayListBox.ToSharedRef(), TEXT("draw"));
    if (NOT TestTrue(TEXT("physical production overlay action targets the first same-name entity"),
        HeldTrackAToggle.IsValid() && Click(Slate, HeldTrackAToggle.ToSharedRef())
            && TrackA.Has<ck::FTag_AudioTrack_DebugDraw>()))
    { return false; }
    TrackA.Try_Remove<ck::FFragment_AudioTrack_Debug>();
    TrackA.Try_Remove<ck::FTag_AudioTrack_DebugDraw>();
    HeldTrackAToggle->ToggleCheckedState();
    TestTrue(TEXT("held-control probe dispatches while its production generation is current"),
        TrackA.Has<ck::FTag_AudioTrack_DebugDraw>());
    TrackA.Try_Remove<ck::FFragment_AudioTrack_Debug>();
    TrackA.Try_Remove<ck::FTag_AudioTrack_DebugDraw>();

    auto TrackInfoB = TrackInfoA;
    TrackInfoB.TrackEntity = TrackB;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoB};
    DebuggerWindow->_Collector.Collect(nullptr);
    const auto TrackBStructureSignature = DebuggerWindow->DoBuild_Signature();
    const auto TrackBAllSignature = DebuggerWindow->DoBuild_AllTracksSignature();
    TestTrue(TEXT("same-label entity replacement changes both production structure signatures"),
        TrackBStructureSignature != TrackAStructureSignature
            && TrackBAllSignature != TrackAAllSignature);
    DebuggerWindow->Tick(DebuggerWindow->GetCachedGeometry(), FPlatformTime::Seconds(), 0.0f);
    HeldTrackAToggle->ToggleCheckedState();
    TestFalse(TEXT("held same-name predecessor action is revoked when the production list replaces it"),
        TrackA.Has<ck::FTag_AudioTrack_DebugDraw>());

    const TSharedPtr<SCheckBox> HeldTrackBToggle = FindCheckBoxWithText(
        DebuggerWindow->_OverlayListBox.ToSharedRef(), TEXT("draw"));
    if (NOT TestTrue(TEXT("replacement overlay action routes to the new same-name entity"),
        HeldTrackBToggle.IsValid() && Click(Slate, HeldTrackBToggle.ToSharedRef())
            && TrackB.Has<ck::FTag_AudioTrack_DebugDraw>()))
    { return false; }
    TrackB.Try_Remove<ck::FFragment_AudioTrack_Debug>();
    TrackB.Try_Remove<ck::FTag_AudioTrack_DebugDraw>();

    DebuggerWindow->DoRecord_VolumeHistory();
    DebuggerWindow->DoRecord_Events();
    const TSharedPtr<SButton> LiveCrossfadeTab = FindButtonWithText(GetTabs().ToSharedRef(), TEXT("Crossfade"));
    if (NOT TestTrue(TEXT("production Crossfade tab remains physically selectable"),
        LiveCrossfadeTab.IsValid() && Click(Slate, LiveCrossfadeTab.ToSharedRef())))
    { return false; }
    DebuggerWindow->_VolumeHistory.Reset();
    TrackInfoA.TrackName = TEXT("Fade A");
    TrackInfoA.CurrentVolume = 0.25f;
    TrackInfoA.State = ECk_AudioTrack_State::FadingIn;
    TrackInfoB.TrackName = TEXT("Fade B");
    TrackInfoB.CurrentVolume = 0.75f;
    TrackInfoB.State = ECk_AudioTrack_State::FadingOut;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA, TrackInfoB};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoRecord_VolumeHistory();
    TrackInfoA.CurrentVolume = 0.45f;
    TrackInfoB.CurrentVolume = 0.55f;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA, TrackInfoB};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoRecord_VolumeHistory();
    TestTrue(TEXT("production sampling drives both retained Crossfade series and authored legend"),
        DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2
            && DebuggerWindow->_CrossfadeSeriesA->Num() == 2
            && DebuggerWindow->_CrossfadeSeriesB->Num() == 2
            && (*DebuggerWindow->_CrossfadeSeriesA)[0] == 0.75f
            && (*DebuggerWindow->_CrossfadeSeriesA)[1] == 0.55f
            && (*DebuggerWindow->_CrossfadeSeriesB)[0] == 0.25f
            && (*DebuggerWindow->_CrossfadeSeriesB)[1] == 0.45f
            && TaggedText(CrossfadeView->GetRegion(TEXT("main")), TEXT("audio-crossfade-legend"))
                == TEXT("Fade B   ·   Fade A"));

    TrackInfoA.TrackName = TEXT("Spatial track");
    TrackInfoA.State = ECk_AudioTrack_State::Playing;
    TrackInfoA.CurrentVolume = 0.8f;
    TrackInfoA.HasSpatialData = true;
    TrackInfoA.IsAttenuated = true;
    TrackInfoA.IsSpatialized = true;
    TrackInfoA.AttenuationGain = 0.25f;
    TrackInfoA.DistanceToListener = 250.0f;
    TrackInfoA.BearingDegrees = 90.0f;
    TrackInfoA.InnerRadius = 100.0f;
    TrackInfoA.FalloffDistance = 900.0f;
    TrackInfoA.MaxFalloffDistance = 1000.0f;
    TrackInfoA.AttenuationAssetName = TEXT("Spatial attenuation");
    TrackInfoA.FalloffCurve = {1.0f, 0.75f, 0.25f, 0.0f};
    FixtureSnapshot.HasListener = true;
    FixtureSnapshot.ListenerSource = TEXT("fixture listener");
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_SpatialView();
    const TSharedPtr<SButton> SpatialTab = FindButtonWithText(GetTabs().ToSharedRef(), TEXT("Spatial"));
    if (NOT TestTrue(TEXT("populated Spatial page remains physically selectable"),
        SpatialTab.IsValid() && Click(Slate, SpatialTab.ToSharedRef())))
    { return false; }
    const TSharedPtr<SCheckBox> SpatialTrackToggle = FindCheckBoxWithText(
        SpatialPageView->GetRegion(TEXT("main")), TEXT("Spatial track"));
    if (NOT TestTrue(TEXT("production Spatial selector physically pins the track before it becomes 2D"),
        SpatialTrackToggle.IsValid() && Click(Slate, SpatialTrackToggle.ToSharedRef())
            && NOT DebuggerWindow->_SelectedSpatialTrackKey.IsEmpty()))
    { return false; }
    const TSharedPtr<FCkAudioDebugger_SpatialView> SpatialModel = DebuggerWindow->_SpatialView;
    const TSharedPtr<SCkAudioDebugger_FalloffCurve> SpatialCurve = DebuggerWindow->_AttenuationCurve;
    const auto SpatialRadar = DebuggerWindow->_Radar;
    const auto SpatialRepeat = SpatialPageView->GetRepeat(TEXT("audio-spatial-tracks"));
    const TSharedRef<SWidget> LiveAttenuationMain = AttenuationView->GetRegion(TEXT("main"));
    TSharedPtr<SWidget> SpatialPlots = FindTaggedWidget(
        DebuggerWindow->_PageSwitcher.ToSharedRef(), TEXT("audio-spatial-plots"));
    TSharedPtr<SWidget> SpatialMessage = FindTaggedWidget(
        DebuggerWindow->_PageSwitcher.ToSharedRef(), TEXT("audio-spatial-unavailable-wrap"));
    const bool SpatialProjectionIsMounted = TestTrue(TEXT("production spatial projection mounts its authored and retained widgets"),
        SpatialModel.IsValid() && SpatialCurve.IsValid() && SpatialPlots.IsValid() && SpatialMessage.IsValid());
    const bool SpatialProjectionIsVisible = TestTrue(TEXT("production spatial projection exposes plots and hides unavailable state"),
        SpatialPlots.IsValid() && SpatialMessage.IsValid() && SpatialPlots->GetVisibility().IsVisible()
            && SpatialMessage->GetVisibility() == EVisibility::Collapsed
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 3);
    const bool SpatialAttenuationIsRetained = TestTrue(TEXT("production spatial projection retains the authored arithmetic and live native curve"),
        SpatialCurve.IsValid() && ContainsWidget(DebuggerWindow->_AttenuationPanelHost.ToSharedRef(), LiveAttenuationMain)
            && ContainsWidget(LiveAttenuationMain, SpatialCurve.ToSharedRef()));
    const bool SpatialCurveIsArranged = TestTrue(TEXT("production spatial projection arranges the live native curve at its authored height"),
        SpatialCurve.IsValid() && SpatialCurve->GetCachedGeometry().GetLocalSize().X > 0.0f
            && FMath::IsNearlyEqual(SpatialCurve->GetCachedGeometry().GetLocalSize().Y, 110.0f));
    if (NOT SpatialProjectionIsMounted || NOT SpatialProjectionIsVisible
        || NOT SpatialAttenuationIsRetained || NOT SpatialCurveIsArranged)
    { return false; }
    TestTrue(TEXT("collector projection renders all attenuation values and exact volume-times-gain arithmetic"),
        TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-heading")) == TEXT("Why the audible volume is 0.20")
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-distance")) == TEXT("2.5 m")
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-bearing")) == TEXT("90°  right")
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-gain")) == TEXT("0.25")
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-track-volume")) == TEXT("0.80")
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-audible")) == TEXT("0.20")
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-asset")) == TEXT("Spatial attenuation"));

    if (NOT RunFixturePhase([&, this]() -> bool
    {
        const auto RefreshSpatial = [&]()
        {
            DebuggerWindow->_Collector.Collect(nullptr);
            DebuggerWindow->DoUpdate_SpatialView();
            SpatialRepeat->TryRefresh();
            TickSlate(Slate);
        };
        const auto OriginalTrack = TrackInfoA;
        const auto PinnedKey = DebuggerWindow->_SelectedSpatialTrackKey;
        const auto PinnedRow = SpatialRepeat->GetItemWidget(PinnedKey);
        if (NOT TestTrue(TEXT("physical Spatial pin is keyed and owns the exact retained toggle/status leaf"),
            PinnedRow.IsValid() && SpatialTrackToggle->IsChecked()
                && FindWidgetType(PinnedRow.ToSharedRef(), TEXT("SCkDebug_StatusPill")).IsValid())) { return false; }
        const auto StatusText = FindWidgetType(PinnedRow.ToSharedRef(), TEXT("STextBlock"));
        if (NOT TestTrue(TEXT("retained Spatial status exposes its live name and tone text"), StatusText.IsValid())) { return false; }
        auto EqualName = OriginalTrack;
        EqualName.TrackEntity = TrackB;
        EqualName.IsVirtualized = true;
        FixtureSnapshot.Directors[0].Tracks = {OriginalTrack, EqualName};
        RefreshSpatial();
        TestTrue(TEXT("equal-name Spatial records stay distinct and pinning overrides diagnostic ranking"),
            SpatialRepeat->GetItemCount() == 2 && SpatialRepeat->GetItemWidget(PinnedKey) == PinnedRow
                && DebuggerWindow->TryGet_SelectedSpatialTrack() != nullptr
                && DebuggerWindow->TryGet_SelectedSpatialTrack()->TrackEntity == TrackA);
        if (NOT TestTrue(TEXT("physical re-click clears the pin and follows the virtualized track"),
            Click(Slate, SpatialTrackToggle.ToSharedRef()) && DebuggerWindow->_SelectedSpatialTrackKey.IsEmpty()
                && DebuggerWindow->TryGet_SelectedSpatialTrack() != nullptr
                && DebuggerWindow->TryGet_SelectedSpatialTrack()->TrackEntity == TrackB)) { return false; }
        if (NOT TestTrue(TEXT("physical selector can pin the first equal-name entity again"),
            Click(Slate, SpatialTrackToggle.ToSharedRef()) && DebuggerWindow->_SelectedSpatialTrackKey == PinnedKey)) { return false; }
        auto ChangedTrack = OriginalTrack;
        ChangedTrack.TrackName = TEXT("Renamed spatial track");
        ChangedTrack.IsVirtualized = true;
        ChangedTrack.InnerRadius = 200.0f;
        FixtureSnapshot.Directors[0].Tracks = {ChangedTrack, EqualName};
        RefreshSpatial();
        TestTrue(TEXT("live name, tone, legend and alert update without rebuilding the selector or Radar"),
            SpatialRepeat->GetItemWidget(PinnedKey) == PinnedRow && DebuggerWindow->_Radar == SpatialRadar
                && StaticCastSharedPtr<STextBlock>(StatusText)->GetText().ToString() == ChangedTrack.TrackName
                && StaticCastSharedPtr<STextBlock>(StatusText)->GetColorAndOpacity().GetSpecifiedColor().Equals(CkStyle::GetToneColor(ECk_Tone::Err))
                && TaggedText(SpatialPageView->GetRegion(TEXT("main")), TEXT("audio-spatial-legend")) == TEXT("inner 2.0m  ·  falloff 10.0m")
                && TaggedText(SpatialPageView->GetRegion(TEXT("main")), TEXT("audio-spatial-alert-body")).Contains(TEXT("is virtualized")));
        const auto SpatialKeyBeforeOverlay = PinnedKey;
        DebuggerWindow->DoRebuild_OverlayList();
        const auto FilterBefore = DebuggerWindow->_FilterString;
        const auto PlayingBefore = DebuggerWindow->_ShowPlaying;
        DebuggerWindow->_FilterString = TEXT("no matching mixer track");
        DebuggerWindow->_ShowPlaying = false;
        RefreshSpatial();
        TestTrue(TEXT("mixer filters and Overlay rebuilding preserve Spatial records and physical identity"),
            SpatialRepeat->GetItemCount() == 2 && SpatialRepeat->GetItemWidget(PinnedKey) == PinnedRow
                && DebuggerWindow->_SpatialRecords->FindRecord(SpatialKeyBeforeOverlay).IsValid());
        DebuggerWindow->_FilterString = FilterBefore;
        DebuggerWindow->_ShowPlaying = PlayingBefore;

        const auto Revision = SpatialPageView->GetRevision();
        const auto Reload = SpatialPageView->TryReload(SpatialMarkup, SpatialCss, TEXT("Audio compatible Spatial candidate"));
        TickSlate(Slate);
        if (NOT Reload.Succeeded) { AddError(FString::Join(Reload.Errors, TEXT("\n"))); }
        TestTrue(TEXT("compatible Spatial reload retains selector, pin, exact Radar, attenuation host and shared model"),
            Reload.Succeeded && SpatialPageView->GetRevision() > Revision
                && SpatialRepeat->GetItemWidget(PinnedKey) == PinnedRow && SpatialTrackToggle->IsChecked()
                && DebuggerWindow->_Radar == SpatialRadar && DebuggerWindow->_SpatialView == SpatialModel
                && ContainsWidget(SpatialPageView->GetRegion(TEXT("main")), SpatialRadar.ToSharedRef())
                && ContainsWidget(SpatialPageView->GetRegion(TEXT("main")), SpatialCurve.ToSharedRef()));
        const auto AcceptedRevision = SpatialPageView->GetRevision();
        const auto Rejected = SpatialPageView->TryReload(SpatialMarkup.Replace(
            TEXT("item-action=\"audio-spatial-select\""), TEXT("item-action=\"missing-spatial-action\"")), SpatialCss);
        TestTrue(TEXT("rejected Spatial candidate leaves all mounted interaction and plot state intact"),
            NOT Rejected.Succeeded && SpatialPageView->GetRevision() == AcceptedRevision
                && SpatialRepeat->GetItemWidget(PinnedKey) == PinnedRow
                && DebuggerWindow->_SelectedSpatialTrackKey == PinnedKey && DebuggerWindow->_Radar == SpatialRadar);

        const auto StyleBefore = StyleSettings->Selection;
        StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Normal;
        StyleSettings->NotifyChanged();
        DebuggerWindow->OnStyleRevisionChanged();
        RefreshSpatial();
        const auto Footer = FindTaggedWidget(SpatialPageView->GetRegion(TEXT("main")), TEXT("audio-spatial-listener"));
        if (NOT TestTrue(TEXT("authored Spatial footer is available for style and scroll checks"),
            Footer.IsValid() && Footer->GetTypeAsString() == TEXT("SCkFlexText"))) { return false; }
        const auto NormalFont = StaticCastSharedPtr<SCkFlexText>(Footer)->GetFont().Size;
        StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Large;
        StyleSettings->NotifyChanged();
        DebuggerWindow->OnStyleRevisionChanged();
        RefreshSpatial();
        const auto LargeFooter = FindTaggedWidget(SpatialPageView->GetRegion(TEXT("main")), TEXT("audio-spatial-listener"));
        TestTrue(TEXT("Spatial style revision updates authored text while retaining selector and Radar identity"),
            LargeFooter.IsValid() && LargeFooter->GetTypeAsString() == TEXT("SCkFlexText")
                && StaticCastSharedPtr<SCkFlexText>(LargeFooter)->GetFont().Size > NormalFont
                && SpatialRepeat->GetItemWidget(PinnedKey) == PinnedRow && DebuggerWindow->_Radar == SpatialRadar);
        StyleSettings->Selection = StyleBefore;
        StyleSettings->NotifyChanged();
        DebuggerWindow->OnStyleRevisionChanged();
        RefreshSpatial();

        ChangedTrack.TrackName = FString::ChrN(180, TCHAR('W'));
        FixtureSnapshot.Directors[0].Tracks = {ChangedTrack, EqualName};
        HostWindow->Resize(FVector2D{460.0f, 520.0f});
        RefreshSpatial();
        const auto SelectorScroll = SpatialPageView->GetScroll(TEXT("audio-spatial-selector-scroll"));
        const auto ShellScroll = View->GetScroll(TEXT("audio-shell-scroll"));
        if (NOT TestTrue(TEXT("scroll fixture retains its two Spatial records"),
            SelectorScroll.IsValid() && ShellScroll.IsValid()
                && DebuggerWindow->_SpatialRecords->GetRecords().Num() == 2)) { return false; }
        const auto LastKey = DebuggerWindow->_SpatialRecords->GetRecords().Last()->GetKey();
        const auto LastRow = SpatialRepeat->GetItemWidget(LastKey);
        if (LastRow.IsValid())
        {
            SelectorScroll->SetScrollOffset(SelectorScroll->GetScrollOffsetOfEnd());
            ShellScroll->ScrollDescendantIntoView(LastRow.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
            TickSlate(Slate);
        }
        const auto LastGeometry = LastRow.IsValid() ? LastRow->GetCachedGeometry() : FGeometry{};
        const auto HostGeometry = DebuggerWindow->GetCachedGeometry();
        TestTrue(TEXT("narrow Spatial selector reaches its trailing track through horizontal scroll"),
            LastRow.IsValid() && ShellScroll->GetScrollOffsetOfEnd() > 0.0f && ShellScroll->GetScrollOffset() > 0.0f
                && LastGeometry.GetLocalSize().X > 0.0f
                && LastGeometry.GetAbsolutePosition().X >= HostGeometry.GetAbsolutePosition().X - 1.0f
                && LastGeometry.GetAbsolutePosition().X + LastGeometry.GetAbsoluteSize().X
                    <= HostGeometry.GetAbsolutePosition().X + HostGeometry.GetAbsoluteSize().X + 1.0f);
        SelectorScroll->SetScrollOffset(0.0f);
        ShellScroll->SetScrollOffset(0.0f);
        HostWindow->Resize(FVector2D{1100.0f, 720.0f});
        ChangedTrack.TrackName = OriginalTrack.TrackName;
        ChangedTrack.State = ECk_AudioTrack_State::Stopped;
        FixtureSnapshot.Directors[0].Tracks = {ChangedTrack, EqualName};
        // Dispatch sees the fresh stopped state even before records are reconciled.
        DebuggerWindow->_Collector.Collect(nullptr);
        SpatialTrackToggle->ToggleCheckedState();
        TestEqual(TEXT("held selector rejects a track stopped since its last publication"), DebuggerWindow->_SelectedSpatialTrackKey, PinnedKey);
        RefreshSpatial();
        SpatialTrackToggle->ToggleCheckedState();
        TestTrue(TEXT("stopped or removed selector controls retire and cannot toggle the old pin"),
            NOT SpatialRepeat->GetItemWidget(PinnedKey).IsValid() && DebuggerWindow->_SelectedSpatialTrackKey == PinnedKey);
        const auto ReplacementToggle = FindCheckBoxWithText(SpatialPageView->GetRegion(TEXT("main")), EqualName.TrackName);
        if (NOT TestTrue(TEXT("physical same-name replacement selection resolves the remaining full entity identity"),
            ReplacementToggle.IsValid() && Click(Slate, ReplacementToggle.ToSharedRef())
                && DebuggerWindow->_SelectedSpatialTrackKey != PinnedKey
                && DebuggerWindow->TryGet_SelectedSpatialTrack() != nullptr
                && DebuggerWindow->TryGet_SelectedSpatialTrack()->TrackEntity == TrackB)) { return false; }
        FixtureSnapshot.Directors[0].Tracks = {EqualName, EqualName};
        RefreshSpatial();
        TestTrue(TEXT("duplicate Spatial identities fail closed with no partially actionable records or plot"),
            NOT DebuggerWindow->_SpatialRecordsReady && DebuggerWindow->_SpatialRecords->GetRecords().IsEmpty()
                && SpatialRepeat->GetItemCount() == 0 && NOT SpatialModel->HasSelection);
        FixtureSnapshot.Directors[0].Tracks = {OriginalTrack};
        RefreshSpatial();
        const auto RecoveredToggle = FindCheckBoxWithText(SpatialPageView->GetRegion(TEXT("main")), OriginalTrack.TrackName);
        if (NOT TestTrue(TEXT("valid Spatial publication recovers a physically selectable full identity after rejection"),
            DebuggerWindow->_SpatialRecordsReady && SpatialRepeat->GetItemCount() == 1
                && RecoveredToggle.IsValid() && Click(Slate, RecoveredToggle.ToSharedRef())
                && SpatialModel->HasSpatialData && DebuggerWindow->_SelectedSpatialTrackKey == PinnedKey)) { return false; }
        return true;
    })) { return false; }

    SpatialPlots = FindTaggedWidget(SpatialPageView->GetRegion(TEXT("main")), TEXT("audio-spatial-plots"));
    SpatialMessage = FindTaggedWidget(SpatialPageView->GetRegion(TEXT("main")), TEXT("audio-spatial-unavailable-wrap"));
    if (NOT TestTrue(TEXT("accepted Spatial tree retains live plot and unavailable hosts"),
        SpatialPlots.IsValid() && SpatialMessage.IsValid())) { return false; }
    const int64 AttenuationRevision = AttenuationView->GetRevision();
    const FCkUiLoadResult AttenuationReloaded = AttenuationView->TryReload(
        AttenuationMarkup, AttenuationCss, TEXT("Audio compatible attenuation candidate"));
    TickSlate(Slate);
    if (NOT AttenuationReloaded.Succeeded)
    { AddError(FString::Join(AttenuationReloaded.Errors, TEXT("\n"))); }
    TestTrue(TEXT("compatible attenuation reload retains the exact curve, model, values and selected Spatial page"),
        AttenuationReloaded.Succeeded && AttenuationView->GetRevision() > AttenuationRevision
            && DebuggerWindow->_SpatialView == SpatialModel && DebuggerWindow->_AttenuationCurve == SpatialCurve
            && ContainsWidget(AttenuationView->GetRegion(TEXT("main")), SpatialCurve.ToSharedRef())
            && TaggedText(AttenuationView->GetRegion(TEXT("main")), TEXT("audio-attenuation-audible")) == TEXT("0.20")
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 3);
    const int64 AttenuationRevisionBeforeReject = AttenuationView->GetRevision();
    const FCkUiLoadResult AttenuationRejected = AttenuationView->TryReload(
        InvalidAttenuationStartupMarkup, TEXT(""), TEXT("Audio rejected attenuation candidate"));
    TestTrue(TEXT("missing attenuation curve is rejected without changing the accepted block"),
        NOT AttenuationRejected.Succeeded
            && FString::Join(AttenuationRejected.Errors, TEXT("\n")).Contains(TEXT("missing-attenuation-curve"))
            && AttenuationView->GetRevision() == AttenuationRevisionBeforeReject
            && ContainsWidget(AttenuationView->GetRegion(TEXT("main")), SpatialCurve.ToSharedRef())
            && TaggedText(AttenuationView->GetRegion(TEXT("main")), TEXT("audio-attenuation-audible")) == TEXT("0.20"));

    TrackInfoA.CurrentVolume = 0.6f;
    TrackInfoA.AttenuationGain = 0.5f;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_SpatialView();
    TestTrue(TEXT("same selected track updates authored arithmetic through the retained spatial model"),
        DebuggerWindow->_SpatialView == SpatialModel && DebuggerWindow->_AttenuationCurve == SpatialCurve
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-track-volume")) == TEXT("0.60")
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-gain")) == TEXT("0.50")
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-audible")) == TEXT("0.30"));
    TrackInfoA.IsVirtualized = true;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_SpatialView();
    TestEqual(TEXT("virtualization projects zero audible volume without changing the track volume"),
        TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-audible")), FString{TEXT("0.00")});
    TrackInfoA.IsVirtualized = false;
    TrackInfoA.IsAttenuated = false;
    TrackInfoA.AttenuationAssetName.Reset();
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_SpatialView();
    TestTrue(TEXT("unattenuated spatial data keeps native gain and asset text semantics"),
        TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-gain")) == TEXT("n/a (not attenuated)")
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-audible")) == TEXT("0.60")
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-asset")) == TEXT("(none)"));
    TrackInfoA.HasSpatialData = false;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_SpatialView();
    TickSlate(Slate);
    TestTrue(TEXT("a selected 2D track hides attenuation and clears previously rendered numeric data"),
        SpatialModel->HasSelection && NOT SpatialModel->HasSpatialData
            && SpatialPlots->GetVisibility() == EVisibility::Collapsed
            && SpatialMessage->GetVisibility().IsVisible()
            && TaggedText(SpatialPageView->GetRegion(TEXT("main")), TEXT("audio-spatial-unavailable")).Contains(TEXT("has no spatial data"))
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-audible")).IsEmpty());
    FixtureSnapshot.Directors[0].Tracks.Reset();
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_SpatialView();
    TickSlate(Slate);
    TestTrue(TEXT("removing the selected track leaves no stale attenuation selection or curve samples"),
        NOT SpatialModel->HasSelection && SpatialModel->FalloffCurve.IsEmpty()
            && SpatialPlots->GetVisibility() == EVisibility::Collapsed
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-heading")).IsEmpty());
    TrackInfoA.HasSpatialData = true;
    TrackInfoA.IsAttenuated = true;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_SpatialView();
    SpatialRepeat->TryRefresh();
    TestTrue(TEXT("spatial lifecycle assertion begins from populated authored state"),
        SpatialModel->HasSpatialData && NOT SpatialModel->FalloffCurve.IsEmpty()
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-audible")) == TEXT("0.30"));
    DebuggerWindow->DoUpdate_DirectorRecords();
    DirectorsRepeat->TryRefresh();
    const auto LiveDirectorRow = DirectorsRepeat->GetItemWidget(DirectorKey);
    const auto LiveDirectorRef = LiveDirectorRow.IsValid() ? FindEntityRef(LiveDirectorRow.ToSharedRef()) : nullptr;
    if (NOT TestTrue(TEXT("director teardown fixture holds a currently published entity link"), LiveDirectorRef.IsValid()))
    { return false; }
    DebuggerWindow->DoUpdate_TrackRecords();
    TracksRepeat->TryRefresh();
    const auto LiveTrackRow = TracksRepeat->GetItemWidget(TrackRecordKey);
    const auto LiveTrackLink = LiveTrackRow.IsValid() ? FindTaggedWidget(LiveTrackRow.ToSharedRef(), TEXT("audio-track-entity")) : nullptr;
    const auto LiveTrackRef = LiveTrackLink.IsValid() ? FindEntityRef(LiveTrackLink.ToSharedRef()) : nullptr;
    if (NOT TestTrue(TEXT("Tracks teardown fixture holds a currently published entity link"), LiveTrackRef.IsValid()))
    { return false; }
    const auto SessionFixture = FixtureSnapshot;
    const auto HeldSpatialToggle = FindCheckBoxWithText(SpatialPageView->GetRegion(TEXT("main")), TrackInfoA.TrackName);
    const auto HeldSpatialFooter = FindTaggedWidget(SpatialPageView->GetRegion(TEXT("main")), TEXT("audio-spatial-listener"));
    if (NOT TestTrue(TEXT("Spatial lifecycle fixture holds a published selector and weak listener footer"),
        HeldSpatialToggle.IsValid() && HeldSpatialFooter.IsValid())) { return false; }
    UWorld* InvalidatedWorld = NewObject<UWorld>();
    UWorld* UnrelatedWorld = NewObject<UWorld>();
    DebuggerWindow->_ObservedWorld = InvalidatedWorld;
    ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Broadcast(UnrelatedWorld);
    TestTrue(TEXT("unrelated world invalidation leaves the observed Audio session intact"),
        DebuggerWindow->_ObservedWorld.Get() == InvalidatedWorld
            && DebuggerWindow->_Collector.Get_Snapshot().HasWorld);
    ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Broadcast(InvalidatedWorld);
    HeldSpatialToggle->ToggleCheckedState();
    TestTrue(TEXT("world invalidation clears Spatial records and synchronously revokes a held selector"),
        DebuggerWindow->_SpatialRecords->GetRecords().IsEmpty() && DebuggerWindow->_SelectedSpatialTrackKey.IsEmpty());
    ProbeRetainedEntityRef(LiveDirectorRef.ToSharedRef());
    ProbeRetainedEntityRef(LiveTrackRef.ToSharedRef());
    TestTrue(TEXT("world invalidation clears Tracks records and synchronously revokes held track actions"),
        DebuggerWindow->_TrackRecords->GetRecords().IsEmpty() && TrackNavigationResults->Num() == 4);
    TestEqual(TEXT("world invalidation revokes held director navigation synchronously"), NavigationResults->Num(), 3);
    TestTrue(TEXT("world invalidation clears native Events history without resetting toolbar preferences"),
        LiveEventLog->Get_EntryCount() == 0 && DebuggerWindow->_EventLog == LiveEventLog
            && NOT DebuggerWindow->_EventsShowStateChanges && DebuggerWindow->_EventsShowFades
            && DebuggerWindow->_EventsShowVirtualization && DebuggerWindow->_EventsShowLifecycle);
    HeldTrackBToggle->ToggleCheckedState();
    TestTrue(TEXT("matching world invalidation synchronously clears handle-backed Audio state"),
        NOT DebuggerWindow->_Collector.Get_Snapshot().HasWorld
            && DebuggerWindow->_InvalidatedWorld.Get() == InvalidatedWorld
            && DebuggerWindow->_DirectorSlots.IsEmpty()
            && DebuggerWindow->_DirectorPageSlots.IsEmpty()
            && DebuggerWindow->_DirectorRecords->GetRecords().IsEmpty()
            && DebuggerWindow->_TrackSlots.IsEmpty()
            && DebuggerWindow->_VolumeHistory.IsEmpty()
            && DebuggerWindow->_TrackWatch.IsEmpty()
            && DebuggerWindow->_OverlayListBox->GetChildren()->Num() == 0
            && DebuggerWindow->_SpatialSelectorBox->GetChildren()->Num() == 0
            && DebuggerWindow->_StatConcurrency->ToString() == TEXT("0 / 0"));
    TestFalse(TEXT("held production overlay action remains inert after world invalidation"),
        TrackB.Has<ck::FTag_AudioTrack_DebugDraw>());
    TestTrue(TEXT("world invalidation clears the shared spatial model observed by retained authored text and curve"),
        DebuggerWindow->_SpatialView == SpatialModel && NOT SpatialModel->HasSelection
            && NOT SpatialModel->HasSpatialData && SpatialModel->FalloffCurve.IsEmpty()
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-audible")).IsEmpty());

    DebuggerWindow->_Collector._SnapshotOverrideForTests.Emplace(SessionFixture);
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoUpdate_SpatialView();
    HeldSpatialToggle->ToggleCheckedState();
    TestTrue(TEXT("same-identity next-session records do not revive a prior-session held selector"),
        DebuggerWindow->_SelectedSpatialTrackKey.IsEmpty() && NOT DebuggerWindow->_SpatialRecords->GetRecords().IsEmpty());
    TestTrue(TEXT("session clearing is independently primed with live attenuation data"),
        SpatialModel->HasSpatialData
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-audible")) == TEXT("0.30"));
    DebuggerWindow->_ObservedWorld = InvalidatedWorld;
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Broadcast();
    TestTrue(TEXT("session invalidation independently clears Audio state"),
        NOT DebuggerWindow->_Collector.Get_Snapshot().HasWorld
            && DebuggerWindow->_DirectorRecords->GetRecords().IsEmpty()
            && DebuggerWindow->_InvalidatedWorld.Get() == InvalidatedWorld
            && DebuggerWindow->_TrackRecords->GetRecords().IsEmpty()
            && DebuggerWindow->_SpatialRecords->GetRecords().IsEmpty()
            && NOT SpatialModel->HasSpatialData && SpatialModel->FalloffCurve.IsEmpty()
            && TaggedText(LiveAttenuationMain, TEXT("audio-attenuation-audible")).IsEmpty());
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Broadcast();
    TestTrue(TEXT("repeated session invalidation preserves the blocked-world marker"),
        DebuggerWindow->_InvalidatedWorld.Get() == InvalidatedWorld);
    if (NOT TestTrue(TEXT("Events controls still accept physical preference input after session invalidation"),
        Click(Slate, FallbackEventsTab.ToSharedRef())
            && Click(Slate, LiveEventsStateToggle.ToSharedRef())
            && DebuggerWindow->_EventsShowStateChanges && DebuggerWindow->_EventsShowFades
            && DebuggerWindow->_EventsShowVirtualization && DebuggerWindow->_EventsShowLifecycle
            && NOT DebuggerWindow->_Collector.Get_Snapshot().HasWorld
            && LiveEventLog->Get_EntryCount() == 0))
    { return false; }

    auto Module = FCkAudioDebuggerModule{};
    Module._DebuggerWindow = DebuggerWindow;
    Module._DebuggerTab = SNew(SDockTab);
    Module.HandleEnginePreExit();
    TestTrue(TEXT("module pre-exit releases Audio tab and window ownership without a close request"),
        NOT Module._DebuggerTab.IsValid() && NOT Module._DebuggerWindow.IsValid());
    const TWeakPtr<SCkAudioDebuggerWindow> ReleasedPopulatedOwner = DebuggerWindow;
    const TWeakPtr<FCkUiView> ReleasedPopulatedDirectors = DirectorsView;
    const TWeakPtr<FCkUiView> ReleasedPopulatedTracks = TracksView;
    const TWeakPtr<FCkUiView> ReleasedPopulatedSpatial = SpatialPageView;
    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    DebuggerWindow.Reset();
    DirectorsView.Reset();
    ProbeRetainedEntityRef(LiveDirectorRef.ToSharedRef());
    TracksView.Reset();
    SpatialPageView.Reset();
    HeldSpatialToggle->ToggleCheckedState();
    TestTrue(TEXT("held Spatial controls and footer release the owner/view and stay inert after teardown"),
        NOT ReleasedPopulatedOwner.IsValid() && NOT ReleasedPopulatedSpatial.IsValid()
            && NOT HeldSpatialToggle->IsChecked()
            && StaticCastSharedPtr<SCkFlexText>(HeldSpatialFooter)->GetText().IsEmpty());
    ProbeRetainedEntityRef(LiveTrackRef.ToSharedRef());
    TestTrue(TEXT("held Tracks leaves release the owner/view and remain inert after teardown"),
        NOT ReleasedPopulatedOwner.IsValid() && NOT ReleasedPopulatedTracks.IsValid()
            && TrackNavigationResults->Num() == 4);
    TestTrue(TEXT("held director leaves neither retain the released owner/view nor dispatch after teardown"),
        NOT ReleasedPopulatedOwner.IsValid() && NOT ReleasedPopulatedDirectors.IsValid()
            && NavigationResults->Num() == 3);
    return true;
    });
}

#endif
