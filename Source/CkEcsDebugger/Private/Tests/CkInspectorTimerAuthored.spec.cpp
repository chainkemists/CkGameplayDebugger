#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Timer.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkTimer/CkTimer_Processor.h"
#include "CkTimer/CkTimer_Utils.h"

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
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_inspector_timer_authored_test
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

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButton(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto ContainsText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock"))
        { if (StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString() == InText) { return true; } }
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText"))
        { if (StaticCastSharedRef<SCkFlexText>(InRoot)->GetText().ToString() == InText) { return true; } }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (ContainsText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; } }
        return false;
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

    auto PumpSetup(ck::FEcsWorld& InWorld) -> void
    {
        ck::FProcessor_Timer_Setup{InWorld.Get_Registry()}.Pump();
    }

    auto PumpRequests(ck::FEcsWorld& InWorld) -> void
    {
        ck::FProcessor_Timer_HandleRequests{InWorld.Get_Registry()}.Pump();
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope() { if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); } }
        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorTimerAuthored,
    "Ck.UiAuthoring.EcsDebugger.TimerInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorTimerAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_timer_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = UCkDebuggerStyleSettings::Get_Mutable();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), StyleSettings)) { return false; }
    const ECkDebugAxis_EditControlStyle PreviousStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    UWorld* const TestWorld = GWorld;
    if (NOT TestTrue(TEXT("fixture has a transient owner and automation world"),
        ck::IsValid(LifetimeOwner) && TestWorld != nullptr)) { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    auto OwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    auto OwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    auto ParamsA = FCk_Fragment_Timer_ParamsData{FCk_Time{10.0}};
    ParamsA.Set_StartingState(ECk_Timer_State::Running)
        .Set_CountDirection(ECk_Timer_CountDirection::CountUp)
        .Set_Behavior(ECk_Timer_Behavior::PauseOnDone);
    auto ParamsB = FCk_Fragment_Timer_ParamsData{FCk_Time{20.0}};
    ParamsB.Set_StartingState(ECk_Timer_State::Paused)
        .Set_CountDirection(ECk_Timer_CountDirection::CountDown)
        .Set_Behavior(ECk_Timer_Behavior::StopOnDone);
    FCk_Handle_Timer TimerA = UCk_Utils_Timer_UE::Add(OwnerA, ParamsA);
    const FCk_Handle_Timer TimerB = UCk_Utils_Timer_UE::Add(OwnerB, ParamsB);
    PumpSetup(World);
    if (NOT TestTrue(TEXT("fixture creates two complete independent timers"),
        ck::IsValid(TimerA) && ck::IsValid(TimerB) && TimerA != TimerB
            && TimerA.Has<ck::FFragment_Timer_Current>() && TimerB.Has<ck::FFragment_Timer_Current>())) { return false; }

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    auto Inspector = FCkInspector_Timer{};
    Inspector.Set_EditGuard(EditGuard);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(TimerA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(TimerB);
    if (NOT TestEqual(TEXT("A mounts the authored Timer inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_TimerAuthored")})
        || NOT TestEqual(TEXT("B mounts the authored Timer inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_TimerAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_TimerAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_TimerAuthored>(RenderedA);
    const TSharedRef<SCkInspector_TimerAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_TimerAuthored>(RenderedB);
    TestTrue(TEXT("Timer builds own independent retained views"),
        AuthoredA->Get_View().IsValid() && AuthoredB->Get_View().IsValid() && AuthoredA->Get_View() != AuthoredB->Get_View());
    TestTrue(TEXT("authored Timer projects independent configuration and live state"),
        AuthoredA->Get_DirectionText() == TEXT("Count Up") && AuthoredB->Get_DirectionText() == TEXT("Count Down")
            && AuthoredA->Get_StateText() == TEXT("Running") && AuthoredB->Get_StateText() == TEXT("Paused")
            && AuthoredA->Get_GoalText() != AuthoredB->Get_GoalText()
            && ContainsText(RenderedA, AuthoredA->Get_GoalText())
            && ContainsText(RenderedB, AuthoredB->Get_GoalText()));

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(TimerA); RowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(TimerB); RowsB = Capture.Get_Rows(); }
    TestTrue(TEXT("native capture preserves all fourteen Timer rows"),
        RowsA.Num() == 14 && RowsB.Num() == 14 && RowsA.Contains(TEXT("Name:"))
            && RowsA.Contains(TEXT("Direction:")) && RowsA.Contains(TEXT("Behavior:"))
            && RowsA.Contains(TEXT("State:")) && RowsA.Contains(TEXT("Goal:"))
            && RowsA.Contains(TEXT("Elapsed:")) && RowsA.Contains(TEXT("Remaining:"))
            && RowsA.Contains(TEXT("Done:")) && RowsA.Contains(TEXT("Playback:"))
            && RowsA.Contains(TEXT("Position:")) && RowsA.Contains(TEXT("Set Direction:"))
            && RowsA.Contains(TEXT("Jump Mode:")) && RowsA.Contains(TEXT("Jump (s):"))
            && RowsA.Contains(TEXT("Consume (s):")));
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TSharedPtr<SCkInspector_TimerAuthored> DiffAuthored;
    { const FCkInspector_DiffMarkScope DiffScope{&Differing}; DiffAuthored =
        StaticCastSharedRef<SCkInspector_TimerAuthored>(Inspector.Build_Inspector(TimerA)); }
    TestTrue(TEXT("authored labels receive exact native Timer diff marks"),
        DiffAuthored.IsValid() && DiffAuthored->Is_DiffMarked(TEXT("Direction:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Behavior:")) && DiffAuthored->Is_DiffMarked(TEXT("State:")));

    const TSharedPtr<SButton> PauseButton = FindButton(RenderedA, TEXT("timer-pause"));
    const TSharedPtr<SButton> CompleteButton = FindButton(RenderedB, TEXT("timer-complete"));
    const TSharedPtr<SWidget> DirectionInput = FindTaggedWidget(RenderedA, TEXT("timer-direction-input"));
    const TSharedPtr<SWidget> JumpInput = FindTaggedWidget(RenderedA, TEXT("timer-jump-input"));
    const TSharedPtr<SWidget> ConsumeInput = FindTaggedWidget(RenderedA, TEXT("timer-consume-input"));
    const TSharedPtr<SEditableTextBox> JumpEditor = JumpInput.IsValid() ? FindEditor(JumpInput.ToSharedRef()) : nullptr;
    const TSharedPtr<SEditableTextBox> ConsumeEditor = ConsumeInput.IsValid() ? FindEditor(ConsumeInput.ToSharedRef()) : nullptr;
    if (NOT TestTrue(TEXT("authored Timer mounts its physical actions and canonical edit leaves"),
        PauseButton.IsValid() && CompleteButton.IsValid() && DirectionInput.IsValid()
            && JumpEditor.IsValid() && ConsumeEditor.IsValid())) { return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{560.0f, 520.0f})
        .CreateTitleBar(false).HasCloseButton(false)[RenderedA];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    Tick(Slate);
    TestTrue(TEXT("world-bound LocalOk enables Timer actions and edit ports"),
        PauseButton->IsEnabled() && DirectionInput->IsEnabled() && JumpInput->IsEnabled() && ConsumeInput->IsEnabled());

    PauseButton->SimulateClick();
    PumpRequests(World);
    TestEqual(TEXT("physical Pause routes through the Timer request processor"),
        UCk_Utils_Timer_UE::Get_CurrentState(TimerA), ECk_Timer_State::Paused);
    AuthoredA->Commit_Direction(1);
    TestEqual(TEXT("direction port routes the immediate public mutator"),
        UCk_Utils_Timer_UE::Get_CountDirection(TimerA), ECk_Timer_CountDirection::CountDown);
    AuthoredA->Commit_JumpMode(1);
    if (NOT TestTrue(TEXT("physical Jump and Consume commits are accepted"),
        SetAndCommit(Slate, JumpEditor.ToSharedRef(), TEXT("4"))
            && SetAndCommit(Slate, ConsumeEditor.ToSharedRef(), TEXT("1")))) { return false; }
    PumpRequests(World);
    TestFalse(TEXT("physical numeric commits close Timer edit scopes"), EditGuard->Get_HasActiveEdit());
    TestFalse(TEXT("Timer request processor drains both physical numeric requests"),
        TimerA.Has<ck::FFragment_Timer_Requests>());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Timer resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorTimer.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorTimer.ui.css")))))
    { return false; }
    TestTrue(TEXT("HTML owns every Timer section, row, action, and native edit placement"),
        Markup.Contains(TEXT(">Identity</text>")) && Markup.Contains(TEXT(">Configuration</text>"))
            && Markup.Contains(TEXT(">Live State</text>")) && Markup.Contains(TEXT(">Controls</text>"))
            && Markup.Contains(TEXT("action=\"timer-pause\"")) && Markup.Contains(TEXT("action=\"timer-reverse\""))
            && Markup.Contains(TEXT("bind=\"timer-direction-port\"")) && Markup.Contains(TEXT("bind=\"timer-jump-port\"")));
    const int64 Revision = AuthoredA->Get_View()->GetRevision();
    TestTrue(TEXT("compatible Timer reload is accepted"),
        AuthoredA->Get_View()->TryReload(Markup, Stylesheet, TEXT("Timer compatible candidate")).Succeeded);
    const TSharedRef<SWidget> MainBefore = AuthoredB->Get_View()->GetRegion(TEXT("main"));
    const int64 RejectedRevision = AuthoredB->Get_View()->GetRevision();
    TestFalse(TEXT("missing Timer native port is rejected atomically"), AuthoredB->Get_View()->TryReload(
        Markup.Replace(TEXT("timer-direction-port"), TEXT("timer-missing-port")), Stylesheet,
        TEXT("Timer rejected candidate")).Succeeded);
    TestTrue(TEXT("reloads preserve accepted identity and rejected tree"),
        AuthoredA->Get_View()->GetRevision() > Revision
            && &AuthoredB->Get_View()->GetRegion(TEXT("main")).Get() == &MainBefore.Get()
            && AuthoredB->Get_View()->GetRevision() == RejectedRevision);

    TimerA.Try_Remove<ck::FFragment_Timer_Current>();
    RenderedA->SlatePrepass();
    TestTrue(TEXT("Current-only removal invalidates held Timer controls"),
        ck::IsValid(TimerA) && TimerA.Has<ck::FFragment_Timer_Params>() && NOT Inspector.CanInspect(TimerA)
            && NOT PauseButton->IsEnabled() && NOT JumpInput->IsEnabled());
    PauseButton->SimulateClick();
    AuthoredA->Commit_Jump(9.0f);
    TestFalse(TEXT("held Timer controls cannot enqueue after composition loss"),
        TimerA.Has<ck::FFragment_Timer_Requests>());

    TSharedPtr<SCkInspector_TimerAuthored> DestructorAuthored;
    TWeakPtr<FCkUiView> DestructorView;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_Timer>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_TimerAuthored>(DestructorInspector->Build_Inspector(TimerB));
        DestructorView = DestructorAuthored->Get_View();
    }
    TestTrue(TEXT("Timer inspector destruction releases its authored ports and view"),
        DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted() && NOT DestructorView.IsValid());
    Inspector.OnDeactivated();
    TestTrue(TEXT("Timer deactivation releases every retained authored build"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT EditGuard->Get_HasActiveEdit());
    CompleteButton->SimulateClick();
    TestFalse(TEXT("held Timer actions remain inert after deactivation"), TimerB.Has<ck::FFragment_Timer_Requests>());
    return true;
}

#endif
