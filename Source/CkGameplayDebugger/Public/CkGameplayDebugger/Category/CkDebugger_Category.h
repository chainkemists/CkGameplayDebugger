#pragma once

#include "CkCore/Time/CkTime.h"

#include "CkGameplayDebugger/Category/CkDebugger_Category_Data.h"

#if WITH_GAMEPLAY_DEBUGGER
#include <GameplayDebuggerCategory.h>
#endif

#include <CoreMinimal.h>

// --------------------------------------------------------------------------------------------------------------------

#if WITH_GAMEPLAY_DEBUGGER

class CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_Category : public FGameplayDebuggerCategory
{
    friend class ACk_GameplayDebugger_DebugBridge_UE;

public:
    FCk_GameplayDebugger_Category();

public:
    using OnActivatedDelegateType = FCk_Delegate_GameplayDebugger_OnActivated;
    using OnDeactivatedDelegateType = FCk_Delegate_GameplayDebugger_OnDeactivated;

    using OnCollectDataPayloadType = FCk_Payload_GameplayDebugger_OnCollectData;
    using OnCollectDataDelegateType = FCk_Delegate_GameplayDebugger_OnCollectData;

    using OnDrawDataPayloadType = FCk_Payload_GameplayDebugger_OnDrawData;
    using OnDrawDataDelegateType = FCk_Delegate_GameplayDebugger_OnDrawData;

public:
    static auto MakeInstance() -> TSharedRef<FGameplayDebuggerCategory>;
    static auto Get_Instance() -> TSharedPtr<FCk_GameplayDebugger_Category>;

public:
    auto CollectData(
        APlayerController* InOwnerPC,
        AActor* InDebugActor) -> void override;

    auto DrawData(
        APlayerController* InOwnerPC,
        FGameplayDebuggerCanvasContext& InCanvasContext)-> void override;

protected:
    auto OnGameplayDebuggerActivated() -> void override;
    auto OnGameplayDebuggerDeactivated() -> void override;

private:
    OnActivatedDelegateType _OnActivatedDelegate;
    OnDeactivatedDelegateType _OnDeactivatedDelegate;
    OnCollectDataDelegateType _OnCollectDataDelegate;
    OnDrawDataDelegateType _OnDrawDataDelegate;

    FCk_WorldTime _LastUpdated;

    TArray<TWeakObjectPtr<UWorld>> _Worlds;
    int32 _NextWorldToUse = 0;

private:
    static TSharedPtr<FCk_GameplayDebugger_Category> _This;
};

#endif // WITH_GAMEPLAY_DEBUGGER

// --------------------------------------------------------------------------------------------------------------------
