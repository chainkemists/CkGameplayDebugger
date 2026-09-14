#include "CkAudioDebugger/Window/SCkAudioDebuggerWindow.h"
#include "../../CkAudioDebugger_Module.h"

#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"
#include "CkSlateLayout/SCkUiSurface.h"

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
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_audio_debugger_authored_shell_tests
{
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAudioDebugger_AuthoredShell,
    "Ck.AudioDebugger.AuthoredShell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkAudioDebugger_AuthoredShell::RunTest(const FString&) -> bool
{
    using namespace ck_audio_debugger_authored_shell_tests;

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

    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    TestTrue(TEXT("authored shell retains all four production presentation boundaries"),
        DebuggerWindow->_Tabs.IsValid()
            && DebuggerWindow->_StatCards.IsValid()
            && DebuggerWindow->_FilterRow.IsValid()
            && DebuggerWindow->_PageSwitcher.IsValid()
            && ContainsWidget(Main, DebuggerWindow->_Tabs.ToSharedRef())
            && ContainsWidget(Main, DebuggerWindow->_StatCards.ToSharedRef())
            && ContainsWidget(Main, DebuggerWindow->_FilterRow.ToSharedRef())
            && ContainsWidget(Main, DebuggerWindow->_PageSwitcher.ToSharedRef()));
    TestEqual(TEXT("production page switcher retains all six Audio pages"),
        DebuggerWindow->_PageSwitcher->GetNumWidgets(), 6);
    TestEqual(TEXT("production Audio shell preserves Tracks as its default page"),
        DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex(), 1);
    TestTrue(TEXT("authored shell exposes horizontal overflow reachability"),
        View->GetScroll(TEXT("audio-shell-scroll")).IsValid());

    const TSharedPtr<SButton> CrossfadeTab = FindButtonWithText(
        DebuggerWindow->_Tabs.ToSharedRef(), TEXT("Crossfade"));
    TestTrue(TEXT("physical nondefault tab selection routes through the production tab strip"),
        CrossfadeTab.IsValid() && Click(Slate, CrossfadeTab.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Css;
    const FString Directory = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"))
        : FString{};
    if (NOT TestTrue(TEXT("installed Audio shell resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(Css, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.css")))))
    { return false; }

    const auto Revision = View->GetRevision();
    const FCkUiLoadResult Reloaded = View->TryReload(Markup, Css, TEXT("Audio compatible shell candidate"));
    TickSlate(Slate);
    if (NOT Reloaded.Succeeded) { AddError(FString::Join(Reloaded.Errors, TEXT("\n"))); }
    TestTrue(TEXT("compatible reload retains every native boundary and page state"),
        Reloaded.Succeeded && View->GetRevision() > Revision
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_Tabs.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_StatCards.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_FilterRow.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef())
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

    HostWindow->Resize(FVector2D{480.0f, 480.0f});
    TickSlate(Slate);
    TestTrue(TEXT("actual narrow Audio shell keeps horizontal overflow reachable"),
        View->GetScroll(TEXT("audio-shell-scroll"))->GetScrollOffsetOfEnd() > 0.0f);

    HostWindow->Resize(FVector2D{1100.0f, 240.0f});
    TickSlate(Slate);
    TestTrue(TEXT("short Audio shell lets its page body shrink without vertical clipping pressure"),
        DebuggerWindow->_PageSwitcher->GetCachedGeometry().GetLocalSize().Y > 0.0f
            && DebuggerWindow->_PageSwitcher->GetCachedGeometry().GetLocalSize().Y < 320.0f);

    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    TickSlate(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    DebuggerWindow.Reset();
    View.Reset();
    TestFalse(TEXT("authored Audio view releases with its production window"), ReleasedView.IsValid());

    const FString InvalidStartupMarkup =
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-audio-port\"/></region></ui>");
    bool MarkupRestored = false;
    ON_SCOPE_EXIT
    {
        if (NOT MarkupRestored)
        { FFileHelper::SaveStringToFile(Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html"))); }
    };
    if (NOT TestTrue(TEXT("Audio fixture installs its valid-but-unbound startup candidate"),
        FFileHelper::SaveStringToFile(
            InvalidStartupMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")))))
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
    TestTrue(TEXT("invalid startup resource mounts the complete native fallback"),
        View.IsValid() && NOT View->GetLastResult().Succeeded
            && DebuggerWindow->_UsingNativeFallback
            && ContainsWidget(DebuggerWindow->_AuthoredShellHost.ToSharedRef(),
                DebuggerWindow->_PageSwitcher.ToSharedRef()));

    MarkupRestored = FFileHelper::SaveStringToFile(
        Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")));
    TestTrue(TEXT("Audio fixture restores the valid production resource"), MarkupRestored);
    DebuggerWindow->PollAuthoredShell(FPlatformTime::Seconds() + 10.0);
    TickSlate(Slate);
    TestTrue(TEXT("valid file change recovers a live startup fallback without reopening"),
        View->GetLastResult().Succeeded && NOT DebuggerWindow->_UsingNativeFallback
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_Tabs.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_StatCards.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_FilterRow.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef()));

    using namespace ck::registry_table;
    auto Registry = EnttRegistryType{};
    const auto RegistrySlot = Allocate(&Registry);
    ON_SCOPE_EXIT
    {
        DebuggerWindow->HandleSessionInvalidated();
        Free(RegistrySlot);
    };

    const auto DirectorEntity = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
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
    TestTrue(TEXT("an empty director participates in the structure signature and dedicated page"),
        NOT EmptyDirectorSignature.IsEmpty()
            && DebuggerWindow->_DirectorSlots.IsEmpty()
            && DebuggerWindow->_DirectorPageSlots.Num() == 1
            && DebuggerWindow->_DirectorPageSlots[0].ActiveText->GetText().ToString() == TEXT("0 / 4 active"));

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
    TestTrue(TEXT("both Audio pages receive the live director concurrency count"),
        DebuggerWindow->_DirectorSlots.Num() == 1
            && DebuggerWindow->_DirectorPageSlots.Num() == 1
            && DebuggerWindow->_DirectorSlots[0].ActiveText->GetText().ToString() == TEXT("1 / 4 active")
            && DebuggerWindow->_DirectorPageSlots[0].ActiveText->GetText().ToString() == TEXT("1 / 4 active"));

    const TSharedPtr<SButton> OverlayTab = FindButtonWithText(DebuggerWindow->_Tabs.ToSharedRef(), TEXT("Overlay"));
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
    const auto SessionFixture = FixtureSnapshot;
    UWorld* InvalidatedWorld = NewObject<UWorld>();
    UWorld* UnrelatedWorld = NewObject<UWorld>();
    DebuggerWindow->_ObservedWorld = InvalidatedWorld;
    ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Broadcast(UnrelatedWorld);
    TestTrue(TEXT("unrelated world invalidation leaves the observed Audio session intact"),
        DebuggerWindow->_ObservedWorld.Get() == InvalidatedWorld
            && DebuggerWindow->_Collector.Get_Snapshot().HasWorld);
    ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Broadcast(InvalidatedWorld);
    HeldTrackBToggle->ToggleCheckedState();
    TestTrue(TEXT("matching world invalidation synchronously clears handle-backed Audio state"),
        NOT DebuggerWindow->_Collector.Get_Snapshot().HasWorld
            && DebuggerWindow->_InvalidatedWorld.Get() == InvalidatedWorld
            && DebuggerWindow->_DirectorSlots.IsEmpty()
            && DebuggerWindow->_DirectorPageSlots.IsEmpty()
            && DebuggerWindow->_TrackSlots.IsEmpty()
            && DebuggerWindow->_VolumeHistory.IsEmpty()
            && DebuggerWindow->_TrackWatch.IsEmpty()
            && DebuggerWindow->_OverlayListBox->GetChildren()->Num() == 0
            && DebuggerWindow->_SpatialSelectorBox->GetChildren()->Num() == 0
            && DebuggerWindow->_StatConcurrency->ToString() == TEXT("0 / 0"));
    TestFalse(TEXT("held production overlay action remains inert after world invalidation"),
        TrackB.Has<ck::FTag_AudioTrack_DebugDraw>());

    DebuggerWindow->_Collector._SnapshotOverrideForTests.Emplace(SessionFixture);
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->_ObservedWorld = InvalidatedWorld;
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Broadcast();
    TestTrue(TEXT("session invalidation independently clears Audio state"),
        NOT DebuggerWindow->_Collector.Get_Snapshot().HasWorld
            && DebuggerWindow->_InvalidatedWorld.Get() == InvalidatedWorld);
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Broadcast();
    TestTrue(TEXT("repeated session invalidation preserves the blocked-world marker"),
        DebuggerWindow->_InvalidatedWorld.Get() == InvalidatedWorld);

    auto Module = FCkAudioDebuggerModule{};
    Module._DebuggerWindow = DebuggerWindow;
    Module._DebuggerTab = SNew(SDockTab);
    Module.HandleEnginePreExit();
    TestTrue(TEXT("module pre-exit releases Audio tab and window ownership without a close request"),
        NOT Module._DebuggerTab.IsValid() && NOT Module._DebuggerWindow.IsValid());
    return true;
}

#endif
