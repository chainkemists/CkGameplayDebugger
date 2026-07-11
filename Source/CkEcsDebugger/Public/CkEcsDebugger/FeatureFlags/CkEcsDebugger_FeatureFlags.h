#pragma once

#include "CkEcs/Registry/CkRegistry.h"

#include <CoreMinimal.h>

// --------------------------------------------------------------------------------------------------------------------
// Debugger-side feature-flag registration (redesign spec §5 + Phase A).
//
// CkEcs owns the bit-cache MECHANISM (ck::debug_feature_flags) but is feature-agnostic;
// this module links every feature module, so the FeatureId → marker-fragment table lives
// here. RegisterAll() is idempotent and called once at module startup; the cache only
// costs anything after EnableFor() connects it to a registry (the debugger observing a
// world).
//
// FeatureIds are the debugger-wide short names (aligned with icon ids and, later, rail
// chips / query tokens / archetype FeatureIds).
// --------------------------------------------------------------------------------------------------------------------

namespace ck::ecs_debugger_feature_flags
{
    CKECSDEBUGGER_API auto RegisterAll() -> void;

    CKECSDEBUGGER_API auto EnableFor(const FCk_Registry& InRegistry) -> void;
    CKECSDEBUGGER_API auto DisableFor(const FCk_Registry& InRegistry) -> void;
}
