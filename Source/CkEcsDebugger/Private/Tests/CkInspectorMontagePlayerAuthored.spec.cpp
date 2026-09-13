#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_MontagePlayer.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkAnimation/MontagePlayer/CkMontagePlayer_Fragment.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
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

namespace ck_inspector_montage_player_authored_test
{
    auto CreatePlayer(
        const FCk_Handle& InOwner,
        const ECk_MontagePlayer_StateKind InKind,
        const float InPlayRate,
        const FName InSection,
        const bool bInAddParams = true) -> FCk_Handle
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
        if (ck::Is_NOT_Valid(Entity))
        { return {}; }
        if (bInAddParams)
        { Entity.Add<ck::FFragment_MontagePlayer_Params>(FCk_Fragment_MontagePlayer_ParamsData{}); }
        auto State = FCk_MontagePlayer_State{};
        State.Set_Kind(InKind).Set_PlayRate(InPlayRate).Set_SectionName(InSection);
        Entity.Add<ck::FFragment_MontagePlayer_Current>(State);
        return Entity;
    }

    auto FindTagged(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag)
        { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTagged(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButton(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindEditor(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SEditableTextBox"))
        { return StaticCastSharedRef<SEditableTextBox>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableTextBox> Found = FindEditor(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindEditorUnderTag(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SEditableTextBox>
    {
        const TSharedPtr<SWidget> Tagged = FindTagged(InRoot, InTag);
        return Tagged.IsValid() ? FindEditor(Tagged.ToSharedRef()) : nullptr;
    }

    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope()
        {
            if (Window.IsValid())
            { Slate.DestroyWindowImmediately(Window.ToSharedRef()); }
        }

        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorMontagePlayerAuthored,
    "Ck.UiAuthoring.EcsDebugger.MontagePlayerInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorMontagePlayerAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_montage_player_authored_test;

    if (NOT FSlateApplication::IsInitialized())
    { AddError(TEXT("Montage Player authored inspector test requires Slate.")); return false; }

    auto InvalidInspector = FCkInspector_MontagePlayer{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build mounts the authored Montage Player shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_MontagePlayerAuthored")}))
    {
        AddError(InvalidInspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_MontagePlayerAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_MontagePlayerAuthored>(InvalidRendered);
    TestTrue(TEXT("default-invalid Montage Player shell fails closed"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && NOT InvalidAuthored->Get_HasControls() && NOT InvalidAuthored->Get_CanRequest());
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid Montage Player shell releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted()
            && NOT InvalidAuthored->Get_View().IsValid());

    auto EcsWorld = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(EcsWorld.Get_Registry());
    UWorld* const AuthorityWorld = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT
    {
        if (AuthorityWorld != nullptr)
        { AuthorityWorld->DestroyWorld(false); }
    };
    if (NOT TestTrue(TEXT("fixture has a transient owner and standalone authority world"),
        ck::IsValid(LifetimeOwner) && AuthorityWorld != nullptr && AuthorityWorld->GetNetMode() == NM_Standalone))
    { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(AuthorityWorld);
    UCk_Utils_Net_UE::Add(LifetimeOwner, FCk_Net_ConnectionSettings{
        ECk_Replication::DoesNotReplicate,
        ECk_Net_NetModeType::Host,
        ECk_Net_EntityNetRole::Authority});

    auto PlayerA = CreatePlayer(LifetimeOwner, ECk_MontagePlayer_StateKind::Pause, 1.5f, TEXT("Idle"));
    auto PlayerB = CreatePlayer(LifetimeOwner, ECk_MontagePlayer_StateKind::Stop, 0.75f, NAME_None);
    auto CurrentOnly = CreatePlayer(
        LifetimeOwner, ECk_MontagePlayer_StateKind::Resume, 2.0f, TEXT("CurrentOnly"), false);
    if (NOT TestTrue(TEXT("fixture creates two typed players and one Current-only player"),
        ck::IsValid(PlayerA) && ck::IsValid(PlayerB) && ck::IsValid(CurrentOnly)
            && PlayerA.Has<ck::FFragment_MontagePlayer_Params>()
            && PlayerA.Has<ck::FFragment_MontagePlayer_Current>()
            && NOT CurrentOnly.Has<ck::FFragment_MontagePlayer_Params>()
            && CurrentOnly.Has<ck::FFragment_MontagePlayer_Current>()))
    { return false; }

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    auto Inspector = FCkInspector_MontagePlayer{};
    Inspector.Set_EditGuard(EditGuard);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(PlayerA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(PlayerB);
    const TSharedRef<SWidget> RenderedCurrentOnly = Inspector.Build_Inspector(CurrentOnly);
    if (NOT TestEqual(TEXT("typed player A mounts the authored inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_MontagePlayerAuthored")})
        || NOT TestEqual(TEXT("typed player B mounts the authored inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_MontagePlayerAuthored")})
        || NOT TestEqual(TEXT("Current-only player mounts the authored inspector"),
            RenderedCurrentOnly->GetTypeAsString(), FString{TEXT("SCkInspector_MontagePlayerAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_MontagePlayerAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_MontagePlayerAuthored>(RenderedA);
    const TSharedRef<SCkInspector_MontagePlayerAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_MontagePlayerAuthored>(RenderedB);
    const TSharedRef<SCkInspector_MontagePlayerAuthored> AuthoredCurrentOnly =
        StaticCastSharedRef<SCkInspector_MontagePlayerAuthored>(RenderedCurrentOnly);
    if (NOT TestTrue(TEXT("each Montage Player build owns an independent retained view and staged state"),
        AuthoredA->Get_View().IsValid() && AuthoredB->Get_View().IsValid()
            && AuthoredA->Get_View() != AuthoredB->Get_View()
            && AuthoredA->Get_PendingSectionText() == TEXT("Idle")
            && AuthoredB->Get_PendingSectionText() == TEXT("(none)")
            && FMath::IsNearlyEqual(AuthoredA->Get_PendingBlendOut(), 0.25f)
            && FMath::IsNearlyEqual(AuthoredB->Get_PendingBlendOut(), 0.25f)))
    { return false; }
    TestTrue(TEXT("typed live projection preserves state, weak-object and timing semantics"),
        AuthoredA->Get_IsAvailable() && AuthoredA->Get_HasControls()
            && AuthoredA->Get_StateText() == TEXT("Pause")
            && AuthoredA->Get_ActiveMontageText() == TEXT("(none)")
            && AuthoredA->Get_PositionText() == TEXT("--")
            && FMath::IsNearlyZero(AuthoredA->Get_PositionFraction())
            && AuthoredA->Get_AnimInstanceText() == TEXT("Invalid")
            && AuthoredA->Get_PlayRateText() == TEXT("1.50")
            && AuthoredA->Get_CatchUpRemainingText() == TEXT("0.000 s"));
    TestTrue(TEXT("Current-only player remains readable but exposes no typed controls"),
        Inspector.CanInspect(CurrentOnly) && AuthoredCurrentOnly->Get_IsAvailable()
            && NOT AuthoredCurrentOnly->Get_HasControls() && NOT AuthoredCurrentOnly->Get_CanRequest()
            && AuthoredCurrentOnly->Get_StateText() == TEXT("Resume")
            && AuthoredCurrentOnly->Get_PlayRateText() == TEXT("2.00"));

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    auto RowsCurrentOnly = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(PlayerA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(PlayerB); RowsB = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(CurrentOnly); RowsCurrentOnly = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TSharedPtr<SCkInspector_MontagePlayerAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_MontagePlayerAuthored>(Inspector.Build_Inspector(PlayerA));
    }
    TestTrue(TEXT("native capture remains the authority for every authored Montage Player row"),
        RowsA.Num() == 9 && RowsCurrentOnly.Num() == 6
            && RowsA.FindRef(TEXT("State:")) == TEXT("Pause")
            && RowsA.FindRef(TEXT("Active Montage:")) == TEXT("(none)")
            && RowsA.FindRef(TEXT("Position:")) == TEXT("--")
            && RowsA.FindRef(TEXT("Anim Instance:")) == TEXT("Invalid")
            && RowsA.FindRef(TEXT("Play Rate:")) == TEXT("1.50")
            && RowsA.FindRef(TEXT("Catch-up Remaining:")) == TEXT("0.000 s")
            && RowsA.Contains(TEXT("Blend Out (s):")) && RowsA.Contains(TEXT("Section:"))
            && RowsA.Contains(TEXT("Playback:")) && NOT RowsCurrentOnly.Contains(TEXT("Playback:"))
            && Differing.Contains(TEXT("State:")) && Differing.Contains(TEXT("Play Rate:"))
            && Differing.Contains(TEXT("Section:")) && DiffAuthored.IsValid()
            && DiffAuthored->Is_DiffMarked(TEXT("State:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Play Rate:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Section:")));

    const TSharedPtr<SButton> PauseButton = FindButton(RenderedA, TEXT("montage-player-pause"));
    const TSharedPtr<SButton> ResumeButton = FindButton(RenderedA, TEXT("montage-player-resume"));
    const TSharedPtr<SButton> StopButton = FindButton(RenderedA, TEXT("montage-player-stop"));
    const TSharedPtr<SButton> JumpButton = FindButton(RenderedA, TEXT("montage-player-jump"));
    const TSharedPtr<SEditableTextBox> BlendEditor =
        FindEditorUnderTag(RenderedA, TEXT("montage-player-blend-out-input"));
    const TSharedPtr<SEditableTextBox> SectionEditor =
        FindEditorUnderTag(RenderedA, TEXT("montage-player-section-input"));
    const TSharedPtr<SWidget> CurrentOnlyControls =
        FindTagged(RenderedCurrentOnly, TEXT("montage-player-controls-section"));
    if (NOT TestTrue(TEXT("authored physical actions and staged native editors mount"),
        PauseButton.IsValid() && ResumeButton.IsValid() && StopButton.IsValid() && JumpButton.IsValid()
            && BlendEditor.IsValid() && SectionEditor.IsValid() && CurrentOnlyControls.IsValid()))
    { return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{620.0f, 900.0f}).CreateTitleBar(false).HasCloseButton(false)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[RenderedA]
            + SVerticalBox::Slot().AutoHeight()[RenderedB]
            + SVerticalBox::Slot().AutoHeight()[RenderedCurrentOnly]
        ];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    Tick(Slate);
    TestTrue(TEXT("standalone authority enables every physical Montage Player control"),
        AuthoredA->Get_CanRequest() && PauseButton->IsEnabled() && ResumeButton->IsEnabled()
            && StopButton->IsEnabled() && JumpButton->IsEnabled()
            && BlendEditor->IsEnabled() && SectionEditor->IsEnabled());
    auto CurrentOnlyVisiblePath = FWidgetPath{};
    TestFalse(TEXT("Current-only controls are absent from the effective visible Slate path"),
        Slate.GeneratePathToWidgetUnchecked(
            CurrentOnlyControls.ToSharedRef(), CurrentOnlyVisiblePath, EVisibility::Visible));

    Slate.SetUserFocus(0, BlendEditor.ToSharedRef(), EFocusCause::SetDirectly);
    BlendEditor->SetText(FText::FromString(TEXT("0.75")));
    Slate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
    Tick(Slate);
    Slate.SetUserFocus(0, SectionEditor.ToSharedRef(), EFocusCause::SetDirectly);
    SectionEditor->SetText(FText::FromString(TEXT("Run")));
    Slate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
    Tick(Slate);
    TestTrue(TEXT("physical staged editors retain independent Stop and Jump payload state"),
        FMath::IsNearlyEqual(AuthoredA->Get_PendingBlendOut(), 0.75f)
            && AuthoredA->Get_PendingSectionText() == TEXT("Run")
            && FMath::IsNearlyEqual(AuthoredB->Get_PendingBlendOut(), 0.25f)
            && AuthoredB->Get_PendingSectionText() == TEXT("(none)"));

    PauseButton->SimulateClick();
    ResumeButton->SimulateClick();
    StopButton->SimulateClick();
    JumpButton->SimulateClick();
    if (NOT TestTrue(TEXT("four physical actions enqueue exactly four public requests"),
        PlayerA.Has<ck::FFragment_MontagePlayer_Requests>()
            && PlayerA.Get<ck::FFragment_MontagePlayer_Requests>().Get_Requests().Num() == 4))
    { return false; }
    const auto& Requests = PlayerA.Get<ck::FFragment_MontagePlayer_Requests>().Get_Requests();
    const auto* StopRequest = std::get_if<FCk_Request_MontagePlayer_Stop>(&Requests[2]);
    const auto* JumpRequest = std::get_if<FCk_Request_MontagePlayer_JumpToSection>(&Requests[3]);
    TestTrue(TEXT("physical requests preserve exact click order and staged payloads"),
        std::holds_alternative<FCk_Request_MontagePlayer_Pause>(Requests[0])
            && std::holds_alternative<FCk_Request_MontagePlayer_Resume>(Requests[1])
            && StopRequest != nullptr && JumpRequest != nullptr
            && FMath::IsNearlyEqual(StopRequest->Get_BlendOutTime().Get_Seconds(), 0.75)
            && JumpRequest->Get_SectionName() == TEXT("Run"));
    PlayerA.Try_Remove<ck::FFragment_MontagePlayer_Requests>();
    AuthoredB->Request_Jump();
    TestFalse(TEXT("empty staged Section rejects Jump without publishing a request fragment"),
        PlayerB.Has<ck::FFragment_MontagePlayer_Requests>());

    UWorld* const ClientWorld = UWorld::CreateWorld(EWorldType::PIE, false);
    ON_SCOPE_EXIT
    {
        if (ClientWorld != nullptr)
        { ClientWorld->DestroyWorld(false); }
    };
    if (NOT TestTrue(TEXT("fixture creates a client PIE world"), ClientWorld != nullptr))
    { return false; }
    ClientWorld->SetPlayInEditorInitialNetMode(NM_Client);
    auto ClientOwner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    if (NOT TestTrue(TEXT("fixture creates a client ownership entity"), ck::IsValid(ClientOwner)))
    { return false; }
    ClientOwner.Add<TWeakObjectPtr<UWorld>>(ClientWorld);
    auto ClientPlayer = CreatePlayer(ClientOwner, ECk_MontagePlayer_StateKind::Play, 1.0f, TEXT("Client"));
    const TSharedRef<SWidget> ClientRendered = Inspector.Build_Inspector(ClientPlayer);
    if (NOT TestEqual(TEXT("client player mounts the authored inspector"),
        ClientRendered->GetTypeAsString(), FString{TEXT("SCkInspector_MontagePlayerAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_MontagePlayerAuthored> ClientAuthored =
        StaticCastSharedRef<SCkInspector_MontagePlayerAuthored>(ClientRendered);
    const TSharedPtr<SButton> ClientPause = FindButton(ClientAuthored, TEXT("montage-player-pause"));
    FWindowScope ClientWindowScope{Slate};
    ClientWindowScope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{480.0f, 360.0f}).CreateTitleBar(false).HasCloseButton(false)[ClientAuthored];
    Slate.AddWindow(ClientWindowScope.Window.ToSharedRef(), true);
    Tick(Slate);
    if (NOT TestTrue(TEXT("client Montage Player retains a distinct physical Pause action"),
        ClientPause.IsValid() && ClientPause != PauseButton))
    { return false; }
    ClientPause->SlatePrepass();
    TestTrue(TEXT("AuthorityOnly rejects the client view and physical action with a reason"),
        NOT ClientAuthored->Get_CanRequest() && NOT ClientPause->IsEnabled()
            && NOT ClientAuthored->Get_RequestDisabledReason().IsEmpty());
    ClientPause->SimulateClick();
    TestFalse(TEXT("disabled client action cannot enqueue or trip the public authority ensure"),
        ClientPlayer.Has<ck::FFragment_MontagePlayer_Requests>());

    auto WorldlessEcs = ck::FEcsWorld{};
    auto WorldlessOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(WorldlessEcs.Get_Registry());
    auto WorldlessPlayer = CreatePlayer(
        WorldlessOwner, ECk_MontagePlayer_StateKind::Play, 1.0f, TEXT("Worldless"));
    if (NOT TestTrue(TEXT("fixture creates a structurally valid worldless Montage Player"),
        ck::IsValid(WorldlessOwner) && ck::IsValid(WorldlessPlayer)))
    { return false; }
    const TSharedRef<SWidget> WorldlessRendered = Inspector.Build_Inspector(WorldlessPlayer);
    if (NOT TestEqual(TEXT("worldless player mounts the authored inspector"),
        WorldlessRendered->GetTypeAsString(), FString{TEXT("SCkInspector_MontagePlayerAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_MontagePlayerAuthored> WorldlessAuthored =
        StaticCastSharedRef<SCkInspector_MontagePlayerAuthored>(WorldlessRendered);
    const TSharedPtr<SButton> WorldlessPause = FindButton(WorldlessAuthored, TEXT("montage-player-pause"));
    if (NOT TestTrue(TEXT("worldless Montage Player retains a physical Pause action"),
        WorldlessPause.IsValid()))
    { return false; }
    WorldlessPause->SlatePrepass();
    TestTrue(TEXT("AuthorityOnly fails closed when a world cannot be established"),
        NOT WorldlessAuthored->Get_CanRequest() && NOT WorldlessPause->IsEnabled()
            && WorldlessAuthored->Get_RequestDisabledReason().Contains(TEXT("authority cannot be established")));
    WorldlessPause->SimulateClick();
    TestFalse(TEXT("worldless physical action cannot publish a Montage Player request"),
        WorldlessPlayer.Has<ck::FFragment_MontagePlayer_Requests>());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Stylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Montage Player resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(
            Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorMontagePlayer.ui.html")))
        && FFileHelper::LoadFileToString(
            Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorMontagePlayer.ui.css")))))
    { return false; }
    TestTrue(TEXT("resource owns the complete layout and only two specialized native leaves"),
        Markup.Contains(TEXT(">Montage Player</text>"))
            && Markup.Contains(TEXT(">State:</text>"))
            && Markup.Contains(TEXT(">Active Montage:</text>"))
            && Markup.Contains(TEXT(">Position:</text>"))
            && Markup.Contains(TEXT(">Anim Instance:</text>"))
            && Markup.Contains(TEXT(">Play Rate:</text>"))
            && Markup.Contains(TEXT(">Catch-up Remaining:</text>"))
            && Markup.Contains(TEXT(">Blend Out (s):</text>"))
            && Markup.Contains(TEXT(">Section:</text>"))
            && Markup.Contains(TEXT(">Playback:</text>"))
            && Markup.Contains(TEXT("bind=\"montage-player-blend-out-port\""))
            && Markup.Contains(TEXT("bind=\"montage-player-section-port\""))
            && Markup.Contains(TEXT("action=\"montage-player-pause\""))
            && Markup.Contains(TEXT("action=\"montage-player-resume\""))
            && Markup.Contains(TEXT("action=\"montage-player-stop\""))
            && Markup.Contains(TEXT("action=\"montage-player-jump\""))
            && Markup.Contains(TEXT("tooltip-bind=\"montage-player-pause-tooltip\""))
            && Markup.Contains(TEXT("tooltip-bind=\"montage-player-resume-tooltip\""))
            && Markup.Contains(TEXT("tooltip-bind=\"montage-player-stop-tooltip\""))
            && Markup.Contains(TEXT("tooltip-bind=\"montage-player-jump-tooltip\""))
            && Stylesheet.Contains(TEXT(".montage-player-inspector"))
            && Stylesheet.Contains(TEXT(".montage-player-row"))
            && Stylesheet.Contains(TEXT(".montage-player-controls"))
            && Stylesheet.Contains(TEXT(".montage-player-actions"))
            && Stylesheet.Contains(TEXT(".montage-player-unavailable")));

    const int64 RevisionBefore = AuthoredA->Get_View()->GetRevision();
    TestTrue(TEXT("compatible Montage Player reload is accepted"),
        AuthoredA->Get_View()->TryReload(Markup, Stylesheet, TEXT("Montage Player compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible reload retains the physical Pause action identity"),
        AuthoredA->Get_View()->GetRevision() > RevisionBefore
            && FindButton(RenderedA, TEXT("montage-player-pause")) == PauseButton);
    const TSharedRef<SWidget> MainBefore = AuthoredB->Get_View()->GetRegion(TEXT("main"));
    const TSharedPtr<SWidget> SectionPortBefore =
        FindTagged(MainBefore, TEXT("montage-player-section-input"));
    const int64 RejectedRevision = AuthoredB->Get_View()->GetRevision();
    TestFalse(TEXT("missing Montage Player native port rejects atomically"),
        AuthoredB->Get_View()->TryReload(
            Markup.Replace(TEXT("bind=\"montage-player-section-port\""), TEXT("bind=\"missing-port\"")),
            Stylesheet, TEXT("Montage Player missing port candidate")).Succeeded);
    TestFalse(TEXT("missing Montage Player action rejects atomically"),
        AuthoredB->Get_View()->TryReload(
            Markup.Replace(TEXT("action=\"montage-player-pause\""), TEXT("action=\"missing-action\"")),
            Stylesheet, TEXT("Montage Player missing action candidate")).Succeeded);
    TestTrue(TEXT("rejected reload preserves revision, tree, port and live action"),
        AuthoredB->Get_View()->GetRevision() == RejectedRevision
            && &AuthoredB->Get_View()->GetRegion(TEXT("main")).Get() == &MainBefore.Get()
            && FindTagged(MainBefore, TEXT("montage-player-section-input")) == SectionPortBefore
            && FindButton(MainBefore, TEXT("montage-player-pause")).IsValid());

    TestTrue(TEXT("fixture removes Current while the typed entity remains live"),
        PlayerA.Try_Remove<ck::FFragment_MontagePlayer_Current>()
            && ck::IsValid(PlayerA) && NOT Inspector.CanInspect(PlayerA));
    PauseButton->SlatePrepass();
    TestTrue(TEXT("held authored view and action fail closed after composition loss"),
        NOT AuthoredA->Get_IsAvailable() && NOT AuthoredA->Get_CanRequest() && NOT PauseButton->IsEnabled());
    PauseButton->SimulateClick();
    TestFalse(TEXT("stale held action cannot enqueue after composition loss"),
        PlayerA.Has<ck::FFragment_MontagePlayer_Requests>());
    PlayerB.Add<ck::FTag_DestroyEntity_Initiate>();
    TestTrue(TEXT("pending destruction makes a still-live Montage Player unavailable"),
        ck::IsValid(PlayerB) && NOT Inspector.CanInspect(PlayerB)
            && NOT AuthoredB->Get_IsAvailable() && NOT AuthoredB->Get_CanRequest());

    TSharedPtr<SCkInspector_MontagePlayerAuthored> DestructorAuthored;
    TWeakPtr<FCkUiView> DestructorView;
    auto DestructorPlayer = CreatePlayer(
        LifetimeOwner, ECk_MontagePlayer_StateKind::Play, 1.0f, TEXT("Destructor"));
    {
        auto DestructorInspector = MakeUnique<FCkInspector_MontagePlayer>();
        const TSharedRef<SWidget> DestructorRendered = DestructorInspector->Build_Inspector(DestructorPlayer);
        if (NOT TestEqual(TEXT("destructor fixture mounts the authored inspector"),
            DestructorRendered->GetTypeAsString(), FString{TEXT("SCkInspector_MontagePlayerAuthored")}))
        {
            AddError(DestructorInspector->Get_LastAuthoredLoadError());
            return false;
        }
        DestructorAuthored = StaticCastSharedRef<SCkInspector_MontagePlayerAuthored>(DestructorRendered);
        DestructorView = DestructorAuthored->Get_View();
    }
    TestTrue(TEXT("Montage Player inspector destruction releases its retained view"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorView.IsValid());
    Inspector.OnDeactivated();
    const FString ReleasedSection = AuthoredA->Get_PendingSectionText();
    SectionEditor->SetText(FText::FromString(TEXT("MustNotReactivate")));
    TestTrue(TEXT("deactivation releases every retained Montage Player view and keeps held editors inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && AuthoredCurrentOnly->Is_Inert()
            && DiffAuthored->Is_Inert() && ClientAuthored->Is_Inert() && WorldlessAuthored->Is_Inert()
            && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredB->Get_View().IsValid()
            && AuthoredA->Get_PendingSectionText() == ReleasedSection
            && NOT EditGuard->Get_HasActiveEdit());
    return true;
}

#endif
