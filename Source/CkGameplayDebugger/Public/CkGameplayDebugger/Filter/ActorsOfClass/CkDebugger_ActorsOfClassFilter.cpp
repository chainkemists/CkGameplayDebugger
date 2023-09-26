#include "CkDebugger_ActorsOfClassFilter.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkGameplayDebugger/Engine/CkDebugger_GameplayDebuggerTypes.h"

#include <Engine/World.h>
#include <GameFramework/Pawn.h>
#include <Kismet/GameplayStatics.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameplayDebugger_ActorsOfClass_DebugFilter_UE::
    DoGatherAndFilterActors_Implementation(
        const FCk_GameplayDebugger_GatherAndFilterActors_Params& InParams)
    -> FCk_GameplayDebugger_GatherAndFilterActors_Result
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& drawData = InParams.Get_DrawData();

    const auto& ownerPC = drawData.Get_OwnerPC().Get();
    if (ck::Is_NOT_Valid(ownerPC))
    { return {}; }

    const auto& controlledPawn = ownerPC->GetPawn();
    if (ck::Is_NOT_Valid(controlledPawn))
    { return {}; }

    const auto& canvasContext = drawData.Get_CanvasContext();
    if (ck::Is_NOT_Valid(canvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return {}; }

    const auto& filteredActors = [&]() -> FCk_GameplayDebugger_DebugActorList
    {
        TArray<AActor*> outFoundActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), _ValidActorClass, outFoundActors);

        switch (_ActorVisibilityPolicy)
        {
            case ECk_GameplayDebugger_Filter_ActorVisibility_Policy::Default:
            {
                return FCk_GameplayDebugger_DebugActorList{outFoundActors};
            };
            case ECk_GameplayDebugger_Filter_ActorVisibility_Policy::VisibleOnScreenOnly:
            {
                const auto& Get_IsOnScreen = [&](const AActor* InActor) -> bool
                {
                    return DoGet_ActorScreenPosition(*InActor, *canvasContext).Z > 0;
                };

                const auto& actorsOnScreen = outFoundActors.FilterByPredicate(Get_IsOnScreen);

                return FCk_GameplayDebugger_DebugActorList{actorsOnScreen};
            }
            default:
            {
                CK_INVALID_ENUM(_ActorVisibilityPolicy);
                return {};
            }
        }
    }();

    return FCk_GameplayDebugger_GatherAndFilterActors_Result
    {
        FCk_GameplayDebugger_DebugActorList{filteredActors}
    };
#else
    return {};
#endif
}

auto
    UCk_GameplayDebugger_ActorsOfClass_DebugFilter_UE::
    DoSortFilteredActors_Implementation(
        const FCk_GameplayDebugger_SortFilteredActors_Params& InParams)
    -> FCk_GameplayDebugger_SortFilteredActors_Result
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& drawData = InParams.Get_DrawData();

    const auto& ownerPC = drawData.Get_OwnerPC().Get();
    if (ck::Is_NOT_Valid(ownerPC))
    { return {}; }

    const auto& controlledPawn = ownerPC->GetPawn();
    if (ck::Is_NOT_Valid(controlledPawn))
    { return {}; }

    const auto& canvasContext = drawData.Get_CanvasContext();
    if (ck::Is_NOT_Valid(canvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return {}; }

    const auto& pawnLocation = controlledPawn->GetActorLocation();

    const auto& Get_IsNear = [&](const AActor& InActor) -> bool
    {
        return FVector::DistSquared(InActor.GetActorLocation(), pawnLocation) < Get_SortingMaxDistance();
    };

    const auto& sortingPredicate = [&](const AActor& lhs, const AActor& rhs) -> bool
    {
        const auto& isLhsNear = Get_IsNear(lhs);
        const auto& isRhsNear = Get_IsNear(rhs);

        if (isLhsNear != isRhsNear)
        { return isLhsNear; }

        return DoGet_ActorScreenPosition(lhs, *canvasContext).X < DoGet_ActorScreenPosition(rhs, *canvasContext).X;
    };

    auto sortedFilteredActors = InParams.Get_FilteredActors().Get_DebugActors();
    sortedFilteredActors.Sort(sortingPredicate);

    return FCk_GameplayDebugger_SortFilteredActors_Result
    {
        FCk_GameplayDebugger_DebugActorList{sortedFilteredActors}
    };
#else
    return {};
#endif
}

auto
    UCk_GameplayDebugger_ActorsOfClass_DebugFilter_UE::
    DoGet_ActorScreenPosition(
        const AActor& InActor,
        const FGameplayDebuggerCanvasContext& InCanvasContext) const
    -> FVector
{
#if WITH_GAMEPLAY_DEBUGGER
    static constexpr double notVisible = -1.0;
    static constexpr double visible = 1.0;

    //const auto& location = InActor.GetActorLocation() + FVector{0, 0, InActor.GetSimpleCollisionHalfHeight()};
    const auto& location = InActor.GetActorLocation();
    const auto& isActorLocationVisible = InCanvasContext.IsLocationVisible(location);

    const auto& position2D = InCanvasContext.ProjectLocation(location);

    return FVector{position2D.X, position2D.Y, isActorLocationVisible ? visible : notVisible};
#else
    return {};
#endif
}

// --------------------------------------------------------------------------------------------------------------------
