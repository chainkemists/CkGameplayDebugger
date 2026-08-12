#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"

#include "CkCore/Format/CkFormat.h"

#include "CkEditorTools/Style/CkStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_types
{
    constexpr auto DegreesPerOctant = 45.0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_EpisodeRow::
    Get_CauseLabel() const
    -> FString
{
    if (ChordCauseArmed && HoldCauseArmed)
    { return ck::Format_UE(TEXT("chord({}f) + hold({}f)"), ChordWindowFrames, HoldSiblingFrames); }

    if (ChordCauseArmed)
    { return ck::Format_UE(TEXT("chord({}f)"), ChordWindowFrames); }

    if (HoldCauseArmed)
    { return ck::Format_UE(TEXT("hold({}f)"), HoldSiblingFrames); }

    return TEXT("spent");
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_LayerRow::
    Get_StackLabel() const
    -> FString
{
    if (IsGlobalActionLayer)
    { return ck::Format_UE(TEXT("{} [global actions]"), DebugName); }

    return ck::Format_UE(TEXT("{} (p{})"), DebugName, Priority);
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::intent_debugger
{
    auto
        Get_Label(
            const FCk_Input_ButtonId& InButton)
        -> FString
    {
        const auto TierLabel = InButton.Get_Tier() == ECk_Input_ButtonTier::Mapped ? TEXT("M") : TEXT("P");
        return ck::Format_UE(TEXT("{}:{}"), TierLabel, InButton.Get_Name());
    }

    auto
        Get_Label(
            ECk_Intent_Octant InOctant)
        -> FString
    {
        switch (InOctant)
        {
            case ECk_Intent_Octant::E:  return TEXT("E");
            case ECk_Intent_Octant::NE: return TEXT("NE");
            case ECk_Intent_Octant::N:  return TEXT("N");
            case ECk_Intent_Octant::NW: return TEXT("NW");
            case ECk_Intent_Octant::W:  return TEXT("W");
            case ECk_Intent_Octant::SW: return TEXT("SW");
            case ECk_Intent_Octant::S:  return TEXT("S");
            case ECk_Intent_Octant::SE: return TEXT("SE");
            case ECk_Intent_Octant::Neutral:
            default: return TEXT("Neutral");
        }
    }

    auto
        Get_Label(
            ECk_Intent_CleanedAxis InAxis)
        -> FString
    {
        switch (InAxis)
        {
            case ECk_Intent_CleanedAxis::Positive: return TEXT("Positive");
            case ECk_Intent_CleanedAxis::Negative: return TEXT("Negative");
            case ECk_Intent_CleanedAxis::Neutral:
            default: return TEXT("Neutral");
        }
    }

    auto
        Get_Label(
            ECk_Intent_Phase InPhase)
        -> FString
    {
        switch (InPhase)
        {
            case ECk_Intent_Phase::Pending:   return TEXT("Pending");
            case ECk_Intent_Phase::Completed: return TEXT("Completed");
            case ECk_Intent_Phase::Failed:    return TEXT("Failed");
            case ECk_Intent_Phase::Active:    return TEXT("Active");
            case ECk_Intent_Phase::Idle:
            default: return TEXT("Idle");
        }
    }

    auto
        Get_Label(
            ECk_Intent_ScanStepOutcome InOutcome)
        -> FString
    {
        switch (InOutcome)
        {
            case ECk_Intent_ScanStepOutcome::Matched:          return TEXT("Matched");
            case ECk_Intent_ScanStepOutcome::NotSatisfied:     return TEXT("NotSatisfied");
            case ECk_Intent_ScanStepOutcome::WindowExhausted:  return TEXT("WindowExhausted");
            case ECk_Intent_ScanStepOutcome::ContiguityBroken: return TEXT("ContiguityBroken");
            default: return TEXT("?");
        }
    }

    auto
        Get_Label(
            ECk_InputLayer_CaptureBehavior InBehavior)
        -> FString
    {
        return InBehavior == ECk_InputLayer_CaptureBehavior::Consume ? TEXT("Consume") : TEXT("PassThrough");
    }

    auto
        Get_PhaseColor(
            ECk_Intent_Phase InPhase)
        -> FLinearColor
    {
        switch (InPhase)
        {
            case ECk_Intent_Phase::Pending:   return CkStyle::GetToneColor(ECk_Tone::Warn);
            case ECk_Intent_Phase::Completed: return CkStyle::GetToneColor(ECk_Tone::Ok);
            case ECk_Intent_Phase::Failed:    return CkStyle::GetToneColor(ECk_Tone::Err);
            case ECk_Intent_Phase::Active:    return CkStyle::GetToneColor(ECk_Tone::Info);
            case ECk_Intent_Phase::Idle:
            default: return CkStyle::GetToneDimColor(ECk_Tone::Neutral);
        }
    }

    auto
        Get_StepOutcomeColor(
            ECk_Intent_ScanStepOutcome InOutcome)
        -> FLinearColor
    {
        switch (InOutcome)
        {
            case ECk_Intent_ScanStepOutcome::Matched:          return CkStyle::GetToneColor(ECk_Tone::Ok);
            case ECk_Intent_ScanStepOutcome::WindowExhausted:  return CkStyle::GetToneColor(ECk_Tone::Warn);
            case ECk_Intent_ScanStepOutcome::ContiguityBroken: return CkStyle::GetToneColor(ECk_Tone::Accent);
            case ECk_Intent_ScanStepOutcome::NotSatisfied:
            default: return CkStyle::GetToneColor(ECk_Tone::Err);
        }
    }

    auto
        Get_OctantUnitVector(
            ECk_Intent_Octant InOctant)
        -> FVector2D
    {
        if (InOctant == ECk_Intent_Octant::Neutral)
        { return FVector2D::ZeroVector; }

        const auto Degrees = (static_cast<double>(InOctant) - 1.0) * ck_intent_debugger_types::DegreesPerOctant;
        const auto Radians = FMath::DegreesToRadians(Degrees);

        return FVector2D{FMath::Cos(Radians), FMath::Sin(Radians)};
    }
}

// --------------------------------------------------------------------------------------------------------------------
