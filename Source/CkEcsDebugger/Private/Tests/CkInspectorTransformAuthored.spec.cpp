#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Transform.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Processor.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"

namespace ck_inspector_transform_authored_test
{
    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto FindTaggedButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if ((InRoot->GetTypeAsString() == TEXT("SButton") || InRoot->GetTypeAsString() == TEXT("SCkUiStyledButton"))
            && InRoot->GetTag() == InTag) { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindTaggedButton(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto CreateTransform(const FCk_Handle& InOwner, const FVector& InLocation, const bool bInterpolation) -> FCk_Handle
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
        if (ck::Is_NOT_Valid(Entity)) { return {}; }
        UCk_Utils_Transform_UE::Add(Entity,
            FTransform{FRotator{10.0f, 20.0f, 30.0f}, InLocation, FVector{2.0f, 3.0f, 4.0f}},
            ECk_Replication::DoesNotReplicate);
        if (bInterpolation)
        { UCk_Utils_TransformInterpolation_UE::Add(Entity, FCk_Transform_Interpolation_Settings{}); }
        return Entity;
    }

    auto PumpTransformRequests(ck::FEcsWorld& InWorld) -> void
    { ck::FProcessor_Transform_HandleRequests{InWorld.Get_Registry()}.Pump(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorTransformAuthored,
    "Ck.UiAuthoring.EcsDebugger.TransformInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorTransformAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_transform_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = GetMutableDefault<UCkDebuggerStyleSettings>();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), StyleSettings)) { return false; }
    const ECkDebugAxis_EditControlStyle PreviousStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    if (NOT TestTrue(TEXT("fixture has transient ownership and an automation world"),
        ck::IsValid(LifetimeOwner) && GWorld != nullptr)) { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(GWorld);
    FCk_Handle EntityA = CreateTransform(LifetimeOwner, FVector{125.0f, -75.0f, 20.0f}, false);
    FCk_Handle EntityB = CreateTransform(LifetimeOwner, FVector{-8.0f, 444.0f, 96.0f}, true);
    if (NOT TestTrue(TEXT("fixture composes distinct Transform states and an optional interpolation sibling"),
        ck::IsValid(EntityA) && ck::IsValid(EntityB) && EntityA != EntityB
            && UCk_Utils_TransformInterpolation_UE::Has(EntityB))) { return false; }

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    auto Inspector = FCkInspector_Transform{};
    Inspector.Set_EditGuard(EditGuard);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("first build mounts Transform's authored shell"), RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_TransformAuthored")})
        || NOT TestEqual(TEXT("second build mounts an independent Transform authored shell"), RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_TransformAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_TransformAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_TransformAuthored>(RenderedA);
    const TSharedRef<SCkInspector_TransformAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_TransformAuthored>(RenderedB);
    const TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    const TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TestTrue(TEXT("independent views project live values and optional structure"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB
            && AuthoredA->Get_Location().Equals(FVector{125.0f, -75.0f, 20.0f})
            && AuthoredB->Get_Location().Equals(FVector{-8.0f, 444.0f, 96.0f})
            && AuthoredA->Get_Rotation().Equals(FRotator{10.0f, 20.0f, 30.0f})
            && AuthoredA->Get_Scale().Equals(FVector{2.0f, 3.0f, 4.0f})
            && NOT AuthoredA->Get_HasInterpolation() && AuthoredB->Get_HasInterpolation());

    ViewA->GetRegion(TEXT("main"))->SlatePrepass();
    ViewB->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("markup mounts individual canonical leaves and authored actions"),
        FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("transform-space-input")).IsValid()
            && FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("transform-set-location-x-input")).IsValid()
            && FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("transform-set-rotation-y-input")).IsValid()
            && FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("transform-set-scale-z-input")).IsValid()
            && FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("transform-location-offset-x-input")).IsValid()
            && FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("transform-rotation-offset-z-input")).IsValid()
            && FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("transform-add-location")).IsValid()
            && FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("transform-force-refresh")).IsValid());

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native capture remains complete fallback and diff authority"),
        RowsA.Num() == 11 && RowsB.Num() == 13
            && RowsA.Contains(TEXT("Location:")) && RowsA.Contains(TEXT("Apply:"))
            && NOT RowsA.Contains(TEXT("Goal Location Offset:")) && RowsB.Contains(TEXT("Goal Location Offset:"))
            && Differing.Contains(TEXT("Location:")));

    AuthoredA->Commit_EditSpace(1);
    AuthoredA->Commit_SetLocation(FVector{10.0f, 20.0f, 30.0f});
    PumpTransformRequests(World);
    TestTrue(TEXT("set-location commit uses the public request processor"),
        AuthoredA->Get_Location().Equals(FVector{10.0f, 20.0f, 30.0f}));
    AuthoredA->Commit_SetRotation(FRotator{5.0f, 15.0f, 25.0f});
    PumpTransformRequests(World);
    TestTrue(TEXT("set-rotation commit preserves R/P/Y ordering"),
        AuthoredA->Get_Rotation().Equals(FRotator{5.0f, 15.0f, 25.0f}));
    AuthoredA->Commit_SetScale(FVector{6.0f, 7.0f, 8.0f});
    PumpTransformRequests(World);
    TestTrue(TEXT("set-scale commit uses the public request processor"),
        AuthoredA->Get_Scale().Equals(FVector{6.0f, 7.0f, 8.0f}));

    const TSharedPtr<SButton> AddLocationButton = FindTaggedButton(
        ViewA->GetRegion(TEXT("main")), TEXT("transform-add-location"));
    if (NOT TestTrue(TEXT("physical authored Add Location action is mounted and enabled"),
        AddLocationButton.IsValid() && AddLocationButton->IsEnabled())) { return false; }
    AuthoredA->Commit_LocationOffset(FVector{1.0f, 2.0f, 3.0f});
    AuthoredB->Commit_LocationOffset(FVector{100.0f, 200.0f, 300.0f});
    AddLocationButton->SimulateClick();
    TestTrue(TEXT("physical authored action dispatches exactly one location request"),
        EntityA.Has<ck::FFragment_Transform_Requests>()
            && EntityA.Get<ck::FFragment_Transform_Requests>().Get_LocationRequests().Num() == 1);
    PumpTransformRequests(World);
    TestTrue(TEXT("pending offsets are isolated per authored view"),
        AuthoredA->Get_Location().Equals(FVector{11.0f, 22.0f, 33.0f})
            && AuthoredB->Get_Location().Equals(FVector{-8.0f, 444.0f, 96.0f}));
    AuthoredA->Commit_RotationOffset(FRotator{1.0f, 2.0f, 3.0f});
    AuthoredA->Request_AddRotation();
    AuthoredA->Request_ForceRefresh();
    TestTrue(TEXT("authored Apply actions enqueue rotation and refresh through typed requests"),
        EntityA.Has<ck::FFragment_Transform_Requests>()
            && EntityA.Get<ck::FFragment_Transform_Requests>().Get_RotationRequests().Num() == 1
            && EntityA.Get<ck::FFragment_Transform_Requests>().Get_ForceRefreshRequests().Num() == 1);
    PumpTransformRequests(World);

    AuthoredB->Commit_InterpolationLocation(FVector{9.0f, 8.0f, 7.0f});
    AuthoredB->Commit_InterpolationRotation(FRotator{6.0f, 5.0f, 4.0f});
    TestTrue(TEXT("interpolation editors publish both live goal fragments"),
        EntityB.Has<ck::FFragment_TransformInterpolation_NewGoal_Location>()
            && EntityB.Has<ck::FFragment_TransformInterpolation_NewGoal_Rotation>());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString Root = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("Transform authored resources are installed"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Root, TEXT("EcsInspectorTransform.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Root, TEXT("EcsInspectorTransform.ui.css"))))) { return false; }
    TestTrue(TEXT("HTML owns every label, section, row and action rather than a whole-panel native wrapper"),
        Markup.Contains(TEXT(">Location:</text>")) && Markup.Contains(TEXT(">Set Location:</text>"))
            && Markup.Contains(TEXT(">Rotation Offset (R,P,Y):</text>"))
            && Markup.Contains(TEXT(">Goal Rotation Offset (R,P,Y):</text>"))
            && Markup.Contains(TEXT("action=\"transform-add-location\""))
            && Markup.Contains(TEXT("transform-orientation-port"))
            && NOT Markup.Contains(TEXT("transform-native-controls-port")));

    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload remains local to its retained view"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Transform compatible candidate")).Succeeded);
    TestTrue(TEXT("missing native binding reload is atomically rejected"), NOT ViewB->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"broken\" bind=\"missing-transform-port\" /></region></ui>"),
        TEXT(""), TEXT("Transform rejected candidate")).Succeeded && ViewB->GetRevision() == RevisionBBefore);

    TestTrue(TEXT("fragment removal succeeds"), EntityA.Try_Remove<ck::FFragment_Transform>());
    TestTrue(TEXT("mounted controls fail closed after composition loss"),
        NOT AuthoredA->Get_IsAvailable() && NOT AuthoredA->Get_CanRequest()
            && AuthoredA->Get_AxisText(TEXT("location"), 0) == TEXT("--"));
    AuthoredA->Commit_SetLocation(FVector{999.0f});
    AuthoredA->Request_AddLocation();
    TestFalse(TEXT("stale controls do not recreate request state"), EntityA.Has<ck::FFragment_Transform_Requests>());

    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases every retained authored view and edit scope"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted()
            && NOT EditGuard->Get_HasActiveEdit());
    return true;
}

#endif
