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
    auto Get_SelectionEntities() -> TArray<FCk_Handle>;
    auto Get_PrimarySelectionEntity() -> FCk_Handle;

private:
    TArray<FCk_Handle> _PreviousEntities;

public:
    CK_PROPERTY_GET(_PreviousEntities);
};

// --------------------------------------------------------------------------------------------------------------------