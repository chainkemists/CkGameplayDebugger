#pragma once

#include "CkAnimation/AnimPlan/CkAnimPlan_Fragment_Data.h"

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
    static auto
    RenderEntityAnimPlansSection(
        const FCk_Handle& InEntity) -> void;

    static auto
    RenderAnimPlansTable(
        const FCk_Handle& InEntity) -> void;

    static auto
    Request_RenderTableRow_AnimPlanValue(
        const char* InLabel,
        const FString& InValue,
        bool InIsValid) -> void;

    static auto
    Request_RenderTableRow_AnimPlanGoal(
        const char* InLabel,
        const FString& InValue) -> void;

private:
    FCk_Entity _HistoryOfEntity;
    TArray<FGameplayTag> _Goal;
    TArray<FGameplayTag> _Cluster;
    TArray<FGameplayTag> _State;
};

// --------------------------------------------------------------------------------------------------------------------