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

UENUM(BlueprintType)
enum class ECk_GameplayDebugger_Filter_PreviousSelection_Policy : uint8
{
    Default,

    // If the previously selected actor is amongst the list of gathered actors
    // do not filter it out to ensure debug actor stability
    DontFilterOutPreviousSelection,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_GameplayDebugger_Filter_PreviousSelection_Policy);

// --------------------------------------------------------------------------------------------------------------------
