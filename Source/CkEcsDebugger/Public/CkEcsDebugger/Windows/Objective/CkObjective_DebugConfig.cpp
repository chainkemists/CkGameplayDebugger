#include "CkObjective_DebugConfig.h"

#include "CkObjective/Objective/CkObjective_Utils.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    UCk_Objective_DebugWindowConfig::
    Reset()
    -> void
{
    Super::Reset();

    SortByName = false;
    ShowNotStarted = true;
    ShowActive = true;
    ShowCompleted = true;
    ShowFailed = true;
    ShowSubObjectives = true;

    NotStartedColor = _NotStartedColor;
    ActiveColor = _ActiveColor;
    CompletedColor = _CompletedColor;
    FailedColor = _FailedColor;
}

auto
    UCk_Objective_DebugWindowConfig::
    Get_ObjectiveColor(
        const FCk_Handle_Objective& InObjective) const
    -> FVector4f
{
    switch (UCk_Utils_Objective_UE::Get_Status(InObjective))
    {
        case ECk_ObjectiveStatus::NotStarted:
            return NotStartedColor;
        case ECk_ObjectiveStatus::Active:
            return ActiveColor;
        case ECk_ObjectiveStatus::Completed:
            return CompletedColor;
        case ECk_ObjectiveStatus::Failed:
            return FailedColor;
        default:
            return NotStartedColor;
    }
}

//--------------------------------------------------------------------------------------------------------------------------