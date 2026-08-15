#pragma once

#include "CoreMinimal.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkJolt/CkJolt_Common.h"
#include "CkJolt/Body/CkJoltBody_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

/*
 * Which of the four body-backing features produced a row. The debug-draw colour classes are finer-grained
 * (a JoltBody splits four ways by motion type and sleep state); this is the ENTITY-shaped grouping the
 * outliner lists by, and it lines up one-to-one with the window's population toggles.
 */
enum class ECkJoltDebugger_Population : uint8
{
    JoltBody,
    BakedStatic,
    Sensor,
    Character
};

// --------------------------------------------------------------------------------------------------------------------

/*
 * One outliner row, collected flat off the ECS registry. The handle is the ONLY registry-bearing field in the
 * whole debugger — every panel below the outliner reads plain values out of this struct, so clearing the
 * collector on session invalidation is the single place PIE handles die.
 *
 * BodyKey addresses the drawn body on the facility's public surface (Set_HighlightedBody / TryPick_Body). It is
 * UNSET for a row with no drawn body behind it — a baked static actor whose bodies were already removed. For a
 * JoltStaticActor row it is the FIRST of the actor's baked bodies: the facility highlights one key, so a
 * multi-body actor highlights its first body while every one of its bodies stays PICKABLE through the
 * collector's key index.
 */
struct FCkJoltDebugger_BodySnapshot
{
    FCk_Handle Handle;

    TOptional<uint64> BodyKey;

    ECkJoltDebugger_Population Population = ECkJoltDebugger_Population::JoltBody;

    ECk_MotionType        MotionType = ECk_MotionType::Static;
    ECk_Jolt_SleepState   SleepState = ECk_Jolt_SleepState::Awake;
    bool                  HasSimulationState = false;

    FString DisplayName;

    FString SourceActorName;
    int32   NumBodies = 0;

    FVector LinearVelocity = FVector::ZeroVector;
    bool    HasLinearVelocity = false;
};

// --------------------------------------------------------------------------------------------------------------------
