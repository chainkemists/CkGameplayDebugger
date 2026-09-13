#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Probes.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkShapes/CkShapes_Common.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkSpatialQuery/Probe/CkProbe_Utils.h"

#include "Engine/World.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/IToolTip.h"

namespace ck_inspector_probes_authored_test
{
    auto FindSwitch(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SCkDebug_Switch>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_Switch") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SCkDebug_Switch>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCkDebug_Switch> Found = FindSwitch(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto MakeClick() -> FPointerEvent
    {
        return FPointerEvent{
            0, FVector2D{5.0f, 5.0f}, FVector2D{5.0f, 5.0f}, TSet<FKey>{EKeys::LeftMouseButton},
            EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
    }

    auto GetToolTipText(const TSharedRef<SWidget>& InWidget) -> FString
    {
        const TSharedPtr<IToolTip> Tooltip = InWidget->GetToolTip();
        if (NOT Tooltip.IsValid()) { return {}; }
        const TSharedRef<SWidget> Content = Tooltip->GetContentWidget();
        Content->SlatePrepass();
        return Content->GetAccessibleText().ToString();
    }

    auto CreateProbe(FCk_Handle& InOwner, FCk_Fragment_Probe_ParamsData InParams) -> FCk_Handle_Probe
    {
        return UCk_Utils_Probe_UE::Create(InOwner, FTransform::Identity,
            FCk_AnyShape{FCk_ShapeSphere_Dimensions{25.0f}}, InParams, FCk_Probe_DebugInfo{});
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorProbesAuthored,
    "Ck.UiAuthoring.EcsDebugger.ProbesInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorProbesAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_probes_authored_test;

    auto InvalidInspector = FCkInspector_Probes{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build mounts the authored Probes shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_ProbesAuthored")}))
    { AddError(InvalidInspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_ProbesAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_ProbesAuthored>(InvalidRendered);
    TestTrue(TEXT("default-invalid Probes shell fails closed"), InvalidAuthored->Is_Mounted()
        && NOT InvalidAuthored->Get_IsAvailable() && InvalidAuthored->Get_NameText() == TEXT("--")
        && NOT InvalidAuthored->Get_CanToggleDebugDraw());
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid Probes shell releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted()
            && NOT InvalidAuthored->Get_View().IsValid());

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    UWorld* const StandaloneWorld = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { if (StandaloneWorld != nullptr) { StandaloneWorld->DestroyWorld(false); } };
    if (NOT TestTrue(TEXT("fixture has a transient owner and standalone cosmetic world"),
        ck::IsValid(LifetimeOwner) && StandaloneWorld != nullptr
            && StandaloneWorld->GetNetMode() == NM_Standalone))
    { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(StandaloneWorld);

    auto ParamsA = FCk_Fragment_Probe_ParamsData{TAG_Probe};
    ParamsA.Set_ResponsePolicy(ECk_ProbeResponse_Policy::Notify);
    ParamsA.Set_Filter(FGameplayTagContainer{TAG_Probe});
    ParamsA.Set_MotionType(ECk_MotionType::Kinematic);
    ParamsA.Set_MotionQuality(ECk_MotionQuality::LinearCast);
    auto ParamsB = FCk_Fragment_Probe_ParamsData{TAG_Probe};
    ParamsB.Set_ResponsePolicy(ECk_ProbeResponse_Policy::Silent);
    ParamsB.Set_MotionType(ECk_MotionType::Static);
    ParamsB.Set_MotionQuality(ECk_MotionQuality::Discrete);
    FCk_Handle_Probe ProbeA = CreateProbe(LifetimeOwner, ParamsA);
    FCk_Handle_Probe ProbeB = CreateProbe(LifetimeOwner, ParamsB);
    FCk_Handle_Probe ProbeC = CreateProbe(LifetimeOwner, ParamsB);
    if (NOT TestTrue(TEXT("fixture creates three distinct probes through the public Create path"),
        ck::IsValid(ProbeA) && ck::IsValid(ProbeB) && ck::IsValid(ProbeC)
            && ProbeA != ProbeB && ProbeA != ProbeC && ProbeB != ProbeC
            && UCk_Utils_Probe_UE::Has(ProbeA) && UCk_Utils_Probe_UE::Has(ProbeB)
            && UCk_Utils_Probe_UE::Has(ProbeC)))
    { return false; }

    auto Inspector = FCkInspector_Probes{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(ProbeA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(ProbeB);
    if (NOT TestEqual(TEXT("first probe mounts the authored Probes inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_ProbesAuthored")})
        || NOT TestEqual(TEXT("second probe mounts the authored Probes inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_ProbesAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_ProbesAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_ProbesAuthored>(RenderedA);
    const TSharedRef<SCkInspector_ProbesAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_ProbesAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TestTrue(TEXT("each Probes build owns an independent accepted view and overlap collection"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && ViewA != ViewB && AuthoredA->Get_Overlaps().IsValid() && AuthoredB->Get_Overlaps().IsValid()
            && AuthoredA->Get_Overlaps() != AuthoredB->Get_Overlaps());
    TestTrue(TEXT("authored Probes project exact live configuration and empty overlaps"),
        AuthoredA->Get_NameText() == TEXT("Probe") && AuthoredA->Get_StateText() == TEXT("Enabled")
            && AuthoredA->Get_ResponseText() == TEXT("Notify")
            && AuthoredA->Get_MotionText() == TEXT("Kinematic")
            && AuthoredA->Get_QualityText() == TEXT("LinearCast (CCD)")
            && AuthoredA->Get_FilterText().Contains(TEXT("Probe"))
            && AuthoredB->Get_ResponseText() == TEXT("Silent")
            && AuthoredB->Get_MotionText() == TEXT("Static")
            && AuthoredB->Get_QualityText() == TEXT("Discrete")
            && AuthoredB->Get_FilterText() == TEXT("(Empty)"));

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(ProbeA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(ProbeB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native capture remains Probes multi-selection authority"),
        RowsA.Num() == 8 && RowsA.FindRef(TEXT("Name:")) == TEXT("Probe")
            && RowsA.FindRef(TEXT("State:")) == TEXT("Enabled")
            && RowsA.FindRef(TEXT("Response:")) == TEXT("Notify")
            && RowsB.FindRef(TEXT("Response:")) == TEXT("Silent")
            && RowsA.FindRef(TEXT("Motion:")) == TEXT("Kinematic")
            && RowsB.FindRef(TEXT("Motion:")) == TEXT("Static")
            && Differing.Contains(TEXT("Response:")) && Differing.Contains(TEXT("Motion:"))
            && Differing.Contains(TEXT("Quality:")) && Differing.Contains(TEXT("Filter:")));
    TSharedPtr<SCkInspector_ProbesAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_ProbesAuthored>(Inspector.Build_Inspector(ProbeA));
    }
    TestTrue(TEXT("authored Probes labels receive exact native diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_DiffMarked(TEXT("Response:"))
        && DiffAuthored->Is_DiffMarked(TEXT("Motion:"))
        && DiffAuthored->Is_DiffMarked(TEXT("Quality:"))
        && DiffAuthored->Is_DiffMarked(TEXT("Filter:"))
        && NOT DiffAuthored->Is_DiffMarked(TEXT("Name:")));

    const TSharedPtr<SCkDebug_Switch> SwitchB = FindSwitch(RenderedB, TEXT("probe-debug-draw-switch"));
    if (NOT TestTrue(TEXT("authored Probes mounts a physical debug-draw switch"), SwitchB.IsValid()))
    { return false; }
    SwitchB->SlatePrepass();
    TestTrue(TEXT("standalone probe enables its CosmeticOnly debug-draw control"),
        AuthoredB->Get_CanToggleDebugDraw() && SwitchB->IsEnabled()
            && AuthoredB->Get_DebugDrawDisabledReason().Contains(TEXT("Enable or disable")));
    Inspector.Tick(ProbeA, 0.0f);
    TestTrue(TEXT("inspecting A applies the enabled debug-draw preference"),
        ProbeA.Has<ck::FTag_Probe_DebugDraw>() && NOT ProbeB.Has<ck::FTag_Probe_DebugDraw>());
    Inspector.Tick(ProbeB, 0.0f);
    TestTrue(TEXT("selection switch transfers debug-draw ownership without leaking A"),
        NOT ProbeA.Has<ck::FTag_Probe_DebugDraw>() && ProbeB.Has<ck::FTag_Probe_DebugDraw>());
    SwitchB->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    TestFalse(TEXT("physical authored switch disables the selected probe immediately"),
        ProbeB.Has<ck::FTag_Probe_DebugDraw>());
    SwitchB->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    TestTrue(TEXT("physical authored switch re-enables the selected probe immediately"),
        ProbeB.Has<ck::FTag_Probe_DebugDraw>());

    const TSharedRef<SCkInspector_ProbesAuthored> DuplicateB =
        StaticCastSharedRef<SCkInspector_ProbesAuthored>(Inspector.Build_Inspector(ProbeB));
    DuplicateB->Release();
    TestTrue(TEXT("releasing one duplicate view cannot tear down inspector-owned draw state"),
        DuplicateB->Is_Inert() && ProbeB.Has<ck::FTag_Probe_DebugDraw>());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Probes resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorProbes.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorProbes.ui.css")))))
    { return false; }
    TestTrue(TEXT("HTML/CSS owns every Probes row, switch, overlap repeat, and unavailable state"),
        Markup.Contains(TEXT(">Probe</text>")) && Markup.Contains(TEXT(">Name:</text>"))
            && Markup.Contains(TEXT(">State:</text>")) && Markup.Contains(TEXT(">Debug Draw:</text>"))
            && Markup.Contains(TEXT("debug-switch"))
            && Markup.Contains(TEXT("tooltip-bind=\"probe-debug-draw-tooltip\""))
            && Markup.Contains(TEXT(">Overlaps:</text>"))
            && Markup.Contains(TEXT("debug-entity-ref"))
            && Markup.Contains(TEXT("item-action=\"probe-navigate-overlap\""))
            && Markup.Contains(TEXT(">Response:</text>")) && Markup.Contains(TEXT(">Motion:</text>"))
            && Markup.Contains(TEXT(">Quality:</text>")) && Markup.Contains(TEXT(">Filter:</text>"))
            && Stylesheet.Contains(TEXT(".probe-inspector")) && Stylesheet.Contains(TEXT(".probe-row"))
            && Stylesheet.Contains(TEXT(".probe-switch")) && Stylesheet.Contains(TEXT(".probe-overlaps"))
            && Stylesheet.Contains(TEXT(".probe-unavailable")));
    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible Probes reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Probes A compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible reload retains switch identity and isolates B"),
        ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore
            && FindSwitch(RenderedB, TEXT("probe-debug-draw-switch")) == SwitchB);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RejectedRevision = ViewB->GetRevision();
    TestFalse(TEXT("missing Probes switch action is rejected atomically"), ViewB->TryReload(
        Markup.Replace(TEXT("changed=\"probe-debug-draw-changed\""),
            TEXT("changed=\"probe-debug-draw-missing\"")),
        Stylesheet, TEXT("Probes B rejected action candidate")).Succeeded);
    TestTrue(TEXT("rejected action retains B tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get()
            && ViewB->GetRevision() == RejectedRevision);
    TestFalse(TEXT("missing Probes overlap field is rejected atomically"), ViewB->TryReload(
        Markup.Replace(TEXT("name-field=\"overlap-name\""), TEXT("name-field=\"overlap-missing\"")),
        Stylesheet, TEXT("Probes B rejected field candidate")).Succeeded);
    TestTrue(TEXT("rejected field retains B tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get()
            && ViewB->GetRevision() == RejectedRevision);

    UWorld* const DedicatedWorld = UWorld::CreateWorld(EWorldType::PIE, false);
    ON_SCOPE_EXIT { if (DedicatedWorld != nullptr) { DedicatedWorld->DestroyWorld(false); } };
    if (NOT TestTrue(TEXT("fixture creates a dedicated-server PIE world"), DedicatedWorld != nullptr))
    { return false; }
    DedicatedWorld->SetPlayInEditorInitialNetMode(NM_DedicatedServer);
    auto DedicatedOwner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    DedicatedOwner.Add<TWeakObjectPtr<UWorld>>(DedicatedWorld);
    FCk_Handle_Probe DedicatedProbe = CreateProbe(DedicatedOwner, ParamsA);
    const TSharedRef<SCkInspector_ProbesAuthored> DedicatedAuthored =
        StaticCastSharedRef<SCkInspector_ProbesAuthored>(Inspector.Build_Inspector(DedicatedProbe));
    const TSharedPtr<SCkDebug_Switch> DedicatedSwitch =
        FindSwitch(DedicatedAuthored, TEXT("probe-debug-draw-switch"));
    if (NOT TestTrue(TEXT("dedicated-server Probes mounts its physical switch"), DedicatedSwitch.IsValid()))
    { return false; }
    DedicatedSwitch->SlatePrepass();
    TestTrue(TEXT("dedicated-server Probes mounts a disabled physical switch with its gate reason"),
        DedicatedWorld->GetNetMode() == NM_DedicatedServer
            && NOT DedicatedAuthored->Get_CanToggleDebugDraw() && NOT DedicatedSwitch->IsEnabled()
            && NOT DedicatedAuthored->Get_DebugDrawDisabledReason().IsEmpty()
            && GetToolTipText(DedicatedSwitch.ToSharedRef())
                == DedicatedAuthored->Get_DebugDrawDisabledReason());
    DedicatedSwitch->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    TestFalse(TEXT("dedicated-server physical switch cannot add debug draw"),
        DedicatedProbe.Has<ck::FTag_Probe_DebugDraw>());
    Inspector.Tick(DedicatedProbe, 0.0f);
    TestTrue(TEXT("gated selection still cleans prior owned debug draw without mutating dedicated probe"),
        NOT ProbeB.Has<ck::FTag_Probe_DebugDraw>() && NOT DedicatedProbe.Has<ck::FTag_Probe_DebugDraw>());
    Inspector.Tick(ProbeB, 0.0f);
    TestTrue(TEXT("returning to B reapplies the retained preference"), ProbeB.Has<ck::FTag_Probe_DebugDraw>());

    ProbeA.Try_Remove<ck::FFragment_Probe_Params>();
    const TSharedPtr<SCkDebug_Switch> HeldSwitchA = FindSwitch(RenderedA, TEXT("probe-debug-draw-switch"));
    if (NOT TestTrue(TEXT("composition-loss fixture retains its physical switch"), HeldSwitchA.IsValid()))
    { return false; }
    HeldSwitchA->SlatePrepass();
    TestTrue(TEXT("composition loss makes the retained authored view unavailable and inert"),
        ck::IsValid(ProbeA) && NOT Inspector.CanInspect(ProbeA) && NOT AuthoredA->Get_IsAvailable()
            && NOT HeldSwitchA->IsEnabled());
    HeldSwitchA->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    TestFalse(TEXT("held physical switch cannot mutate after composition loss"),
        ProbeA.Has<ck::FTag_Probe_DebugDraw>());
    DedicatedProbe.Add<ck::FTag_DestroyEntity_Initiate>();
    TestTrue(TEXT("pending destruction makes a probe unavailable before registry teardown"),
        ck::IsValid(DedicatedProbe) && NOT Inspector.CanInspect(DedicatedProbe)
            && NOT DedicatedAuthored->Get_IsAvailable());

    TSharedPtr<SCkInspector_ProbesAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_Probes>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_ProbesAuthored>(
            DestructorInspector->Build_Inspector(ProbeC));
        DestructorInspector->Tick(ProbeC, 0.0f);
        {
            auto ObservingInspector = FCkInspector_Probes{};
            ObservingInspector.Tick(ProbeC, 0.0f);
            ObservingInspector.OnDeactivated();
        }
        TestTrue(TEXT("observer teardown preserves debug draw enabled by another inspector"),
            ProbeC.Has<ck::FTag_Probe_DebugDraw>());
    }
    TestTrue(TEXT("Probes inspector destruction makes its authored build inert and disables owned draw"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert()
            && NOT DestructorAuthored->Is_Mounted() && NOT ProbeC.Has<ck::FTag_Probe_DebugDraw>());
    Inspector.Tick(ProbeB, 0.0f);
    const TWeakPtr<FCkUiView> ReleasedA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedB = ViewB;
    Inspector.OnDeactivated();
    TestTrue(TEXT("Probes deactivation disables every owned probe and makes every retained view inert"),
        NOT ProbeB.Has<ck::FTag_Probe_DebugDraw>() && AuthoredA->Is_Inert() && AuthoredB->Is_Inert()
            && DiffAuthored->Is_Inert() && DedicatedAuthored->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted());
    ViewA.Reset();
    ViewB.Reset();
    TestFalse(TEXT("Probes deactivation releases every per-build view"),
        ReleasedA.IsValid() || ReleasedB.IsValid());
    return true;
}

#endif
