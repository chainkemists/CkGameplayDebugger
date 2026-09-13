#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Shapes.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkShapes/Box/CkShapeBox_Processor.h"
#include "CkShapes/Box/CkShapeBox_Utils.h"
#include "CkShapes/Capsule/CkShapeCapsule_Processor.h"
#include "CkShapes/Capsule/CkShapeCapsule_Utils.h"
#include "CkShapes/Cylinder/CkShapeCylinder_Processor.h"
#include "CkShapes/Cylinder/CkShapeCylinder_Utils.h"
#include "CkShapes/Sphere/CkShapeSphere_Processor.h"
#include "CkShapes/Sphere/CkShapeSphere_Utils.h"
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
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

namespace ck_inspector_shapes_authored_test
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

    auto FindEditor(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SEditableTextBox"))
        { return StaticCastSharedRef<SEditableTextBox>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableTextBox> Found = FindEditor(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto SetAndCommit(FSlateApplication& InSlate, const TSharedRef<SEditableTextBox>& InInput,
        const FString& InText) -> bool
    {
        InSlate.SetUserFocus(0, InInput, EFocusCause::SetDirectly);
        Tick(InSlate);
        InInput->SetText(FText::FromString(InText));
        if (NOT InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0}))
        { return false; }
        Tick(InSlate);
        return true;
    }

    auto PumpShapeRequests(ck::FEcsWorld& InWorld) -> void
    {
        ck::FProcessor_ShapeSphere_HandleRequests{InWorld.Get_Registry()}.Pump();
        ck::FProcessor_ShapeBox_HandleRequests{InWorld.Get_Registry()}.Pump();
        ck::FProcessor_ShapeCapsule_HandleRequests{InWorld.Get_Registry()}.Pump();
        ck::FProcessor_ShapeCylinder_HandleRequests{InWorld.Get_Registry()}.Pump();
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope() { if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); } }
        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorShapesAuthored,
    "Ck.UiAuthoring.EcsDebugger.ShapesInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorShapesAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_shapes_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = GetMutableDefault<UCkDebuggerStyleSettings>();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), StyleSettings)) { return false; }
    const ECkDebugAxis_EditControlStyle PreviousEditControlStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousEditControlStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    UWorld* const TestWorld = GWorld;
    if (NOT TestTrue(TEXT("fixture has a transient owner and automation world"),
        ck::IsValid(LifetimeOwner) && TestWorld != nullptr)) { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(TestWorld);

    auto SphereEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    auto BoxEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    auto CapsuleEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    auto CylinderEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    auto BoxDimensions = FCk_ShapeBox_Dimensions{FVector{10.0f, 20.0f, 30.0f}};
    BoxDimensions.Set_ConvexRadius(4.0f);
    auto CylinderDimensions = FCk_ShapeCylinder_Dimensions{80.0f, 40.0f};
    CylinderDimensions.Set_ConvexRadius(6.0f);
    const auto Sphere = UCk_Utils_ShapeSphere_UE::Add(SphereEntity, FCk_Fragment_ShapeSphere_ParamsData{
        FCk_ShapeSphere_Dimensions{25.0f}});
    const auto Box = UCk_Utils_ShapeBox_UE::Add(BoxEntity, FCk_Fragment_ShapeBox_ParamsData{BoxDimensions});
    const auto Capsule = UCk_Utils_ShapeCapsule_UE::Add(CapsuleEntity, FCk_Fragment_ShapeCapsule_ParamsData{
        FCk_ShapeCapsule_Dimensions{70.0f, 35.0f}});
    const auto Cylinder = UCk_Utils_ShapeCylinder_UE::Add(CylinderEntity, FCk_Fragment_ShapeCylinder_ParamsData{CylinderDimensions});
    if (NOT TestTrue(TEXT("fixture creates four distinct public-API shape features"),
        ck::IsValid(Sphere) && ck::IsValid(Box) && ck::IsValid(Capsule) && ck::IsValid(Cylinder)
            && SphereEntity != BoxEntity && BoxEntity != CapsuleEntity && CapsuleEntity != CylinderEntity)) { return false; }

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    auto Inspector = FCkInspector_Shapes{};
    Inspector.Set_EditGuard(EditGuard);
    const TSharedRef<SCkInspector_ShapesAuthored> SphereAuthored =
        StaticCastSharedRef<SCkInspector_ShapesAuthored>(Inspector.Build_Inspector(SphereEntity));
    const TSharedRef<SCkInspector_ShapesAuthored> BoxAuthored =
        StaticCastSharedRef<SCkInspector_ShapesAuthored>(Inspector.Build_Inspector(BoxEntity));
    const TSharedRef<SCkInspector_ShapesAuthored> CapsuleAuthored =
        StaticCastSharedRef<SCkInspector_ShapesAuthored>(Inspector.Build_Inspector(CapsuleEntity));
    const TSharedRef<SCkInspector_ShapesAuthored> CylinderAuthored =
        StaticCastSharedRef<SCkInspector_ShapesAuthored>(Inspector.Build_Inspector(CylinderEntity));
    if (NOT TestTrue(TEXT("all four single-feature inspector builds mount authored Shapes views"),
        SphereAuthored->Is_Mounted() && BoxAuthored->Is_Mounted() && CapsuleAuthored->Is_Mounted()
            && CylinderAuthored->Is_Mounted() && SphereAuthored->Get_View().IsValid()
            && BoxAuthored->Get_View().IsValid() && CapsuleAuthored->Get_View().IsValid()
            && CylinderAuthored->Get_View().IsValid()))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    TestTrue(TEXT("each view exposes only its own authored section and exact initial dimensions"),
        SphereAuthored->Get_HasSphere() && NOT SphereAuthored->Get_HasBox() && FMath::IsNearlyEqual(SphereAuthored->Get_SphereRadius(), 25.0f)
            && BoxAuthored->Get_HasBox() && BoxAuthored->Get_BoxHalfExtents().Equals(FVector{10.0f, 20.0f, 30.0f})
            && FMath::IsNearlyEqual(BoxAuthored->Get_BoxConvexRadius(), 4.0f)
            && CapsuleAuthored->Get_HasCapsule() && FMath::IsNearlyEqual(CapsuleAuthored->Get_CapsuleHalfHeight(), 70.0f)
            && FMath::IsNearlyEqual(CapsuleAuthored->Get_CapsuleRadius(), 35.0f)
            && CylinderAuthored->Get_HasCylinder() && FMath::IsNearlyEqual(CylinderAuthored->Get_CylinderHalfHeight(), 80.0f)
            && FMath::IsNearlyEqual(CylinderAuthored->Get_CylinderRadius(), 40.0f)
            && FMath::IsNearlyEqual(CylinderAuthored->Get_CylinderConvexRadius(), 6.0f));
    SphereAuthored->Get_View()->GetRegion(TEXT("main"))->SlatePrepass();
    BoxAuthored->Get_View()->GetRegion(TEXT("main"))->SlatePrepass();
    const TSharedPtr<SWidget> SphereHeading = FindTaggedWidget(
        SphereAuthored->Get_View()->GetRegion(TEXT("main")), TEXT("shapes-sphere-heading"));
    const TSharedPtr<SWidget> SphereBoxSection = FindTaggedWidget(
        SphereAuthored->Get_View()->GetRegion(TEXT("main")), TEXT("shapes-box-section"));
    const TSharedPtr<SWidget> BoxSection = FindTaggedWidget(
        BoxAuthored->Get_View()->GetRegion(TEXT("main")), TEXT("shapes-box-section"));
    TestTrue(TEXT("authored section visibility follows each complete shape composition"),
        SphereHeading.IsValid() && SphereHeading->GetVisibility() == EVisibility::Visible
            && SphereBoxSection.IsValid() && SphereBoxSection->GetVisibility() == EVisibility::Collapsed
            && BoxSection.IsValid() && BoxSection->GetVisibility() == EVisibility::Visible);

    auto SphereRows = TMap<FString, FString>{};
    auto BoxRows = TMap<FString, FString>{};
    auto CapsuleRows = TMap<FString, FString>{};
    auto CylinderRows = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(SphereEntity); SphereRows = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(BoxEntity); BoxRows = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(CapsuleEntity); CapsuleRows = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(CylinderEntity); CylinderRows = Capture.Get_Rows(); }
    TestTrue(TEXT("native capture preserves all eight authored shape rows"),
        SphereRows.Num() == 1 && BoxRows.Num() == 2 && CapsuleRows.Num() == 2 && CylinderRows.Num() == 3
            && SphereRows.Contains(TEXT("Radius:")) && BoxRows.Contains(TEXT("Half Extents:"))
            && BoxRows.Contains(TEXT("Convex Radius:")) && CapsuleRows.Contains(TEXT("Half Height:"))
            && CapsuleRows.Contains(TEXT("Radius:")) && CylinderRows.Contains(TEXT("Half Height:"))
            && CylinderRows.Contains(TEXT("Radius:")) && CylinderRows.Contains(TEXT("Convex Radius:")));

    const TSharedPtr<SWidget> SphereInput = FindTaggedWidget(SphereAuthored, TEXT("shapes-sphere-radius-input"));
    const TSharedPtr<SWidget> BoxXInput = FindTaggedWidget(BoxAuthored, TEXT("shapes-box-half-extents-x-input"));
    const TSharedPtr<SWidget> BoxYInput = FindTaggedWidget(BoxAuthored, TEXT("shapes-box-half-extents-y-input"));
    const TSharedPtr<SWidget> BoxZInput = FindTaggedWidget(BoxAuthored, TEXT("shapes-box-half-extents-z-input"));
    const TSharedPtr<SWidget> BoxConvexInput = FindTaggedWidget(BoxAuthored, TEXT("shapes-box-convex-radius-input"));
    const TSharedPtr<SWidget> CapsuleHalfHeightInput = FindTaggedWidget(
        CapsuleAuthored, TEXT("shapes-capsule-half-height-input"));
    const TSharedPtr<SWidget> CapsuleRadiusInput = FindTaggedWidget(
        CapsuleAuthored, TEXT("shapes-capsule-radius-input"));
    const TSharedPtr<SWidget> CylinderHalfHeightInput = FindTaggedWidget(
        CylinderAuthored, TEXT("shapes-cylinder-half-height-input"));
    const TSharedPtr<SWidget> CylinderRadiusInput = FindTaggedWidget(
        CylinderAuthored, TEXT("shapes-cylinder-radius-input"));
    const TSharedPtr<SWidget> CylinderConvexInput = FindTaggedWidget(
        CylinderAuthored, TEXT("shapes-cylinder-convex-radius-input"));
    const TSharedPtr<SEditableTextBox> SphereText = SphereInput.IsValid() ? FindEditor(SphereInput.ToSharedRef()) : nullptr;
    const TSharedPtr<SEditableTextBox> BoxXText = BoxXInput.IsValid() ? FindEditor(BoxXInput.ToSharedRef()) : nullptr;
    const auto FindInputEditor = [](const TSharedPtr<SWidget>& InInput)
    { return InInput.IsValid() ? FindEditor(InInput.ToSharedRef()) : TSharedPtr<SEditableTextBox>{}; };
    const TSharedPtr<SEditableTextBox> BoxYText = FindInputEditor(BoxYInput);
    const TSharedPtr<SEditableTextBox> BoxZText = FindInputEditor(BoxZInput);
    const TSharedPtr<SEditableTextBox> BoxConvexText = FindInputEditor(BoxConvexInput);
    const TSharedPtr<SEditableTextBox> CapsuleHalfHeightText = FindInputEditor(CapsuleHalfHeightInput);
    const TSharedPtr<SEditableTextBox> CapsuleRadiusText = FindInputEditor(CapsuleRadiusInput);
    const TSharedPtr<SEditableTextBox> CylinderHalfHeightText = FindInputEditor(CylinderHalfHeightInput);
    const TSharedPtr<SEditableTextBox> CylinderRadiusText = FindInputEditor(CylinderRadiusInput);
    const TSharedPtr<SEditableTextBox> CylinderConvexText = FindInputEditor(CylinderConvexInput);
    if (NOT TestTrue(TEXT("all eight authored rows expose their canonical physical numeric leaves"),
        SphereText.IsValid() && BoxXText.IsValid() && BoxYText.IsValid() && BoxZText.IsValid()
            && BoxConvexText.IsValid() && CapsuleHalfHeightText.IsValid() && CapsuleRadiusText.IsValid()
            && CylinderHalfHeightText.IsValid() && CylinderRadiusText.IsValid() && CylinderConvexText.IsValid()))
    { return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{520.0f, 300.0f})
        .CreateTitleBar(false).HasCloseButton(false)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SphereAuthored]
            + SVerticalBox::Slot().AutoHeight()[BoxAuthored]
            + SVerticalBox::Slot().AutoHeight()[CapsuleAuthored]
            + SVerticalBox::Slot().AutoHeight()[CylinderAuthored]];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    Tick(Slate);
    SphereInput->SlatePrepass();
    BoxXInput->SlatePrepass();
    if (NOT TestTrue(TEXT("world-bound LocalOk enables physical authored shape controls"),
        SphereInput->IsEnabled() && BoxXInput->IsEnabled() && SphereAuthored->Get_CanRequest())) { return false; }
    const auto CommitAndPump = [&Slate, &World](const TSharedPtr<SEditableTextBox>& InEditor, const TCHAR* InValue)
    {
        const bool bCommitted = SetAndCommit(Slate, InEditor.ToSharedRef(), InValue);
        PumpShapeRequests(World);
        return bCommitted;
    };
    if (NOT TestTrue(TEXT("every authored shape field accepts a physical Enter commit"),
        CommitAndPump(SphereText, TEXT("-5"))
            && CommitAndPump(BoxXText, TEXT("55"))
            && CommitAndPump(BoxYText, TEXT("65"))
            && CommitAndPump(BoxZText, TEXT("-75"))
            && CommitAndPump(BoxConvexText, TEXT("8"))
            && CommitAndPump(CapsuleHalfHeightText, TEXT("75"))
            && CommitAndPump(CapsuleRadiusText, TEXT("45"))
            && CommitAndPump(CylinderHalfHeightText, TEXT("90"))
            && CommitAndPump(CylinderRadiusText, TEXT("50"))
            && CommitAndPump(CylinderConvexText, TEXT("7")))) { return false; }
    TestFalse(TEXT("physical commits close all inspector edit scopes"), EditGuard->Get_HasActiveEdit());
    TestTrue(TEXT("request processors apply clamped values and preserve shape siblings"),
        FMath::IsNearlyZero(UCk_Utils_ShapeSphere_UE::Get_Dimensions(Sphere).Get_Radius())
            && UCk_Utils_ShapeBox_UE::Get_Dimensions(Box).Get_HalfExtents().Equals(FVector{55.0f, 65.0f, -75.0f})
            && FMath::IsNearlyEqual(UCk_Utils_ShapeBox_UE::Get_Dimensions(Box).Get_ConvexRadius(), 8.0f)
            && FMath::IsNearlyEqual(UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Capsule).Get_HalfHeight(), 75.0f)
            && FMath::IsNearlyEqual(UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Capsule).Get_Radius(), 45.0f)
            && FMath::IsNearlyEqual(UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_HalfHeight(), 90.0f)
            && FMath::IsNearlyEqual(UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_Radius(), 50.0f)
            && FMath::IsNearlyEqual(UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_ConvexRadius(), 7.0f));

    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({CapsuleRows, CylinderRows});
    TSharedPtr<SCkInspector_ShapesAuthored> DiffAuthored;
    { const FCkInspector_DiffMarkScope DiffScope{&Differing}; DiffAuthored = StaticCastSharedRef<SCkInspector_ShapesAuthored>(Inspector.Build_Inspector(CylinderEntity)); }
    TestTrue(TEXT("authored labels receive the exact native diff marks"),
        DiffAuthored.IsValid() && DiffAuthored->Is_DiffMarked(TEXT("Half Height:")) == Differing.Contains(TEXT("Half Height:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Radius:")) == Differing.Contains(TEXT("Radius:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Convex Radius:")) == Differing.Contains(TEXT("Convex Radius:")));

    FString Markup, Stylesheet;
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Shapes resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorShapes.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorShapes.ui.css"))))) { return false; }
    TestTrue(TEXT("HTML owns four conditional sections, eight labels, and unique native ports"),
        Markup.Contains(TEXT("shapes-sphere-visible")) && Markup.Contains(TEXT("shapes-box-visible"))
            && Markup.Contains(TEXT("shapes-capsule-visible")) && Markup.Contains(TEXT("shapes-cylinder-visible"))
            && Markup.Contains(TEXT("shapes-sphere-radius-port")) && Markup.Contains(TEXT("shapes-box-half-extents-port"))
            && Markup.Contains(TEXT("shapes-cylinder-convex-radius-port")) && Stylesheet.Contains(TEXT(".shapes-section")));
    const int64 Revision = SphereAuthored->Get_View()->GetRevision();
    TestTrue(TEXT("compatible Shapes reload is accepted"), SphereAuthored->Get_View()->TryReload(Markup, Stylesheet, TEXT("Shapes compatible candidate")).Succeeded);
    const TSharedRef<SWidget> MainBefore = BoxAuthored->Get_View()->GetRegion(TEXT("main"));
    const int64 RejectedRevision = BoxAuthored->Get_View()->GetRevision();
    TestFalse(TEXT("missing Shapes port is rejected atomically"), BoxAuthored->Get_View()->TryReload(
        Markup.Replace(TEXT("shapes-box-half-extents-port"), TEXT("shapes-missing-port")), Stylesheet,
        TEXT("Shapes rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected Shapes reload preserves the existing tree"), SphereAuthored->Get_View()->GetRevision() > Revision
        && &BoxAuthored->Get_View()->GetRegion(TEXT("main")).Get() == &MainBefore.Get()
        && BoxAuthored->Get_View()->GetRevision() == RejectedRevision);

    SphereEntity.Try_Remove<ck::FFragment_ShapeSphere_Params>();
    SphereInput->SlatePrepass();
    TestTrue(TEXT("Params-only removal invalidates held Sphere controls"),
        ck::IsValid(SphereEntity) && SphereEntity.Has<ck::FFragment_ShapeSphere_Current>()
            && NOT Inspector.CanInspect(SphereEntity) && NOT SphereAuthored->Get_HasSphere() && NOT SphereInput->IsEnabled());
    SphereAuthored->Commit_SphereRadius(12.0f);
    TestFalse(TEXT("stale Sphere controls cannot enqueue after Params removal"), SphereEntity.Has<ck::FFragment_ShapeSphere_Requests>());
    BoxEntity.Try_Remove<ck::FFragment_ShapeBox_Current>();
    BoxXInput->SlatePrepass();
    TestTrue(TEXT("Current-only removal invalidates held Box controls"),
        ck::IsValid(BoxEntity) && BoxEntity.Has<ck::FFragment_ShapeBox_Params>()
            && NOT Inspector.CanInspect(BoxEntity) && NOT BoxAuthored->Get_HasBox() && NOT BoxXInput->IsEnabled());
    BoxAuthored->Commit_BoxHalfExtents(FVector{1.0f, 2.0f, 3.0f});
    TestFalse(TEXT("stale Box controls cannot enqueue after Current removal"), BoxEntity.Has<ck::FFragment_ShapeBox_Requests>());

    TSharedPtr<SCkInspector_ShapesAuthored> DestructorAuthored;
    TWeakPtr<FCkUiView> DestructorView;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_Shapes>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_ShapesAuthored>(DestructorInspector->Build_Inspector(CylinderEntity));
        DestructorView = DestructorAuthored->Get_View();
    }
    TestTrue(TEXT("inspector destruction releases retained authored ports, scopes, and view"),
        DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted() && NOT DestructorView.IsValid());
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases every held Shapes view and edit scope"),
        SphereAuthored->Is_Inert() && BoxAuthored->Is_Inert() && CapsuleAuthored->Is_Inert()
            && CylinderAuthored->Is_Inert() && DiffAuthored->Is_Inert() && NOT EditGuard->Get_HasActiveEdit());
    CapsuleAuthored->Commit_CapsuleRadius(1.0f);
    TestFalse(TEXT("held authored controls remain inert after deactivation"), CapsuleEntity.Has<ck::FFragment_ShapeCapsule_Requests>());
    return true;
}

#endif
