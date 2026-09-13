#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_ProbeTraces.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkShapes/CkShapes_Common.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiRepeat.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkSpatialQuery/Probe/CkProbeTrace_Utils.h"

#include "Engine/World.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace ck_inspector_probe_traces_authored_test
{
    template <typename T_Widget>
    auto FindWidget(const TSharedRef<SWidget>& InRoot, const FString& InType) -> TSharedPtr<T_Widget>
    {
        if (InRoot->GetTypeAsString() == InType)
        { return StaticCastSharedRef<T_Widget>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<T_Widget> Found = FindWidget<T_Widget>(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

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

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorProbeTracesAuthored,
    "Ck.UiAuthoring.EcsDebugger.ProbeTracesInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorProbeTracesAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_probe_traces_authored_test;

    auto InvalidInspector = FCkInspector_ProbeTraces{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector({});
    if (NOT TestEqual(TEXT("default-invalid build mounts the authored Probe Trace shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_ProbeTracesAuthored")}))
    { AddError(InvalidInspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_ProbeTracesAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_ProbeTracesAuthored>(InvalidRendered);
    TestTrue(TEXT("default-invalid authored Probe Trace fails closed"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && NOT InvalidAuthored->Get_CanToggleEnabled()
            && InvalidAuthored->Get_TypeText() == TEXT("--"));
    const TWeakPtr<FCkUiView> ReleasedInvalid = InvalidAuthored->Get_View();
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid authored Probe Trace releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted()
            && NOT InvalidAuthored->Get_View().IsValid() && NOT ReleasedInvalid.IsValid());

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    UWorld* const StandaloneWorld = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { if (StandaloneWorld != nullptr) { StandaloneWorld->DestroyWorld(false); } };
    if (NOT TestTrue(TEXT("fixture has a transient owner and standalone local world"),
        ck::IsValid(LifetimeOwner) && StandaloneWorld != nullptr
            && StandaloneWorld->GetNetMode() == NM_Standalone))
    { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(StandaloneWorld);
    FCk_Handle_Transform Start = UCk_Utils_Transform_UE::Create(
        LifetimeOwner, FTransform::Identity, ECk_Replication::DoesNotReplicate);
    auto TargetA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    auto TargetB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    UCk_Utils_Handle_UE::Set_DebugName(TargetA, TEXT("Target A"), ECk_Override::Override);
    UCk_Utils_Handle_UE::Set_DebugName(TargetB, TEXT("Target B"), ECk_Override::Override);

    auto RaySettings = FCk_Probe_RayCastPersistent_Settings{
        Start, FVector{125.0, -25.0, 5.0}, FGameplayTagContainer{TAG_Probe}};
    RaySettings.Set_TracePolicy(ECk_ProbeTrace_Policy::Multi);
    FCk_Handle_ProbeTrace Ray = UCk_Utils_ProbeTrace_UE::Create_LineTrace_Persistent(RaySettings);
    auto ShapeSettings = FCk_Probe_ShapeCastPersistent_Settings{
        Start, FVector{0.0, 75.0, -10.0},
        FCk_AnyShape{FCk_ShapeSphere_Dimensions{25.0f}}, FGameplayTagContainer{}};
    ShapeSettings.Set_TracePolicy(ECk_ProbeTrace_Policy::Single);
    FCk_Handle_ProbeTrace Shape = UCk_Utils_ProbeTrace_UE::Create_ShapeTrace_Persistent(ShapeSettings);
    if (NOT TestTrue(TEXT("fixture creates distinct ray and shape traces through public APIs"),
        ck::IsValid(Start) && ck::IsValid(Ray) && ck::IsValid(Shape) && Ray != Shape
            && UCk_Utils_ProbeTrace_UE::Has(Ray) && UCk_Utils_ProbeTrace_UE::Has(Shape)))
    { return false; }
    Ray.Get<TSet<FCk_Probe_OverlapInfo>>().Add(FCk_Probe_OverlapInfo{TargetA});

    auto Inspector = FCkInspector_ProbeTraces{};
    const TSharedRef<SWidget> RenderedRay = Inspector.Build_Inspector(Ray);
    const TSharedRef<SWidget> RenderedShape = Inspector.Build_Inspector(Shape);
    if (NOT TestEqual(TEXT("ray trace mounts authored Probe Trace inspector"),
            RenderedRay->GetTypeAsString(), FString{TEXT("SCkInspector_ProbeTracesAuthored")})
        || NOT TestEqual(TEXT("shape trace mounts authored Probe Trace inspector"),
            RenderedShape->GetTypeAsString(), FString{TEXT("SCkInspector_ProbeTracesAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_ProbeTracesAuthored> AuthoredRay =
        StaticCastSharedRef<SCkInspector_ProbeTracesAuthored>(RenderedRay);
    const TSharedRef<SCkInspector_ProbeTracesAuthored> AuthoredShape =
        StaticCastSharedRef<SCkInspector_ProbeTracesAuthored>(RenderedShape);
    TSharedPtr<FCkUiView> ViewRay = AuthoredRay->Get_View();
    TSharedPtr<FCkUiView> ViewShape = AuthoredShape->Get_View();
    TestTrue(TEXT("each trace build owns an independent retained view and overlap collection"),
        AuthoredRay->Is_Mounted() && AuthoredShape->Is_Mounted()
            && ViewRay.IsValid() && ViewShape.IsValid() && ViewRay != ViewShape
            && AuthoredRay->Get_Overlaps().IsValid() && AuthoredShape->Get_Overlaps().IsValid()
            && AuthoredRay->Get_Overlaps() != AuthoredShape->Get_Overlaps());

    const TSharedPtr<SCkUiRepeat> OverlapRepeat =
        FindWidget<SCkUiRepeat>(RenderedRay, TEXT("SCkUiRepeat"));
    if (NOT TestTrue(TEXT("authored Probe Trace mounts its retained overlap presenter"),
        OverlapRepeat.IsValid()))
    { return false; }

    TestTrue(TEXT("authored ray projects exact live configuration"),
        AuthoredRay->Get_IsAvailable() && AuthoredRay->Get_IsEnabled()
            && AuthoredRay->Get_TypeText() == TEXT("RayCast")
            && AuthoredRay->Get_DirectionXText() == TEXT("125.000")
            && AuthoredRay->Get_DirectionYText() == TEXT("-25.000")
            && AuthoredRay->Get_DirectionZText() == TEXT("5.000")
            && AuthoredRay->Get_PolicyText() == TEXT("Multi")
            && NOT AuthoredRay->Get_IsShapeVisible()
            && AuthoredRay->Get_FilterText().Contains(TEXT("Probe"))
            && AuthoredRay->Get_HasOverlaps());
    TestTrue(TEXT("authored shape projects its optional shape branch"),
        AuthoredShape->Get_IsAvailable() && AuthoredShape->Get_IsEnabled()
            && AuthoredShape->Get_TypeText() == TEXT("ShapeCast")
            && AuthoredShape->Get_DirectionYText() == TEXT("75.000")
            && AuthoredShape->Get_PolicyText() == TEXT("Single")
            && AuthoredShape->Get_IsShapeVisible() && AuthoredShape->Get_ShapeText() == TEXT("Sphere")
            && AuthoredShape->Get_FilterText() == TEXT("(Empty)")
            && NOT AuthoredShape->Get_HasOverlaps());

    auto RowsRay = TMap<FString, FString>{};
    auto RowsShape = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Ray); RowsRay = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Shape); RowsShape = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsRay, RowsShape});
    TestTrue(TEXT("native capture remains exact multi-selection authority"),
        RowsRay.Num() == 6 && RowsShape.Num() == 7
            && RowsRay.FindRef(TEXT("Type:")) == TEXT("RayCast")
            && RowsShape.FindRef(TEXT("Type:")) == TEXT("ShapeCast")
            && RowsRay.FindRef(TEXT("Policy:")) == TEXT("Multi")
            && RowsShape.FindRef(TEXT("Policy:")) == TEXT("Single")
            && RowsShape.FindRef(TEXT("Shape:")) == TEXT("Sphere")
            && Differing.Contains(TEXT("Type:")) && Differing.Contains(TEXT("Direction:"))
            && Differing.Contains(TEXT("Policy:")) && Differing.Contains(TEXT("Filter:")));
    TSharedPtr<SCkInspector_ProbeTracesAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_ProbeTracesAuthored>(Inspector.Build_Inspector(Ray));
    }
    TestTrue(TEXT("authored labels receive exact native diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_DiffMarked(TEXT("Type:"))
        && DiffAuthored->Is_DiffMarked(TEXT("Direction:"))
        && DiffAuthored->Is_DiffMarked(TEXT("Policy:"))
        && NOT DiffAuthored->Is_DiffMarked(TEXT("Enabled:")));

    const TSharedPtr<SCkDebug_Switch> EnabledSwitch =
        FindSwitch(RenderedRay, TEXT("probe-trace-enabled-switch"));
    if (NOT TestTrue(TEXT("authored Probe Trace mounts its physical enabled switch"), EnabledSwitch.IsValid()))
    { return false; }
    EnabledSwitch->SlatePrepass();
    TestTrue(TEXT("standalone trace enables its LocalOk control"),
        AuthoredRay->Get_CanToggleEnabled() && EnabledSwitch->IsEnabled()
            && AuthoredRay->Get_EnabledTooltip().Contains(TEXT("Enable or disable")));
    EnabledSwitch->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    TestTrue(TEXT("physical switch disables synchronously and clears overlaps"),
        NOT AuthoredRay->Get_IsEnabled() && Ray.Has<ck::FTag_ProbeTrace_Disabled>()
            && Ray.Get<TSet<FCk_Probe_OverlapInfo>>().IsEmpty());
    EnabledSwitch->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    TestTrue(TEXT("physical switch re-enables synchronously"),
        AuthoredRay->Get_IsEnabled() && NOT Ray.Has<ck::FTag_ProbeTrace_Disabled>());

    Ray.Get<TSet<FCk_Probe_OverlapInfo>>().Add(FCk_Probe_OverlapInfo{TargetA});
    AuthoredRay->Tick(FGeometry{}, 0.0, 0.0f);
    TestTrue(TEXT("overlap presenter reconciles the published Target A identity"),
        OverlapRepeat->TryRefresh());
    const TSharedPtr<FCkUiCollection> RayOverlaps = AuthoredRay->Get_Overlaps();
    const FString TargetAKey = RayOverlaps.IsValid() && NOT RayOverlaps->GetRecords().IsEmpty()
        ? RayOverlaps->GetRecords()[0]->GetKey() : FString{};
    const TSharedPtr<SCkDebug_EntityRef> HeldTargetRef =
        FindWidget<SCkDebug_EntityRef>(RenderedRay, TEXT("SCkDebug_EntityRef"));
    TestTrue(TEXT("overlap collection mounts a clickable entity reference"),
        RayOverlaps.IsValid() && RayOverlaps->GetRecords().Num() == 1
            && NOT TargetAKey.IsEmpty() && HeldTargetRef.IsValid()
            && HeldTargetRef->OnCursorQuery(
                FGeometry::MakeRoot(FVector2D{80.0f, 20.0f}, FSlateLayoutTransform{}), MakeClick()).IsEventHandled());
    Ray.Get<TSet<FCk_Probe_OverlapInfo>>().Reset();
    Ray.Get<TSet<FCk_Probe_OverlapInfo>>().Add(FCk_Probe_OverlapInfo{TargetB});
    AuthoredRay->Tick(FGeometry{}, 0.0, 0.0f);
    TestTrue(TEXT("overlap presenter reconciles the published Target B identity"),
        OverlapRepeat->TryRefresh());
    TestTrue(TEXT("equal-count overlap replacement publishes fresh identity"),
        RayOverlaps->GetRecords().Num() == 1
            && RayOverlaps->GetRecords()[0]->GetKey() != TargetAKey);
    TestFalse(TEXT("replaced overlap leaves its held reference inert"),
        HeldTargetRef->OnMouseButtonDown(
            FGeometry::MakeRoot(FVector2D{80.0f, 20.0f}, FSlateLayoutTransform{}), MakeClick()).IsEventHandled());

    Inspector.Tick(Ray, 0.0f);
    TestTrue(TEXT("inspecting ray acquires debug-draw ownership"),
        Ray.Has<ck::FTag_ProbeTrace_DebugDraw>() && NOT Shape.Has<ck::FTag_ProbeTrace_DebugDraw>());
    Inspector.Tick(Shape, 0.0f);
    TestTrue(TEXT("selection switch transfers debug draw without leaking ray"),
        NOT Ray.Has<ck::FTag_ProbeTrace_DebugDraw>() && Shape.Has<ck::FTag_ProbeTrace_DebugDraw>());
    const TSharedRef<SCkInspector_ProbeTracesAuthored> DuplicateShape =
        StaticCastSharedRef<SCkInspector_ProbeTracesAuthored>(Inspector.Build_Inspector(Shape));
    DuplicateShape->Release();
    TestTrue(TEXT("releasing a duplicate view cannot tear inspector-owned draw state"),
        DuplicateShape->Is_Inert() && Shape.Has<ck::FTag_ProbeTrace_DebugDraw>());

    auto ExternalSettings = FCk_Probe_RayCastPersistent_Settings{
        Start, FVector{10.0, 0.0, 0.0}, FGameplayTagContainer{TAG_Probe}};
    FCk_Handle_ProbeTrace ExternalTrace = UCk_Utils_ProbeTrace_UE::Create_LineTrace_Persistent(ExternalSettings);
    ExternalTrace.AddOrGet<ck::FTag_ProbeTrace_DebugDraw>();
    {
        auto Observer = FCkInspector_ProbeTraces{};
        Observer.Tick(ExternalTrace, 0.0f);
        Observer.OnDeactivated();
    }
    TestTrue(TEXT("observer teardown preserves externally owned debug draw"),
        ExternalTrace.Has<ck::FTag_ProbeTrace_DebugDraw>());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Probe Trace resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup,
            *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorProbeTraces.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet,
            *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorProbeTraces.ui.css")))))
    { return false; }
    TestTrue(TEXT("HTML/CSS owns the complete Probe Trace surface without native ports"),
        Markup.Contains(TEXT(">Enabled:</text>")) && Markup.Contains(TEXT(">Type:</text>"))
            && Markup.Contains(TEXT(">Direction:</text>")) && Markup.Contains(TEXT(">Policy:</text>"))
            && Markup.Contains(TEXT(">Shape:</text>")) && Markup.Contains(TEXT(">Filter:</text>"))
            && Markup.Contains(TEXT(">Overlaps:</text>")) && Markup.Contains(TEXT("debug-switch"))
            && Markup.Contains(TEXT("debug-entity-ref")) && Markup.Contains(TEXT("<repeat"))
            && NOT Markup.Contains(TEXT("<native"))
            && Stylesheet.Contains(TEXT(".probe-trace-inspector"))
            && Stylesheet.Contains(TEXT(".probe-trace-overlaps"))
            && Stylesheet.Contains(TEXT(".probe-trace-unavailable")));
    const int64 RayRevision = ViewRay->GetRevision();
    const int64 ShapeRevision = ViewShape->GetRevision();
    TestTrue(TEXT("compatible reload is accepted by ray"),
        ViewRay->TryReload(Markup, Stylesheet, TEXT("Probe Trace compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible reload isolates shape view"),
        ViewRay->GetRevision() > RayRevision && ViewShape->GetRevision() == ShapeRevision);
    const TSharedRef<SWidget> ShapeMainBefore = ViewShape->GetRegion(TEXT("main"));
    const int64 RejectedRevision = ViewShape->GetRevision();
    TestFalse(TEXT("missing switch action is rejected atomically"), ViewShape->TryReload(
        Markup.Replace(TEXT("changed=\"probe-trace-enabled-changed\""),
            TEXT("changed=\"probe-trace-enabled-missing\"")),
        Stylesheet, TEXT("Probe Trace rejected action candidate")).Succeeded);
    TestFalse(TEXT("missing overlap field is rejected atomically"), ViewShape->TryReload(
        Markup.Replace(TEXT("name-field=\"overlap-name\""), TEXT("name-field=\"overlap-missing\"")),
        Stylesheet, TEXT("Probe Trace rejected field candidate")).Succeeded);
    TestTrue(TEXT("rejected resources preserve the accepted shape tree and revision"),
        &ViewShape->GetRegion(TEXT("main")).Get() == &ShapeMainBefore.Get()
            && ViewShape->GetRevision() == RejectedRevision);

    Ray.Try_Remove<ck::FFragment_ProbeTrace_RayCast>();
    EnabledSwitch->SlatePrepass();
    TestTrue(TEXT("composition loss fails closed while the retained trace view remains safe"),
        ck::IsValid(Ray) && NOT Inspector.CanInspect(Ray) && NOT AuthoredRay->Get_IsAvailable()
            && NOT EnabledSwitch->IsEnabled());
    EnabledSwitch->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    TestFalse(TEXT("held switch cannot mutate after composition loss"),
        Ray.Has<ck::FTag_ProbeTrace_Disabled>());
    Shape.Add<ck::FTag_DestroyEntity_Initiate>();
    Inspector.Tick(Shape, 0.0f);
    TestTrue(TEXT("pending destruction removes owned draw and makes authored shape unavailable"),
        NOT Shape.Has<ck::FTag_ProbeTrace_DebugDraw>() && NOT AuthoredShape->Get_IsAvailable());

    FCk_Handle_ProbeTrace DestructorTrace = UCk_Utils_ProbeTrace_UE::Create_LineTrace_Persistent(RaySettings);
    TSharedPtr<SCkInspector_ProbeTracesAuthored> DestructorAuthored;
    TWeakPtr<FCkUiView> ReleasedDestructor;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_ProbeTraces>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_ProbeTracesAuthored>(
            DestructorInspector->Build_Inspector(DestructorTrace));
        ReleasedDestructor = DestructorAuthored->Get_View();
        DestructorInspector->Tick(DestructorTrace, 0.0f);
        TestTrue(TEXT("destructor fixture owns draw before teardown"),
            DestructorTrace.Has<ck::FTag_ProbeTrace_DebugDraw>());
    }
    TestTrue(TEXT("inspector destructor releases its view and owned draw"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert()
            && NOT DestructorAuthored->Is_Mounted() && NOT ReleasedDestructor.IsValid()
            && NOT DestructorTrace.Has<ck::FTag_ProbeTrace_DebugDraw>());

    const TWeakPtr<FCkUiView> ReleasedRay = ViewRay;
    const TWeakPtr<FCkUiView> ReleasedShape = ViewShape;
    const TWeakPtr<FCkUiView> ReleasedDiff = DiffAuthored->Get_View();
    ViewRay.Reset();
    ViewShape.Reset();
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes every retained Probe Trace view inert"),
        AuthoredRay->Is_Inert() && AuthoredShape->Is_Inert() && DiffAuthored->Is_Inert()
            && DuplicateShape->Is_Inert()
            && NOT AuthoredRay->Is_Mounted() && NOT AuthoredShape->Is_Mounted());
    TestFalse(TEXT("deactivation releases every per-build view"),
        ReleasedRay.IsValid() || ReleasedShape.IsValid() || ReleasedDiff.IsValid());
    return true;
}

#endif
