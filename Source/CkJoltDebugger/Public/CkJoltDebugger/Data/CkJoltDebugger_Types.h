#pragma once

#include "CoreMinimal.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkJolt/CkJolt_Common.h"
#include "CkJolt/Body/CkJoltBody_Fragment_Data.h"
#include "CkJolt/Constraint/CkJoltConstraint_Fragment_Data.h"
#include "CkJolt/Subsystem/CkJolt_DebugDrawTarget.h"

// --------------------------------------------------------------------------------------------------------------------

/*
 * Which of the five Jolt-backing features produced a row. The debug-draw colour classes are finer-grained
 * (a JoltBody splits four ways by motion type and sleep state); this is the ENTITY-shaped grouping the
 * outliner lists by, and the first four line up one-to-one with the window's population toggles.
 *
 * Constraint is the odd one out and deliberately so (P8-D55): a constraint entity draws NO body of its own,
 * so it has no colour class and no population toggle. It is listed because "what is joined to what" is a
 * question this window is opened to answer, and selecting one highlights the two bodies it joins.
 */
enum class ECkJoltDebugger_Population : uint8
{
    JoltBody,
    BakedStatic,
    Sensor,
    Character,
    Constraint
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

    /*
     * CONSTRAINT rows only. The row's own BodyKey stays UNSET — a constraint draws nothing, and a key here
     * would collide with the body row that already owns it in the shared key -> row lookup. These are the
     * bodies the constraint JOINS, in A,B order; a world-anchored constraint contributes only A, and the
     * FIRST is what the facility takes as the primary of the resulting highlight.
     */
    TArray<uint64> ConstraintBodyKeys;

    ECk_JoltConstraint_Type ConstraintType = ECk_JoltConstraint_Type::Distance;

    bool IsBodyBWorldAnchor = false;

    /*
     * What the facility's health scan found wrong with this row's body, stamped onto the snapshot AFTER the
     * collector pass (the collector reads the ECS, never JPH). None for a row with no drawn body behind it,
     * and None whenever the scan is unarmed.
     */
    ECk_Jolt_DebugDraw_ProblemFlags ProblemFlags = ECk_Jolt_DebugDraw_ProblemFlags::None;

    auto Get_HasProblem() const -> bool
    { return ProblemFlags != ECk_Jolt_DebugDraw_ProblemFlags::None; }
};

// --------------------------------------------------------------------------------------------------------------------

/*
 * Everything the detail panel renders that the OUTLINER row does not carry. Deliberately NOT folded into
 * FCkJoltDebugger_BodySnapshot: the snapshot is the outliner's flat row, copied per row per refresh, and
 * three facility structs plus a contacts array on every one of them would be paid for by every row to
 * serve the one that is selected.
 *
 * All three fields come from the facility's capture, which sampled them in the physics pipeline's
 * async-safe window — this module never reads a JPH body for any of it.
 */
struct FCkJoltDebugger_SelectionFacts
{
    /// The PRIMARY selection's rigid-body sample. Unset for a character, and for a body the last capture
    /// did not draw (a sleeping or static body between scene-revision passes).
    TOptional<FCk_Jolt_DebugDraw_BodySample> BodySample;

    /// The character twin. Mutually exclusive with BodySample — a key is one or the other.
    TOptional<FCk_Jolt_DebugDraw_CharacterSample> CharacterSample;

    /// Bodies the primary selection is touching. Empty unless the window has asked the facility for them
    /// (Set_WantsSelectionContacts), and always empty for a character selection.
    TArray<FCk_Jolt_DebugDraw_ContactEntry> Contacts;

    auto Reset() -> void
    {
        BodySample.Reset();
        CharacterSample.Reset();
        Contacts.Reset();
    }
};

// --------------------------------------------------------------------------------------------------------------------
