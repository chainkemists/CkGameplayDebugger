#include "CkDebugOverlay_Provider_Jolt.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Jolt feature Utils — the clean enum/bool/FVector accessor surface (no raw Jolt types).
#include "CkJolt/Body/CkJoltBody_Utils.h"
#include "CkJolt/Character/CkJoltCharacter_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Jolt,
    "Ck.OnScreenDebugger.Provider.Jolt")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Jolt_MotionType,
    "Ck.OnScreenDebugger.Provider.Jolt.MotionType")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Jolt_SleepState,
    "Ck.OnScreenDebugger.Provider.Jolt.SleepState")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Jolt_Velocity,
    "Ck.OnScreenDebugger.Provider.Jolt.Velocity")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Jolt_GroundState,
    "Ck.OnScreenDebugger.Provider.Jolt.GroundState")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()          { return TAG_Ck_OnScreenDebugger_Provider_Jolt; }
    FGameplayTag FieldTag_MotionType()  { return TAG_Ck_OnScreenDebugger_Provider_Jolt_MotionType; }
    FGameplayTag FieldTag_SleepState()  { return TAG_Ck_OnScreenDebugger_Provider_Jolt_SleepState; }
    FGameplayTag FieldTag_Velocity()    { return TAG_Ck_OnScreenDebugger_Provider_Jolt_Velocity; }
    FGameplayTag FieldTag_GroundState() { return TAG_Ck_OnScreenDebugger_Provider_Jolt_GroundState; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Jolt::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Jolt::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_MotionType(),  true },
        { FieldTag_SleepState(),  true },
        { FieldTag_Velocity(),    true },
        { FieldTag_GroundState(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — any entity carrying a Jolt rigid body OR a Jolt character.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Jolt::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_JoltBody_UE::Has(Entity) || UCk_Utils_JoltCharacter_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Jolt::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    // ---- JoltBody rows ----
    if (UCk_Utils_JoltBody_UE::Has(Entity))
    {
        auto       MutableEntity = Entity;
        const auto BodyHandle    = UCk_Utils_JoltBody_UE::CastChecked(MutableEntity);

        if (Cfg.EnabledFields.HasTagExact(FieldTag_MotionType()))
        {
            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_MotionType();
            Row.Value    = FText::FromString(ck::Format_UE(TEXT("{}"),
                UCk_Utils_JoltBody_UE::Get_MotionType(BodyHandle)));
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            Out.Rows.Add(MoveTemp(Row));
        }

        if (Cfg.EnabledFields.HasTagExact(FieldTag_SleepState()))
        {
            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_SleepState();
            Row.Value    = FText::FromString(ck::Format_UE(TEXT("{}"),
                UCk_Utils_JoltBody_UE::Get_SleepState(BodyHandle)));
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            Out.Rows.Add(MoveTemp(Row));
        }

        if (Cfg.EnabledFields.HasTagExact(FieldTag_Velocity()))
        {
            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_Velocity();
            Row.Value    = FText::FromString(ck::Format_UE(TEXT("{}"),
                UCk_Utils_JoltBody_UE::Get_LinearVelocity(BodyHandle)));
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            Out.Rows.Add(MoveTemp(Row));
        }
    }

    // ---- JoltCharacter rows ----
    if (UCk_Utils_JoltCharacter_UE::Has(Entity))
    {
        auto       MutableEntity   = Entity;
        const auto CharacterHandle = UCk_Utils_JoltCharacter_UE::CastChecked(MutableEntity);

        if (Cfg.EnabledFields.HasTagExact(FieldTag_GroundState()))
        {
            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_GroundState();
            Row.Value    = FText::FromString(ck::Format_UE(TEXT("{}"),
                UCk_Utils_JoltCharacter_UE::Get_GroundState(CharacterHandle)));
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            Out.Rows.Add(MoveTemp(Row));
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken — a single short label for world pills / compact mode.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Jolt::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    if (UCk_Utils_JoltBody_UE::Has(Entity))
    {
        auto       MutableEntity = Entity;
        const auto BodyHandle    = UCk_Utils_JoltBody_UE::CastChecked(MutableEntity);
        return ck::Format_UE(TEXT("Jolt:{}"), UCk_Utils_JoltBody_UE::Get_MotionType(BodyHandle));
    }

    if (UCk_Utils_JoltCharacter_UE::Has(Entity))
    {
        auto       MutableEntity   = Entity;
        const auto CharacterHandle = UCk_Utils_JoltCharacter_UE::CastChecked(MutableEntity);
        return ck::Format_UE(TEXT("Jolt:{}"), UCk_Utils_JoltCharacter_UE::Get_GroundState(CharacterHandle));
    }

    return {};
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Jolt)

// --------------------------------------------------------------------------------------------------------------------
