#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "Cog/Public/CogWindow.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_Ecs_DebugWindow : public FCogWindow
{
    using Super = FCogWindow;

public:
    CK_GENERATED_BODY(FCk_Ecs_DebugWindow);

public:
    auto Get_SelectionEntity() -> FCk_Handle;

private:
    FCk_Handle _PreviousEntity;

public:
    CK_PROPERTY_GET(_PreviousEntity);
};

// --------------------------------------------------------------------------------------------------------------------
