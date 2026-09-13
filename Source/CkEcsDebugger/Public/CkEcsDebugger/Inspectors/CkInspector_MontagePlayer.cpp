#include "CkInspector_MontagePlayer.h"

#include "CkAnimation/MontagePlayer/CkMontagePlayer_Fragment.h"
#include "CkAnimation/MontagePlayer/CkMontagePlayer_Utils.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_MontagePlayer)

namespace ck_inspector_montage_player
{
auto IsDestroying(const FCk_Handle &InEntity) -> bool
{
    return ck::Is_NOT_Valid(InEntity) ||
           InEntity.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
                            ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
                            ck::FTag_DestroyEntity_Finalize>();
}

auto HasCurrent(const FCk_Handle &InEntity) -> bool
{
    return NOT IsDestroying(InEntity) && InEntity.Has<ck::FFragment_MontagePlayer_Current>();
}

auto TryGetMontagePlayer(const FCk_Handle &InEntity, FCk_Handle_MontagePlayer &OutPlayer) -> bool
{
    OutPlayer = {};
    if (IsDestroying(InEntity) || NOT InEntity.Has<ck::FFragment_MontagePlayer_Params>() ||
        NOT InEntity.Has<ck::FFragment_MontagePlayer_Current>())
    {
        return false;
    }
    auto Mutable = InEntity;
    OutPlayer = UCk_Utils_MontagePlayer_UE::Cast(Mutable);
    return ck::IsValid(OutPlayer) && OutPlayer.Has<ck::FFragment_MontagePlayer_Params>() &&
           OutPlayer.Has<ck::FFragment_MontagePlayer_Current>();
}

auto GetRequestGate(const FCk_Handle &InEntity) -> FCk_DebugRequest_GateVerdict
{
    FCk_Handle_MontagePlayer Player;
    if (NOT TryGetMontagePlayer(InEntity, Player))
    {
        return {false, FText::FromString(TEXT("Montage Player controls are unavailable."))};
    }
    return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::AuthorityOnly);
}

auto GetTone(const ECk_MontagePlayer_StateKind InKind) -> ECk_Tone
{
    switch (InKind)
    {
    case ECk_MontagePlayer_StateKind::Play:
    case ECk_MontagePlayer_StateKind::Resume:
        return ECk_Tone::Ok;
    case ECk_MontagePlayer_StateKind::Pause:
        return ECk_Tone::Warn;
    case ECk_MontagePlayer_StateKind::JumpToSection:
        return ECk_Tone::Info;
    default:
        return ECk_Tone::Neutral;
    }
}

auto GetStateText(const ECk_MontagePlayer_StateKind InKind) -> FString
{
    switch (InKind)
    {
    case ECk_MontagePlayer_StateKind::Play:
        return TEXT("Play");
    case ECk_MontagePlayer_StateKind::Stop:
        return TEXT("Stop");
    case ECk_MontagePlayer_StateKind::Pause:
        return TEXT("Pause");
    case ECk_MontagePlayer_StateKind::Resume:
        return TEXT("Resume");
    case ECk_MontagePlayer_StateKind::JumpToSection:
        return TEXT("JumpToSection");
    default:
        return TEXT("Unknown");
    }
}

auto GetPlayback(const FCk_Handle &InEntity, float &OutLength) -> float
{
    OutLength = 0.0f;
    if (NOT HasCurrent(InEntity))
    {
        return 0.0f;
    }
    const auto &Current = InEntity.Get<ck::FFragment_MontagePlayer_Current>();
    const UAnimMontage *Montage = Current.Get_ActiveMontage().Get();
    const UAnimInstance *AnimInstance = Current.Get_LastSeenAnimInstance().Get();
    if (ck::Is_NOT_Valid(Montage, ck::IsValid_Policy_NullptrOnly{}) ||
        ck::Is_NOT_Valid(AnimInstance, ck::IsValid_Policy_NullptrOnly{}))
    {
        return 0.0f;
    }
    OutLength = Montage->GetPlayLength();
    return AnimInstance->Montage_GetPosition(Montage);
}

auto DiffColor(const bool bDiff) -> FLinearColor { return bDiff ? CkStyle::Accent() : CkStyle::Text(); }
} // namespace ck_inspector_montage_player

auto SCkInspector_MontagePlayerAuthored::Construct(const FArguments &InArgs) -> void
{
    _Entity = InArgs._Entity;
    _EditGuard = InArgs._EditGuard;
    _DiffLabels = InArgs._DiffLabels;
    FCk_Handle_MontagePlayer Player;
    if (ck_inspector_montage_player::TryGetMontagePlayer(_Entity, Player))
    {
        _PendingSection = UCk_Utils_MontagePlayer_UE::Get_CurrentSection(Player);
    }
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_MontagePlayerAuthored::~SCkInspector_MontagePlayerAuthored() { Release(); }

auto SCkInspector_MontagePlayerAuthored::Get_IsAvailable() const -> bool
{
    return _Active && ck_inspector_montage_player::HasCurrent(_Entity);
}

auto SCkInspector_MontagePlayerAuthored::Get_HasControls() const -> bool
{
    FCk_Handle_MontagePlayer Player;
    return _Active && ck_inspector_montage_player::TryGetMontagePlayer(_Entity, Player);
}

auto SCkInspector_MontagePlayerAuthored::Get_StateText() const -> FString
{
    return Get_IsAvailable() ? ck_inspector_montage_player::GetStateText(
                                   _Entity.Get<ck::FFragment_MontagePlayer_Current>().Get_State().Get_Kind())
                             : TEXT("--");
}

auto SCkInspector_MontagePlayerAuthored::Get_StateForeground() const -> FLinearColor
{
    return Get_IsAvailable() ? CkStyle::GetToneColor(ck_inspector_montage_player::GetTone(
                                   _Entity.Get<ck::FFragment_MontagePlayer_Current>().Get_State().Get_Kind()))
                             : FLinearColor::Transparent;
}

auto SCkInspector_MontagePlayerAuthored::Get_StateBackground() const -> FLinearColor
{
    return Get_IsAvailable() ? CkStyle::GetToneDimColor(ck_inspector_montage_player::GetTone(
                                   _Entity.Get<ck::FFragment_MontagePlayer_Current>().Get_State().Get_Kind()))
                             : FLinearColor::Transparent;
}

auto SCkInspector_MontagePlayerAuthored::Get_ActiveMontageText() const -> FString
{
    if (NOT Get_IsAvailable())
    {
        return TEXT("--");
    }
    const TWeakObjectPtr<UAnimMontage> Montage = _Entity.Get<ck::FFragment_MontagePlayer_Current>().Get_ActiveMontage();
    return Montage.IsValid() ? Montage->GetFName().ToString() : TEXT("(none)");
}

auto SCkInspector_MontagePlayerAuthored::Get_ActiveMontageColor() const -> FLinearColor
{
    if (NOT Get_IsAvailable())
    { return CkStyle::None(); }
    return _Entity.Get<ck::FFragment_MontagePlayer_Current>().Get_ActiveMontage().IsValid()
        ? CkStyle::Value_Object()
        : CkStyle::TextMute();
}

auto SCkInspector_MontagePlayerAuthored::Get_PositionFraction() const -> float
{
    float Length = 0.0f;
    const float Position = ck_inspector_montage_player::GetPlayback(_Entity, Length);
    return Get_IsAvailable() && Length > 0.0f ? Position / Length : 0.0f;
}

auto SCkInspector_MontagePlayerAuthored::Get_PositionText() const -> FString
{
    float Length = 0.0f;
    const float Position = ck_inspector_montage_player::GetPlayback(_Entity, Length);
    return Get_IsAvailable() && Length > 0.0f ? ck::Format_UE(TEXT("{:.2f} / {:.2f}s"), Position, Length) : TEXT("--");
}

auto SCkInspector_MontagePlayerAuthored::Get_AnimInstanceText() const -> FString
{
    if (NOT Get_IsAvailable())
    { return TEXT("--"); }
    return _Entity.Get<ck::FFragment_MontagePlayer_Current>().Get_LastSeenAnimInstance().IsValid()
        ? TEXT("Valid")
        : TEXT("Invalid");
}

auto SCkInspector_MontagePlayerAuthored::Get_AnimInstanceColor() const -> FLinearColor
{
    if (NOT Get_IsAvailable())
    { return CkStyle::None(); }
    return _Entity.Get<ck::FFragment_MontagePlayer_Current>().Get_LastSeenAnimInstance().IsValid()
        ? CkStyle::Ok()
        : CkStyle::Err();
}

auto SCkInspector_MontagePlayerAuthored::Get_PlayRateText() const -> FString
{
    return Get_IsAvailable()
               ? ck::Format_UE(TEXT("{:.2f}"),
                               _Entity.Get<ck::FFragment_MontagePlayer_Current>().Get_State().Get_PlayRate())
               : TEXT("--");
}

auto SCkInspector_MontagePlayerAuthored::Get_CatchUpRemainingText() const -> FString
{
    return Get_IsAvailable()
               ? ck::Format_UE(TEXT("{:.3f} s"),
                               _Entity.Get<ck::FFragment_MontagePlayer_Current>().Get_CatchUpRemaining().Get_Seconds())
               : TEXT("--");
}

auto SCkInspector_MontagePlayerAuthored::Get_CanRequest() const -> bool
{
    return _Active && ck_inspector_montage_player::GetRequestGate(_Entity).IsEnabled;
}

auto SCkInspector_MontagePlayerAuthored::Get_RequestDisabledReason() const -> FString
{
    return _Active ? ck_inspector_montage_player::GetRequestGate(_Entity).Reason.ToString() : FString{};
}

auto SCkInspector_MontagePlayerAuthored::Commit_PendingBlendOut(const float InSeconds) -> void
{
    if (NOT _Active)
    {
        return;
    }
    _PendingBlendOut = FMath::Max(0.0f, InSeconds);
}

auto SCkInspector_MontagePlayerAuthored::Get_PendingSectionText() const -> FString
{
    return _PendingSection.IsNone() ? TEXT("(none)") : _PendingSection.ToString();
}

auto SCkInspector_MontagePlayerAuthored::Commit_PendingSection(const FName InSection) -> void
{
    if (NOT _Active)
    {
        return;
    }
    _PendingSection = InSection;
}

auto SCkInspector_MontagePlayerAuthored::Request_Pause() -> void
{
    FCk_Handle_MontagePlayer Player;
    if (_Active && ck_inspector_montage_player::TryGetMontagePlayer(_Entity, Player) &&
        ck_inspector_montage_player::GetRequestGate(_Entity).IsEnabled)
    {
        UCk_Utils_MontagePlayer_UE::Request_Pause(Player, FCk_Request_MontagePlayer_Pause{}, {});
    }
}

auto SCkInspector_MontagePlayerAuthored::Request_Resume() -> void
{
    FCk_Handle_MontagePlayer Player;
    if (_Active && ck_inspector_montage_player::TryGetMontagePlayer(_Entity, Player) &&
        ck_inspector_montage_player::GetRequestGate(_Entity).IsEnabled)
    {
        UCk_Utils_MontagePlayer_UE::Request_Resume(Player, FCk_Request_MontagePlayer_Resume{}, {});
    }
}

auto SCkInspector_MontagePlayerAuthored::Request_Stop() -> void
{
    FCk_Handle_MontagePlayer Player;
    if (_Active && ck_inspector_montage_player::TryGetMontagePlayer(_Entity, Player) &&
        ck_inspector_montage_player::GetRequestGate(_Entity).IsEnabled)
    {
        UCk_Utils_MontagePlayer_UE::Request_Stop(
            Player, FCk_Request_MontagePlayer_Stop{FCk_Time{static_cast<double>(FMath::Max(0.0f, _PendingBlendOut))}},
            {});
    }
}

auto SCkInspector_MontagePlayerAuthored::Request_Jump() -> void
{
    FCk_Handle_MontagePlayer Player;
    if (_Active && NOT _PendingSection.IsNone() && ck_inspector_montage_player::TryGetMontagePlayer(_Entity, Player) &&
        ck_inspector_montage_player::GetRequestGate(_Entity).IsEnabled)
    {
        UCk_Utils_MontagePlayer_UE::Request_JumpToSection(Player,
                                                          FCk_Request_MontagePlayer_JumpToSection{_PendingSection}, {});
    }
}

auto SCkInspector_MontagePlayerAuthored::Build_BlendOutPort() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_MontagePlayerAuthored> Weak{SharedThis(this)};
    const TSharedPtr<FCkInspectorEditScope> Scope = MakeShared<FCkInspectorEditScope>(_EditGuard);
    _EditScopes.Add(Scope);
    const TSharedRef<SCkDebug_NumericEditor> Editor =
        SNew(SCkDebug_NumericEditor)
            .Tag(TEXT("montage-player-blend-out-input"))
            .Value_Lambda(
                [Weak]()
                {
                    const auto Widget = Weak.Pin();
                    return Widget.IsValid() ? Widget->Get_PendingBlendOut() : 0.0;
                })
            .Kind(ECkDebug_NumericKind::Float)
            .MinValue(0.0)
            .Width(72.0f)
            .ForegroundColor(CkStyle::Value_Numeric())
            .OnValueCommitted_Lambda(
                [Weak](const double Value)
                {
                    if (const auto Widget = Weak.Pin(); Widget.IsValid())
                    {
                        Widget->Commit_PendingBlendOut(static_cast<float>(Value));
                    }
                })
            .OnEditStateChanged_Lambda(
                [Weak, Scope](const bool bEditing)
                {
                    const auto Widget = Weak.Pin();
                    Scope->Set_Active(bEditing && Widget.IsValid() && NOT Widget->Is_Inert());
                });
    Editor->SetEnabled(TAttribute<bool>::CreateLambda(
        [Weak]()
        {
            const auto Widget = Weak.Pin();
            return Widget.IsValid() && Widget->Get_CanRequest();
        }));
    Editor->SetToolTipText(TAttribute<FText>::CreateLambda(
        [Weak]()
        {
            const auto Widget = Weak.Pin();
            return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty();
        }));
    return Editor;
}

auto SCkInspector_MontagePlayerAuthored::Build_SectionPort() -> TSharedRef<SWidget>
{
    struct FTextState
    {
        bool bEditing = false;
        bool bChanged = false;
        FText Frozen;
    };
    const TWeakPtr<SCkInspector_MontagePlayerAuthored> Weak{SharedThis(this)};
    const TSharedPtr<FCkInspectorEditScope> Scope = MakeShared<FCkInspectorEditScope>(_EditGuard);
    const TSharedRef<FTextState> State = MakeShared<FTextState>();
    _EditScopes.Add(Scope);
    return SNew(SBox).WidthOverride(
        160.0f)[SNew(SEditableTextBox)
                    .Tag(TEXT("montage-player-section-input"))
                    .Text_Lambda(
                        [Weak, State]()
                        {
                            const auto Widget = Weak.Pin();
                            return State->bEditing
                                       ? State->Frozen
                                       : FText::FromString(Widget.IsValid() ? Widget->Get_PendingSectionText()
                                                                            : FString{});
                        })
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .SelectAllTextWhenFocused(true)
                    .RevertTextOnEscape(true)
                    .IsEnabled_Lambda(
                        [Weak]()
                        {
                            const auto Widget = Weak.Pin();
                            return Widget.IsValid() && Widget->Get_CanRequest();
                        })
                    .ToolTipText_Lambda(
                        [Weak]()
                        {
                            const auto Widget = Weak.Pin();
                            return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason())
                                                    : FText::GetEmpty();
                        })
                    .OnBeginTextEdit_Lambda(
                        [Weak, State, Scope](const FText &Text)
                        {
                            const auto Widget = Weak.Pin();
                            if (NOT Widget.IsValid() || Widget->Is_Inert())
                            {
                                State->bEditing = false;
                                State->bChanged = false;
                                State->Frozen = FText::GetEmpty();
                                Scope->Set_Active(false);
                                return;
                            }
                            State->bEditing = true;
                            State->Frozen = Text;
                            Scope->Set_Active(true);
                        })
                    .OnTextChanged_Lambda(
                        [Weak, State, Scope](const FText &Text)
                        {
                            const auto Widget = Weak.Pin();
                            if (NOT Widget.IsValid() || Widget->Is_Inert())
                            {
                                State->bEditing = false;
                                State->bChanged = false;
                                State->Frozen = FText::GetEmpty();
                                Scope->Set_Active(false);
                                return;
                            }
                            State->bEditing = true;
                            State->bChanged = true;
                            State->Frozen = Text;
                            Scope->Set_Active(true);
                        })
                    .OnTextCommitted_Lambda(
                        [Weak, State, Scope](const FText &Text, const ETextCommit::Type Type)
                        {
                            const bool bCommit = State->bChanged && SCkDebug_NumericEditor::Should_Commit(Type);
                            State->bEditing = false;
                            State->bChanged = false;
                            State->Frozen = FText::GetEmpty();
                            Scope->Set_Active(false);
                            if (bCommit)
                            {
                                if (const auto Widget = Weak.Pin(); Widget.IsValid())
                                {
                                    Widget->Commit_PendingSection(FName{*Text.ToString().TrimStartAndEnd()});
                                }
                            }
                        })];
}

auto SCkInspector_MontagePlayerAuthored::Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>
{
    const TSharedRef<SBox> Port = SNew(SBox);
    Port->SetContent(MoveTemp(InLeaf));
    _NativePorts.Add(Port);
    return Port;
}

auto SCkInspector_MontagePlayerAuthored::Detach_NativePorts() -> void
{
    for (const TSharedPtr<SBox> &Port : _NativePorts)
    {
        if (Port.IsValid())
        {
            Port->SetContent(SNullWidget::NullWidget);
        }
    }
    _NativePorts.Reset();
}

auto SCkInspector_MontagePlayerAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid())
        {
            Errors.Add(TEXT("Debugger UI registry is unavailable."));
        }
        if (NOT Plugin.IsValid())
        {
            Errors.Add(TEXT("CkDebugger plugin is unavailable."));
        }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }
    const TWeakPtr<SCkInspector_MontagePlayerAuthored> Weak{SharedThis(this)};
    FCkUiView::FNativeBindings Ports;
    Ports.Add(TEXT("montage-player-blend-out-port"), Make_NativePort(Build_BlendOutPort()));
    Ports.Add(TEXT("montage-player-section-port"), Make_NativePort(Build_SectionPort()));
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText =
        [&Data, Weak](const FString &Key, FString (SCkInspector_MontagePlayerAuthored::*Getter)() const)
    {
        Data.Text.Add(Key, TAttribute<FText>::CreateLambda(
                               [Weak, Getter]()
                               {
                                   const auto Widget = Weak.Pin();
                                   return Widget.IsValid() && NOT Widget->Is_Inert()
                                              ? FText::FromString((Widget.Get()->*Getter)())
                                              : FText::GetEmpty();
                               }));
    };
    BindText(TEXT("montage-player-state"), &SCkInspector_MontagePlayerAuthored::Get_StateText);
    BindText(TEXT("montage-player-active-montage"), &SCkInspector_MontagePlayerAuthored::Get_ActiveMontageText);
    BindText(TEXT("montage-player-position"), &SCkInspector_MontagePlayerAuthored::Get_PositionText);
    BindText(TEXT("montage-player-anim-instance"), &SCkInspector_MontagePlayerAuthored::Get_AnimInstanceText);
    BindText(TEXT("montage-player-play-rate"), &SCkInspector_MontagePlayerAuthored::Get_PlayRateText);
    BindText(TEXT("montage-player-catch-up"), &SCkInspector_MontagePlayerAuthored::Get_CatchUpRemainingText);
    BindText(TEXT("montage-player-disabled-reason"), &SCkInspector_MontagePlayerAuthored::Get_RequestDisabledReason);
    Data.Text.Add(TEXT("montage-player-pause-label"), FText::FromString(TEXT("Pause")));
    Data.Text.Add(TEXT("montage-player-resume-label"), FText::FromString(TEXT("Resume")));
    Data.Text.Add(TEXT("montage-player-stop-label"), FText::FromString(TEXT("Stop")));
    Data.Text.Add(TEXT("montage-player-jump-label"), FText::FromString(TEXT("Jump")));
    Data.Text.Add(TEXT("montage-player-pause-tooltip"),
                  FText::FromString(TEXT("Request_Pause — freezes the active montage where it stands.")));
    Data.Text.Add(TEXT("montage-player-resume-tooltip"),
                  FText::FromString(TEXT("Request_Resume — continues a paused montage.")));
    Data.Text.Add(TEXT("montage-player-stop-tooltip"),
                  FText::FromString(TEXT("Request_Stop, blending out over the Blend Out seconds above.")));
    Data.Text.Add(TEXT("montage-player-jump-tooltip"),
                  FText::FromString(TEXT("Request_JumpToSection with the Section name above. Ignored while it is empty.")));
    Data.Visibility.Add(TEXT("montage-player-available"), TAttribute<bool>::CreateLambda(
                                                              [Weak]()
                                                              {
                                                                  const auto Widget = Weak.Pin();
                                                                  return Widget.IsValid() && Widget->Get_IsAvailable();
                                                              }));
    Data.Visibility.Add(TEXT("montage-player-unavailable"), TAttribute<bool>::CreateLambda(
                                                                [Weak]()
                                                                {
                                                                    const auto Widget = Weak.Pin();
                                                                    return NOT Widget.IsValid() ||
                                                                           NOT Widget->Get_IsAvailable();
                                                                }));
    Data.Visibility.Add(TEXT("montage-player-controls"), TAttribute<bool>::CreateLambda(
                                                             [Weak]()
                                                             {
                                                                 const auto Widget = Weak.Pin();
                                                                 return Widget.IsValid() && Widget->Get_HasControls();
                                                             }));
    Data.Visibility.Add(TEXT("montage-player-can-request"), TAttribute<bool>::CreateLambda(
                                                                [Weak]()
                                                                {
                                                                    const auto Widget = Weak.Pin();
                                                                    return Widget.IsValid() && Widget->Get_CanRequest();
                                                                }));
    Data.Number.Add(TEXT("montage-player-position-fraction"),
                    TAttribute<float>::CreateLambda(
                        [Weak]()
                        {
                            const auto Widget = Weak.Pin();
                            return Widget.IsValid() ? Widget->Get_PositionFraction() : 0.0f;
                        }));
    Data.Color.Add(TEXT("montage-player-state-foreground"),
                   TAttribute<FLinearColor>::CreateLambda(
                       [Weak]()
                       {
                           const auto Widget = Weak.Pin();
                           return Widget.IsValid() ? Widget->Get_StateForeground() : FLinearColor::Transparent;
                       }));
    Data.Color.Add(TEXT("montage-player-state-background"),
                   TAttribute<FLinearColor>::CreateLambda(
                       [Weak]()
                       {
                           const auto Widget = Weak.Pin();
                           return Widget.IsValid() ? Widget->Get_StateBackground() : FLinearColor::Transparent;
                       }));
    Data.Color.Add(TEXT("montage-player-active-montage-color"),
                   TAttribute<FLinearColor>::CreateLambda(
                       [Weak]()
                       {
                           const auto Widget = Weak.Pin();
                           return Widget.IsValid() ? Widget->Get_ActiveMontageColor() : FLinearColor::Transparent;
                       }));
    Data.Color.Add(TEXT("montage-player-anim-instance-color"),
                   TAttribute<FLinearColor>::CreateLambda(
                       [Weak]()
                       {
                           const auto Widget = Weak.Pin();
                           return Widget.IsValid() ? Widget->Get_AnimInstanceColor() : FLinearColor::Transparent;
                       }));
    Data.Color.Add(TEXT("montage-player-meter-fill"), CkStyle::Accent());
    const auto AddDiffColor = [&Data, Weak](const FString &Key, const FString &Label)
    {
        Data.Color.Add(Key, TAttribute<FLinearColor>::CreateLambda(
                                [Weak, Label]()
                                {
                                    const auto Widget = Weak.Pin();
                                    return Widget.IsValid()
                                               ? ck_inspector_montage_player::DiffColor(Widget->Is_DiffMarked(Label))
                                               : FLinearColor::Transparent;
                                }));
    };
    AddDiffColor(TEXT("montage-player-state-diff-color"), TEXT("State:"));
    AddDiffColor(TEXT("montage-player-active-montage-diff-color"), TEXT("Active Montage:"));
    AddDiffColor(TEXT("montage-player-position-diff-color"), TEXT("Position:"));
    AddDiffColor(TEXT("montage-player-anim-instance-diff-color"), TEXT("Anim Instance:"));
    AddDiffColor(TEXT("montage-player-play-rate-diff-color"), TEXT("Play Rate:"));
    AddDiffColor(TEXT("montage-player-catch-up-remaining-diff-color"), TEXT("Catch-up Remaining:"));
    AddDiffColor(TEXT("montage-player-blend-out-diff-color"), TEXT("Blend Out (s):"));
    AddDiffColor(TEXT("montage-player-section-diff-color"), TEXT("Section:"));
    AddDiffColor(TEXT("montage-player-playback-diff-color"), TEXT("Playback:"));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda(
        [Weak]()
        {
            const auto Widget = Weak.Pin();
            return Widget.IsValid() && Widget->Get_CanRequest();
        });
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("montage-player-pause"), FSimpleDelegate::CreateLambda(
                                                  [Weak]()
                                                  {
                                                      if (const auto Widget = Weak.Pin(); Widget.IsValid())
                                                      {
                                                          Widget->Request_Pause();
                                                      }
                                                  }));
    Actions.Add(TEXT("montage-player-resume"), FSimpleDelegate::CreateLambda(
                                                   [Weak]()
                                                   {
                                                       if (const auto Widget = Weak.Pin(); Widget.IsValid())
                                                       {
                                                           Widget->Request_Resume();
                                                       }
                                                   }));
    Actions.Add(TEXT("montage-player-stop"), FSimpleDelegate::CreateLambda(
                                                 [Weak]()
                                                 {
                                                     if (const auto Widget = Weak.Pin(); Widget.IsValid())
                                                     {
                                                         Widget->Request_Stop();
                                                     }
                                                 }));
    Actions.Add(TEXT("montage-player-jump"), FSimpleDelegate::CreateLambda(
                                                 [Weak]()
                                                 {
                                                     if (const auto Widget = Weak.Pin(); Widget.IsValid())
                                                     {
                                                         Widget->Request_Jump();
                                                     }
                                                 }));
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(Ports), MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorMontagePlayer.ui.html")),
                        FPaths::Combine(Root, TEXT("EcsInspectorMontagePlayer.ui.css")));
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

auto SCkInspector_MontagePlayerAuthored::Tick(const FGeometry &Geometry, const double CurrentTime,
                                              const float DeltaTime) -> void
{
    SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
    if (_Active && _View.IsValid())
    {
        _View->PollFiles();
        if (NOT _View->GetLastResult().Succeeded)
        {
            _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n"));
        }
        else
        {
            _LoadError.Reset();
        }
    }
}

auto SCkInspector_MontagePlayerAuthored::Release() -> void
{
    if (NOT _Active)
    {
        return;
    }
    _Active = false;
    for (const TSharedPtr<FCkInspectorEditScope> &Scope : _EditScopes)
    {
        if (Scope.IsValid())
        {
            Scope->Set_Active(false);
        }
    }
    _EditScopes.Reset();
    Detach_NativePorts();
    _Entity = {};
    _EditGuard.Reset();
    _View.Reset();
    _Mounted = false;
}

FCkInspector_MontagePlayer::~FCkInspector_MontagePlayer() { OnDeactivated(); }
auto FCkInspector_MontagePlayer::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Montage Player"));
}
auto FCkInspector_MontagePlayer::CanInspect(const FCk_Handle &Entity) const -> bool
{
    return ck_inspector_montage_player::HasCurrent(Entity);
}

auto FCkInspector_MontagePlayer::Build_NativeBody(const FCk_Handle &Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    Builder.SetEditGuard(Get_EditGuard());
    if (NOT ck_inspector_montage_player::HasCurrent(Entity))
    {
        return Builder.Build(Entity);
    }
    const auto Captured = Entity;
    Builder.AddHeader(FText::FromString(TEXT("Montage Player")));
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda(
            [Captured]()
            {
                return ck_inspector_montage_player::HasCurrent(Captured)
                           ? FText::FromString(ck_inspector_montage_player::GetStateText(
                                 Captured.Get<ck::FFragment_MontagePlayer_Current>().Get_State().Get_Kind()))
                           : FText::FromString(TEXT("--"));
            }),
        TAttribute<ECk_Tone>::CreateLambda(
            [Captured]()
            {
                return ck_inspector_montage_player::HasCurrent(Captured)
                           ? ck_inspector_montage_player::GetTone(
                                 Captured.Get<ck::FFragment_MontagePlayer_Current>().Get_State().Get_Kind())
                           : ECk_Tone::Neutral;
            }));
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Active Montage:")),
        [Captured](const FCk_Handle &)
        {
            if (NOT ck_inspector_montage_player::HasCurrent(Captured))
            { return FText::FromString(TEXT("--")); }
            const auto Montage = Captured.Get<ck::FFragment_MontagePlayer_Current>().Get_ActiveMontage();
            return Montage.IsValid() ? FText::FromName(Montage->GetFName()) : FText::FromString(TEXT("(none)"));
        },
        [Captured](const FCk_Handle &) -> FLinearColor
        {
            if (NOT ck_inspector_montage_player::HasCurrent(Captured))
            { return CkStyle::None(); }
            return Captured.Get<ck::FFragment_MontagePlayer_Current>().Get_ActiveMontage().IsValid()
                ? CkStyle::Value_Object()
                : CkStyle::TextMute();
        });
    Builder.AddMeterRow(FText::FromString(TEXT("Position:")),
                        TAttribute<float>::CreateLambda(
                            [Captured]()
                            {
                                float Length = 0.0f;
                                const float Position = ck_inspector_montage_player::GetPlayback(Captured, Length);
                                return Length > 0.0f ? Position / Length : 0.0f;
                            }),
                        ECk_Tone::Accent,
                        TAttribute<FText>::CreateLambda(
                            [Captured]()
                            {
                                float Length = 0.0f;
                                const float Position = ck_inspector_montage_player::GetPlayback(Captured, Length);
                                return Length > 0.0f ? FText::FromString(
                                                           ck::Format_UE(TEXT("{:.2f} / {:.2f}s"), Position, Length))
                                                     : FText::FromString(TEXT("--"));
                            }));
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Anim Instance:")),
        [Captured](const FCk_Handle &)
        {
            if (NOT ck_inspector_montage_player::HasCurrent(Captured))
            { return FText::FromString(TEXT("--")); }
            return FText::FromString(
                Captured.Get<ck::FFragment_MontagePlayer_Current>().Get_LastSeenAnimInstance().IsValid()
                    ? TEXT("Valid") : TEXT("Invalid"));
        },
        [Captured](const FCk_Handle &) -> FLinearColor
        {
            if (NOT ck_inspector_montage_player::HasCurrent(Captured))
            { return CkStyle::None(); }
            return Captured.Get<ck::FFragment_MontagePlayer_Current>().Get_LastSeenAnimInstance().IsValid()
                ? CkStyle::Ok()
                : CkStyle::Err();
        });
    Builder.AddRow(
        FText::FromString(TEXT("Play Rate:")),
        [Captured](const FCk_Handle &)
        {
            return ck_inspector_montage_player::HasCurrent(Captured)
                       ? FText::FromString(ck::Format_UE(
                             TEXT("{:.2f}"),
                             Captured.Get<ck::FFragment_MontagePlayer_Current>().Get_State().Get_PlayRate()))
                       : FText::FromString(TEXT("--"));
        },
        CkStyle::Value_Numeric());
    Builder.AddRow(
        FText::FromString(TEXT("Catch-up Remaining:")),
        [Captured](const FCk_Handle &)
        {
            return ck_inspector_montage_player::HasCurrent(Captured)
                       ? FText::FromString(ck::Format_UE(
                             TEXT("{:.3f} s"),
                             Captured.Get<ck::FFragment_MontagePlayer_Current>().Get_CatchUpRemaining().Get_Seconds()))
                       : FText::FromString(TEXT("--"));
        },
        CkStyle::Value_Numeric());
    FCk_Handle_MontagePlayer Player;
    if (NOT ck_inspector_montage_player::TryGetMontagePlayer(Entity, Player))
    {
        return Builder.Build(Entity);
    }
    const TSharedRef<float> BlendOut = MakeShared<float>(0.25f);
    const TSharedRef<FName> Section = MakeShared<FName>(UCk_Utils_MontagePlayer_UE::Get_CurrentSection(Player));
    Builder.AddHeader(FText::FromString(TEXT("Controls")));
    Builder.AddNumericRow(
        FText::FromString(TEXT("Blend Out (s):")), TAttribute<float>::CreateLambda([BlendOut]() { return *BlendOut; }),
        [BlendOut](const float Value) { *BlendOut = FMath::Max(0.0f, Value); }, 0.0f, {},
        ECk_DebugRequest_Requirement::AuthorityOnly);
    Builder.AddNameEntryRow(
        FText::FromString(TEXT("Section:")),
        TAttribute<FText>::CreateLambda(
            [Section]() { return Section->IsNone() ? FText::FromString(TEXT("(none)")) : FText::FromName(*Section); }),
        [Section](const FName Value) { *Section = Value; }, ECk_DebugRequest_Requirement::AuthorityOnly);
    Builder.AddActionRow(FText::FromString(TEXT("Playback:")),
                         {{FText::FromString(TEXT("Pause")),
                           FText::FromString(TEXT("Request_Pause — freezes the active montage where it stands.")),
                           [Captured]()
                           {
                               FCk_Handle_MontagePlayer Current;
                               if (ck_inspector_montage_player::TryGetMontagePlayer(Captured, Current) &&
                                   ck_inspector_montage_player::GetRequestGate(Captured).IsEnabled)
                               {
                                   UCk_Utils_MontagePlayer_UE::Request_Pause(Current, {}, {});
                               }
                           },
                           ECk_DebugRequest_Requirement::AuthorityOnly},
                          {FText::FromString(TEXT("Resume")),
                           FText::FromString(TEXT("Request_Resume — continues a paused montage.")),
                           [Captured]()
                           {
                               FCk_Handle_MontagePlayer Current;
                               if (ck_inspector_montage_player::TryGetMontagePlayer(Captured, Current) &&
                                   ck_inspector_montage_player::GetRequestGate(Captured).IsEnabled)
                               {
                                   UCk_Utils_MontagePlayer_UE::Request_Resume(Current, {}, {});
                               }
                           },
                           ECk_DebugRequest_Requirement::AuthorityOnly},
                          {FText::FromString(TEXT("Stop")),
                           FText::FromString(TEXT("Request_Stop, blending out over the Blend Out seconds above.")),
                           [Captured, BlendOut]()
                           {
                               FCk_Handle_MontagePlayer Current;
                               if (ck_inspector_montage_player::TryGetMontagePlayer(Captured, Current) &&
                                   ck_inspector_montage_player::GetRequestGate(Captured).IsEnabled)
                               {
                                   UCk_Utils_MontagePlayer_UE::Request_Stop(
                                       Current,
                                       FCk_Request_MontagePlayer_Stop{
                                           FCk_Time{static_cast<double>(FMath::Max(0.0f, *BlendOut))}},
                                       {});
                               }
                           },
                           ECk_DebugRequest_Requirement::AuthorityOnly},
                          {FText::FromString(TEXT("Jump")),
                           FText::FromString(TEXT("Request_JumpToSection with the Section name above. Ignored while it is empty.")),
                           [Captured, Section]()
                           {
                               FCk_Handle_MontagePlayer Current;
                               if (NOT Section->IsNone() &&
                                   ck_inspector_montage_player::TryGetMontagePlayer(Captured, Current) &&
                                   ck_inspector_montage_player::GetRequestGate(Captured).IsEnabled)
                               {
                                   UCk_Utils_MontagePlayer_UE::Request_JumpToSection(
                                       Current, FCk_Request_MontagePlayer_JumpToSection{*Section}, {});
                               }
                           },
                           ECk_DebugRequest_Requirement::AuthorityOnly}});
    return Builder.Build(Entity);
}

auto FCkInspector_MontagePlayer::Build_Inspector(const FCk_Handle &Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    {
        return NativeBody;
    }
    auto DiffLabels = TSet<FString>{};
    for (const FString &Label : TArray<FString>{TEXT("State:"), TEXT("Active Montage:"), TEXT("Position:"),
                                                TEXT("Anim Instance:"), TEXT("Play Rate:"), TEXT("Catch-up Remaining:"),
                                                TEXT("Blend Out (s):"), TEXT("Section:"), TEXT("Playback:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        {
            DiffLabels.Add(Label);
        }
    }
    const TSharedRef<SCkInspector_MontagePlayerAuthored> Authored = SNew(SCkInspector_MontagePlayerAuthored)
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

auto FCkInspector_MontagePlayer::Tick(const FCk_Handle &Entity, const float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_MontagePlayerAuthored> &Instance)
                                 { return NOT Instance.IsValid() || Instance.Pin()->Is_Inert(); });
    _LastAuthoredLoadError.Reset();
    for (const auto &Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        {
            _LastAuthoredLoadError = Instance->Get_LoadError();
            break;
        }
    }
}

auto FCkInspector_MontagePlayer::OnDeactivated() -> void
{
    for (const auto &Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid())
        {
            Instance->Release();
        }
    }
    _AuthoredInstances.Reset();
    _LastAuthoredLoadError.Reset();
}
