#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#define private public
#include "CkAudio/AudioDirector/CkAudioDirector_Fragment.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#undef private

#include "CkAudio/AudioDirector/CkAudioDirector_Utils.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEcsDebugger/Inspectors/CkInspector_Audio.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

#include <variant>

namespace ck_inspector_audio_authored_test
{
    auto FindButton(const TSharedRef<SWidget>& Root, const FName Tag) -> TSharedPtr<SButton>
    {
        if (Root->GetTypeAsString() == TEXT("SButton") && Root->GetTag() == Tag)
        { return StaticCastSharedRef<SButton>(Root); }
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (const TSharedPtr<SButton> Found = FindButton(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), Tag); Found.IsValid()) { return Found; } }
        return nullptr;
    }

    auto FindInput(const TSharedRef<SWidget>& Root) -> TSharedPtr<SEditableTextBox>
    {
        if (Root->GetTypeAsString() == TEXT("SEditableTextBox")
            || Root->GetTypeAsString() == TEXT("SCkUiTextInputBox"))
        { return StaticCastSharedRef<SEditableTextBox>(Root); }
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (const TSharedPtr<SEditableTextBox> Found = FindInput(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid()) { return Found; } }
        return nullptr;
    }

    auto FindSwitch(const TSharedRef<SWidget>& Root, const FName Tag) -> TSharedPtr<SCkDebug_Switch>
    {
        if (Root->GetTypeAsString() == TEXT("SCkDebug_Switch") && Root->GetTag() == Tag)
        { return StaticCastSharedRef<SCkDebug_Switch>(Root); }
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCkDebug_Switch> Found = FindSwitch(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), Tag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto ToggleSwitch(const TSharedRef<SCkDebug_Switch>& Switch) -> void
    {
        const FGeometry Geometry = FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{});
        const TSet<FKey> PressedButtons{EKeys::LeftMouseButton};
        const FPointerEvent Click{0, FVector2D{5.0f, 5.0f}, FVector2D{5.0f, 5.0f}, PressedButtons,
            EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
        Switch->OnMouseButtonDown(Geometry, Click);
    }

    auto Tick(FSlateApplication& Slate) -> void { Slate.PumpMessages(); Slate.Tick(); Slate.Tick(); }

    auto Commit(FSlateApplication& Slate, const TSharedRef<SEditableTextBox>& Input, const FString& Value) -> bool
    {
        Slate.SetUserFocus(0, Input, EFocusCause::SetDirectly);
        Tick(Slate);
        Input->SetText(FText::FromString(Value));
        if (NOT Slate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0}))
        { return false; }
        Tick(Slate);
        return true;
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope() { if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); } }
        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };

    auto CreateTrack(const FCk_Handle& Owner, const ECk_AudioTrack_State State, const float Volume) -> FCk_Handle
    {
        auto Track = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
        Track.Add<ck::FFragment_AudioTrack_Params>();
        auto& Current = Track.Add<ck::FFragment_AudioTrack_Current>();
        Current._State = State;
        Current._CurrentVolume = Volume;
        Current._TargetVolume = Volume + 0.25f;
        Current._FadeSpeed = 0.75f;
        Current._PlaybackPercent = 0.625f;
        Current._IsVirtualized = true;
        return Track;
    }

    auto MakeTrackHandle(const FCk_Handle& Track) -> FCk_Handle_AudioTrack
    { return UCk_Utils_AudioTrack_UE::CastChecked(Track); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorAudioAuthored,
    "Ck.UiAuthoring.EcsDebugger.AudioInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorAudioAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_audio_authored_test;

    auto InvalidInspector = FCkInspector_Audio{};
    const TSharedRef<SCkInspector_AudioAuthored> Invalid = StaticCastSharedRef<SCkInspector_AudioAuthored>(InvalidInspector.Build_Inspector({}));
    TestTrue(*FString::Printf(TEXT("default-invalid Audio shell mounts fail closed without requiring track or director data (load error: %s)"),
        *InvalidInspector.Get_LastAuthoredLoadError()),
        Invalid->Is_Mounted() && NOT Invalid->Get_IsTrackAvailable() && NOT Invalid->Get_IsDirectorAvailable()
            && Invalid->Get_TrackStateText() == TEXT("--") && Invalid->Get_DirectorActiveTracksText() == TEXT("--"));
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid Audio shell releases its view and collection"),
        Invalid->Is_Inert() && NOT Invalid->Get_View().IsValid() && NOT Invalid->Get_TracksCollection().IsValid());

    auto World = ck::FEcsWorld{};
    auto Owner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    UWorld* const LocalWorld = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { if (LocalWorld != nullptr) { LocalWorld->DestroyWorld(false); } };
    if (NOT TestTrue(TEXT("fixture has transient owner and standalone world"), ck::IsValid(Owner) && LocalWorld != nullptr && LocalWorld->GetNetMode() == NM_Standalone)) { return false; }
    Owner.Add<TWeakObjectPtr<UWorld>>(LocalWorld);
    auto TrackOnly = CreateTrack(Owner, ECk_AudioTrack_State::Playing, 0.5f);
    auto TrackBeta = CreateTrack(Owner, ECk_AudioTrack_State::Paused, 0.8f);
    auto DirectorOnly = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    DirectorOnly.Add<TWeakObjectPtr<UWorld>>(LocalWorld);
    DirectorOnly.Add<ck::FFragment_AudioDirector_Params>();
    auto& DirectorCurrent = DirectorOnly.Add<ck::FFragment_AudioDirector_Current>();
    DirectorCurrent._CurrentHighestPriority = 7;
    DirectorCurrent._HasFiredAllTracksFinished = false;
    DirectorCurrent._ActiveTracks.Add(MakeTrackHandle(TrackOnly));
    DirectorCurrent._TracksByName.Add(TEXT("Alpha"), MakeTrackHandle(TrackOnly));
    DirectorCurrent._TracksByName.Add(TEXT("Beta"), MakeTrackHandle(TrackBeta));
    auto Combined = CreateTrack(Owner, ECk_AudioTrack_State::FadingIn, 0.25f);
    Combined.Add<TWeakObjectPtr<UWorld>>(LocalWorld);
    Combined.Add<ck::FFragment_AudioDirector_Params>();
    auto& CombinedDirector = Combined.Add<ck::FFragment_AudioDirector_Current>();
    CombinedDirector._CurrentHighestPriority = 3;
    CombinedDirector._HasFiredAllTracksFinished = true;
    CombinedDirector._TracksByName.Add(TEXT("Alpha"), MakeTrackHandle(TrackOnly));
    if (NOT TestTrue(TEXT("fixture composes real track-only director-only combined and Current-only entities"),
        ck::IsValid(TrackOnly) && ck::IsValid(DirectorOnly) && ck::IsValid(Combined))) { return false; }

    auto Inspector = FCkInspector_Audio{};
    const TSharedRef<SCkInspector_AudioAuthored> TrackView = StaticCastSharedRef<SCkInspector_AudioAuthored>(Inspector.Build_Inspector(TrackOnly));
    const TSharedRef<SCkInspector_AudioAuthored> DirectorView = StaticCastSharedRef<SCkInspector_AudioAuthored>(Inspector.Build_Inspector(DirectorOnly));
    const TSharedRef<SCkInspector_AudioAuthored> CombinedView = StaticCastSharedRef<SCkInspector_AudioAuthored>(Inspector.Build_Inspector(Combined));
    auto CurrentOnly = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    CurrentOnly.Add<ck::FFragment_AudioTrack_Current>();
    const TSharedRef<SCkInspector_AudioAuthored> CurrentOnlyView = StaticCastSharedRef<SCkInspector_AudioAuthored>(Inspector.Build_Inspector(CurrentOnly));
    auto CurrentOnlyDirector = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    CurrentOnlyDirector.Add<ck::FFragment_AudioDirector_Current>();
    const TSharedRef<SCkInspector_AudioAuthored> CurrentOnlyDirectorView =
        StaticCastSharedRef<SCkInspector_AudioAuthored>(Inspector.Build_Inspector(CurrentOnlyDirector));
    TSharedPtr<FCkUiView> TrackUi = TrackView->Get_View();
    TSharedPtr<FCkUiView> DirectorUi = DirectorView->Get_View();
    TSharedPtr<FCkUiCollection> Tracks = DirectorView->Get_TracksCollection();
    if (NOT TestTrue(*FString::Printf(TEXT("all production shapes mount independent authored views and director owns a repeat collection (load error: %s)"),
        *Inspector.Get_LastAuthoredLoadError()),
        TrackView->Is_Mounted() && DirectorView->Is_Mounted() && CombinedView->Is_Mounted()
            && CurrentOnlyView->Is_Mounted() && CurrentOnlyDirectorView->Is_Mounted()
            && TrackUi.IsValid() && DirectorUi.IsValid() && TrackUi != DirectorUi && Tracks.IsValid() && Tracks->GetRecords().Num() == 2)) { return false; }
    auto ExpectedTrackKeys = TArray<FString>{};
    for (const auto& [Name, Handle] : DirectorCurrent.Get_TracksByName())
    { ExpectedTrackKeys.Add(Name.ToString()); }
    TestTrue(TEXT("authored director records preserve native TMap iteration order"),
        ExpectedTrackKeys.Num() == 2
            && Tracks->GetRecords()[0]->GetKey() == ExpectedTrackKeys[0]
            && Tracks->GetRecords()[1]->GetKey() == ExpectedTrackKeys[1]);
    TestTrue(TEXT("track-only director-only combined and Current-only presentation preserve availability parity"),
        TrackView->Get_IsTrackAvailable() && NOT TrackView->Get_IsDirectorAvailable()
            && NOT DirectorView->Get_IsTrackAvailable() && DirectorView->Get_IsDirectorAvailable()
            && CombinedView->Get_IsTrackAvailable() && CombinedView->Get_IsDirectorAvailable()
            && CurrentOnlyView->Get_IsTrackAvailable() && NOT CurrentOnlyView->Get_CanRequestTrack()
            && CurrentOnlyDirectorView->Get_IsDirectorAvailable()
            && NOT CurrentOnlyDirectorView->Get_CanRequestDirector());
    TestTrue(TEXT("track projections expose exact typed values and playback percentage"),
        TrackView->Get_TrackStateText() == TEXT("Playing")
            && TrackView->Get_TrackVolumeText() == TEXT("0.500 → 0.750") && TrackView->Get_TrackFadeSpeedText() == TEXT("0.750")
            && TrackView->Get_TrackPlaybackText() == TEXT("62.5%")
            && TrackView->Get_TrackVirtualizedText() == TEXT("Yes"));
    TestTrue(TEXT("director projections expose exact active count priority and finished values"),
        DirectorView->Get_DirectorActiveTracksText() == TEXT("1") && DirectorView->Get_DirectorPriorityText() == TEXT("7")
            && DirectorView->Get_DirectorAllFinishedText() == TEXT("No") && CombinedView->Get_DirectorAllFinishedText() == TEXT("Yes"));

    auto TrackRows = TMap<FString, FString>{}; auto DirectorRows = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture{}; Inspector.Build_Inspector(TrackOnly); TrackRows = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture{}; Inspector.Build_Inspector(DirectorOnly); DirectorRows = Capture.Get_Rows(); }
    const TSet<FString> Diffs = FCkInspectorWidgetBuilder::Compute_DifferingLabels({TrackRows, DirectorRows});
    TestTrue(TEXT("native row capture remains the exact Audio comparison authority"),
        TrackRows.FindRef(TEXT("State:")) == TEXT("Playing")
            && TrackRows.FindRef(TEXT("Volume (cur → target):")) == TEXT("0.500 → 0.750")
            && TrackRows.FindRef(TEXT("Playback:")) == TEXT("Play Stop")
            && DirectorRows.FindRef(TEXT("Active Tracks:")) == TEXT("1") && Diffs.Contains(TEXT("State:"))
            && Diffs.Contains(TEXT("Active Tracks:")) && Diffs.Contains(TEXT("Highest Priority:")));
    CombinedDirector._TracksByName[TEXT("Alpha")] = MakeTrackHandle(TrackBeta);
    auto DirectorVariantRows = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture{}; Inspector.Build_Inspector(Combined); DirectorVariantRows = Capture.Get_Rows(); }
    const TSet<FString> DirectorDiffs = FCkInspectorWidgetBuilder::Compute_DifferingLabels({DirectorRows, DirectorVariantRows});
    TestTrue(TEXT("same-name director rows retain handle values and expose cross-entity handle differences"),
        DirectorRows.FindRef(TEXT("Alpha")) == ck::Format_UE(TEXT("[{}]"), MakeTrackHandle(TrackOnly))
            && DirectorRows.FindRef(TEXT("Alpha Actions")) == TEXT("Stop")
            && DirectorDiffs.Contains(TEXT("Alpha")));
    TSet<FString> AuthoredDiffs = Diffs;
    for (const FString& Label : DirectorDiffs)
    { AuthoredDiffs.Add(Label); }
    TSharedPtr<SCkInspector_AudioAuthored> DiffView;
    { const FCkInspector_DiffMarkScope Scope{&AuthoredDiffs}; DiffView = StaticCastSharedRef<SCkInspector_AudioAuthored>(Inspector.Build_Inspector(Combined)); }
    TestTrue(TEXT("authored Audio labels retain exact native diff verdicts"), DiffView.IsValid()
        && DiffView->Is_DiffMarked(TEXT("State:")) && DiffView->Is_DiffMarked(TEXT("Active Tracks:"))
        && DiffView->Is_DiffMarked(TEXT("Alpha")));

    const TSharedPtr<SButton> Play = FindButton(TrackView, TEXT("audio-track-play"));
    const TSharedPtr<SButton> Stop = FindButton(TrackView, TEXT("audio-track-stop"));
    const TSharedPtr<SButton> StopAll = FindButton(DirectorView, TEXT("audio-director-stop-all"));
    const TSharedPtr<SButton> StopAlpha = FindButton(DirectorView, TEXT("audio-director-track-stop"));
    const TSharedPtr<SEditableTextBox> Volume = FindInput(TrackView);
    const TSharedPtr<SCkDebug_Switch> DebugDraw = FindSwitch(TrackView, TEXT("audio-track-debug-toggle"));
    FSlateApplication& Slate = FSlateApplication::Get(); FWindowScope Scope{Slate};
    Scope.Window = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{640, 720})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[TrackView]
            + SVerticalBox::Slot().AutoHeight()[DirectorView]
        ];
    Slate.AddWindow(Scope.Window.ToSharedRef(), true);
    Tick(Slate);
    if (NOT TestTrue(TEXT("LocalOk standalone track actions and volume input are physical and enabled"),
        TrackView->Get_CanRequestTrack() && TrackView->Get_CanDebugDraw() && Play.IsValid() && Stop.IsValid() && StopAll.IsValid() && StopAlpha.IsValid()
            && Volume.IsValid() && DebugDraw.IsValid() && Play->IsEnabled() && Stop->IsEnabled()
            && StopAll->IsEnabled() && StopAlpha->IsEnabled() && DebugDraw->IsEnabled())) { return false; }
    Play->SimulateClick();
    Stop->SimulateClick();
    Commit(Slate, Volume.ToSharedRef(), TEXT("0.125"));
    ToggleSwitch(DebugDraw.ToSharedRef());
    TestTrue(TEXT("physical standalone Debug Draw switch adds its typed debug state immediately"),
        TrackOnly.Has_All<ck::FFragment_AudioTrack_Debug, ck::FTag_AudioTrack_DebugDraw>());
    ToggleSwitch(DebugDraw.ToSharedRef());
    TestFalse(TEXT("physical standalone Debug Draw switch removes its typed debug state immediately"),
        TrackOnly.Has_Any<ck::FFragment_AudioTrack_Debug, ck::FTag_AudioTrack_DebugDraw>());
    StopAll->SimulateClick();
    StopAlpha->SimulateClick();
    if (NOT TestTrue(TEXT("physical Audio controls queue guarded public requests before inspecting payloads"),
        TrackOnly.Has<ck::FFragment_AudioTrack_Requests>() && DirectorOnly.Has<ck::FFragment_AudioDirector_Requests>()
            && TrackOnly.Get<ck::FFragment_AudioTrack_Requests>().Get_Requests().Num() == 3
            && DirectorOnly.Get<ck::FFragment_AudioDirector_Requests>().Get_Requests().Num() == 2)) { return false; }
    const auto& TrackRequests = TrackOnly.Get<ck::FFragment_AudioTrack_Requests>().Get_Requests();
    const auto& DirectorRequests = DirectorOnly.Get<ck::FFragment_AudioDirector_Requests>().Get_Requests();
    TestTrue(TEXT("play stop and Set Volume retain exact request types payloads and zero fade"),
        std::holds_alternative<FCk_Request_AudioTrack_Play>(TrackRequests[0]) && std::holds_alternative<FCk_Request_AudioTrack_Stop>(TrackRequests[1])
            && std::holds_alternative<FCk_Request_AudioTrack_SetVolume>(TrackRequests[2])
            && std::get<FCk_Request_AudioTrack_Play>(TrackRequests[0]).Get_FadeInTime() == FCk_Time::ZeroSecond()
            && std::get<FCk_Request_AudioTrack_Stop>(TrackRequests[1]).Get_FadeOutTime() == FCk_Time::ZeroSecond()
            && FMath::IsNearlyEqual(std::get<FCk_Request_AudioTrack_SetVolume>(TrackRequests[2]).Get_TargetVolume(), 0.125f)
            && std::get<FCk_Request_AudioTrack_SetVolume>(TrackRequests[2]).Get_FadeTime() == FCk_Time::ZeroSecond()
            && std::holds_alternative<FCk_Request_AudioDirector_StopAllTracks>(DirectorRequests[0])
            && std::holds_alternative<FCk_Request_AudioDirector_StopTrack>(DirectorRequests[1])
            && std::get<FCk_Request_AudioDirector_StopAllTracks>(DirectorRequests[0]).Get_FadeOutTime().IsSet()
            && std::get<FCk_Request_AudioDirector_StopAllTracks>(DirectorRequests[0]).Get_FadeOutTime().GetValue() == FCk_Time::ZeroSecond()
            && std::get<FCk_Request_AudioDirector_StopTrack>(DirectorRequests[1]).Get_TrackName() == TEXT("Alpha")
            && std::get<FCk_Request_AudioDirector_StopTrack>(DirectorRequests[1]).Get_FadeOutTime().IsSet()
            && std::get<FCk_Request_AudioDirector_StopTrack>(DirectorRequests[1]).Get_FadeOutTime().GetValue()
                == FCk_Time::ZeroSecond());

    UWorld* const DedicatedWorld = UWorld::CreateWorld(EWorldType::PIE, false);
    ON_SCOPE_EXIT { if (DedicatedWorld != nullptr) { DedicatedWorld->DestroyWorld(false); } };
    if (NOT TestTrue(TEXT("fixture creates dedicated-server cosmetic-denial world"), DedicatedWorld != nullptr)) { return false; }
    DedicatedWorld->SetPlayInEditorInitialNetMode(NM_DedicatedServer);
    Owner.Try_Remove<TWeakObjectPtr<UWorld>>();
    auto DedicatedOwner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    DedicatedOwner.Add<TWeakObjectPtr<UWorld>>(DedicatedWorld);
    auto Dedicated = CreateTrack(DedicatedOwner, ECk_AudioTrack_State::Stopped, 0.0f);
    const TSharedRef<SCkInspector_AudioAuthored> DedicatedView = StaticCastSharedRef<SCkInspector_AudioAuthored>(Inspector.Build_Inspector(Dedicated));
    const TSharedPtr<SButton> DedicatedPlay = FindButton(DedicatedView, TEXT("audio-track-play"));
    const TSharedPtr<SCkDebug_Switch> DedicatedDebugDraw =
        FindSwitch(DedicatedView, TEXT("audio-track-debug-toggle"));
    if (DedicatedDebugDraw.IsValid())
    { DedicatedDebugDraw->SlatePrepass(); }
    const bool bCanRequestTrack = DedicatedView->Get_CanRequestTrack();
    const bool bCanDebugDraw = DedicatedView->Get_CanDebugDraw();
    const bool bPlayEnabled = DedicatedPlay.IsValid() && DedicatedPlay->IsEnabled();
    const bool bDebugDrawEnabled = DedicatedDebugDraw.IsValid() && DedicatedDebugDraw->IsEnabled();
    if (NOT TestTrue(*FString::Printf(TEXT("LocalOk track requests remain available while dedicated-server cosmetic debug draw is denied (mode=%d request=%d debug=%d play-valid=%d play-enabled=%d switch-valid=%d switch-enabled=%d)"),
        static_cast<int32>(DedicatedWorld->GetNetMode()), bCanRequestTrack, bCanDebugDraw,
        DedicatedPlay.IsValid(), bPlayEnabled, DedicatedDebugDraw.IsValid(), bDebugDrawEnabled),
        DedicatedWorld->GetNetMode() == NM_DedicatedServer
            && bCanRequestTrack && NOT bCanDebugDraw
            && DedicatedPlay.IsValid() && bPlayEnabled
            && DedicatedDebugDraw.IsValid() && NOT bDebugDrawEnabled))
    { return false; }
    ToggleSwitch(DedicatedDebugDraw.ToSharedRef());
    TestFalse(TEXT("disabled dedicated-server cosmetic control cannot mutate debug-draw state"),
        Dedicated.Has_Any<ck::FFragment_AudioTrack_Debug, ck::FTag_AudioTrack_DebugDraw>());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger")); FString Markup, Css;
    const FString Resources = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Audio resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Resources, TEXT("EcsInspectorAudio.ui.html")))
        && FFileHelper::LoadFileToString(Css, *FPaths::Combine(Resources, TEXT("EcsInspectorAudio.ui.css"))))) { return false; }
    TestTrue(TEXT("Audio resource owns all native parity rows controls and repeat"), Markup.Contains(TEXT("audio-track-volume-input"))
        && Markup.Contains(TEXT("audio-track-debug-toggle")) && Markup.Contains(TEXT("audio-director-stop-all"))
        && Markup.Contains(TEXT("audio-director-track-records")) && Markup.Contains(TEXT("audio-director-stop-track"))
        && Markup.Contains(TEXT("audio-unavailable")) && Markup.Contains(TEXT("<debug-status"))
        && Markup.Contains(TEXT("<debug-meter")) && NOT Markup.Contains(TEXT("<native")));
    const int64 CompatibleRevision = DirectorUi->GetRevision();
    const TSharedPtr<const FCkUiRecord> AlphaBefore = Tracks->FindRecord(TEXT("Alpha"));
    TestTrue(TEXT("compatible Audio reload preserves view repeat and physical action identity"),
        DirectorUi->TryReload(Markup, Css, TEXT("Audio compatible")).Succeeded
            && DirectorUi->GetRevision() > CompatibleRevision
            && Tracks->FindRecord(TEXT("Alpha")) == AlphaBefore
            && FindButton(DirectorView, TEXT("audio-director-track-stop")) == StopAlpha);
    DirectorCurrent._TracksByName[TEXT("Alpha")] = MakeTrackHandle(TrackBeta);
    DirectorView->Tick(FGeometry::MakeRoot(FVector2D{1, 1}, FSlateLayoutTransform{}), 0, 0);
    const TSharedPtr<const FCkUiRecord> AlphaAfter = Tracks->FindRecord(TEXT("Alpha"));
    const FCkUiFieldValue* AlphaHandle = AlphaAfter.IsValid() ? AlphaAfter->FindField(TEXT("handle")) : nullptr;
    TestTrue(TEXT("same-key replacement retains record identity and publishes the replacement handle"),
        AlphaAfter == AlphaBefore && AlphaHandle != nullptr
            && AlphaHandle->Text.ToString() == ck::Format_UE(TEXT("[{}]"), MakeTrackHandle(TrackBeta)));
    const int64 DirectorRevision = DirectorUi->GetRevision(); const TSharedRef<SWidget> DirectorRoot = DirectorUi->GetRegion(TEXT("main"));
    TestFalse(TEXT("missing Audio action binding is rejected atomically"), DirectorUi->TryReload(Markup.Replace(TEXT("audio-director-stop-all"), TEXT("audio-missing-stop-all")), Css, TEXT("Audio rejected action")).Succeeded);
    TestFalse(TEXT("missing Audio collection binding is rejected atomically"), DirectorUi->TryReload(Markup.Replace(TEXT("audio-director-tracks"), TEXT("audio-missing-tracks")), Css, TEXT("Audio rejected collection")).Succeeded);
    TestTrue(TEXT("rejected Audio reload preserves prior tree and revision"), &DirectorUi->GetRegion(TEXT("main")).Get() == &DirectorRoot.Get() && DirectorUi->GetRevision() == DirectorRevision);
    DirectorCurrent._TracksByName.Remove(TEXT("Alpha")); DirectorView->Tick(FGeometry::MakeRoot(FVector2D{1, 1}, FSlateLayoutTransform{}), 0, 0);
    StopAlpha->SimulateClick();
    TestTrue(TEXT("removal drops the repeated record and retires its held per-name Stop action"),
        NOT Tracks->FindRecord(TEXT("Alpha")).IsValid() && DirectorOnly.Get<ck::FFragment_AudioDirector_Requests>().Get_Requests().Num() == 2);
    TrackOnly.Try_Remove<ck::FFragment_AudioTrack_Params>();
    Play->SlatePrepass();
    Play->SimulateClick();
    TestTrue(TEXT("missing Params and Current fail closed and held controls cannot queue additional work"),
        NOT TrackView->Get_CanRequestTrack() && NOT Play->IsEnabled() && TrackOnly.Get<ck::FFragment_AudioTrack_Requests>().Get_Requests().Num() == 3);
    CurrentOnly.Try_Remove<ck::FFragment_AudioTrack_Current>();
    TestTrue(TEXT("missing Current fails the Current-only authored projection closed"),
        NOT CurrentOnlyView->Get_IsTrackAvailable() && CurrentOnlyView->Get_TrackStateText() == TEXT("--"));
    DirectorOnly.Add<ck::FTag_DestroyEntity_Initiate>(); StopAll->SimulateClick();
    TestTrue(TEXT("pending destruction makes held director control inert"), NOT DirectorView->Get_CanRequestDirector() && DirectorOnly.Get<ck::FFragment_AudioDirector_Requests>().Get_Requests().Num() == 2);
    TSharedPtr<SCkInspector_AudioAuthored> DestructorView;
    { auto DestructorInspector = MakeUnique<FCkInspector_Audio>(); DestructorView = StaticCastSharedRef<SCkInspector_AudioAuthored>(DestructorInspector->Build_Inspector(Combined)); }
    Inspector.OnDeactivated();
    TestTrue(TEXT("destructor and deactivation release every authored Audio view and collection"), DestructorView->Is_Inert()
        && TrackView->Is_Inert() && DirectorView->Is_Inert() && CombinedView->Is_Inert()
        && CurrentOnlyView->Is_Inert() && CurrentOnlyDirectorView->Is_Inert()
        && DedicatedView->Is_Inert() && DiffView->Is_Inert()
        && NOT TrackView->Get_View().IsValid() && NOT DirectorView->Get_View().IsValid()
        && NOT DirectorView->Get_TracksCollection().IsValid());
    DedicatedPlay->SimulateClick();
    ToggleSwitch(DedicatedDebugDraw.ToSharedRef());
    TestFalse(TEXT("held physical controls remain inert after inspector deactivation"),
        Dedicated.Has<ck::FFragment_AudioTrack_Requests>()
            || Dedicated.Has_Any<ck::FFragment_AudioTrack_Debug, ck::FTag_AudioTrack_DebugDraw>());
    TrackUi.Reset();
    DirectorUi.Reset();
    Tracks.Reset();
    return true;
}

#endif
