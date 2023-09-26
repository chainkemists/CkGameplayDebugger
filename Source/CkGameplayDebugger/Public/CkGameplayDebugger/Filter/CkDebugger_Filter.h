#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Object/CkWorldContextObject.h"
#include "CkCore/Time/CkTime.h"

#include "CkGameplayDebugger/CkDebugger_Common.h"
#include "CkGameplayDebugger/Filter/CkDebugger_Filter_Data.h"

#include "CkDebugger_Filter.generated.h"

#define CK_GAMEPLAY_DEBUGGER_INVALID_FILTER_NAME TEXT("[NAMELESS FILTER]")

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Abstract, Blueprintable, EditInlineNew)
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_DebugFilter_UE : public UCk_GameWorldContextObject_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameplayDebugger_DebugFilter_UE);

public:
    auto ActivateFilter() -> void;
    auto DeactivateFilter() -> void;
    auto Get_HasValidFilterName() const -> bool;
    auto Get_SortedFilteredActors(const FCk_GameplayDebugger_GetSortedFilteredActors_Params& InParams) -> FCk_GameplayDebugger_DebugActorList;
    auto GatherAndFilterActors(const FCk_GameplayDebugger_GatherAndFilterActors_Params& InParams) -> FCk_GameplayDebugger_GatherAndFilterActors_Result;
    auto SortFilteredActors(const FCk_GameplayDebugger_SortFilteredActors_Params& InParams) -> FCk_GameplayDebugger_SortFilteredActors_Result;

protected:
    UFUNCTION(BlueprintNativeEvent,
              Category = "Ck|GameplayDebugger|Filter",
              meta     = (DisplayName = "GatherAndFilterActors"))
    FCk_GameplayDebugger_GatherAndFilterActors_Result
    DoGatherAndFilterActors(
        const FCk_GameplayDebugger_GatherAndFilterActors_Params& InParams);

    UFUNCTION(BlueprintNativeEvent,
              Category = "Ck|GameplayDebugger|Filter",
              meta     = (DisplayName = "SortFilteredActors"))
    FCk_GameplayDebugger_SortFilteredActors_Result
    DoSortFilteredActors(
        const FCk_GameplayDebugger_SortFilteredActors_Params& InParams);

protected:
    auto UpdateFilterName(FName InNewFilterName) -> void;

private:
    // Name of the filter displayed on screen when it is active
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FName _FilterName = CK_GAMEPLAY_DEBUGGER_INVALID_FILTER_NAME;

    // List of debug action blueprints to run when the filter is active
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced,
              meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<class UCk_GameplayDebugger_DebugAction_UE>> _FilterDebugActions;

    // Frequency at which the filter is gathering and filtering actors
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_Time _UpdateFrequency = FCk_Time::Zero;

private:
    FCk_Time _TimeSinceLastUpdate;
    FCk_GameplayDebugger_DebugActorList _CachedActorList;

public:
    CK_PROPERTY_GET(_FilterName);
    CK_PROPERTY_GET(_FilterDebugActions);
};

// --------------------------------------------------------------------------------------------------------------------