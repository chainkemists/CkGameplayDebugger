#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Tween.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkTween/CkTween_Processor.h"
#include "CkTween/CkTween_Utils.h"

#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

namespace ck_inspector_tween_authored_test
{
    auto FindTagged(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (const TSharedPtr<SWidget> Found = FindTagged(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; } }
        return nullptr;
    }

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (const TSharedPtr<SButton> Found = FindButton(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; } }
        return nullptr;
    }

    auto PumpRequests(ck::FEcsWorld& InWorld) -> void
    {
        ck::FProcessor_Tween_HandleRequests{InWorld.Get_Registry()}.Pump();
    }

    auto FindEditor(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SEditableTextBox")) { return StaticCastSharedRef<SEditableTextBox>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (const TSharedPtr<SEditableTextBox> Found = FindEditor(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid()) { return Found; } }
        return nullptr;
    }

    auto FindCombo(const TSharedRef<SWidget>& InRoot, const FName InHostTag) -> TSharedPtr<SComboBox<TSharedPtr<FString>>>
    {
        const TSharedPtr<SWidget> Host = FindTagged(InRoot, InHostTag);
        FChildren* Children = Host.IsValid() ? Host->GetChildren() : nullptr;
        if (Children == nullptr || Children->Num() != 1) { return nullptr; }
        return StaticCastSharedRef<SComboBox<TSharedPtr<FString>>>(ConstCastSharedRef<SWidget>(Children->GetChildAt(0)));
    }

    auto Tick(FSlateApplication& InSlate) -> void { InSlate.PumpMessages(); InSlate.Tick(); InSlate.Tick(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorTweenAuthored,
    "Ck.UiAuthoring.EcsDebugger.TweenInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorTweenAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_tween_authored_test;

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    UWorld* const TestWorld = GWorld;
    if (NOT TestTrue(TEXT("fixture has a transient owner and automation world"), ck::IsValid(LifetimeOwner) && TestWorld != nullptr)) { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    auto OwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    auto OwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    FCk_Handle_Tween TweenA = UCk_Utils_Tween_UE::Create_TweenFloat(OwnerA, 0.0f, 1.0f, 4.0f);
    FCk_Handle_Tween TweenB = UCk_Utils_Tween_UE::Create_TweenFloat(OwnerB, 0.0f, 1.0f, 0.0f);
    TweenA.AddOrGet<ck::FFragment_Tween_Chain>().Set_NextTween(TweenB);
    if (NOT TestTrue(TEXT("fixture creates duration and no-duration Tweens"), ck::IsValid(TweenA) && ck::IsValid(TweenB)
        && TweenA.Has<ck::FFragment_Tween_Params>() && TweenA.Has<ck::FFragment_Tween_Current>())) { return false; }

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    auto Inspector = FCkInspector_Tween{};
    Inspector.Set_EditGuard(EditGuard);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(TweenA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(TweenB);
    if (NOT TestEqual(TEXT("duration Tween mounts authored inspector"), RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_TweenAuthored")})
        || NOT TestEqual(TEXT("no-duration Tween mounts authored inspector"), RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_TweenAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_TweenAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_TweenAuthored>(RenderedA);
    const TSharedRef<SCkInspector_TweenAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_TweenAuthored>(RenderedB);
    TestTrue(TEXT("views retain independent staged Stop behavior and live projections"), AuthoredA->Get_View() != AuthoredB->Get_View()
        && AuthoredA->Get_TimeText().Contains(TEXT("/")) && AuthoredB->Get_TimeText() == TEXT("0.0000")
        && AuthoredA->Get_NextTweenText() != TEXT("(None)") && AuthoredB->Get_NextTweenText() == TEXT("--"));

    auto RowsA = TMap<FString, FString>{}; auto RowsB = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(TweenA); RowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(TweenB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TSharedPtr<SCkInspector_TweenAuthored> DiffAuthored;
    { const FCkInspector_DiffMarkScope DiffScope{&Differing}; DiffAuthored = StaticCastSharedRef<SCkInspector_TweenAuthored>(Inspector.Build_Inspector(TweenA)); }
    TestTrue(TEXT("native capture supplies every authored row label and diff labels"), RowsA.Num() == 10 && RowsB.Num() == 9
        && RowsA.Contains(TEXT("State:"))
        && RowsA.Contains(TEXT("Time:")) && RowsA.Contains(TEXT("Loop:")) && RowsA.Contains(TEXT("Reversed:"))
        && RowsA.Contains(TEXT("Time Multiplier:")) && RowsA.Contains(TEXT("Playback:"))
        && RowsA.Contains(TEXT("Stop Behavior:")) && RowsA.Contains(TEXT("Stop:"))
        && RowsA.Contains(TEXT("Set Time Multiplier:")) && RowsA.Contains(TEXT("Next Tween:"))
        && NOT RowsB.Contains(TEXT("Next Tween:")) && DiffAuthored->Is_DiffMarked(TEXT("Time:")));

    const TSharedPtr<SButton> PauseButton = FindButton(RenderedA, TEXT("tween-pause"));
    const TSharedPtr<SButton> ResumeButton = FindButton(RenderedA, TEXT("tween-resume"));
    const TSharedPtr<SButton> RestartButton = FindButton(RenderedA, TEXT("tween-restart"));
    const TSharedPtr<SButton> StopButton = FindButton(RenderedA, TEXT("tween-stop"));
    const TSharedPtr<SEditableTextBox> MultiplierEditor = FindEditor(RenderedA);
    const TSharedPtr<SComboBox<TSharedPtr<FString>>> StopBehaviorComboB = FindCombo(RenderedB, TEXT("tween-stop-behavior-input"));
    if (NOT TestTrue(TEXT("authored physical action buttons and multiplier editor mount"), PauseButton.IsValid()
        && ResumeButton.IsValid() && RestartButton.IsValid() && StopButton.IsValid() && MultiplierEditor.IsValid()
        && StopBehaviorComboB.IsValid())) { return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    const TSharedRef<SWindow> Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{560.0f, 880.0f})
        .CreateTitleBar(false).HasCloseButton(false)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[RenderedA]
            + SVerticalBox::Slot().AutoHeight()[RenderedB]
        ];
    Slate.AddWindow(Window, true); Tick(Slate);
    ON_SCOPE_EXIT { Slate.DestroyWindowImmediately(Window); };
    TestTrue(TEXT("world-bound LocalOk enables physical Tween controls"), PauseButton->IsEnabled() && MultiplierEditor->IsEnabled());
    Slate.SetUserFocus(0, StopBehaviorComboB.ToSharedRef(), EFocusCause::SetDirectly); Tick(Slate);
    Slate.ProcessKeyDownEvent(FKeyEvent{EKeys::Down, FModifierKeysState{}, 0, false, 0, 0}); Tick(Slate);
    TestTrue(TEXT("physical Stop Behavior selection reaches SelfDestruct"), StopBehaviorComboB->GetSelectedItem().IsValid()
        && *StopBehaviorComboB->GetSelectedItem() == TEXT("SelfDestruct"));
    Slate.SetUserFocus(0, MultiplierEditor.ToSharedRef(), EFocusCause::SetDirectly); Tick(Slate);
    MultiplierEditor->SetText(FText::FromString(TEXT("2")));
    Slate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0}); Tick(Slate);
    PumpRequests(World);
    TestTrue(TEXT("public multiplier request projects live state"), AuthoredA->Get_MultiplierText().Contains(TEXT("2")));
    PauseButton->SimulateClick(); PumpRequests(World);
    TestTrue(TEXT("physical Pause routes through Tween request processor"), AuthoredA->Get_StateText().Contains(TEXT("Paused")));
    ResumeButton->SimulateClick(); PumpRequests(World);
    TweenA.Get<ck::FFragment_Tween_Current>().Set_CurrentTime(1.5f);
    RestartButton->SimulateClick(); PumpRequests(World);
    TestTrue(TEXT("physical Resume and Restart apply through Tween request processor"),
        AuthoredA->Get_StateText() == TEXT("Playing")
            && FMath::IsNearlyZero(TweenA.Get<ck::FFragment_Tween_Current>().Get_CurrentTime())
            && NOT TweenA.Has<ck::FFragment_Tween_Requests>());
    StopButton->SimulateClick(); PumpRequests(World);
    TestTrue(TEXT("physical DoNothing Stop applies through Tween request processor"),
        AuthoredA->Get_StateText() == TEXT("Cancelled") && NOT TweenA.Has<ck::FFragment_Tween_Requests>());
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString Root = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("Tween resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Root, TEXT("EcsInspectorTween.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Root, TEXT("EcsInspectorTween.ui.css"))))) { return false; }
    const int64 Revision = AuthoredA->Get_View()->GetRevision();
    TestTrue(TEXT("compatible Tween reload succeeds"), AuthoredA->Get_View()->TryReload(Markup, Stylesheet, TEXT("Tween compatible candidate")).Succeeded);
    const TSharedRef<SWidget> MainBefore = AuthoredB->Get_View()->GetRegion(TEXT("main"));
    const TSharedPtr<SWidget> PortBefore = FindTagged(MainBefore, TEXT("tween-multiplier-input"));
    const int64 RejectedRevision = AuthoredB->Get_View()->GetRevision();
    TestFalse(TEXT("missing native port rejects atomically"), AuthoredB->Get_View()->TryReload(Markup.Replace(TEXT("bind=\"tween-multiplier-port\""), TEXT("bind=\"missing-port\"")), Stylesheet, TEXT("Tween bad port")).Succeeded);
    TestFalse(TEXT("missing action rejects atomically"), AuthoredB->Get_View()->TryReload(Markup.Replace(TEXT("action=\"tween-pause\""), TEXT("action=\"missing-action\"")), Stylesheet, TEXT("Tween bad action")).Succeeded);
    TestTrue(TEXT("rejected reload preserves revision, tree, and native port identity"), AuthoredA->Get_View()->GetRevision() > Revision
        && AuthoredB->Get_View()->GetRevision() == RejectedRevision && &AuthoredB->Get_View()->GetRegion(TEXT("main")).Get() == &MainBefore.Get()
        && FindTagged(AuthoredB->Get_View()->GetRegion(TEXT("main")), TEXT("tween-multiplier-input")) == PortBefore);
    const TSharedPtr<SButton> RetainedPauseB = FindButton(AuthoredB->Get_View()->GetRegion(TEXT("main")), TEXT("tween-pause"));
    if (NOT TestTrue(TEXT("rejected reload retains the mounted Pause action"), RetainedPauseB.IsValid())) { return false; }
    RetainedPauseB->SimulateClick(); PumpRequests(World);
    TestTrue(TEXT("rejected reload leaves the prior action bindings operational"), AuthoredB->Get_StateText() == TEXT("Paused"));
    AuthoredB->Request_Stop(); PumpRequests(World);
    TestTrue(TEXT("per-view staged Stop behavior stays isolated and initiates only B destruction"),
        ck::IsValid(TweenA) && NOT TweenA.Has<ck::FTag_DestroyEntity_Initiate>()
            && TweenB.Has<ck::FTag_DestroyEntity_Initiate>() && NOT AuthoredB->Get_CanRequest());

    auto OwnerC = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    FCk_Handle_Tween TweenC = UCk_Utils_Tween_UE::Create_TweenFloat(OwnerC, 0.0f, 1.0f, 1.0f);
    const TSharedRef<SCkInspector_TweenAuthored> SelfDestructAuthored = StaticCastSharedRef<SCkInspector_TweenAuthored>(Inspector.Build_Inspector(TweenC));
    SelfDestructAuthored->Commit_StopBehavior(1);
    SelfDestructAuthored->Request_Stop();
    PumpRequests(World);
    Inspector.Tick(TweenC, 0.0f);
    TestTrue(TEXT("SelfDestruct invalidation leaves subsequent Tick and held authored view safe"),
        TweenC.Has<ck::FTag_DestroyEntity_Initiate>() && NOT SelfDestructAuthored->Get_CanRequest());

    auto CurrentOnly = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    CurrentOnly.Add<ck::FFragment_Tween_Current>();
    const TSharedRef<SCkInspector_TweenAuthored> CurrentOnlyAuthored = StaticCastSharedRef<SCkInspector_TweenAuthored>(Inspector.Build_Inspector(CurrentOnly));
    const TSharedPtr<SWidget> CurrentOnlyControls = FindTagged(CurrentOnlyAuthored, TEXT("tween-controls-section"));
    const TSharedRef<SWindow> CurrentOnlyWindow = SNew(SWindow).AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{420.0f, 260.0f}).CreateTitleBar(false).HasCloseButton(false)[CurrentOnlyAuthored];
    Slate.AddWindow(CurrentOnlyWindow, true); Tick(Slate);
    ON_SCOPE_EXIT { Slate.DestroyWindowImmediately(CurrentOnlyWindow); };
    TestTrue(TEXT("Current-only Tween remains inspectable"), Inspector.CanInspect(CurrentOnly));
    TestFalse(TEXT("Current-only fixture genuinely omits Tween Params"),
        CurrentOnly.Has<ck::FFragment_Tween_Params>());
    TestTrue(TEXT("Current-only authored view remains available"), CurrentOnlyAuthored->Get_IsAvailable());
    TestEqual(TEXT("Current-only authored time preserves plain native formatting"),
        CurrentOnlyAuthored->Get_TimeText(), FString{TEXT("0.0000")});
    TestFalse(TEXT("Current-only Tween cannot dispatch typed requests"), CurrentOnlyAuthored->Get_CanRequest());
    TestTrue(TEXT("Current-only authored controls section is present for retained visibility binding"),
        CurrentOnlyControls.IsValid());
    if (CurrentOnlyControls.IsValid())
    {
        auto VisiblePath = FWidgetPath{};
        TestFalse(TEXT("Current-only authored controls are absent from the effective visible Slate path"),
            Slate.GeneratePathToWidgetUnchecked(CurrentOnlyControls.ToSharedRef(), VisiblePath, EVisibility::Visible));
    }

    TweenA.Try_Remove<ck::FFragment_Tween_Current>();
    Inspector.Tick(TweenA, 0.0f);
    TestTrue(TEXT("structural fragment loss requests a safe rebuild and disables held controls"), NOT Inspector.CanInspect(TweenA) && NOT AuthoredA->Get_CanRequest());
    AuthoredA->Request_Stop();
    TestFalse(TEXT("stale controls fail closed after composition loss"), TweenA.Has<ck::FFragment_Tween_Requests>());

    TSharedPtr<SCkInspector_TweenAuthored> DestructorAuthored;
    TWeakPtr<FCkUiView> DestructorView;
    auto OwnerD = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    FCk_Handle_Tween TweenD = UCk_Utils_Tween_UE::Create_TweenFloat(OwnerD, 0.0f, 1.0f, 1.0f);
    { auto DestructorInspector = MakeUnique<FCkInspector_Tween>(); DestructorAuthored = StaticCastSharedRef<SCkInspector_TweenAuthored>(DestructorInspector->Build_Inspector(TweenD)); DestructorView = DestructorAuthored->Get_View(); }
    TestTrue(TEXT("destructor releases authored view"), DestructorAuthored->Is_Inert() && NOT DestructorView.IsValid());
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases retained authored views"), AuthoredA->Is_Inert() && AuthoredB->Is_Inert()
        && DiffAuthored->Is_Inert() && SelfDestructAuthored->Is_Inert() && CurrentOnlyAuthored->Is_Inert()
        && NOT EditGuard->Get_HasActiveEdit());
    return true;
}

#endif
