#include "CkDebugOverlay_Provider_Physics.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Physics fragments — mirroring CkInspector_Physics.cpp includes
#include "CkPhysics/Velocity/CkVelocity_Fragment.h"
#include "CkPhysics/Acceleration/CkAcceleration_Fragment.h"
#include "CkPhysics/EulerIntegrator/CkEulerIntegrator_Fragment.h"
#include "CkPhysics/PredictedVelocity/CkPredictedVelocity_Fragment.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Physics,
    "Ck.OnScreenDebugger.Provider.Physics")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Physics_Velocity,
    "Ck.OnScreenDebugger.Provider.Physics.Velocity")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Physics_MovementMode,
    "Ck.OnScreenDebugger.Provider.Physics.MovementMode")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()            { return TAG_Ck_OnScreenDebugger_Provider_Physics; }
    FGameplayTag FieldTag_Velocity()      { return TAG_Ck_OnScreenDebugger_Provider_Physics_Velocity; }
    FGameplayTag FieldTag_MovementMode()  { return TAG_Ck_OnScreenDebugger_Provider_Physics_MovementMode; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Physics::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Physics::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Velocity(),     true  },
        { FieldTag_MovementMode(), true  },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_Physics::CanInspect:
//   Entity.Has_Any<FFragment_Velocity_Current, FFragment_Acceleration_Current,
//                  FFragment_EulerIntegrator_Current, FFragment_PredictedVelocity_Current>()
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Physics::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return Entity.Has_Any<
        ck::FFragment_Velocity_Current,
        ck::FFragment_Acceleration_Current,
        ck::FFragment_EulerIntegrator_Current,
        ck::FFragment_PredictedVelocity_Current>();
}

// --------------------------------------------------------------------------------------------------------------------
// Collect — mirrors FCkInspector_Physics::Build_Inspector extraction.
//
// "MovementMode" as a discrete label doesn't exist in the physics fragments.
// The CkPhysics inspector shows Velocity, Acceleration, Predicted Velocity,
// and Euler Integrator distance offset — none of these is a "movement mode"
// enum. We map:
//   Velocity      → FFragment_Velocity_Current::Get_CurrentVelocity() (FVector)
//   MovementMode  → derived: speed magnitude and "Moving"/"Idle" heuristic,
//                   since there is no discrete movement-mode enum in the fragments.
//
// BATCH-VERIFY: confirm whether a discrete movement mode fragment exists in
// CkPhysics (e.g. FFragment_MovementMode_Current). If so, replace the
// heuristic below with a direct enum read.
//
// BATCH-VERIFY fragment accessors (all confirmed from CkInspector_Physics.cpp):
//   FFragment_Velocity_Current::Get_CurrentVelocity()       → FVector
//   FFragment_PredictedVelocity_Current::Get_CurrentVelocity() → FVector
//   FFragment_EulerIntegrator_Current::Get_DistanceOffset() → FVector
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Physics::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    // Resolve the best velocity source (prefer FFragment_Velocity_Current, fall
    // back to FFragment_PredictedVelocity_Current).
    FVector Velocity       = FVector::ZeroVector;
    bool    bHasVelocity   = false;

    if (Entity.Has<ck::FFragment_Velocity_Current>())
    {
        Velocity     = Entity.Get<ck::FFragment_Velocity_Current>().Get_CurrentVelocity();
        bHasVelocity = true;
    }
    else if (Entity.Has<ck::FFragment_PredictedVelocity_Current>())
    {
        Velocity     = Entity.Get<ck::FFragment_PredictedVelocity_Current>().Get_CurrentVelocity();
        bHasVelocity = true;
    }

    // --- Velocity ---
    if (Cfg.EnabledFields.HasTagExact(FieldTag_Velocity()) && bHasVelocity)
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Velocity();
        Row.Value    = FText::FromString(ck::Format_UE(TEXT("{}"), Velocity));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // --- MovementMode (heuristic) ---
    if (Cfg.EnabledFields.HasTagExact(FieldTag_MovementMode()))
    {
        FString ModeStr;
        if (bHasVelocity)
        {
            const float Speed = Velocity.Size2D(); // XY speed in cm/s
            ModeStr = (Speed > 10.0f) ? FString::Printf(TEXT("Moving (%.0fcm/s)"), Speed)
                                      : FString(TEXT("Idle"));
        }
        else if (Entity.Has<ck::FFragment_EulerIntegrator_Current>())
        {
            const FVector Offset = Entity.Get<ck::FFragment_EulerIntegrator_Current>().Get_DistanceOffset();
            ModeStr = FString::Printf(TEXT("Integrating (%.0f)"), Offset.Size());
        }
        else
        {
            ModeStr = TEXT("(no velocity)");
        }

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_MovementMode();
        Row.Value    = FText::FromString(ModeStr);
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Physics::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    FVector Vel = FVector::ZeroVector;
    if (Entity.Has<ck::FFragment_Velocity_Current>())
        Vel = Entity.Get<ck::FFragment_Velocity_Current>().Get_CurrentVelocity();
    else if (Entity.Has<ck::FFragment_PredictedVelocity_Current>())
        Vel = Entity.Get<ck::FFragment_PredictedVelocity_Current>().Get_CurrentVelocity();

    const float Speed = Vel.Size2D();
    return FString::Printf(TEXT("Phys:%.0fcm/s"), Speed);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Physics)

// --------------------------------------------------------------------------------------------------------------------
