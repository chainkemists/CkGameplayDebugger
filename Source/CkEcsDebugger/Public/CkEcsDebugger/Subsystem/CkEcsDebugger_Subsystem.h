#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Subsystems/GameWorldSubsytem/CkGameWorldSubsystem.h"

#include "CogCommon/Public/CogCommon.h"

#include "CkEcsDebugger_Subsystem.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(DisplayName = "CkSubsystem_EcsDebugger")
class CKECSDEBUGGER_API UCk_EcsDebugger_Subsystem_UE : public UCk_Game_WorldSubsystem_Base_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_EcsDebugger_Subsystem_UE);

public:
    auto Initialize(FSubsystemCollectionBase& Collection) -> void override;
    auto Deinitialize() -> void override;

protected:
    /** Called when world is ready to start gameplay before the game mode transitions to the correct state and call BeginPlay on all actors */
    auto OnWorldBeginPlay(UWorld& InWorld) -> void override;

public:
    auto Set_SelectedWorld(
        UWorld* InSelectedWorld) -> void;

    auto Set_SelectionEntities(
        const TArray<FCk_Handle>& InSelectionEntities,
        UWorld* InSelectionEntitiesOwningWorld) -> void;

    auto Add_SelectionEntity(
        const FCk_Handle& InSelectionEntity,
        UWorld* InSelectionEntityOwningWorld) -> void;

    auto Remove_SelectionEntity(
        const FCk_Handle& InSelectionEntity) -> void;

    auto Clear_SelectionEntities() -> void;

    auto Toggle_SelectionEntity(
        const FCk_Handle& InSelectionEntity,
        UWorld* InSelectionEntityOwningWorld) -> void;

    auto Get_ActorOnSelectedWorld(AActor* InActor) -> AActor*;

    auto Get_PrimarySelectionEntity() -> FCk_Handle;

private:
    UPROPERTY(Transient)
    UWorld* _SelectedWorld;

    UPROPERTY(Transient)
    TArray<FCk_Handle> _PreviousSelectionEntities;

    UPROPERTY(Transient)
    TArray<FCk_Handle> _SelectionEntities;

private:
#if ENABLE_COG
    TWeakObjectPtr<class UCogSubsystem> _CogSubsystem;
#endif

public:
    CK_PROPERTY_GET(_SelectedWorld);
    CK_PROPERTY_GET(_SelectionEntities);
    CK_PROPERTY_GET(_PreviousSelectionEntities);
};

// --------------------------------------------------------------------------------------------------------------------