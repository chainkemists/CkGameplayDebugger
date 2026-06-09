#pragma once

#include "NativeGameplayTags.h"
#include "GameplayTagContainer.h"

// --------------------------------------------------------------------------------------------------------------------

CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger);
CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger_Layout);
CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger_Provider);

// Layout leaf tags — one per pre-tuned default layout
CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger_Layout_AI);
CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger_Layout_Animation);
CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger_Layout_Movement);
CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger_Layout_Combat);
CKENTITYDEBUGOVERLAY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ck_OnScreenDebugger_Layout_Overview);

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugoverlay
{
    CKENTITYDEBUGOVERLAY_API auto Get_LeafName(const FGameplayTag& InTag) -> FString;
}

// --------------------------------------------------------------------------------------------------------------------
