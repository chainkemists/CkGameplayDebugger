#include "CkInspector_MontagePlayer.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAnimation/MontagePlayer/CkMontagePlayer_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_MontagePlayer)

// =====================================================================================================================

namespace
{
    auto Format_StateKind_Color(ECk_MontagePlayer_StateKind Kind) -> FLinearColor
    {
        switch (Kind)
        {
            case ECk_MontagePlayer_StateKind::Play:           return CkDebugStyle::Status_Active();
            case ECk_MontagePlayer_StateKind::Stop:           return CkDebugStyle::TextMute();
            case ECk_MontagePlayer_StateKind::Pause:          return CkDebugStyle::Warn();
            case ECk_MontagePlayer_StateKind::Resume:         return CkDebugStyle::Ok();
            case ECk_MontagePlayer_StateKind::JumpToSection:  return CkDebugStyle::Value_Enum();
            default:                                           return CkDebugStyle::None();
        }
    }

    auto Format_StateKind_String(ECk_MontagePlayer_StateKind Kind) -> FString
    {
        switch (Kind)
        {
            case ECk_MontagePlayer_StateKind::Play:           return TEXT("Play");
            case ECk_MontagePlayer_StateKind::Stop:           return TEXT("Stop");
            case ECk_MontagePlayer_StateKind::Pause:          return TEXT("Pause");
            case ECk_MontagePlayer_StateKind::Resume:         return TEXT("Resume");
            case ECk_MontagePlayer_StateKind::JumpToSection:  return TEXT("JumpToSection");
            default:                                           return TEXT("Unknown");
        }
    }
}

// =====================================================================================================================

auto FCkInspector_MontagePlayer::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Montage Player"));
}

auto FCkInspector_MontagePlayer::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has<ck::FFragment_MontagePlayer_Current>();
}

// =====================================================================================================================

auto FCkInspector_MontagePlayer::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    const auto& Frag         = Entity.Get<ck::FFragment_MontagePlayer_Current>();
    const auto& State        = Frag.Get_State();
    const auto  Kind         = State.Get_Kind();
    const auto  ActiveMontage = Frag.Get_ActiveMontage();
    const auto  AnimInstance  = Frag.Get_LastSeenAnimInstance();
    const auto  CatchUp       = Frag.Get_CatchUpRemaining();

    Builder.AddHeader(FText::FromString(TEXT("Montage Player")));

    Builder.AddRow(
        FText::FromString(TEXT("State:")),
        [Kind](const FCk_Handle&)
        { return FText::FromString(Format_StateKind_String(Kind)); },
        Format_StateKind_Color(Kind));

    Builder.AddRow(
        FText::FromString(TEXT("Active Montage:")),
        [ActiveMontage](const FCk_Handle&)
        {
            if (NOT ActiveMontage.IsValid())
            { return FText::FromString(TEXT("(none)")); }
            return FText::FromString(ActiveMontage.Get()->GetFName().ToString());
        },
        ActiveMontage.IsValid() ? CkDebugStyle::Value_Object() : CkDebugStyle::TextMute());

    Builder.AddRow(
        FText::FromString(TEXT("Anim Instance:")),
        [AnimInstance](const FCk_Handle&)
        { return FText::FromString(AnimInstance.IsValid() ? TEXT("Valid") : TEXT("Invalid")); },
        AnimInstance.IsValid() ? CkDebugStyle::Ok() : CkDebugStyle::Err());

    Builder.AddRow(
        FText::FromString(TEXT("Play Rate:")),
        [State](const FCk_Handle&)
        { return FText::FromString(FString::Printf(TEXT("%.2f"), State.Get_PlayRate())); },
        CkDebugStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Catch-up Remaining:")),
        [CatchUp](const FCk_Handle&)
        { return FText::FromString(FString::Printf(TEXT("%.3f s"), CatchUp.Get_Seconds())); },
        CkDebugStyle::Value_Numeric());

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_MontagePlayer::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
