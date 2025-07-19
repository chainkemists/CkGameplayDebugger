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
    RenderTick(
        float InDeltaT) -> void override;

    auto
    GameTick(
        float InDeltaT) -> void override;

    auto
    RenderMenu() -> void;

    auto
    RenderOpenedProbes() -> void;

    auto
    RenderTable(
        const TArray<FCk_Handle>& InSelectionEntities) -> void;

    auto
    RenderProbeInfo(
        const FCk_Handle_Probe& InProbe) -> void;

    auto
    RenderProbeContextMenu(
        const FCk_Handle_Probe& InProbe,
        int32 InIndex) -> void;

    auto
    OpenProbe(
        const FCk_Handle_Probe& InProbe) -> void;

    auto
    CloseProbe(
        const FCk_Handle_Probe& InProbe) -> void;

    auto
    ProcessProbeEnableDisable(
        FCk_Handle_Probe& InProbe) -> void;

private:
    auto
    DoGet_ProbeName(
        const FCk_Handle_Probe& InProbe) const
        -> FName;

    auto
    DoGet_ProbeMotionType(
        const FCk_Handle_Probe& InProbe) const
        -> FString;

    auto
    DoGet_ProbeMotionQuality(
        const FCk_Handle_Probe& InProbe) const
        -> FString;

private:
    auto
    AddToFilteredProbes(
        const FCk_Handle& InEntity) -> void;

private:
    UCk_Probes_DebugWindowConfig* _Config = nullptr;

    FCk_Handle_Probe _ProbeHandleToToggle;
    TArray<FCk_Handle_Probe> _OpenedProbes;

    TArray<FCk_Handle_Probe> _FilteredProbes;
};

// --------------------------------------------------------------------------------------------------------------------