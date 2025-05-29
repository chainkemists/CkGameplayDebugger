#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "Cog/Public/CogWindow.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_Ecs_DebugWindow : public FCogWindow
{
    using Super = FCogWindow;

public:
    auto Get_SelectionEntity() -> FCk_Handle;
};

// --------------------------------------------------------------------------------------------------------------------
