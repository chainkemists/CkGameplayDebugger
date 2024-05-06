#pragma once

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_World_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

    auto
    GetMainMenuWidgetWidth(
        int32 SubWidgetIndex,
        float MaxWidth) -> float;

    auto
    RenderMainMenuWidget(
        int32 SubWidgetIndex,
        float Width) -> void;

protected:
    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;
};

// --------------------------------------------------------------------------------------------------------------------

