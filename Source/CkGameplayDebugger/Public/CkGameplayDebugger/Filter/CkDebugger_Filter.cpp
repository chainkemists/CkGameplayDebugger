#include "CkDebugger_Filter.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/World/CkWorld_Utils.h"

#include "CkGameplayDebugger/Action/CkDebugger_Action.h"

#include <Engine/World.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameplayDebugger_DebugFilter_UE::
    ActivateFilter()
    -> void
{
    ck::algo::ForEachIsValid(_FilterDebugActions, [](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InDebugAction)
    {
        InDebugAction->ActivateAction();
    });
}

auto
    UCk_GameplayDebugger_DebugFilter_UE::
    DeactivateFilter()
    -> void
{
    ck::algo::ForEachIsValid(_FilterDebugActions, [](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InDebugAction)
    {
        InDebugAction->DeactivateAction();
    });
}

auto
    UCk_GameplayDebugger_DebugFilter_UE::
    Get_HasValidFilterName() const
    -> bool
{
    return _FilterName != NAME_None && _FilterName != CK_GAMEPLAY_DEBUGGER_INVALID_FILTER_NAME;
}

auto
    UCk_GameplayDebugger_DebugFilter_UE::
    Get_SortedFilteredActors(
        const FCk_GameplayDebugger_GetSortedFilteredActors_Params& InParams)
    -> FCk_GameplayDebugger_DebugActorList
{
    if (ck::Is_NOT_Valid(this->GetWorld(), ck::IsValid_Policy_NullptrOnly{}))
    { return {}; }

    _TimeSinceLastUpdate += FCk_Time{ this->GetWorld()->DeltaRealTimeSeconds };

    if (_CachedActorList.Get_DebugActors().IsEmpty())
    {
        _TimeSinceLastUpdate = _UpdateFrequency;
    }

    if (_TimeSinceLastUpdate < _UpdateFrequency)
    {
        return _CachedActorList;
    }

    _TimeSinceLastUpdate = _TimeSinceLastUpdate - _UpdateFrequency;

    const auto& DrawData = InParams.Get_DrawData();

    const auto& FilteredActors = GatherAndFilterActors(FCk_GameplayDebugger_GatherAndFilterActors_Params{DrawData});
    const auto& SortedFilteredActors = SortFilteredActors
    (
        FCk_GameplayDebugger_SortFilteredActors_Params
        {
            DrawData,
            FilteredActors.Get_FilteredDebugActors()
        }
    );

    _CachedActorList = SortedFilteredActors.Get_SortedFilteredActors();
    return _CachedActorList;
}

auto
    UCk_GameplayDebugger_DebugFilter_UE::
    GatherAndFilterActors(
        const FCk_GameplayDebugger_GatherAndFilterActors_Params& InParams)
    -> FCk_GameplayDebugger_GatherAndFilterActors_Result
{
    return DoGatherAndFilterActors(InParams);
}

auto
    UCk_GameplayDebugger_DebugFilter_UE::
    SortFilteredActors(
        const FCk_GameplayDebugger_SortFilteredActors_Params& InParams)
    -> FCk_GameplayDebugger_SortFilteredActors_Result
{
    return DoSortFilteredActors(InParams);
}

auto
    UCk_GameplayDebugger_DebugFilter_UE::
    DoGatherAndFilterActors_Implementation(
        const FCk_GameplayDebugger_GatherAndFilterActors_Params& InParams)
    -> FCk_GameplayDebugger_GatherAndFilterActors_Result
{
    return {};
}

auto
    UCk_GameplayDebugger_DebugFilter_UE::
    DoSortFilteredActors_Implementation(
        const FCk_GameplayDebugger_SortFilteredActors_Params& InParams)
    -> FCk_GameplayDebugger_SortFilteredActors_Result
{
    return FCk_GameplayDebugger_SortFilteredActors_Result{InParams.Get_FilteredActors()};
}

auto
    UCk_GameplayDebugger_DebugFilter_UE::
    UpdateFilterName(
        FName InNewFilterName)
    -> void
{
    _FilterName = InNewFilterName;
}

// --------------------------------------------------------------------------------------------------------------------

