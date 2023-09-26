#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkDebugger_ActorsOfClassFilter_Data.h"
#include "CkGameplayDebugger/Filter/CkDebugger_Filter.h"

#include "CkDebugger_ActorsOfClassFilter.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Abstract, Blueprintable)
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_ActorsOfClass_DebugFilter_UE : public UCk_GameplayDebugger_DebugFilter_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameplayDebugger_ActorsOfClass_DebugFilter_UE);

protected:
    auto DoGatherAndFilterActors_Implementation(
        const FCk_GameplayDebugger_GatherAndFilterActors_Params& InParams) -> FCk_GameplayDebugger_GatherAndFilterActors_Result override;

    auto DoSortFilteredActors_Implementation(
        const FCk_GameplayDebugger_SortFilteredActors_Params& InParams) -> FCk_GameplayDebugger_SortFilteredActors_Result override;

private:
    auto DoGet_ActorScreenPosition(
        const AActor& InActor,
        const class FGameplayDebuggerCanvasContext& InCanvasContext) const -> FVector;

private:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TSubclassOf<AActor> _ValidActorClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_GameplayDebugger_Filter_ActorVisibility_Policy _ActorVisibilityPolicy = ECk_GameplayDebugger_Filter_ActorVisibility_Policy::Default;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true, ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
    float _SortingMaxDistance = 9000000.0f;

public:
    CK_PROPERTY_GET(_ValidActorClass);
    CK_PROPERTY_GET(_ActorVisibilityPolicy);
    CK_PROPERTY_GET(_SortingMaxDistance);
};

// --------------------------------------------------------------------------------------------------------------------
