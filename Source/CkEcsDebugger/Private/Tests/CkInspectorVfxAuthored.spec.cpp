#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Vfx.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkVfx/Cue/CkVfxCue_Fragment_Data.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "NiagaraComponent.h"

#define private public
#include "CkVfx/Cue/CkVfxCue_Fragment.h"
#undef private

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"

#include <variant>

namespace ck_inspector_vfx_authored_test
{
    auto CreateCue(
        const FCk_Handle& InLifetimeOwner,
        const double InStartSeconds,
        const double InDurationSeconds,
        const bool bInPlaying,
        const bool bInFinished) -> FCk_Handle
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InLifetimeOwner);
        auto& Current = Entity.Add<ck::FFragment_VfxCue_Current>();
        Current._EffectStartTime = FCk_Time{InStartSeconds};
        Current._EffectDuration = FCk_Time{InDurationSeconds};
        Current._HasFiredFinished = bInFinished;
        if (bInPlaying)
        { Entity.Add<ck::FTag_VfxCue_IsPlaying>(); }
        return Entity;
    }

    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag)
        { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
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

    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope() { if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); } }
        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorVfxAuthored,
    "Ck.UiAuthoring.EcsDebugger.VfxInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorVfxAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_vfx_authored_test;

    auto InvalidInspector = FCkInspector_Vfx{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build mounts the authored VFX shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_VfxAuthored")}))
    { AddError(InvalidInspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_VfxAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_VfxAuthored>(InvalidRendered);
    TestTrue(TEXT("default-invalid VFX shell fails closed"), InvalidAuthored->Is_Mounted()
        && NOT InvalidAuthored->Get_IsAvailable() && InvalidAuthored->Get_ComponentText() == TEXT("--"));
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid VFX shell releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_View().IsValid());

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    UWorld* const StandaloneWorld = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { if (StandaloneWorld != nullptr) { StandaloneWorld->DestroyWorld(false); } };
    if (NOT TestTrue(TEXT("fixture has a transient owner and standalone cosmetic world"),
        ck::IsValid(LifetimeOwner) && StandaloneWorld != nullptr
            && StandaloneWorld->GetNetMode() == NM_Standalone))
    { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(StandaloneWorld);
    auto Finite = CreateCue(LifetimeOwner, 0.25, 10.0, true, false);
    auto Infinite = CreateCue(LifetimeOwner, 1.5, -1.0, false, true);
    if (NOT TestTrue(TEXT("fixture creates two real typed VFX cue states"),
        ck::IsValid(Finite) && ck::IsValid(Infinite)))
    { return false; }

    auto Inspector = FCkInspector_Vfx{};
    const TSharedRef<SWidget> RenderedFinite = Inspector.Build_Inspector(Finite);
    const TSharedRef<SWidget> RenderedInfinite = Inspector.Build_Inspector(Infinite);
    if (NOT TestEqual(TEXT("finite cue mounts the authored VFX inspector"),
            RenderedFinite->GetTypeAsString(), FString{TEXT("SCkInspector_VfxAuthored")})
        || NOT TestEqual(TEXT("infinite cue mounts the authored VFX inspector"),
            RenderedInfinite->GetTypeAsString(), FString{TEXT("SCkInspector_VfxAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_VfxAuthored> AuthoredFinite =
        StaticCastSharedRef<SCkInspector_VfxAuthored>(RenderedFinite);
    const TSharedRef<SCkInspector_VfxAuthored> AuthoredInfinite =
        StaticCastSharedRef<SCkInspector_VfxAuthored>(RenderedInfinite);
    TSharedPtr<FCkUiView> FiniteView = AuthoredFinite->Get_View();
    TSharedPtr<FCkUiView> InfiniteView = AuthoredInfinite->Get_View();
    TestTrue(TEXT("each VFX build owns an independent accepted view"),
        AuthoredFinite->Is_Mounted() && AuthoredInfinite->Is_Mounted()
            && FiniteView.IsValid() && InfiniteView.IsValid() && FiniteView != InfiniteView);
    TestTrue(TEXT("finite cue projects component, timing, and Playing state"),
        AuthoredFinite->Get_ComponentText() == TEXT("None")
            && AuthoredFinite->Get_StartTimeText() == TEXT("0.250s")
            && AuthoredFinite->Get_HasFiniteDuration()
            && AuthoredFinite->Get_ElapsedDurationText().Contains(TEXT("/ 10.00s"))
            && AuthoredFinite->Get_ElapsedFraction() >= 0.0f
            && AuthoredFinite->Get_StateText() == TEXT("Playing"));
    TestTrue(TEXT("infinite cue projects the unmetered Finished branch"),
        AuthoredInfinite->Get_ComponentText() == TEXT("None")
            && AuthoredInfinite->Get_StartTimeText() == TEXT("1.500s")
            && NOT AuthoredInfinite->Get_HasFiniteDuration()
            && AuthoredInfinite->Get_DurationText() == TEXT("Infinite")
            && AuthoredInfinite->Get_StateText() == TEXT("Finished"));

    const TSharedPtr<SWidget> FiniteMeterRow = FindTaggedWidget(
        FiniteView->GetRegion(TEXT("main")), TEXT("vfx-elapsed-duration-row"));
    const TSharedPtr<SWidget> FiniteDurationRow = FindTaggedWidget(
        FiniteView->GetRegion(TEXT("main")), TEXT("vfx-duration-row"));
    if (NOT TestTrue(TEXT("authored VFX exposes both duration branch nodes"),
        FiniteMeterRow.IsValid() && FiniteDurationRow.IsValid()))
    { return false; }
    FiniteView->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("finite duration selects only the meter row"),
        FiniteMeterRow->GetVisibility() == EVisibility::Visible
            && FiniteDurationRow->GetVisibility() == EVisibility::Collapsed);
    Finite.Get<ck::FFragment_VfxCue_Current>()._EffectDuration = FCk_Time{-1.0};
    FiniteView->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("duration changes switch authored layout without remounting"),
        NOT AuthoredFinite->Get_HasFiniteDuration() && AuthoredFinite->Get_DurationText() == TEXT("Infinite")
            && FiniteMeterRow->GetVisibility() == EVisibility::Collapsed
            && FiniteDurationRow->GetVisibility() == EVisibility::Visible);
    Finite.Get<ck::FFragment_VfxCue_Current>()._EffectDuration = FCk_Time{10.0};

    auto RowsFinite = TMap<FString, FString>{};
    auto RowsInfinite = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Finite); RowsFinite = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Infinite); RowsInfinite = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsFinite, RowsInfinite});
    TestTrue(TEXT("native capture remains VFX multi-selection authority"),
        RowsFinite.FindRef(TEXT("Component:")) == TEXT("None")
            && RowsFinite.Contains(TEXT("Elapsed / Duration:"))
            && RowsInfinite.FindRef(TEXT("Duration:")) == TEXT("Infinite")
            && RowsFinite.FindRef(TEXT("State:")) == TEXT("Playing")
            && RowsInfinite.FindRef(TEXT("State:")) == TEXT("Finished")
            && Differing.Contains(TEXT("Start Time:")) && Differing.Contains(TEXT("Elapsed / Duration:"))
            && Differing.Contains(TEXT("Duration:")) && Differing.Contains(TEXT("State:")));
    TSharedPtr<SCkInspector_VfxAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_VfxAuthored>(Inspector.Build_Inspector(Finite));
    }
    TestTrue(TEXT("authored VFX labels receive exact native diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_DiffMarked(TEXT("Start Time:"))
        && DiffAuthored->Is_DiffMarked(TEXT("Elapsed / Duration:"))
        && DiffAuthored->Is_DiffMarked(TEXT("Duration:"))
        && DiffAuthored->Is_DiffMarked(TEXT("State:")));

    const TSharedPtr<SButton> PlayButton = FindButton(RenderedFinite, TEXT("vfx-play"));
    const TSharedPtr<SButton> StopButton = FindButton(RenderedFinite, TEXT("vfx-stop"));
    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{480.0f, 320.0f})
        .CreateTitleBar(false).HasCloseButton(false)[RenderedFinite];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    Tick(Slate);
    const bool bGateEnabled = TestTrue(TEXT("standalone VFX entity passes the authored CosmeticOnly request gate"),
        AuthoredFinite->Get_CanRequest());
    if (NOT bGateEnabled)
    { AddError(AuthoredFinite->Get_RequestDisabledReason()); }
    const bool bActionsMounted = TestTrue(TEXT("authored VFX mounts physical Play and Stop actions"),
        PlayButton.IsValid() && StopButton.IsValid());
    const bool bPlayEnabled = TestTrue(TEXT("authored VFX Play action is enabled"),
        PlayButton.IsValid() && PlayButton->IsEnabled());
    const bool bStopEnabled = TestTrue(TEXT("authored VFX Stop action is enabled"),
        StopButton.IsValid() && StopButton->IsEnabled());
    if (NOT bGateEnabled || NOT bActionsMounted || NOT bPlayEnabled || NOT bStopEnabled)
    { return false; }
    PlayButton->SimulateClick();
    StopButton->SimulateClick();
    if (NOT TestTrue(TEXT("physical actions route exactly two public VFX requests"),
        Finite.Has<ck::FFragment_VfxCue_Requests>()
            && Finite.Get<ck::FFragment_VfxCue_Requests>().Get_Requests().Num() == 2))
    { return false; }
    TestTrue(TEXT("routed VFX requests retain Play then Stop types"),
        std::holds_alternative<FCk_Request_VfxCue_Play>(
            Finite.Get<ck::FFragment_VfxCue_Requests>().Get_Requests()[0])
        && std::holds_alternative<FCk_Request_VfxCue_Stop>(
            Finite.Get<ck::FFragment_VfxCue_Requests>().Get_Requests()[1]));
    Finite.Try_Remove<ck::FFragment_VfxCue_Requests>();
    UWorld* const DedicatedWorld = UWorld::CreateWorld(EWorldType::PIE, false);
    ON_SCOPE_EXIT { if (DedicatedWorld != nullptr) { DedicatedWorld->DestroyWorld(false); } };
    if (NOT TestTrue(TEXT("fixture creates a dedicated-server PIE world"), DedicatedWorld != nullptr))
    { return false; }
    DedicatedWorld->SetPlayInEditorInitialNetMode(NM_DedicatedServer);
    auto DedicatedOwner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    DedicatedOwner.Add<TWeakObjectPtr<UWorld>>(DedicatedWorld);
    auto DedicatedCue = CreateCue(DedicatedOwner, 0.0, 1.0, false, false);
    const TSharedRef<SCkInspector_VfxAuthored> DedicatedAuthored =
        StaticCastSharedRef<SCkInspector_VfxAuthored>(Inspector.Build_Inspector(DedicatedCue));
    const TSharedPtr<SButton> DedicatedPlay = FindButton(DedicatedAuthored, TEXT("vfx-play"));
    FWindowScope DedicatedWindowScope{Slate};
    DedicatedWindowScope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{480.0f, 320.0f}).CreateTitleBar(false).HasCloseButton(false)[DedicatedAuthored];
    Slate.AddWindow(DedicatedWindowScope.Window.ToSharedRef(), true);
    Tick(Slate);
    if (DedicatedPlay.IsValid()) { DedicatedPlay->SlatePrepass(); }
    const bool bDedicatedMode = TestEqual(TEXT("fixture world reports dedicated-server net mode"),
        DedicatedWorld->GetNetMode(), NM_DedicatedServer);
    const bool bDedicatedActionMounted = TestTrue(TEXT("dedicated-server VFX mounts its physical Play action"),
        DedicatedPlay.IsValid() && DedicatedPlay != PlayButton);
    const bool bDedicatedViewDisabled = TestFalse(TEXT("dedicated-server VFX rejects authored request admission"),
        DedicatedAuthored->Get_CanRequest());
    const bool bDedicatedActionDisabled = TestTrue(TEXT("dedicated-server VFX physical Play action is disabled"),
        DedicatedPlay.IsValid() && NOT DedicatedPlay->IsEnabled());
    if (NOT bDedicatedMode || NOT bDedicatedActionMounted || NOT bDedicatedViewDisabled
        || NOT bDedicatedActionDisabled)
    { return false; }
    DedicatedPlay->SimulateClick();
    TestFalse(TEXT("dedicated-server VFX physical Play cannot enqueue a request"),
        DedicatedCue.Has<ck::FFragment_VfxCue_Requests>());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed VFX resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorVfx.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorVfx.ui.css")))))
    { return false; }
    TestTrue(TEXT("resource owns the complete VFX layout and both actions"),
        Markup.Contains(TEXT(">VFX Cue</text>")) && Markup.Contains(TEXT(">Component:</text>"))
            && Markup.Contains(TEXT(">Start Time:</text>")) && Markup.Contains(TEXT(">Elapsed / Duration:</text>"))
            && Markup.Contains(TEXT(">Duration:</text>")) && Markup.Contains(TEXT(">State:</text>"))
            && Markup.Contains(TEXT(">Playback:</text>")) && Markup.Contains(TEXT("action=\"vfx-play\""))
            && Markup.Contains(TEXT("action=\"vfx-stop\""))
            && Stylesheet.Contains(TEXT(".vfx-inspector")) && Stylesheet.Contains(TEXT(".vfx-row"))
            && Stylesheet.Contains(TEXT(".vfx-label")) && Stylesheet.Contains(TEXT(".vfx-meter-value"))
            && Stylesheet.Contains(TEXT(".vfx-actions")) && Stylesheet.Contains(TEXT(".vfx-unavailable")));
    const int64 FiniteRevisionBefore = FiniteView->GetRevision();
    const int64 InfiniteRevisionBefore = InfiniteView->GetRevision();
    TestTrue(TEXT("compatible VFX reload is accepted by finite view"),
        FiniteView->TryReload(Markup, Stylesheet, TEXT("VFX finite compatible candidate")).Succeeded);
    TestTrue(TEXT("finite reload retains identity without mutating infinite view"),
        FiniteView->GetRevision() > FiniteRevisionBefore && InfiniteView->GetRevision() == InfiniteRevisionBefore
            && FindButton(RenderedFinite, TEXT("vfx-play")) == PlayButton);
    const TSharedRef<SWidget> InfiniteMainBefore = InfiniteView->GetRegion(TEXT("main"));
    const int64 InfiniteRevisionBeforeRejected = InfiniteView->GetRevision();
    TestFalse(TEXT("missing VFX action is rejected atomically"), InfiniteView->TryReload(
        Markup.Replace(TEXT("action=\"vfx-play\""), TEXT("action=\"vfx-missing\"")),
        Stylesheet, TEXT("VFX infinite rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected VFX reload retains tree and revision"),
        &InfiniteView->GetRegion(TEXT("main")).Get() == &InfiniteMainBefore.Get()
            && InfiniteView->GetRevision() == InfiniteRevisionBeforeRejected);

    TestTrue(TEXT("fixture removes VFX current state while entity remains live"),
        Finite.Try_Remove<ck::FFragment_VfxCue_Current>() && ck::IsValid(Finite) && NOT Inspector.CanInspect(Finite));
    PlayButton->SlatePrepass();
    TestTrue(TEXT("mounted VFX state fails closed after composition loss"),
        NOT AuthoredFinite->Get_IsAvailable() && AuthoredFinite->Get_StateText() == TEXT("--")
            && NOT PlayButton->IsEnabled());
    PlayButton->SimulateClick();
    TestFalse(TEXT("held VFX action cannot enqueue after composition loss"),
        Finite.Has<ck::FFragment_VfxCue_Requests>());
    Infinite.Add<ck::FTag_DestroyEntity_Initiate>();
    TestTrue(TEXT("pending destruction makes VFX unavailable before registry teardown"),
        ck::IsValid(Infinite) && NOT Inspector.CanInspect(Infinite) && NOT AuthoredInfinite->Get_IsAvailable());

    TSharedPtr<SCkInspector_VfxAuthored> DestructorAuthored;
    {
        Infinite.Try_Remove<ck::FTag_DestroyEntity_Initiate>();
        auto DestructorInspector = MakeUnique<FCkInspector_Vfx>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_VfxAuthored>(
            DestructorInspector->Build_Inspector(Infinite));
    }
    TestTrue(TEXT("VFX inspector destruction makes its authored build inert"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());
    const TWeakPtr<FCkUiView> ReleasedFiniteView = FiniteView;
    const TWeakPtr<FCkUiView> ReleasedInfiniteView = InfiniteView;
    Inspector.OnDeactivated();
    TestTrue(TEXT("VFX deactivation releases every retained authored build"),
        AuthoredFinite->Is_Inert() && AuthoredInfinite->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT AuthoredFinite->Is_Mounted() && NOT AuthoredInfinite->Is_Mounted());
    FiniteView.Reset();
    InfiniteView.Reset();
    TestFalse(TEXT("VFX deactivation releases every per-build view"),
        ReleasedFiniteView.IsValid() || ReleasedInfiniteView.IsValid());
    return true;
}

#endif
