#include "CkDebugger_ActorsOfClassFilter.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/World/CkWorld_Utils.h"

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
    const auto& DrawData = InParams.Get_DrawData();

    const auto& OwnerPC = DrawData.Get_OwnerPC().Get();
    if (ck::Is_NOT_Valid(OwnerPC))
    { return {}; }

    if (const auto& ControlledPawn = OwnerPC->GetPawn();
        ck::Is_NOT_Valid(ControlledPawn))
    { return {}; }

    const auto& CanvasContext = DrawData.Get_CanvasContext();
    if (ck::Is_NOT_Valid(CanvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return {}; }

    const auto& FilteredActors = [&]() -> FCk_GameplayDebugger_DebugActorList
    {
        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClass(Cast<UObject>(DrawData.Get_CurrentWorld().Get()), _ValidActorClass, FoundActors);

        switch (_ActorVisibilityPolicy)
        {
            case ECk_GameplayDebugger_Filter_ActorVisibility_Policy::Default:
            {
                return FCk_GameplayDebugger_DebugActorList{FoundActors};
            };
            case ECk_GameplayDebugger_Filter_ActorVisibility_Policy::VisibleOnScreenOnly:
            {
                const auto& Get_IsOnScreen = [&](const AActor* InActor) -> bool
                {
                    return DoGet_ActorScreenPosition(*InActor, *CanvasContext).Z > 0;
                };

                const auto& ActorsOnScreen = FoundActors.FilterByPredicate(Get_IsOnScreen);

                return FCk_GameplayDebugger_DebugActorList{ActorsOnScreen};
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
        FCk_GameplayDebugger_DebugActorList{FilteredActors}
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
    const auto& DrawData = InParams.Get_DrawData();

    const auto& OwnerPC = DrawData.Get_OwnerPC().Get();
    if (ck::Is_NOT_Valid(OwnerPC))
    { return {}; }

    const auto& ControlledPawn = OwnerPC->GetPawn();
    if (ck::Is_NOT_Valid(ControlledPawn))
    { return {}; }

    const auto& CanvasContext = DrawData.Get_CanvasContext();
    if (ck::Is_NOT_Valid(CanvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return {}; }

    const auto& PawnLocation = ControlledPawn->GetActorLocation();

    const auto& Get_IsNear = [&](const AActor& InActor) -> bool
    {
        return FVector::DistSquared(InActor.GetActorLocation(), PawnLocation) < Get_SortingMaxDistance();
    };

    const auto& SortingPredicate = [&](const AActor& lhs, const AActor& rhs) -> bool
    {
        const auto& isLhsNear = Get_IsNear(lhs);
        const auto& isRhsNear = Get_IsNear(rhs);

        if (isLhsNear != isRhsNear)
        { return isLhsNear; }

        return DoGet_ActorScreenPosition(lhs, *CanvasContext).X < DoGet_ActorScreenPosition(rhs, *CanvasContext).X;
    };

    auto SortedFilteredActors = InParams.Get_FilteredActors().Get_DebugActors();
    SortedFilteredActors.Sort(SortingPredicate);

    return FCk_GameplayDebugger_SortFilteredActors_Result
    {
        FCk_GameplayDebugger_DebugActorList{SortedFilteredActors}
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

    const auto& ActorLocation = InActor.GetActorLocation();
    const auto& IsActorLocationVisible = InCanvasContext.IsLocationVisible(ActorLocation);

    const auto& Position2D = InCanvasContext.ProjectLocation(ActorLocation);

    return FVector{Position2D.X, Position2D.Y, IsActorLocationVisible ? visible : notVisible};
#else
    return {};
#endif
}

// --------------------------------------------------------------------------------------------------------------------
