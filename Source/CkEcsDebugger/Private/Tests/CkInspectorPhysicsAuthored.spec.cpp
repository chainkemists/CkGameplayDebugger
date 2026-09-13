#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Physics.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkPhysics/Acceleration/CkAcceleration_Utils.h"
#include "CkPhysics/EulerIntegrator/CkEulerIntegrator_Fragment.h"
#include "CkPhysics/EulerIntegrator/CkEulerIntegrator_Utils.h"
#include "CkPhysics/PredictedVelocity/CkPredictedVelocity_Fragment.h"
#include "CkPhysics/PredictedVelocity/CkPredictedVelocity_Utils.h"
#include "CkPhysics/Velocity/CkVelocity_Utils.h"
#include "CkSlateLayout/CkUiFloatSeries.h"
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

namespace ck_inspector_physics_authored_test
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
        if ((InRoot->GetTypeAsString() == TEXT("SButton")
                || InRoot->GetTypeAsString() == TEXT("SCkUiStyledButton"))
            && InRoot->GetTag() == InTag) { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindTaggedButton(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto FindDescendantByType(const TSharedRef<SWidget>& InRoot, const FString& InType) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTypeAsString() == InType) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindDescendantByType(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto GetToolTipText(const TSharedRef<SWidget>& InWidget) -> FString
    {
        const TSharedPtr<IToolTip> Tooltip = InWidget->GetToolTip();
        if (NOT Tooltip.IsValid()) { return {}; }
        const TSharedRef<SWidget> Content = Tooltip->GetContentWidget();
        Content->SlatePrepass();
        return Content->GetAccessibleText().ToString();
    }

    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
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
        if (Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f) { return false; }
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
        const FWidgetPath TargetPath = InSlate.LocateWindowUnderMouse(
            Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        if (NOT WidgetPathContains(TargetPath, InWidget)) { return false; }
        const bool bDownHandled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), DownEvent);
        const bool bUpHandled = InSlate.ProcessMouseButtonUpEvent(UpEvent);
        TickSlate(InSlate);
        return bDownHandled && bUpHandled;
    }

    auto SetAndCommit(
        FSlateApplication& InSlate,
        const TSharedRef<SEditableTextBox>& InInput,
        const FString& InText) -> bool
    {
        InSlate.SetUserFocus(0, InInput, EFocusCause::SetDirectly);
        TickSlate(InSlate);
        InInput->SetText(FText::FromString(InText));
        const bool bEnterHandled = InSlate.ProcessKeyDownEvent(
            FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
        TickSlate(InSlate);
        return bEnterHandled;
    }

    auto AddAuthorityAndWorld(FCk_Handle& InEntity) -> void
    {
        InEntity.Add<TWeakObjectPtr<UWorld>>(GWorld);
        UCk_Utils_Net_UE::Add(InEntity, FCk_Net_ConnectionSettings{
            ECk_Replication::DoesNotReplicate,
            ECk_Net_NetModeType::Host,
            ECk_Net_EntityNetRole::Authority});
    }

    auto CreateAllPhysics(const FCk_Handle& InOwner) -> FCk_Handle
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
        if (ck::Is_NOT_Valid(Entity)) { return {}; }
        AddAuthorityAndWorld(Entity);
        UCk_Utils_Velocity_UE::Add(Entity,
            FCk_Fragment_Velocity_ParamsData{ECk_LocalWorld::World, FVector{3.0, 4.0, 12.0}},
            ECk_Replication::DoesNotReplicate);
        UCk_Utils_Acceleration_UE::Add(Entity,
            FCk_Fragment_Acceleration_ParamsData{ECk_LocalWorld::World, FVector{-2.0, 6.0, 1.0}},
            ECk_Replication::DoesNotReplicate);
        UCk_Utils_PredictedVelocity_UE::Add(Entity, FCk_Fragment_PredictedVelocity_ParamsData{});
        UCk_Utils_EulerIntegrator_UE::Request_Start(Entity, {});
        return Entity;
    }

    auto CreateVelocityOnly(const FCk_Handle& InOwner) -> FCk_Handle
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
        if (ck::Is_NOT_Valid(Entity)) { return {}; }
        AddAuthorityAndWorld(Entity);
        UCk_Utils_Velocity_UE::Add(Entity,
            FCk_Fragment_Velocity_ParamsData{ECk_LocalWorld::World, FVector{0.0, 0.0, 5.0}},
            ECk_Replication::DoesNotReplicate);
        return Entity;
    }

    auto CreatePhysicsWithoutWorld(const FCk_Handle& InOwner) -> FCk_Handle
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
        if (ck::Is_NOT_Valid(Entity)) { return {}; }
        UCk_Utils_Net_UE::Add(Entity, FCk_Net_ConnectionSettings{
            ECk_Replication::Replicates,
            ECk_Net_NetModeType::Client,
            ECk_Net_EntityNetRole::Proxy});
        Entity.Add<TWeakObjectPtr<UWorld>>();
        UCk_Utils_Velocity_UE::Add(Entity,
            FCk_Fragment_Velocity_ParamsData{ECk_LocalWorld::World, FVector{1.0, 2.0, 3.0}},
            ECk_Replication::DoesNotReplicate);
        UCk_Utils_Acceleration_UE::Add(Entity,
            FCk_Fragment_Acceleration_ParamsData{ECk_LocalWorld::World, FVector{4.0, 5.0, 6.0}},
            ECk_Replication::DoesNotReplicate);
        return Entity;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorPhysicsAuthored,
    "Ck.UiAuthoring.EcsDebugger.PhysicsInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorPhysicsAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_physics_authored_test;

    if (NOT FSlateApplication::IsInitialized())
    { AddError(TEXT("Physics authored inspector test requires Slate.")); return false; }
    FSlateApplication& Slate = FSlateApplication::Get();

    UCkDebuggerStyleSettings* StyleSettings = GetMutableDefault<UCkDebuggerStyleSettings>();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), StyleSettings)) { return false; }
    const ECkDebugAxis_EditControlStyle PreviousStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto World = ck::FEcsWorld{};
    auto Owner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    auto WorldWithoutWorld = ck::FEcsWorld{};
    auto OwnerWithoutWorld = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(WorldWithoutWorld.Get_Registry());
    if (NOT TestTrue(TEXT("fixture has transient ownership and an automation world"),
        ck::IsValid(Owner) && ck::IsValid(OwnerWithoutWorld) && GWorld != nullptr)) { return false; }
    AddAuthorityAndWorld(Owner);
    auto EntityA = CreateAllPhysics(Owner);
    auto EntityB = CreateVelocityOnly(Owner);
    auto EntityWithoutWorld = CreatePhysicsWithoutWorld(OwnerWithoutWorld);
    if (NOT TestTrue(TEXT("fixture composes all four optional sections and a velocity-only sibling"),
        ck::IsValid(EntityA) && ck::IsValid(EntityB) && ck::IsValid(EntityWithoutWorld)
            && EntityA.Has<ck::FFragment_Velocity_Current>()
            && EntityA.Has<ck::FFragment_Acceleration_Current>()
            && EntityA.Has<ck::FFragment_PredictedVelocity_Current>()
            && EntityA.Has<ck::FFragment_EulerIntegrator_Current>()
            && EntityB.Has<ck::FFragment_Velocity_Current>()
            && NOT EntityB.Has<ck::FFragment_Acceleration_Current>())) { return false; }

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    auto Inspector = FCkInspector_Physics{};
    Inspector.Set_EditGuard(EditGuard);

    TMap<FString, FString> RowsA, RowsB;
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    TestTrue(TEXT("native fallback remains the complete capture authority"),
        RowsA.Num() == 8 && RowsB.Num() == 3
            && RowsA.Contains(TEXT("Current:")) && RowsA.Contains(TEXT("Speed:"))
            && RowsA.Contains(TEXT("Override:")) && RowsA.Contains(TEXT("Velocity:"))
            && RowsA.Contains(TEXT("Prev Location:")) && RowsA.Contains(TEXT("Prev DeltaTime:"))
            && RowsA.Contains(TEXT("Distance Offset:")) && RowsA.Contains(TEXT("Integrator:")));

    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    const TSharedRef<SWidget> RenderedWithoutWorld = Inspector.Build_Inspector(EntityWithoutWorld);
    if (NOT TestEqual(TEXT("all-physics entity mounts an authored view"), RenderedA->GetTypeAsString(),
            FString{TEXT("SCkInspector_PhysicsAuthored")})
        || NOT TestEqual(TEXT("velocity-only entity mounts an independent authored view"), RenderedB->GetTypeAsString(),
            FString{TEXT("SCkInspector_PhysicsAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_PhysicsAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_PhysicsAuthored>(RenderedA);
    const TSharedRef<SCkInspector_PhysicsAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_PhysicsAuthored>(RenderedB);
    const TSharedRef<SCkInspector_PhysicsAuthored> AuthoredWithoutWorld =
        StaticCastSharedRef<SCkInspector_PhysicsAuthored>(RenderedWithoutWorld);
    const TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    const TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    if (NOT TestTrue(TEXT("authored views are independent and project exact optional structure"),
        ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB
            && AuthoredA->Get_HasVelocity() && AuthoredA->Get_HasAcceleration()
            && AuthoredA->Get_HasPredictedVelocity() && AuthoredA->Get_HasEulerIntegrator()
            && AuthoredB->Get_HasVelocity() && NOT AuthoredB->Get_HasAcceleration()
            && NOT AuthoredB->Get_HasPredictedVelocity() && NOT AuthoredB->Get_HasEulerIntegrator())) { return false; }
    TestTrue(TEXT("live physics values and initial speed samples are exact"),
        AuthoredA->Get_AxisText(TEXT("velocity"), 0) == TEXT("3.000")
            && AuthoredA->Get_AxisText(TEXT("acceleration"), 1) == TEXT("6.000")
            && AuthoredA->Get_SpeedText(TEXT("velocity")) == TEXT("13.00")
            && AuthoredB->Get_SpeedText(TEXT("velocity")) == TEXT("5.00")
            && AuthoredA->Get_PreviousDeltaTimeText() == TEXT("0.000 s")
            && AuthoredA->Get_VelocitySeries()->GetSamples().Num() == 1
            && FMath::IsNearlyEqual(AuthoredA->Get_VelocitySeries()->GetSamples()[0], 13.0f));

    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native all-sections versus velocity-only capture computes every authored diff label"),
        Differing.Num() == 8
            && Differing.Contains(TEXT("Current:")) && Differing.Contains(TEXT("Speed:"))
            && Differing.Contains(TEXT("Override:")) && Differing.Contains(TEXT("Velocity:"))
            && Differing.Contains(TEXT("Prev Location:")) && Differing.Contains(TEXT("Prev DeltaTime:"))
            && Differing.Contains(TEXT("Distance Offset:")) && Differing.Contains(TEXT("Integrator:")));
    TSharedRef<SCkInspector_PhysicsAuthored> DiffAuthored = AuthoredA;
    { const FCkInspector_DiffMarkScope DiffScope{&Differing}; DiffAuthored =
        StaticCastSharedRef<SCkInspector_PhysicsAuthored>(Inspector.Build_Inspector(EntityA)); }
    TestTrue(TEXT("captured native diff labels project into the authored view"),
        DiffAuthored->Is_DiffMarked(TEXT("Current:")) && DiffAuthored->Is_DiffMarked(TEXT("Speed:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Integrator:")));

    const TSharedPtr<SWindow> HostWindow = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{900.0f, 900.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[RenderedA]
            + SVerticalBox::Slot().AutoHeight()[RenderedWithoutWorld]];
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);

    const TSharedPtr<SWidget> VelocityInput = FindTaggedWidget(RenderedA, TEXT("physics-velocity-override-x-input"));
    const TSharedPtr<SWidget> AccelerationInput =
        FindTaggedWidget(RenderedA, TEXT("physics-acceleration-override-x-input"));
    const TSharedPtr<SButton> StartButton =
        FindTaggedButton(ViewA->GetRegion(TEXT("main")), TEXT("physics-integrator-start"));
    const TSharedPtr<SButton> StopButton =
        FindTaggedButton(ViewA->GetRegion(TEXT("main")), TEXT("physics-integrator-stop"));
    const TSharedPtr<SWidget> IntegratorSection =
        FindTaggedWidget(RenderedA, TEXT("physics-integrator-section"));
    const TSharedPtr<SWidget> VelocityWithoutWorldInput =
        FindTaggedWidget(RenderedWithoutWorld, TEXT("physics-velocity-override-x-input"));
    const TSharedPtr<SWidget> AccelerationWithoutWorldInput =
        FindTaggedWidget(RenderedWithoutWorld, TEXT("physics-acceleration-override-x-input"));
    if (NOT TestTrue(TEXT("canonical vector ports and physical integrator actions are mounted and enabled"),
        VelocityInput.IsValid() && AccelerationInput.IsValid() && VelocityInput->IsEnabled()
            && AccelerationInput->IsEnabled() && StartButton.IsValid() && StopButton.IsValid()
            && NOT StartButton->IsEnabled() && StopButton->IsEnabled() && IntegratorSection.IsValid()
            && VelocityWithoutWorldInput.IsValid() && AccelerationWithoutWorldInput.IsValid())) { return false; }

    const TSharedPtr<SWidget> VelocityXEditWidget =
        FindDescendantByType(VelocityInput.ToSharedRef(), TEXT("SEditableTextBox"));
    const TSharedPtr<SWidget> VelocityYInput = FindTaggedWidget(RenderedA, TEXT("physics-velocity-override-y-input"));
    const TSharedPtr<SWidget> VelocityZInput = FindTaggedWidget(RenderedA, TEXT("physics-velocity-override-z-input"));
    const TSharedPtr<SWidget> AccelerationYInput =
        FindTaggedWidget(RenderedA, TEXT("physics-acceleration-override-y-input"));
    const TSharedPtr<SWidget> AccelerationZInput =
        FindTaggedWidget(RenderedA, TEXT("physics-acceleration-override-z-input"));
    const TSharedPtr<SWidget> VelocityYEditWidget = VelocityYInput.IsValid()
        ? FindDescendantByType(VelocityYInput.ToSharedRef(), TEXT("SEditableTextBox")) : nullptr;
    const TSharedPtr<SWidget> VelocityZEditWidget = VelocityZInput.IsValid()
        ? FindDescendantByType(VelocityZInput.ToSharedRef(), TEXT("SEditableTextBox")) : nullptr;
    const TSharedPtr<SWidget> AccelerationXEditWidget =
        FindDescendantByType(AccelerationInput.ToSharedRef(), TEXT("SEditableTextBox"));
    const TSharedPtr<SWidget> AccelerationYEditWidget = AccelerationYInput.IsValid()
        ? FindDescendantByType(AccelerationYInput.ToSharedRef(), TEXT("SEditableTextBox")) : nullptr;
    const TSharedPtr<SWidget> AccelerationZEditWidget = AccelerationZInput.IsValid()
        ? FindDescendantByType(AccelerationZInput.ToSharedRef(), TEXT("SEditableTextBox")) : nullptr;
    if (NOT TestTrue(TEXT("each retained vector port exposes three physical editable-text receivers"),
        VelocityXEditWidget.IsValid() && VelocityYEditWidget.IsValid() && VelocityZEditWidget.IsValid()
            && AccelerationXEditWidget.IsValid() && AccelerationYEditWidget.IsValid()
            && AccelerationZEditWidget.IsValid())) { return false; }

    const int32 VelocitySampleCountBeforePhysicalCommit = AuthoredA->Get_VelocitySeries()->GetSamples().Num();
    const bool bVelocityCommitted =
        SetAndCommit(Slate, StaticCastSharedRef<SEditableTextBox>(VelocityXEditWidget.ToSharedRef()), TEXT("6"))
        && SetAndCommit(Slate, StaticCastSharedRef<SEditableTextBox>(VelocityYEditWidget.ToSharedRef()), TEXT("8"))
        && SetAndCommit(Slate, StaticCastSharedRef<SEditableTextBox>(VelocityZEditWidget.ToSharedRef()), TEXT("0"));
    const bool bAccelerationCommitted =
        SetAndCommit(Slate, StaticCastSharedRef<SEditableTextBox>(AccelerationXEditWidget.ToSharedRef()), TEXT("9"))
        && SetAndCommit(Slate, StaticCastSharedRef<SEditableTextBox>(AccelerationYEditWidget.ToSharedRef()), TEXT("2"))
        && SetAndCommit(Slate, StaticCastSharedRef<SEditableTextBox>(AccelerationZEditWidget.ToSharedRef()), TEXT("-3"));
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{640.0f, 480.0f}, FSlateLayoutTransform{}), 0.0, 0.016f);
    TestTrue(TEXT("all six physical native editors accept Enter commits"), bVelocityCommitted && bAccelerationCommitted);
    TestTrue(TEXT("physical native ports route axis-preserving immediate public velocity and acceleration overrides"),
        AuthoredA->Get_Vector(TEXT("velocity")).Equals(FVector{6.0, 8.0, 0.0})
            && AuthoredA->Get_Vector(TEXT("acceleration")).Equals(FVector{9.0, 2.0, -3.0})
            && AuthoredA->Get_SpeedText(TEXT("velocity")) == TEXT("10.00"));
    TestTrue(TEXT("mounted speed history grows and ends at the committed velocity magnitude"),
        AuthoredA->Get_VelocitySeries()->GetSamples().Num() > VelocitySampleCountBeforePhysicalCommit
            && FMath::IsNearlyEqual(AuthoredA->Get_VelocitySeries()->GetSamples().Last(), 10.0f));
    const FVector NoWorldVelocityBefore = AuthoredWithoutWorld->Get_Vector(TEXT("velocity"));
    const FVector NoWorldAccelerationBefore = AuthoredWithoutWorld->Get_Vector(TEXT("acceleration"));
    AuthoredWithoutWorld->Commit_Velocity(FVector{90.0});
    AuthoredWithoutWorld->Commit_Acceleration(FVector{90.0});
    VelocityWithoutWorldInput->SlatePrepass();
    AccelerationWithoutWorldInput->SlatePrepass();
    TestFalse(TEXT("worldless Velocity override is authority-gated"), AuthoredWithoutWorld->Get_CanOverrideVelocity());
    TestFalse(TEXT("worldless Acceleration override is authority-gated"),
        AuthoredWithoutWorld->Get_CanOverrideAcceleration());
    TestTrue(TEXT("AuthorityOnly ports remain visible but disabled with an actionable reason"),
        NOT VelocityWithoutWorldInput->IsEnabled() && NOT AccelerationWithoutWorldInput->IsEnabled()
            && GetToolTipText(VelocityWithoutWorldInput.ToSharedRef()).Contains(TEXT("authority cannot be established"))
            && GetToolTipText(AccelerationWithoutWorldInput.ToSharedRef()).Contains(TEXT("authority cannot be established")));
    TestTrue(TEXT("worldless AuthorityOnly handlers fail closed without mutating fragments"),
        AuthoredWithoutWorld->Get_Vector(TEXT("velocity")).Equals(NoWorldVelocityBefore)
            && AuthoredWithoutWorld->Get_Vector(TEXT("acceleration")).Equals(NoWorldAccelerationBefore));

    AuthoredA->Request_StartIntegrator();
    TestTrue(TEXT("Start is disabled and fails closed while the integrator is already running"),
        NOT StartButton->IsEnabled()
            && GetToolTipText(StartButton.ToSharedRef()).Contains(TEXT("already running"))
            && EntityA.Has<ck::FFragment_EulerIntegrator_Current>());
    TestTrue(TEXT("physical Stop action dispatches through the authored action binding"), Click(Slate, StopButton.ToSharedRef()));
    Inspector.Tick(EntityA, 0.0f);
    TickSlate(Slate);
    TestTrue(TEXT("physical Stop action removes the integrator and requests a structural rebuild"),
        NOT EntityA.Has<ck::FFragment_EulerIntegrator_Current>() && Inspector.NeedsRebuild()
            && IntegratorSection->GetVisibility() == EVisibility::Collapsed);
    const TSharedRef<SCkInspector_PhysicsAuthored> RebuiltAfterStop =
        StaticCastSharedRef<SCkInspector_PhysicsAuthored>(Inspector.Build_Inspector(EntityA));
    RebuiltAfterStop->SlatePrepass();
    const TSharedPtr<SWidget> RebuiltIntegratorSection =
        FindTaggedWidget(RebuiltAfterStop, TEXT("physics-integrator-section"));
    TestTrue(TEXT("rebuilt authored structure keeps the removed Euler section collapsed"),
        NOT RebuiltAfterStop->Get_HasEulerIntegrator() && RebuiltIntegratorSection.IsValid()
            && RebuiltIntegratorSection->GetVisibility() == EVisibility::Collapsed);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString Root = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Physics resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Root, TEXT("EcsInspectorPhysics.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Root, TEXT("EcsInspectorPhysics.ui.css")))))
    { return false; }
    TestTrue(TEXT("HTML owns every section, row, sparkline and action around two narrow vector ports"),
        Markup.Contains(TEXT(">Velocity</text>")) && Markup.Contains(TEXT(">Acceleration</text>"))
            && Markup.Contains(TEXT(">Predicted Velocity</text>")) && Markup.Contains(TEXT(">Euler Integrator</text>"))
            && Markup.Contains(TEXT("<debug-sparkline")) && Markup.Contains(TEXT("<debug-inspector-action"))
            && Markup.Contains(TEXT("physics-velocity-override-port"))
            && Markup.Contains(TEXT("physics-acceleration-override-port"))
            && NOT Markup.Contains(TEXT("physics-native-body-port")));

    const int64 RevisionA = ViewA->GetRevision();
    const TSharedPtr<SWidget> OriginalVelocityPort = VelocityInput;
    const TSharedPtr<SButton> OriginalStart = StartButton;
    TestTrue(TEXT("compatible reload preserves retained port and action identity"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Physics compatible candidate")).Succeeded
            && ViewA->GetRevision() > RevisionA
            && FindTaggedWidget(RenderedA, TEXT("physics-velocity-override-x-input")) == OriginalVelocityPort
            && FindTaggedButton(ViewA->GetRegion(TEXT("main")), TEXT("physics-integrator-start")) == OriginalStart);
    const int64 RevisionB = ViewB->GetRevision();
    const TSharedRef<SWidget> MainB = ViewB->GetRegion(TEXT("main"));
    TestTrue(TEXT("missing native binding reload is rejected atomically"),
        NOT ViewB->TryReload(Markup.Replace(TEXT("physics-velocity-override-port"),
                TEXT("physics-missing-port")), Stylesheet, TEXT("Physics rejected candidate")).Succeeded
            && ViewB->GetRevision() == RevisionB && &ViewB->GetRegion(TEXT("main")).Get() == &MainB.Get());
    TestTrue(TEXT("missing integrator action reload is rejected atomically"),
        NOT ViewB->TryReload(Markup.Replace(TEXT("action=\"physics-integrator-start\""),
                TEXT("action=\"physics-missing-start\"")), Stylesheet,
            TEXT("Physics missing action candidate")).Succeeded
            && ViewB->GetRevision() == RevisionB && &ViewB->GetRegion(TEXT("main")).Get() == &MainB.Get());

    TestTrue(TEXT("velocity Current fragment removal preserves the entity"),
        EntityA.Try_Remove<ck::FFragment_Velocity_Current>() && ck::IsValid(EntityA));
    VelocityInput->SlatePrepass();
    AuthoredA->Commit_Velocity(FVector{99.0});
    TestTrue(TEXT("held velocity port fails closed after composition loss"),
        NOT AuthoredA->Get_HasVelocity() && NOT AuthoredA->Get_CanOverrideVelocity()
            && AuthoredA->Get_AxisText(TEXT("velocity"), 0) == TEXT("--") && NOT VelocityInput->IsEnabled()
            && NOT EntityA.Has<ck::FFragment_Velocity_Current>());

    TSharedPtr<SCkInspector_PhysicsAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_Physics>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_PhysicsAuthored>(
            DestructorInspector->Build_Inspector(EntityB));
    }
    TestTrue(TEXT("inspector destruction releases retained views and ports"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert()
            && NOT DestructorAuthored->Is_Mounted() && NOT DestructorAuthored->Get_View().IsValid()
            && NOT DestructorAuthored->Get_VelocitySeries().IsValid());

    Inspector.OnDeactivated();
    AuthoredA->Commit_Acceleration(FVector{100.0});
    TestTrue(TEXT("deactivation releases every authored view, series, port, callback and edit scope"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && AuthoredWithoutWorld->Is_Inert()
            && DiffAuthored->Is_Inert() && RebuiltAfterStop->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted()
            && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredA->Get_PredictedVelocitySeries().IsValid()
            && NOT EditGuard->Get_HasActiveEdit()
            && EntityA.Get<ck::FFragment_Acceleration_Current>().Get_CurrentAcceleration().Equals(
                FVector{9.0, 2.0, -3.0}));
    return true;
}

#endif
