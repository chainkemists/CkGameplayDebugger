#pragma once

#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Fragment_Data.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"
#include "CkEcsDebugger/Windows/Objective/CkObjective_DebugConfig.h"

// --------------------------------------------------------------------------------------------------------------------

struct FCk_Handle_ObjectiveOwner;
struct FCk_Handle_Objective;

class FCk_Objective_DebugWindow : public FCk_Ecs_DebugWindow
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
    RenderOpenedObjectives() -> void;

    auto
    RenderObjectiveTree(
        const TArray<FCk_Handle_ObjectiveOwner>& InObjectiveOwners) -> void;

    auto
    RenderObjectiveOwnerTree(
        const FCk_Handle_ObjectiveOwner& InObjectiveOwner) -> void;

    auto
    RenderObjectiveNode(
        const FCk_Handle_Objective& InObjective) -> void;

    auto
    RenderObjectiveInfo(
        const FCk_Handle_Objective& InObjective) -> void;

    auto
    OpenObjective(
        const FCk_Handle_Objective& InObjective) -> void;

    auto
    CloseObjective(
        const FCk_Handle_Objective& InObjective) -> void;

    auto
    ProcessObjectiveAction(
        FCk_Handle_Objective& InObjective) -> void;

private:
    auto
    DoGet_ObjectiveName(
        const FCk_Handle_Objective& InObjective) const
        -> FText;

    auto
    AddToFilteredObjectives(
        FCk_Handle_ObjectiveOwner InObjectiveOwner) -> void;

    auto
    ShouldShowObjective(
        const FCk_Handle_Objective& InObjective,
        const FCk_Handle_ObjectiveOwner& InRootOwner) const -> bool;

    auto
    PassesStatusFilter(
        const FCk_Handle_Objective& InObjective) const -> bool;

private:
    enum class ObjectiveAction
    {
        None,
        Complete,
        Fail
    };

    TObjectPtr<UCk_Objective_DebugWindowConfig> _Config;

    FCk_Handle_Objective _ObjectiveHandleToProcess;
    FCk_Handle_Objective _SelectedObjective;
    ObjectiveAction _PendingAction = ObjectiveAction::None;
    TArray<FCk_Handle_Objective> _OpenedObjectives;

    TMap<FCk_Handle_ObjectiveOwner, TArray<FCk_Handle_Objective>> _FilteredObjectives;
};

// --------------------------------------------------------------------------------------------------------------------