#include "CkInspector_Timer.h"

#include "CkCore/Chrono/CkChrono.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Time/CkTime.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkTimer/CkTimer_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Timer)

// =====================================================================================================================

namespace ck_inspector_timer
{
    auto TryGetTimer(const FCk_Handle& InEntity, FCk_Handle_Timer& OutTimer) -> bool
    {
        OutTimer = {};
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_Timer_UE::Has(InEntity)) { return false; }
        auto MutableEntity = InEntity;
        OutTimer = UCk_Utils_Timer_UE::Cast(MutableEntity);
        return ck::IsValid(OutTimer)
            && OutTimer.Has<ck::FFragment_Timer_Params>()
            && OutTimer.Has<ck::FFragment_Timer_Current>();
    }

    // Elapsed / goal, matching the ratio the numeric rows report. A zero (or negative) goal has no meaningful
    // progress — read it as empty rather than dividing by zero.
    auto Get_Progress(
        const FCk_Handle_Timer& InTimer)
        -> float
    {
        if (ck::Is_NOT_Valid(InTimer))
        { return 0.0f; }

        const auto Chrono  = UCk_Utils_Timer_UE::Get_CurrentTimerValue(InTimer);
        const auto GoalMs  = Chrono.Get_GoalValue().Get_Milliseconds();

        if (GoalMs <= 0.0)
        { return 0.0f; }

        return FMath::Clamp(static_cast<float>(Chrono.Get_TimeElapsed().Get_Milliseconds() / GoalMs), 0.0f, 1.0f);
    }

    // The meter's tone is fixed at compose time, so it cannot dim while the timer is paused — a full-brightness bar
    // that is not advancing reads as a live timer. This pill carries that signal instead, and states the reason.
    auto Get_StateTone(
        const FCk_Handle_Timer& InTimer)
        -> ECk_Tone
    {
        if (ck::Is_NOT_Valid(InTimer))
        { return ECk_Tone::Neutral; }

        return UCk_Utils_Timer_UE::Get_CurrentState(InTimer) == ECk_Timer_State::Running
            ? ECk_Tone::Accent
            : ECk_Tone::Neutral;
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// =====================================================================================================================

auto SCkInspector_TimerAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _EditGuard = InArgs._EditGuard;
    _DiffLabels = InArgs._DiffLabels;

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_AuthoredView())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_TimerAuthored::~SCkInspector_TimerAuthored()
{
    Release();
}

auto SCkInspector_TimerAuthored::Get_IsAvailable() const -> bool
{
    auto Timer = FCk_Handle_Timer{};
    return _Active && ck_inspector_timer::TryGetTimer(_Entity, Timer);
}

auto SCkInspector_TimerAuthored::Get_CanRequest() const -> bool
{
    return Get_IsAvailable()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_TimerAuthored::Get_RequestDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable()) { return TEXT("Timer is unavailable."); }
    return ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).Reason.ToString();
}

auto SCkInspector_TimerAuthored::Get_NameText() const -> FString
{
    auto Timer = FCk_Handle_Timer{};
    if (NOT ck_inspector_timer::TryGetTimer(_Entity, Timer)) { return TEXT("--"); }
    const FGameplayTag Name = UCk_Utils_Timer_UE::Get_Name(Timer);
    return Name.IsValid() ? Name.ToString() : TEXT("None");
}

auto SCkInspector_TimerAuthored::Get_DirectionText() const -> FString
{
    auto Timer = FCk_Handle_Timer{};
    return ck_inspector_timer::TryGetTimer(_Entity, Timer)
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_Timer_UE::Get_CountDirection(Timer)) : TEXT("--");
}

auto SCkInspector_TimerAuthored::Get_BehaviorText() const -> FString
{
    auto Timer = FCk_Handle_Timer{};
    return ck_inspector_timer::TryGetTimer(_Entity, Timer)
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_Timer_UE::Get_Behavior(Timer)) : TEXT("--");
}

auto SCkInspector_TimerAuthored::Get_StateText() const -> FString
{
    auto Timer = FCk_Handle_Timer{};
    return ck_inspector_timer::TryGetTimer(_Entity, Timer)
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_Timer_UE::Get_CurrentState(Timer)) : TEXT("--");
}

auto SCkInspector_TimerAuthored::Get_StateForeground() const -> FLinearColor
{
    auto Timer = FCk_Handle_Timer{};
    return ck_inspector_timer::TryGetTimer(_Entity, Timer)
        ? CkStyle::GetToneColor(ck_inspector_timer::Get_StateTone(Timer)) : CkStyle::None();
}

auto SCkInspector_TimerAuthored::Get_StateBackground() const -> FLinearColor
{
    auto Timer = FCk_Handle_Timer{};
    return ck_inspector_timer::TryGetTimer(_Entity, Timer)
        ? CkStyle::GetToneDimColor(ck_inspector_timer::Get_StateTone(Timer)) : CkStyle::None();
}

auto SCkInspector_TimerAuthored::Get_GoalText() const -> FString
{
    auto Timer = FCk_Handle_Timer{};
    return ck_inspector_timer::TryGetTimer(_Entity, Timer)
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_Timer_UE::Get_CurrentTimerValue(Timer).Get_GoalValue()) : TEXT("--");
}

auto SCkInspector_TimerAuthored::Get_ElapsedText() const -> FString
{
    auto Timer = FCk_Handle_Timer{};
    if (NOT ck_inspector_timer::TryGetTimer(_Entity, Timer)) { return TEXT("--"); }
    const FCk_Chrono Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(Timer);
    return ck::Format_UE(TEXT("{} / {}"), Chrono.Get_TimeElapsed(), Chrono.Get_GoalValue());
}

auto SCkInspector_TimerAuthored::Get_ElapsedFraction() const -> float
{
    auto Timer = FCk_Handle_Timer{};
    return ck_inspector_timer::TryGetTimer(_Entity, Timer) ? ck_inspector_timer::Get_Progress(Timer) : 0.0f;
}

auto SCkInspector_TimerAuthored::Get_RemainingText() const -> FString
{
    auto Timer = FCk_Handle_Timer{};
    return ck_inspector_timer::TryGetTimer(_Entity, Timer)
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_Timer_UE::Get_CurrentTimerValue(Timer).Get_TimeRemaining()) : TEXT("--");
}

auto SCkInspector_TimerAuthored::Get_DoneText() const -> FString
{
    auto Timer = FCk_Handle_Timer{};
    return ck_inspector_timer::TryGetTimer(_Entity, Timer)
        ? (UCk_Utils_Timer_UE::Get_CurrentTimerValue(Timer).Get_IsDone() ? TEXT("Yes") : TEXT("No")) : TEXT("--");
}

auto SCkInspector_TimerAuthored::Get_DoneColor() const -> FLinearColor
{
    auto Timer = FCk_Handle_Timer{};
    if (NOT ck_inspector_timer::TryGetTimer(_Entity, Timer)) { return CkStyle::None(); }
    return UCk_Utils_Timer_UE::Get_CurrentTimerValue(Timer).Get_IsDone()
        ? CkStyle::Value_Bool_True() : CkStyle::Value_Bool_False();
}

auto SCkInspector_TimerAuthored::Request_Manipulate(const ECk_Timer_Manipulate InManipulate) -> void
{
    auto Timer = FCk_Handle_Timer{};
    if (NOT Get_CanRequest() || NOT ck_inspector_timer::TryGetTimer(_Entity, Timer)) { return; }
    switch (InManipulate)
    {
        case ECk_Timer_Manipulate::Reset: UCk_Utils_Timer_UE::Request_Reset(Timer, {}); break;
        case ECk_Timer_Manipulate::Complete: UCk_Utils_Timer_UE::Request_Complete(Timer, {}); break;
        case ECk_Timer_Manipulate::Stop: UCk_Utils_Timer_UE::Request_Stop(Timer, {}); break;
        case ECk_Timer_Manipulate::Pause: UCk_Utils_Timer_UE::Request_Pause(Timer, {}); break;
        case ECk_Timer_Manipulate::Resume: UCk_Utils_Timer_UE::Request_Resume(Timer, {}); break;
    }
}

auto SCkInspector_TimerAuthored::Request_Reverse() -> void
{
    auto Timer = FCk_Handle_Timer{};
    if (Get_CanRequest() && ck_inspector_timer::TryGetTimer(_Entity, Timer))
    { UCk_Utils_Timer_UE::Request_ReverseDirection(Timer, {}); }
}

auto SCkInspector_TimerAuthored::Commit_Direction(const int32 InIndex) -> void
{
    auto Timer = FCk_Handle_Timer{};
    if (Get_CanRequest() && ck_inspector_timer::TryGetTimer(_Entity, Timer))
    {
        UCk_Utils_Timer_UE::Request_ChangeCountDirection(
            Timer, InIndex == 1 ? ECk_Timer_CountDirection::CountDown : ECk_Timer_CountDirection::CountUp, {});
    }
}

auto SCkInspector_TimerAuthored::Commit_JumpMode(const int32 InIndex) -> void
{
    if (_Active) { _JumpMode = InIndex == 1 ? ECk_RelativeAbsolute::Absolute : ECk_RelativeAbsolute::Relative; }
}

auto SCkInspector_TimerAuthored::Commit_Jump(const float InSeconds) -> void
{
    auto Timer = FCk_Handle_Timer{};
    if (NOT Get_CanRequest() || NOT ck_inspector_timer::TryGetTimer(_Entity, Timer)) { return; }
    _JumpSeconds = InSeconds;
    UCk_Utils_Timer_UE::Request_Jump(
        Timer, FCk_Request_Timer_Jump{FCk_Time{InSeconds}}.Set_JumpMode(_JumpMode), {});
}

auto SCkInspector_TimerAuthored::Commit_Consume(const float InSeconds) -> void
{
    auto Timer = FCk_Handle_Timer{};
    if (NOT Get_CanRequest() || NOT ck_inspector_timer::TryGetTimer(_Entity, Timer)) { return; }
    _ConsumeSeconds = InSeconds;
    UCk_Utils_Timer_UE::Request_Consume(Timer, FCk_Request_Timer_Consume{FCk_Time{InSeconds}}, {});
}

auto SCkInspector_TimerAuthored::Present_EditControl(
    const TSharedRef<SWidget> InInput,
    const TAttribute<FText> InReadOnlyText) -> TSharedRef<SWidget>
{
    const FCkDebuggerStyleSelection& Selection = UCkDebuggerStyleSettings::Get_Selection();
    if (NOT ck::debug_axes::EditControls_AreVisible(Selection))
    { return SNew(STextBlock).Text(InReadOnlyText); }
    if (NOT ck::debug_axes::EditControls_RevealOnHover(Selection)) { return InInput; }

    const TSharedRef<SBox> HoverHost = SNew(SBox);
    const TWeakPtr<SBox> WeakHoverHost{HoverHost};
    const auto IsHovered = [WeakHoverHost]()
    { const TSharedPtr<SBox> Host = WeakHoverHost.Pin(); return Host.IsValid() && Host->IsHovered(); };
    const TSharedRef<STextBlock> ReadOnly = SNew(STextBlock).Text(InReadOnlyText);
    ReadOnly->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Hidden : EVisibility::Visible; }));
    InInput->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Visible : EVisibility::Hidden; }));
    HoverHost->SetContent(SNew(SOverlay) + SOverlay::Slot()[ReadOnly] + SOverlay::Slot()[InInput]);
    return HoverHost;
}

auto SCkInspector_TimerAuthored::Build_DirectionValue() -> TSharedRef<SWidget>
{
    _DirectionOptions = {MakeShared<FString>(TEXT("CountUp")), MakeShared<FString>(TEXT("CountDown"))};
    const TWeakPtr<SCkInspector_TimerAuthored> WeakWidget{SharedThis(this)};
    const TSharedRef<SComboBox<TSharedPtr<FString>>> Combo = SNew(SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&_DirectionOptions)
        .OnGenerateWidget_Lambda([](const TSharedPtr<FString> Item)
        { return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString{})); })
        .OnSelectionChanged_Lambda([WeakWidget](const TSharedPtr<FString> Item, const ESelectInfo::Type SelectInfo)
        {
            if (SelectInfo != ESelectInfo::Direct && Item.IsValid())
            { if (const TSharedPtr<SCkInspector_TimerAuthored> Widget = WeakWidget.Pin(); Widget.IsValid())
                { Widget->Commit_Direction(*Item == TEXT("CountDown") ? 1 : 0); } }
        })
        [SNew(STextBlock).Text_Lambda([WeakWidget]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_DirectionText()) : FText::GetEmpty(); })];
    const TSharedRef<SBox> Input = SNew(SBox).Tag(TEXT("timer-direction-input")).IsEnabled_Lambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); })[Combo];
    Input->SetToolTipText(TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty(); }));
    return Present_EditControl(Input, TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_DirectionText()) : FText::GetEmpty(); }));
}

auto SCkInspector_TimerAuthored::Build_JumpModeValue() -> TSharedRef<SWidget>
{
    _JumpModeOptions = {MakeShared<FString>(TEXT("Relative")), MakeShared<FString>(TEXT("Absolute"))};
    const TWeakPtr<SCkInspector_TimerAuthored> WeakWidget{SharedThis(this)};
    const TSharedRef<SComboBox<TSharedPtr<FString>>> Combo = SNew(SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&_JumpModeOptions)
        .OnGenerateWidget_Lambda([](const TSharedPtr<FString> Item)
        { return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString{})); })
        .OnSelectionChanged_Lambda([WeakWidget](const TSharedPtr<FString> Item, const ESelectInfo::Type SelectInfo)
        {
            if (SelectInfo != ESelectInfo::Direct && Item.IsValid())
            { if (const TSharedPtr<SCkInspector_TimerAuthored> Widget = WeakWidget.Pin(); Widget.IsValid())
                { Widget->Commit_JumpMode(*Item == TEXT("Absolute") ? 1 : 0); } }
        })
        [SNew(STextBlock).Text_Lambda([WeakWidget]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() ? FText::FromString(Widget->_JumpMode == ECk_RelativeAbsolute::Absolute
                ? TEXT("Absolute") : TEXT("Relative")) : FText::GetEmpty();
        })];
    const TSharedRef<SBox> Input = SNew(SBox).Tag(TEXT("timer-jump-mode-input"))[Combo];
    return Present_EditControl(Input, TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->_JumpMode == ECk_RelativeAbsolute::Absolute
            ? TEXT("Absolute") : TEXT("Relative")) : FText::GetEmpty();
    }));
}

auto SCkInspector_TimerAuthored::Build_NumberValue(const FName InTag, const bool bJump) -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_TimerAuthored> WeakWidget{SharedThis(this)};
    const TSharedPtr<FCkInspectorEditScope> Scope = MakeShared<FCkInspectorEditScope>(_EditGuard);
    _EditScopes.Add(Scope);
    const TSharedRef<SCkDebug_NumericEditor> Editor = SNew(SCkDebug_NumericEditor)
        .Tag(InTag)
        .Value_Lambda([WeakWidget, bJump]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? static_cast<double>(bJump ? Widget->_JumpSeconds : Widget->_ConsumeSeconds) : 0.0; })
        .Kind(ECkDebug_NumericKind::Float)
        .Width(72.0f)
        .ForegroundColor(CkStyle::Value_Numeric())
        .OnValueCommitted_Lambda([WeakWidget, bJump](const double InValue)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            { bJump ? Widget->Commit_Jump(static_cast<float>(InValue)) : Widget->Commit_Consume(static_cast<float>(InValue)); } })
        .OnEditStateChanged_Lambda([Scope](const bool bEditing)
        { if (Scope.IsValid()) { Scope->Set_Active(bEditing); } });
    Editor->SetEnabled(TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Editor->SetToolTipText(TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty(); }));
    const TSharedRef<SWidget> Input = Editor;
    return Present_EditControl(Input, TAttribute<FText>::CreateLambda([WeakWidget, bJump]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::AsNumber(bJump ? Widget->_JumpSeconds : Widget->_ConsumeSeconds) : FText::GetEmpty();
    }));
}

auto SCkInspector_TimerAuthored::Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>
{
    const TSharedRef<SBox> Port = SNew(SBox);
    Port->SetContent(MoveTemp(InLeaf));
    _NativePorts.Add(Port);
    return Port;
}

auto SCkInspector_TimerAuthored::Detach_NativePorts() -> void
{
    for (const TSharedPtr<SBox>& Port : _NativePorts)
    { if (Port.IsValid()) { Port->SetContent(SNullWidget::NullWidget); } }
    _NativePorts.Reset();
}

auto SCkInspector_TimerAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_TimerAuthored> WeakWidget{SharedThis(this)};
    auto Ports = FCkUiView::FNativeBindings{};
    Ports.Add(TEXT("timer-direction-port"), Make_NativePort(Build_DirectionValue()));
    Ports.Add(TEXT("timer-jump-mode-port"), Make_NativePort(Build_JumpModeValue()));
    Ports.Add(TEXT("timer-jump-port"), Make_NativePort(Build_NumberValue(TEXT("timer-jump-input"), true)));
    Ports.Add(TEXT("timer-consume-port"), Make_NativePort(Build_NumberValue(TEXT("timer-consume-input"), false)));

    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, WeakWidget](const FString& InName, FString (SCkInspector_TimerAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakWidget, InGetter]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString((Widget.Get()->*InGetter)()) : FText::GetEmpty();
        }));
    };
    BindText(TEXT("timer-name"), &SCkInspector_TimerAuthored::Get_NameText);
    BindText(TEXT("timer-direction"), &SCkInspector_TimerAuthored::Get_DirectionText);
    BindText(TEXT("timer-behavior"), &SCkInspector_TimerAuthored::Get_BehaviorText);
    BindText(TEXT("timer-state"), &SCkInspector_TimerAuthored::Get_StateText);
    BindText(TEXT("timer-goal"), &SCkInspector_TimerAuthored::Get_GoalText);
    BindText(TEXT("timer-elapsed"), &SCkInspector_TimerAuthored::Get_ElapsedText);
    BindText(TEXT("timer-remaining"), &SCkInspector_TimerAuthored::Get_RemainingText);
    BindText(TEXT("timer-done"), &SCkInspector_TimerAuthored::Get_DoneText);
    Data.Text.Add(TEXT("timer-pause-label"), FText::FromString(TEXT("Pause")));
    Data.Text.Add(TEXT("timer-resume-label"), FText::FromString(TEXT("Resume")));
    Data.Text.Add(TEXT("timer-stop-label"), FText::FromString(TEXT("Stop")));
    Data.Text.Add(TEXT("timer-reset-label"), FText::FromString(TEXT("Reset")));
    Data.Text.Add(TEXT("timer-complete-label"), FText::FromString(TEXT("Complete")));
    Data.Text.Add(TEXT("timer-reverse-label"), FText::FromString(TEXT("Reverse")));
    Data.Text.Add(TEXT("timer-pause-tooltip"), FText::FromString(TEXT("Request_Pause — stops advancing, keeps the elapsed value")));
    Data.Text.Add(TEXT("timer-resume-tooltip"), FText::FromString(TEXT("Request_Resume — resumes advancing from the current elapsed value")));
    Data.Text.Add(TEXT("timer-stop-tooltip"), FText::FromString(TEXT("Request_Stop — pauses and rewinds to the starting value")));
    Data.Text.Add(TEXT("timer-reset-tooltip"), FText::FromString(TEXT("Request_Reset — rewinds while retaining run state")));
    Data.Text.Add(TEXT("timer-complete-tooltip"), FText::FromString(TEXT("Request_Complete — jumps to Done and fires completion signals")));
    Data.Text.Add(TEXT("timer-reverse-tooltip"), FText::FromString(TEXT("Request_ReverseDirection — flips CountUp and CountDown")));
    Data.Text.Add(TEXT("timer-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty(); }));
    Data.Number.Add(TEXT("timer-elapsed-fraction"), TAttribute<float>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_ElapsedFraction() : 0.0f; }));
    Data.Visibility.Add(TEXT("timer-can-request"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); });
    Data.Color.Add(TEXT("timer-state-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_StateForeground() : FLinearColor::Transparent; }));
    Data.Color.Add(TEXT("timer-state-background"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_StateBackground() : FLinearColor::Transparent; }));
    Data.Color.Add(TEXT("timer-done-color"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_DoneColor() : FLinearColor::Transparent; }));
    Data.Color.Add(TEXT("timer-elapsed-fill"), CkStyle::GetToneColor(ECk_Tone::Accent));
    for (const FString& Label : {TEXT("Name:"), TEXT("Direction:"), TEXT("Behavior:"), TEXT("State:"),
        TEXT("Goal:"), TEXT("Elapsed:"), TEXT("Remaining:"), TEXT("Done:"), TEXT("Playback:"),
        TEXT("Position:"), TEXT("Set Direction:"), TEXT("Jump Mode:"), TEXT("Jump (s):"), TEXT("Consume (s):")})
    {
        const FString Key = TEXT("timer-") + Label.LeftChop(1).Replace(TEXT(" "), TEXT("-")).Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT("")).ToLower() + TEXT("-diff-color");
        Data.Color.Add(Key, TAttribute<FLinearColor>::CreateLambda([WeakWidget, Label]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_timer::DiffColor(Widget->Is_DiffMarked(Label)) : FLinearColor::Transparent;
        }));
    }

    auto Actions = FCkUiView::FActions{};
    const auto AddManipulate = [&Actions, WeakWidget](const FString& InName, const ECk_Timer_Manipulate InManipulate)
    {
        Actions.Add(InName, FSimpleDelegate::CreateLambda([WeakWidget, InManipulate]()
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Request_Manipulate(InManipulate); } }));
    };
    AddManipulate(TEXT("timer-pause"), ECk_Timer_Manipulate::Pause);
    AddManipulate(TEXT("timer-resume"), ECk_Timer_Manipulate::Resume);
    AddManipulate(TEXT("timer-stop"), ECk_Timer_Manipulate::Stop);
    AddManipulate(TEXT("timer-reset"), ECk_Timer_Manipulate::Reset);
    AddManipulate(TEXT("timer-complete"), ECk_Timer_Manipulate::Complete);
    Actions.Add(TEXT("timer-reverse"), FSimpleDelegate::CreateLambda([WeakWidget]()
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Request_Reverse(); } }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(Ports), MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorTimer.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorTimer.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }
    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_TimerAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_TimerAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    Detach_NativePorts();
    for (const TSharedPtr<FCkInspectorEditScope>& Scope : _EditScopes)
    { if (Scope.IsValid()) { Scope->Set_Active(false); } }
    _EditScopes.Reset();
    _DirectionOptions.Reset();
    _JumpModeOptions.Reset();
    _View.Reset();
    _Mounted = false;
}

// =====================================================================================================================

auto FCkInspector_Timer::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Timer"));
}

auto FCkInspector_Timer::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Timer_UE::Has(Entity);
}

auto FCkInspector_Timer::Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto MutableEntity = Entity;
    const auto TimerHandle = UCk_Utils_Timer_UE::Cast(MutableEntity);

    if (ck::Is_NOT_Valid(TimerHandle))
    { return Builder.Build(Entity, FString()); }

    const auto CapturedTimer = TimerHandle;

    // ---- Identity ----

    Builder.AddRow(
        FText::FromString(TEXT("Name:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto& Name = UCk_Utils_Timer_UE::Get_Name(CapturedTimer);
            return FText::FromString(Name.IsValid() ? Name.ToString() : TEXT("None"));
        },
        CkStyle::Value_Tag());

    // ---- Configuration ----

    Builder.AddRow(
        FText::FromString(TEXT("Direction:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Direction = UCk_Utils_Timer_UE::Get_CountDirection(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Direction));
        },
        CkStyle::Value_Enum());

    Builder.AddRow(
        FText::FromString(TEXT("Behavior:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Behavior = UCk_Utils_Timer_UE::Get_Behavior(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Behavior));
        },
        CkStyle::Value_Enum());

    // ---- Live state ----

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda([CapturedTimer]()
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto State = UCk_Utils_Timer_UE::Get_CurrentState(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), State));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedTimer]()
        {
            return ck_inspector_timer::Get_StateTone(CapturedTimer);
        }));

    Builder.AddRow(
        FText::FromString(TEXT("Goal:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Chrono.Get_GoalValue()));
        },
        CkStyle::Value_Numeric());

    Builder.AddMeterRow(
        FText::FromString(TEXT("Elapsed:")),
        TAttribute<float>::CreateLambda([CapturedTimer]()
        {
            return ck_inspector_timer::Get_Progress(CapturedTimer);
        }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedTimer]()
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{} / {}"), Chrono.Get_TimeElapsed(), Chrono.Get_GoalValue()));
        }));

    Builder.AddRow(
        FText::FromString(TEXT("Remaining:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Chrono.Get_TimeRemaining()));
        },
        CkStyle::Value_Numeric());

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Done:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return FText::FromString(Chrono.Get_IsDone() ? TEXT("Yes") : TEXT("No"));
        },
        [CapturedTimer](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return CkStyle::None(); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return Chrono.Get_IsDone()
                ? CkStyle::Value_Bool_True()
                : CkStyle::Value_Bool_False();
        });

    // ---- Controls ----
    //
    // Every verb below leaves through the public Timer Utils with the typed handle captured BY VALUE
    // and re-validated on fire. Nothing reads a request back: the live rows above are the display, and
    // the queued verbs land a frame later.
    //
    // All of them are LocalOk — FProcessor_Timer_HandleRequests runs on every net mode, and a timer
    // that is not replicated at all is the common case.

    Builder.AddHeader(FText::FromString(TEXT("Controls")));

    Builder.AddActionRow(
        FText::FromString(TEXT("Playback:")),
        {
            FCkInspector_Action
            {
                FText::FromString(TEXT("Pause")),
                FText::FromString(TEXT("Request_Pause — stops advancing, keeps the elapsed value")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_Pause(Timer, {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Resume")),
                FText::FromString(TEXT("Request_Resume — resumes advancing from the current elapsed value")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_Resume(Timer, {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Stop")),
                FText::FromString(TEXT("Request_Stop — pauses AND rewinds to the starting value")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_Stop(Timer, {});
                }
            }
        });

    Builder.AddActionRow(
        FText::FromString(TEXT("Position:")),
        {
            FCkInspector_Action
            {
                FText::FromString(TEXT("Reset")),
                FText::FromString(TEXT("Request_Reset — rewinds to the starting value, leaving the run state alone")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_Reset(Timer, {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Complete")),
                FText::FromString(TEXT("Request_Complete — jumps straight to Done and fires the timer's completion signals")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_Complete(Timer, {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Reverse")),
                FText::FromString(TEXT("Request_ReverseDirection — flips CountUp <-> CountDown immediately (no queue)")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_ReverseDirection(Timer, {});
                }
            }
        });

    // Immediate mutator, so the read-only "Direction:" row above agrees with this dropdown on the very
    // next paint rather than a frame later.
    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Set Direction:")),
        {
            FText::FromString(TEXT("CountUp")),
            FText::FromString(TEXT("CountDown"))
        },
        TAttribute<int32>::CreateLambda([CapturedTimer]()
        {
            if (ck::Is_NOT_Valid(CapturedTimer))
            { return 0; }

            return static_cast<int32>(UCk_Utils_Timer_UE::Get_CountDirection(CapturedTimer));
        }),
        [CapturedTimer](int32 InIndex)
        {
            auto Timer = CapturedTimer;
            if (ck::Is_NOT_Valid(Timer))
            { return; }

            UCk_Utils_Timer_UE::Request_ChangeCountDirection(
                Timer, static_cast<ECk_Timer_CountDirection>(InIndex), {});
        });

    // Jump mode is a pending ARGUMENT, not timer state — it selects how the next committed jump amount
    // is read (delta vs target elapsed), so it writes only to this inspector's box.
    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Jump Mode:")),
        {
            FText::FromString(TEXT("Relative")),
            FText::FromString(TEXT("Absolute"))
        },
        TAttribute<int32>::CreateLambda([Mode = _JumpMode]()
        {
            return static_cast<int32>(*Mode);
        }),
        [Mode = _JumpMode](int32 InIndex)
        {
            *Mode = static_cast<ECk_RelativeAbsolute>(InIndex);
        });

    // Committing the amount IS the verb — the editor commits on enter / lost focus, never per
    // keystroke, so one jump is enqueued per deliberate edit.
    Builder.AddNumericRow(
        FText::FromString(TEXT("Jump (s):")),
        TAttribute<float>::CreateLambda([Seconds = _JumpSeconds]() { return *Seconds; }),
        [CapturedTimer, Seconds = _JumpSeconds, Mode = _JumpMode](float InSeconds)
        {
            *Seconds = InSeconds;

            auto Timer = CapturedTimer;
            if (ck::Is_NOT_Valid(Timer))
            { return; }

            UCk_Utils_Timer_UE::Request_Jump(
                Timer,
                FCk_Request_Timer_Jump{FCk_Time{InSeconds}}.Set_JumpMode(*Mode),
                {});
        });

    Builder.AddNumericRow(
        FText::FromString(TEXT("Consume (s):")),
        TAttribute<float>::CreateLambda([Seconds = _ConsumeSeconds]() { return *Seconds; }),
        [CapturedTimer, Seconds = _ConsumeSeconds](float InSeconds)
        {
            *Seconds = InSeconds;

            auto Timer = CapturedTimer;
            if (ck::Is_NOT_Valid(Timer))
            { return; }

            UCk_Utils_Timer_UE::Request_Consume(Timer, FCk_Request_Timer_Consume{FCk_Time{InSeconds}}, {});
        });

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_Timer::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : {TEXT("Name:"), TEXT("Direction:"), TEXT("Behavior:"), TEXT("State:"),
        TEXT("Goal:"), TEXT("Elapsed:"), TEXT("Remaining:"), TEXT("Done:"), TEXT("Playback:"),
        TEXT("Position:"), TEXT("Set Direction:"), TEXT("Jump Mode:"), TEXT("Jump (s):"), TEXT("Consume (s):")})
    { if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); } }

    const TSharedRef<SCkInspector_TimerAuthored> Authored = SNew(SCkInspector_TimerAuthored)
        .Entity(Entity)
        .EditGuard(Get_EditGuard())
        .DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

FCkInspector_Timer::~FCkInspector_Timer()
{
    OnDeactivated();
}

auto FCkInspector_Timer::Tick(const FCk_Handle&, const float) -> void
{
    _LastAuthoredLoadError.Reset();
    for (const TWeakPtr<SCkInspector_TimerAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_TimerAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()
            && NOT Instance->Get_LoadError().IsEmpty())
        {
            _LastAuthoredLoadError = Instance->Get_LoadError();
            break;
        }
    }
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_TimerAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_Timer::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_TimerAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_TimerAuthored> Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}

// =====================================================================================================================
