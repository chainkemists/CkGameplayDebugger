#pragma once

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_AnimPlan_DebugWindow : public FCk_Ecs_DebugWindow
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
    FCk_Entity _HistoryOfEntity;
    TArray<FGameplayTag> _Goal;
    TArray<FGameplayTag> _Cluster;
    TArray<FGameplayTag> _State;
};

// --------------------------------------------------------------------------------------------------------------------

