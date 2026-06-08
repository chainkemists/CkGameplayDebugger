#pragma once

#include "NativeGameplayTags.h"
#include "GameplayTagContainer.h"

// --------------------------------------------------------------------------------------------------------------------

CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger);
CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger_Layout);
CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger_Provider);

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugoverlay
{
    CKENTITYDEBUGOVERLAY_API auto Get_LeafName(const FGameplayTag& InTag) -> FString;
}

// --------------------------------------------------------------------------------------------------------------------
