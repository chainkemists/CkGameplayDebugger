#pragma once

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_EntityBasics_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

protected:
    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;
};

// --------------------------------------------------------------------------------------------------------------------

