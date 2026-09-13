#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_FogOfWar.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkMinimap/CkFogOfWar_Fragment.h"
#include "CkMinimap/CkFogOfWar_Processor.h"
#include "CkMinimap/CkFogOfWar_Utils.h"
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

namespace ck_inspector_fog_of_war_authored_test
{
    auto Pump(ck::FEcsWorld& InWorld) -> void
    {
        ck::FProcessor_FogOfWar_Setup{InWorld.Get_Registry()}.Pump();
        ck::FProcessor_FogOfWar_HandleRequests{InWorld.Get_Registry()}.Pump();
    }

    auto CreateFog(
        const FCk_Handle& InOwner,
        const FVector2D InCenter,
        const FVector2D InHalfExtents,
        const float InCellSize,
        const float InRevealRadius,
        const float InIntervalSeconds) -> FCk_Handle_FogOfWar
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
        if (ck::Is_NOT_Valid(Entity)) { return {}; }

        auto Params = FCk_Fragment_FogOfWar_ParamsData{
            FCk_Minimap_WorldBounds{InCenter, InHalfExtents}};
        Params.Set_CellSize(InCellSize);
        Params.Set_RevealRadius(InRevealRadius);
        Params.Set_UpdateInterval(FCk_Time{InIntervalSeconds});
        return UCk_Utils_FogOfWar_UE::Add(Entity, Params);
    }

    auto FindTagged(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTagged(
                    ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
                Found.IsValid())
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
                    ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
                Found.IsValid())
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
                    ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)));
                Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto CommitEditor(
        FSlateApplication& InSlate,
        const TSharedRef<SEditableTextBox>& InEditor,
        const FString& InText) -> bool
    {
        InSlate.SetUserFocus(0, InEditor, EFocusCause::SetDirectly);
        TickSlate(InSlate);
        InEditor->SetText(FText::FromString(InText));
        const bool Handled = InSlate.ProcessKeyDownEvent(
            FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
        TickSlate(InSlate);
        return Handled;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorFogOfWarAuthored,
    "Ck.UiAuthoring.EcsDebugger.FogOfWarInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorFogOfWarAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_fog_of_war_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = GetMutableDefault<UCkDebuggerStyleSettings>();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), StyleSettings))
    { return false; }
    const ECkDebugAxis_EditControlStyle PreviousEditControlStyle =
        StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousEditControlStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    UWorld* const TestWorld = GWorld;
    if (NOT TestTrue(TEXT("fixture has a transient owner and automation world"),
        ck::IsValid(LifetimeOwner) && TestWorld != nullptr))
    { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(TestWorld);

    auto EntityA = CreateFog(LifetimeOwner, FVector2D::ZeroVector, FVector2D{400.0f, 400.0f},
        100.0f, 125.0f, 0.25f);
    auto EntityB = CreateFog(LifetimeOwner, FVector2D{50.0f, 25.0f}, FVector2D{300.0f, 200.0f},
        50.0f, 250.0f, 0.0f);
    if (NOT TestTrue(TEXT("fixture creates two complete FogOfWar entities"),
        ck::IsValid(EntityA) && ck::IsValid(EntityB)))
    { return false; }

    TestTrue(TEXT("grids are genuinely unallocated before the production setup processor"),
        UCk_Utils_FogOfWar_UE::Get_CellCounts(EntityA) == FIntPoint::ZeroValue);
    auto PreSetupInspector = FCkInspector_FogOfWar{};
    const TSharedRef<SWidget> PreSetupRendered = PreSetupInspector.Build_Inspector(EntityA);
    if (NOT TestEqual(TEXT("unallocated fog still mounts the authored shell"),
        PreSetupRendered->GetTypeAsString(), FString{TEXT("SCkInspector_FogOfWarAuthored")}))
    {
        AddError(PreSetupInspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_FogOfWarAuthored> PreSetupAuthored =
        StaticCastSharedRef<SCkInspector_FogOfWarAuthored>(PreSetupRendered);
    TestTrue(TEXT("authored grid exposes the production unallocated state"),
        PreSetupAuthored->Get_GridText().StartsWith(TEXT("UNALLOCATED")));
    Pump(World);
    TestTrue(TEXT("production setup allocates both grids"),
        UCk_Utils_FogOfWar_UE::Get_CellCounts(EntityA).X > 0
            && UCk_Utils_FogOfWar_UE::Get_CellCounts(EntityB).X > 0
            && PreSetupAuthored->Get_GridText().Contains(TEXT("8 x 8")));
    PreSetupInspector.OnDeactivated();

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    auto Inspector = FCkInspector_FogOfWar{};
    Inspector.Set_EditGuard(EditGuard);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("A mounts the authored FogOfWar inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_FogOfWarAuthored")})
        || NOT TestEqual(TEXT("B mounts the authored FogOfWar inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_FogOfWarAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }

    const TSharedRef<SCkInspector_FogOfWarAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_FogOfWarAuthored>(RenderedA);
    const TSharedRef<SCkInspector_FogOfWarAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_FogOfWarAuthored>(RenderedB);
    TestTrue(TEXT("authored views independently project complete live fog state"),
        AuthoredA->Get_View().IsValid() && AuthoredB->Get_View().IsValid()
            && AuthoredA->Get_View() != AuthoredB->Get_View()
            && AuthoredA->Get_GridText().Contains(TEXT("8 x 8"))
            && AuthoredB->Get_GridText().Contains(TEXT("12 x 8"))
            && AuthoredA->Get_BoundsCenterText() == TEXT("0  0")
            && AuthoredB->Get_BoundsCenterText() == TEXT("50  25")
            && AuthoredA->Get_ParamsRadiusText() == TEXT("125")
            && AuthoredB->Get_ParamsRadiusText() == TEXT("250")
            && AuthoredB->Get_IntervalText() == TEXT("0 (every frame)"));

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TSharedPtr<SCkInspector_FogOfWarAuthored> DiffAuthored;
    {
        const FCkInspector_DiffMarkScope DiffScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_FogOfWarAuthored>(
            Inspector.Build_Inspector(EntityA));
    }
    TestTrue(TEXT("native capture preserves the legacy nine-key duplicate-label collapse"),
        RowsA.Num() == 9 && RowsB.Num() == 9
            && RowsA.FindRef(TEXT("Grid:")) == TEXT("Reveal All Reset")
            && RowsA.FindRef(TEXT("Reveal Radius:")) == TEXT("125")
            && RowsB.FindRef(TEXT("Reveal Radius:")) == TEXT("250")
            && Differing.Contains(TEXT("Bounds Center:"))
            && Differing.Contains(TEXT("Bounds Half-Extents:"))
            && Differing.Contains(TEXT("Reveal Radius:"))
            && Differing.Contains(TEXT("Interval:"))
            && DiffAuthored.IsValid() && DiffAuthored->Is_DiffMarked(TEXT("Reveal Radius:")));

    AuthoredA->Commit_Location(FVector{10.0f, 20.0f, 30.0f});
    AuthoredA->Commit_Radius(50.0f);
    TestTrue(TEXT("staged reveal payload is owned independently by each retained view"),
        AuthoredA->Get_StagedLocation() == FVector{10.0f, 20.0f, 30.0f}
            && FMath::IsNearlyEqual(AuthoredA->Get_StagedRadius(), 50.0f)
            && AuthoredB->Get_StagedLocation().IsZero()
            && FMath::IsNearlyZero(AuthoredB->Get_StagedRadius()));

    const TSharedPtr<SButton> RevealAll = FindButton(RenderedA, TEXT("fog-reveal-all"));
    const TSharedPtr<SButton> Reset = FindButton(RenderedA, TEXT("fog-reset"));
    const TSharedPtr<SButton> RevealHere = FindButton(RenderedA, TEXT("fog-reveal-here"));
    const TSharedPtr<SWidget> LocationXHost = FindTagged(RenderedA, TEXT("fog-reveal-location-0"));
    const TSharedPtr<SWidget> RadiusHost = FindTagged(RenderedA, TEXT("fog-reveal-radius-input"));
    const TSharedPtr<SEditableTextBox> LocationXEditor = LocationXHost.IsValid()
        ? FindEditor(LocationXHost.ToSharedRef()) : nullptr;
    const TSharedPtr<SEditableTextBox> RadiusEditor = RadiusHost.IsValid()
        ? FindEditor(RadiusHost.ToSharedRef()) : nullptr;
    if (NOT TestTrue(TEXT("all authored actions and canonical native editors physically mount"),
        RevealAll.IsValid() && Reset.IsValid() && RevealHere.IsValid()
            && LocationXEditor.IsValid() && RadiusEditor.IsValid()))
    { return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    const TSharedRef<SWindow> Window = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{620.0f, 760.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[RenderedA]
            + SVerticalBox::Slot().AutoHeight()[RenderedB]
        ];
    Slate.AddWindow(Window, true);
    TickSlate(Slate);
    ON_SCOPE_EXIT { Slate.DestroyWindowImmediately(Window); };
    TestTrue(TEXT("world-bound CosmeticOnly controls are enabled"),
        RevealAll->IsEnabled() && Reset->IsEnabled() && RevealHere->IsEnabled()
            && LocationXHost->IsEnabled() && RadiusHost->IsEnabled());

    TestTrue(TEXT("physical location and radius editors accept Enter commits"),
        CommitEditor(Slate, LocationXEditor.ToSharedRef(), TEXT("75"))
            && CommitEditor(Slate, RadiusEditor.ToSharedRef(), TEXT("60")));
    TestTrue(TEXT("physical editor commits update only A staging and release the shared edit guard"),
        FMath::IsNearlyEqual(AuthoredA->Get_StagedLocation().X, 75.0f)
            && FMath::IsNearlyEqual(AuthoredA->Get_StagedRadius(), 60.0f)
            && AuthoredB->Get_StagedLocation().IsZero()
            && NOT EditGuard->Get_HasActiveEdit());

    RevealAll->SimulateClick();
    TestTrue(TEXT("physical Reveal All queues without mutating the grid before the handler pump"),
        EntityA.Has<ck::FFragment_FogOfWar_Requests>()
            && AuthoredA->Get_ExploredFraction() < 0.01f);
    Pump(World);
    TestTrue(TEXT("production handler applies physical Reveal All"),
        AuthoredA->Get_ExploredFraction() > 0.99f
            && NOT EntityA.Has<ck::FFragment_FogOfWar_Requests>());
    Reset->SimulateClick();
    Pump(World);
    TestTrue(TEXT("production handler applies physical Reset"),
        AuthoredA->Get_ExploredFraction() < 0.01f);
    RevealHere->SimulateClick();
    Pump(World);
    TestTrue(TEXT("production handler applies the physically staged Reveal Here payload"),
        AuthoredA->Get_ExploredFraction() > 0.0f
            && AuthoredA->Get_ExploredFraction() < 1.0f);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString Root = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed FogOfWar authored resources are readable"),
        Plugin.IsValid()
            && FFileHelper::LoadFileToString(Markup,
                *FPaths::Combine(Root, TEXT("EcsInspectorFogOfWar.ui.html")))
            && FFileHelper::LoadFileToString(Stylesheet,
                *FPaths::Combine(Root, TEXT("EcsInspectorFogOfWar.ui.css")))))
    { return false; }
    TestTrue(TEXT("HTML owns every stable row including unique duplicate-label identities"),
        Markup.Contains(TEXT("id=\"fog-grid-value-row\""))
            && Markup.Contains(TEXT("id=\"fog-grid-actions-row\""))
            && Markup.Contains(TEXT("id=\"fog-staged-radius-row\""))
            && Markup.Contains(TEXT("id=\"fog-params-radius-row\""))
            && Markup.Contains(TEXT("bind=\"fog-reveal-location-port\""))
            && Markup.Contains(TEXT("bind=\"fog-reveal-radius-port\"")));

    const int64 RevisionA = AuthoredA->Get_View()->GetRevision();
    const int64 RevisionB = AuthoredB->Get_View()->GetRevision();
    TestTrue(TEXT("compatible reload succeeds and preserves per-view staged payload"),
        AuthoredA->Get_View()->TryReload(
            Markup, Stylesheet, TEXT("FogOfWar compatible candidate")).Succeeded
            && AuthoredA->Get_View()->GetRevision() > RevisionA
            && AuthoredB->Get_View()->GetRevision() == RevisionB
            && FMath::IsNearlyEqual(AuthoredA->Get_StagedRadius(), 60.0f));
    const TSharedRef<SWidget> MainBefore = AuthoredB->Get_View()->GetRegion(TEXT("main"));
    const TSharedPtr<SWidget> PortBefore = FindTagged(MainBefore, TEXT("fog-reveal-radius-input"));
    TestFalse(TEXT("missing FogOfWar native port rejects atomically"),
        AuthoredB->Get_View()->TryReload(
            Markup.Replace(TEXT("bind=\"fog-reveal-radius-port\""),
                TEXT("bind=\"fog-missing-port\"")),
            Stylesheet, TEXT("FogOfWar missing port")).Succeeded);
    TestFalse(TEXT("missing FogOfWar action rejects atomically"),
        AuthoredB->Get_View()->TryReload(
            Markup.Replace(TEXT("action=\"fog-reveal-all\""),
                TEXT("action=\"fog-missing-action\"")),
            Stylesheet, TEXT("FogOfWar missing action")).Succeeded);
    TestTrue(TEXT("rejected reload retains revision, tree and native port identity"),
        AuthoredB->Get_View()->GetRevision() == RevisionB
            && &AuthoredB->Get_View()->GetRegion(TEXT("main")).Get() == &MainBefore.Get()
            && FindTagged(AuthoredB->Get_View()->GetRegion(TEXT("main")),
                TEXT("fog-reveal-radius-input")) == PortBefore);
    const TSharedPtr<SButton> RetainedRevealAll = FindButton(
        AuthoredB->Get_View()->GetRegion(TEXT("main")), TEXT("fog-reveal-all"));
    if (NOT TestTrue(TEXT("rejected reload retains the physical Reveal All action"),
        RetainedRevealAll.IsValid()))
    { return false; }
    RetainedRevealAll->SimulateClick();
    Pump(World);
    TestTrue(TEXT("retained action remains operational after rejected reload"),
        AuthoredB->Get_ExploredFraction() > 0.99f);

    auto DeniedWorld = ck::FEcsWorld{};
    const FCk_Handle DeniedOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(
        DeniedWorld.Get_Registry());
    auto DeniedEntity = CreateFog(DeniedOwner, FVector2D::ZeroVector, FVector2D{100.0f, 100.0f},
        50.0f, 50.0f, 0.0f);
    Pump(DeniedWorld);
    const TSharedRef<SWidget> DeniedRendered = Inspector.Build_Inspector(DeniedEntity);
    if (NOT TestEqual(TEXT("worldless FogOfWar still mounts the authored shell"),
        DeniedRendered->GetTypeAsString(), FString{TEXT("SCkInspector_FogOfWarAuthored")}))
    { return false; }
    const TSharedRef<SCkInspector_FogOfWarAuthored> Denied =
        StaticCastSharedRef<SCkInspector_FogOfWarAuthored>(
            DeniedRendered);
    TestTrue(TEXT("CosmeticOnly request admission fails closed without a world"),
        Denied->Get_IsAvailable() && NOT Denied->Get_CanRequest()
            && Denied->Get_RequestDisabledReason().Contains(TEXT("no world")));
    Denied->Commit_Location(FVector{1.0f, 2.0f, 3.0f});
    Denied->Commit_Radius(10.0f);
    Denied->Request_RevealAll();
    Denied->Request_Reset();
    Denied->Request_RevealHere();
    TestTrue(TEXT("denied editors and stale event dispatch enqueue no request or partial staging"),
        Denied->Get_StagedLocation().IsZero()
            && FMath::IsNearlyZero(Denied->Get_StagedRadius())
            && NOT DeniedEntity.Has<ck::FFragment_FogOfWar_Requests>());

    TSharedPtr<SCkInspector_FogOfWarAuthored> DestructorAuthored;
    TWeakPtr<FCkUiView> DestructorView;
    const TSharedRef<FCkInspectorEditGuard> DestructorGuard = MakeShared<FCkInspectorEditGuard>();
    TSharedPtr<SWidget> DestructorLocationHost;
    TSharedPtr<SEditableTextBox> DestructorLocationEditor;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_FogOfWar>();
        DestructorInspector->Set_EditGuard(DestructorGuard);
        const TSharedRef<SWidget> DestructorRendered = DestructorInspector->Build_Inspector(EntityB);
        DestructorAuthored = StaticCastSharedRef<SCkInspector_FogOfWarAuthored>(DestructorRendered);
        DestructorView = DestructorAuthored->Get_View();
        DestructorLocationHost = FindTagged(DestructorRendered, TEXT("fog-reveal-location-0"));
        DestructorLocationEditor = DestructorLocationHost.IsValid()
            ? FindEditor(DestructorLocationHost.ToSharedRef()) : nullptr;
        TestTrue(TEXT("destructor fixture retains an initially enabled physical editor"),
            DestructorLocationHost.IsValid() && DestructorLocationEditor.IsValid()
                && DestructorLocationHost->IsEnabled());
    }
    TestTrue(TEXT("inspector destruction releases its retained authored view"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert()
            && NOT DestructorAuthored->Is_Mounted() && NOT DestructorView.IsValid());
    if (DestructorLocationHost.IsValid() && DestructorLocationEditor.IsValid())
    {
        const TSharedRef<SWindow> ReleasedEditorWindow = SNew(SWindow)
            .AutoCenter(EAutoCenter::None)
            .ClientSize(FVector2D{180.0f, 80.0f})
            .CreateTitleBar(false)
            .HasCloseButton(false)[DestructorLocationHost.ToSharedRef()];
        Slate.AddWindow(ReleasedEditorWindow, true);
        TickSlate(Slate);
        TestFalse(TEXT("released retained editor disables after standalone remount"),
            DestructorLocationHost->IsEnabled());
        CommitEditor(Slate, DestructorLocationEditor.ToSharedRef(), TEXT("999"));
        TickSlate(Slate);
        Slate.DestroyWindowImmediately(ReleasedEditorWindow);
    }
    TestTrue(TEXT("released retained editor cannot mutate or re-wedge its former guard"),
        DestructorAuthored->Get_StagedLocation().IsZero()
            && NOT EntityB.Has<ck::FFragment_FogOfWar_Requests>()
            && NOT DestructorGuard->Get_HasActiveEdit());

    TestTrue(TEXT("Current loss leaves A live but removes complete inspectability"),
        EntityA.Try_Remove<ck::FFragment_FogOfWar_Current>()
            && ck::IsValid(EntityA) && EntityA.Has<ck::FFragment_FogOfWar_Params>()
            && NOT Inspector.CanInspect(EntityA));
    Inspector.Tick(EntityA, 0.0f);
    RenderedA->SlatePrepass();
    TestFalse(TEXT("held location editor disables after Current loss"),
        LocationXHost->IsEnabled());
    TestFalse(TEXT("held radius editor disables after Current loss"),
        RadiusHost->IsEnabled());
    AuthoredA->Request_Reset();
    AuthoredA->Request_RevealHere();
    TestFalse(TEXT("stale held actions enqueue nothing after Current loss"),
        EntityA.Has<ck::FFragment_FogOfWar_Requests>());
    auto StaleRows = TMap<FString, FString>{};
    {
        const FCkInspector_RowCaptureScope Capture;
        Inspector.Build_Inspector(EntityA);
        StaleRows = Capture.Get_Rows();
    }
    TestTrue(TEXT("native capture also fails closed after independent fragment loss"),
        StaleRows.IsEmpty());

    TestTrue(TEXT("Params loss leaves B live but removes complete inspectability"),
        EntityB.Try_Remove<ck::FFragment_FogOfWar_Params>()
            && ck::IsValid(EntityB) && EntityB.Has<ck::FFragment_FogOfWar_Current>()
            && NOT Inspector.CanInspect(EntityB));
    AuthoredB->Request_RevealAll();
    TestFalse(TEXT("stale B action enqueues nothing after Params loss"),
        EntityB.Has<ck::FFragment_FogOfWar_Requests>());

    auto PendingEntity = CreateFog(LifetimeOwner, FVector2D::ZeroVector,
        FVector2D{100.0f, 100.0f}, 50.0f, 50.0f, 0.0f);
    Pump(World);
    const TSharedRef<SWidget> PendingRendered = Inspector.Build_Inspector(PendingEntity);
    if (NOT TestEqual(TEXT("pending-destruction fixture initially mounts authored shell"),
        PendingRendered->GetTypeAsString(), FString{TEXT("SCkInspector_FogOfWarAuthored")}))
    { return false; }
    const TSharedRef<SCkInspector_FogOfWarAuthored> Pending =
        StaticCastSharedRef<SCkInspector_FogOfWarAuthored>(
            PendingRendered);
    PendingEntity.Add<ck::FTag_DestroyEntity_Initiate>();
    Pending->Request_RevealAll();
    TestTrue(TEXT("pending-destruction FogOfWar fails closed before request enqueue"),
        NOT Pending->Get_IsAvailable()
            && NOT PendingEntity.Has<ck::FFragment_FogOfWar_Requests>());

    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases every authored view and edit scope"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && Denied->Is_Inert() && Pending->Is_Inert()
            && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredB->Get_View().IsValid()
            && NOT EditGuard->Get_HasActiveEdit());
    Pending->Request_Reset();
    Slate.SetUserFocus(0, LocationXEditor.ToSharedRef(), EFocusCause::SetDirectly);
    LocationXEditor->SetText(FText::FromString(TEXT("999")));
    Slate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
    TickSlate(Slate);
    TestFalse(TEXT("held authored action remains inert after deactivation"),
        PendingEntity.Has<ck::FFragment_FogOfWar_Requests>()
            || EditGuard->Get_HasActiveEdit());
    return true;
}

#endif
