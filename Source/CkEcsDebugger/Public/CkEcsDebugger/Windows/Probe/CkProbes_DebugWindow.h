#pragma once

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"
#include "CkEcsDebugger/Windows/Probe/CkProbes_DebugConfig.h"

#include "CkSpatialQuery/Public/CkSpatialQuery/Probe/CkProbe_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

struct FCk_Handle_Probe;

class FCk_Probes_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

protected:
    auto
    ResetConfig() -> void override;

    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;

    auto
    GameTick(
        float InDeltaT) -> void override;

    auto
    RenderMenu() -> void;

    auto
    RenderEntityProbesSection(
        const FCk_Handle& InEntity) -> void;

    auto
    ProcessProbeEnableDisable(
        FCk_Handle_Probe& InProbe) -> void;

private:
    static auto
    Get_ProbeName(
        const FCk_Handle_Probe& InProbe) -> FString;

    static auto
    Get_ProbeMotionType(
        const FCk_Handle_Probe& InProbe) -> FString;

    static auto
    Get_ProbeMotionQuality(
        const FCk_Handle_Probe& InProbe) -> FString;

    static auto
    Get_ProbeResponsePolicy(
        const FCk_Handle_Probe& InProbe) -> FString;

private:
    TObjectPtr<UCk_Probes_DebugWindowConfig> _Config;

    FCk_Handle_Probe _ProbeHandleToToggle;
};

// --------------------------------------------------------------------------------------------------------------------