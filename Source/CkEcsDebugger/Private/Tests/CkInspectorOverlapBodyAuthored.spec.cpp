#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_OverlapBody.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkOverlapBody/Marker/CkMarker_Fragment.h"
#include "CkOverlapBody/Sensor/CkSensor_Fragment.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/World.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ck_inspector_overlap_body_authored_test
{
    auto FindSwitch(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SCkDebug_Switch>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_Switch") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SCkDebug_Switch>(InRoot); }

        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SCkDebug_Switch> Found = FindSwitch(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
            if (Found.IsValid()) { return Found; }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorOverlapBodyAuthored,
    "Ck.UiAuthoring.EcsDebugger.OverlapBodyInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorOverlapBodyAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_overlap_body_authored_test;

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    UWorld* const TestWorld = GWorld;
    if (NOT TestTrue(TEXT("fixture has a transient owner and automation world"),
        ck::IsValid(LifetimeOwner) && TestWorld != nullptr))
    { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    auto MarkerEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    auto SensorEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    if (NOT TestTrue(TEXT("fixture creates distinct overlap-body entities"),
        ck::IsValid(MarkerEntity) && ck::IsValid(SensorEntity) && MarkerEntity != SensorEntity))
    { return false; }

    MarkerEntity.Add<ck::FFragment_Marker_Params>();
    MarkerEntity.Add<ck::FFragment_Marker_Current>(ECk_EnableDisable::Enable);
    SensorEntity.Add<ck::FFragment_Sensor_Params>();
    SensorEntity.Add<ck::FFragment_Sensor_Current>(ECk_EnableDisable::Disable);

    auto Inspector = FCkInspector_OverlapBody{};
    const TSharedRef<SWidget> RenderedMarker = Inspector.Build_Inspector(MarkerEntity);
    const TSharedRef<SWidget> RenderedSensor = Inspector.Build_Inspector(SensorEntity);
    if (NOT TestEqual(TEXT("Marker build returns authored Overlap Body widget"),
            RenderedMarker->GetTypeAsString(), FString{TEXT("SCkInspector_OverlapBodyAuthored")})
        || NOT TestEqual(TEXT("Sensor build returns authored Overlap Body widget"),
            RenderedSensor->GetTypeAsString(), FString{TEXT("SCkInspector_OverlapBodyAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }

    const TSharedRef<SCkInspector_OverlapBodyAuthored> MarkerAuthored =
        StaticCastSharedRef<SCkInspector_OverlapBodyAuthored>(RenderedMarker);
    const TSharedRef<SCkInspector_OverlapBodyAuthored> SensorAuthored =
        StaticCastSharedRef<SCkInspector_OverlapBodyAuthored>(RenderedSensor);
    TSharedPtr<FCkUiView> MarkerView = MarkerAuthored->Get_View();
    TSharedPtr<FCkUiView> SensorView = SensorAuthored->Get_View();
    if (NOT TestTrue(TEXT("each production build owns an independent accepted authored view"),
        MarkerAuthored->Is_Mounted() && SensorAuthored->Is_Mounted()
            && MarkerView.IsValid() && SensorView.IsValid() && MarkerView != SensorView
            && &MarkerView->GetRegion(TEXT("main")).Get() != &SensorView->GetRegion(TEXT("main")).Get()
            && MarkerView->GetLastResult().Succeeded && SensorView->GetLastResult().Succeeded))
    { return false; }

    TestTrue(TEXT("optional authored sections and live state remain entity-local"),
        MarkerAuthored->Get_HasMarker() && NOT MarkerAuthored->Get_HasSensor()
            && MarkerAuthored->Get_MarkerEnabled() && MarkerAuthored->Get_MarkerStateText().Contains(TEXT("Enable"))
            && MarkerAuthored->Get_MarkerShapeText() == TEXT("None")
            && NOT SensorAuthored->Get_HasMarker() && SensorAuthored->Get_HasSensor()
            && NOT SensorAuthored->Get_SensorEnabled() && SensorAuthored->Get_SensorStateText().Contains(TEXT("Disable"))
            && SensorAuthored->Get_SensorShapeText() == TEXT("None")
            && SensorAuthored->Get_MarkerOverlapCountText() == TEXT("0")
            && SensorAuthored->Get_NonMarkerOverlapCountText() == TEXT("0"));
    TestTrue(TEXT("missing shapes retain error-tone ownership"),
        MarkerAuthored->Get_MarkerShapeForeground() == SensorAuthored->Get_SensorShapeForeground()
            && MarkerAuthored->Get_MarkerStateForeground() != SensorAuthored->Get_SensorStateForeground());

    const TSharedRef<SCkInspector_OverlapBodyAuthored> Invalid =
        SNew(SCkInspector_OverlapBodyAuthored).Entity(FCk_Handle{});
    TestTrue(TEXT("default-invalid entity mounts inert empty authored sections"),
        Invalid->Is_Mounted() && NOT Invalid->Get_HasMarker() && NOT Invalid->Get_HasSensor()
            && NOT Invalid->Get_MarkerCanToggle() && NOT Invalid->Get_SensorCanToggle());

    auto MarkerRows = TMap<FString, FString>{};
    auto SensorRows = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(MarkerEntity); MarkerRows = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(SensorEntity); SensorRows = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({MarkerRows, SensorRows});
    TestTrue(TEXT("native capture remains the optional-section and multi-selection authority"),
        MarkerRows.Num() == 3 && SensorRows.Num() == 5
            && MarkerRows.FindRef(TEXT("State:")).Contains(TEXT("Enable"))
            && SensorRows.FindRef(TEXT("State:")).Contains(TEXT("Disable"))
            && SensorRows.FindRef(TEXT("Marker Overlaps:")) == TEXT("0")
            && SensorRows.FindRef(TEXT("Non-Marker Overlaps:")) == TEXT("0")
            && Differing.Contains(TEXT("Enabled:")) && Differing.Contains(TEXT("State:"))
            && Differing.Contains(TEXT("Marker Overlaps:")) && Differing.Contains(TEXT("Non-Marker Overlaps:")));

    TSharedPtr<SCkInspector_OverlapBodyAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_OverlapBodyAuthored>(Inspector.Build_Inspector(SensorEntity));
    }
    TestTrue(TEXT("authored Overlap Body labels receive the exact native diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_EnabledDiffMarked() == Differing.Contains(TEXT("Enabled:"))
        && DiffAuthored->Is_StateDiffMarked() == Differing.Contains(TEXT("State:"))
        && DiffAuthored->Is_ShapeDiffMarked() == Differing.Contains(TEXT("Shape:"))
        && DiffAuthored->Is_MarkerOverlapsDiffMarked() == Differing.Contains(TEXT("Marker Overlaps:"))
        && DiffAuthored->Is_NonMarkerOverlapsDiffMarked() == Differing.Contains(TEXT("Non-Marker Overlaps:")));

    const TSharedPtr<SCkDebug_Switch> MarkerSwitch =
        FindSwitch(RenderedMarker, TEXT("overlap-marker-enabled-switch"));
    if (NOT TestTrue(TEXT("authored Marker enable control is a physical debugger switch"), MarkerSwitch.IsValid()))
    { return false; }
    MarkerSwitch->SlatePrepass();
    TestTrue(TEXT("automation world enables the physical LocalOk Marker request path"),
        MarkerAuthored->Get_MarkerCanToggle() && MarkerSwitch->IsEnabled()
            && MarkerAuthored->Get_MarkerDisabledReason().IsEmpty());
    MarkerSwitch->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    const auto* MarkerRequests = MarkerEntity.Has<ck::FFragment_Marker_Requests>()
        ? &MarkerEntity.Get<ck::FFragment_Marker_Requests>() : nullptr;
    const auto* MarkerRequest = MarkerRequests != nullptr
        && MarkerRequests->Get_EnableDisableRequest().IsSet()
        ? &MarkerRequests->Get_EnableDisableRequest().GetValue() : nullptr;
    if (TestNotNull(TEXT("physical authored switch queues the exact Marker request"), MarkerRequest))
    {
        TestTrue(TEXT("physical authored switch requests Disable"),
            MarkerRequest->Get_EnableDisable() == ECk_EnableDisable::Disable);
    }
    MarkerEntity.Try_Remove<ck::FFragment_Marker_Requests>();

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Stylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Overlap Body resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorOverlapBody.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorOverlapBody.ui.css")))))
    { return false; }
    TestTrue(TEXT("HTML/CSS owns both sections, all eight physical rows, statuses, counts, and switches"),
        Markup.Contains(TEXT(">Marker</text>")) && Markup.Contains(TEXT(">Sensor</text>"))
            && Markup.Contains(TEXT("overlap-marker-enabled-switch"))
            && Markup.Contains(TEXT("overlap-sensor-enabled-switch"))
            && Markup.Contains(TEXT(">Marker Overlaps:</text>"))
            && Markup.Contains(TEXT(">Non-Marker Overlaps:</text>"))
            && Stylesheet.Contains(TEXT("overlap-section")) && Stylesheet.Contains(TEXT("overlap-status")));

    const int64 MarkerRevisionBefore = MarkerView->GetRevision();
    const int64 SensorRevisionBefore = SensorView->GetRevision();
    TestTrue(TEXT("compatible Overlap Body reload is accepted by Marker view"),
        MarkerView->TryReload(Markup, Stylesheet, TEXT("Overlap Marker compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible reload retains physical switch identity without mutating Sensor view"),
        MarkerView->GetRevision() > MarkerRevisionBefore && SensorView->GetRevision() == SensorRevisionBefore
            && FindSwitch(RenderedMarker, TEXT("overlap-marker-enabled-switch")) == MarkerSwitch);
    const TSharedRef<SWidget> SensorMainBefore = SensorView->GetRegion(TEXT("main"));
    const int64 SensorRevisionBeforeRejected = SensorView->GetRevision();
    TestFalse(TEXT("missing switch event binding rejects a candidate atomically"), SensorView->TryReload(
        Markup.Replace(TEXT("changed=\"overlap-sensor-enabled-changed\""),
            TEXT("changed=\"overlap-sensor-missing-changed\"")),
        Stylesheet, TEXT("Overlap Sensor rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected reload retains Sensor tree and revision"),
        &SensorView->GetRegion(TEXT("main")).Get() == &SensorMainBefore.Get()
            && SensorView->GetRevision() == SensorRevisionBeforeRejected);

    MarkerEntity.Try_Remove<ck::FFragment_Marker_Params>();
    MarkerSwitch->SlatePrepass();
    TestTrue(TEXT("partial Marker composition leaves display safe and disables the held switch"),
        MarkerAuthored->Get_HasMarker() && NOT MarkerAuthored->Get_MarkerCanToggle()
            && NOT MarkerSwitch->IsEnabled());
    MarkerAuthored->Set_MarkerEnabled(false);
    TestFalse(TEXT("held switch cannot publish after required Marker composition is removed"),
        MarkerEntity.Has<ck::FFragment_Marker_Requests>());
    MarkerEntity.Try_Remove<ck::FFragment_Marker_Current>();
    TestFalse(TEXT("complete Marker removal collapses its authored section"), MarkerAuthored->Get_HasMarker());

    TWeakPtr<SCkInspector_OverlapBodyAuthored> DestructorAuthored;
    TWeakPtr<FCkUiView> DestructorView;
    {
        const TSharedRef<SWidget> DestructorRendered = Inspector.Build_Inspector(SensorEntity);
        const TSharedRef<SCkInspector_OverlapBodyAuthored> DestructorStrong =
            StaticCastSharedRef<SCkInspector_OverlapBodyAuthored>(DestructorRendered);
        DestructorAuthored = DestructorStrong;
        DestructorView = DestructorStrong->Get_View();
    }
    TestFalse(TEXT("destroyed rendered subtree releases its widget and authored view"),
        DestructorAuthored.IsValid() || DestructorView.IsValid());

    const TWeakPtr<FCkUiView> ReleasedMarkerView = MarkerView;
    const TWeakPtr<FCkUiView> ReleasedSensorView = SensorView;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes all retained Overlap Body views inert"),
        MarkerAuthored->Is_Inert() && SensorAuthored->Is_Inert()
            && NOT MarkerAuthored->Is_Mounted() && NOT SensorAuthored->Is_Mounted());
    MarkerView.Reset();
    SensorView.Reset();
    TestFalse(TEXT("deactivation releases every per-build Overlap Body view"),
        ReleasedMarkerView.IsValid() || ReleasedSensorView.IsValid());
    return true;
}

#endif
