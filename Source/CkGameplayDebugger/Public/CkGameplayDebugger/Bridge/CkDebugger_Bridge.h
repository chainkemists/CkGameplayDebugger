#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkGameplayDebugger/CkDebugger_Common.h"
#include "CkGameplayDebugger/Category/CkDebugger_Category.h"
#include "CkGameplayDebugger/Profile/CkDebugger_Profile.h"

#include <GameFramework/Actor.h>
#include <GameFramework/Info.h>

#include "CkDebugger_Bridge.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotPlaceable, NotBlueprintable, NotBlueprintType)
class CKGAMEPLAYDEBUGGER_API ACk_GameplayDebugger_DebugBridge_UE : public AInfo
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(ACk_GameplayDebugger_DebugBridge_UE);

public:
    ACk_GameplayDebugger_DebugBridge_UE();

public:
    auto LoadNewDebugProfile(class UCk_GameplayDebugger_DebugProfile_PDA* InDebugProfile) -> void;
    auto UnloadCurrentDebugProfile() -> void;

private:
    UFUNCTION()
    void OnGameplayDebugger_Activated();

    UFUNCTION()
    void OnGameplayDebugger_Deactivated();

    UFUNCTION()
    void OnGameplayDebugger_CollectData(
        const FCk_Payload_GameplayDebugger_OnCollectData& InPayload);

    UFUNCTION()
    void OnGameplayDebugger_DrawData(
        const FCk_Payload_GameplayDebugger_OnDrawData& InPayload);

private:
    auto DoActivateCurrentlySelectedFilter() const -> void;
    auto DoDeactivateCurrentlySelectedFilter() const -> void;

    auto DoChangeFilter(
        int32 InPreviousFilterIndex,
        int32 InNewFilterIndex) -> void;

    auto DoHandleDebugActorSelectionCycling(
        const FCk_Payload_GameplayDebugger_OnDrawData& InDrawData) -> void;

    auto DoGet_FormattedCanvasMessage(
        const UCk_GameplayDebugger_DebugFilter_UE* InSelectedFilter,
        const FCk_GameplayDebugger_DebugActorList& InFilteredActors) const -> FString;

    auto DoPerformActionsOnFilteredActors(
        const UCk_GameplayDebugger_DebugFilter_UE* InSelectedFilter,
        const FCk_GameplayDebugger_DebugActorList& InFilteredActors,
        class FGameplayDebuggerCanvasContext* InCanvasContext) const -> void;

    auto DoHandleFilterChanges(
        APlayerController* const& InOwnerPC,
        const FCk_GameplayDebugger_DebugNavControls& InDebugNavControls) -> void;

    auto DoHandleFilterActionActivationToggling(
        APlayerController* const& InOwnerPC,
        const UCk_GameplayDebugger_DebugFilter_UE* InSelectedFilter) const -> void;

    auto DoHandleSelectedActorChange(
        APlayerController* const& InOwnerPC,
        const FCk_GameplayDebugger_DebugNavControls& InDebugNavControls,
        const TArray<TObjectPtr<AActor>>& InSortedFilteredActors) -> void;

private:
    UPROPERTY(Transient)
    TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA> _CurrentlyLoadedDebugProfile;

    UPROPERTY(Transient)
    TObjectPtr<class UCk_GameplayDebugger_DebugWidget_UE> _DebugWidget;

private:
    int32 _CurrentlySelectedActorIndex = 0;
    int32 _CurrentlySelectedFilterIndex = 0;

    TWeakObjectPtr<AActor> _PreviouslySelectedActor;
};

// --------------------------------------------------------------------------------------------------------------------
