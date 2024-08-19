#include "CkEntityCollection_DebugConfig.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    UCk_EntityCollection_DebugWindowConfig::
    Reset()
    -> void
{
    Super::Reset();

    DisplayContentOnSingleLine = false;
    DisplayContentFullName = true;
    RenderCollectionsWithNoContent = true;
}

//--------------------------------------------------------------------------------------------------------------------------
