#pragma once

#include "Containers/UnrealString.h"

// ====================================================================================================================
// Display-name cleanup for debugger surfaces.
//
// UObject-derived debug names carry machinery noise that wastes precious space on
// compact surfaces (overlay plates, entity pills, tree rows):
//   "Default__BP_Foo_C"                  → "BP_Foo"
//   "Ck_CrowdGym_Pathing_PlayerController_0" → "Ck_CrowdGym_Pathing_PlayerController"
//
// Only the unambiguous suffixes are stripped: the "_0" first-instance suffix and the
// "_C" generated-class suffix. "_1"/"_2"… are KEPT — in multi-instance PIE they are
// the only thing distinguishing the instances.
// ====================================================================================================================

namespace ck::DebugNameClean
{
    CKDEBUGGERCOMMON_API auto Get_CleanName(const FString& InRaw) -> FString;
}

// ====================================================================================================================
