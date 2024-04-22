#pragma once

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"
#include "CkEcsDebugger/Windows/Timer/CkTimer_DebugConfig.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_Timer_DebugWindow : public FCk_Ecs_DebugWindow
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

private:
    ImGuiTextFilter _Filter;
    TObjectPtr<UCk_Timer_DebugWindowConfig> _Config;
    TObjectPtr<UCk_Timer_DebugWindowConfigDisplay> _DisplayConfig;
};

// --------------------------------------------------------------------------------------------------------------------

