#pragma once

#include "CkCore/Format/CkFormat.h"

#include "CkDebugger_ActorsOfClassFilter_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_GameplayDebugger_Filter_ActorVisibility_Policy : uint8
{
    // Will filter all actors
    Default,

    // Discard actors that are not on screen
    VisibleOnScreenOnly
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_GameplayDebugger_Filter_ActorVisibility_Policy);

// --------------------------------------------------------------------------------------------------------------------
