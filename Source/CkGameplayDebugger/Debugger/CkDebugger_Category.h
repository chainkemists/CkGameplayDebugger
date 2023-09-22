#pragma once

#if WITH_GAMEPLAY_DEBUGGER
#include <GameplayDebuggerCategory.h>
#endif

#include <CoreMinimal.h>

#include "CkDebugger_Category.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class AActor;
class APlayerController;
class FGameplayDebuggerCanvasContext;
class AGameplayDebuggerCategoryReplicator;

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DYNAMIC_DELEGATE(FCk_Delegate_GameplayDebugger_OnDeactivated);
DECLARE_DYNAMIC_DELEGATE(FCk_Delegate_GameplayDebugger_OnActivated);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_Payload_GameplayDebugger_OnCollectData
{
    GENERATED_BODY();

public:
    FCk_Payload_GameplayDebugger_OnCollectData() = default;
    FCk_Payload_GameplayDebugger_OnCollectData(
        APlayerController* InOwnerPC,
        AActor* InDebugActor,
        AGameplayDebuggerCategoryReplicator* InReplicator);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    TWeakObjectPtr<APlayerController> _OwnerPC;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    TWeakObjectPtr<AActor> _DebugActor;

    UPROPERTY(Transient)
    TWeakObjectPtr<AGameplayDebuggerCategoryReplicator> _Replicator;
};

DECLARE_DYNAMIC_DELEGATE_OneParam
(
    FCk_Delegate_GameplayDebugger_OnCollectData,
    const FCk_Payload_GameplayDebugger_OnCollectData&,
    InPayload
);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_Payload_GameplayDebugger_OnDrawData
{
    GENERATED_BODY();

public:
    FCk_Payload_GameplayDebugger_OnDrawData() = default;
    FCk_Payload_GameplayDebugger_OnDrawData(
        APlayerController* InOwnerPC,
        FGameplayDebuggerCanvasContext* InCanvasContext,
        AGameplayDebuggerCategoryReplicator* InReplicator);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    TWeakObjectPtr<APlayerController> _OwnerPC;

    UPROPERTY(Transient)
    TWeakObjectPtr<AGameplayDebuggerCategoryReplicator> _Replicator;

private:
    FGameplayDebuggerCanvasContext* _CanvasContext;
};

DECLARE_DYNAMIC_DELEGATE_OneParam
(
    FCk_Delegate_GameplayDebugger_OnDrawData,
    const FCk_Payload_GameplayDebugger_OnDrawData&,
    InPayload
);

#if WITH_GAMEPLAY_DEBUGGER

// --------------------------------------------------------------------------------------------------------------------

class CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_Category : public FGameplayDebuggerCategory
{
    friend class ACk_GameplayDebugger_ActorHelper_UE;

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
    auto CollectData(APlayerController* OwnerPC, AActor* DebugActor) -> void override;
    auto DrawData(APlayerController* OwnerPC,FGameplayDebuggerCanvasContext& CanvasContext)-> void override;

protected:
    auto OnGameplayDebuggerActivated() -> void override;
    auto OnGameplayDebuggerDeactivated() -> void override;

private:
    OnActivatedDelegateType _OnActivatedDelegate;
    OnDeactivatedDelegateType _OnDeactivatedDelegate;
    OnCollectDataDelegateType _OnCollectDataDelegate;
    OnDrawDataDelegateType _OnDrawDataDelegate;

private:
    static TSharedPtr<FCk_GameplayDebugger_Category> _This;
};

#endif // WITH_GAMEPLAY_DEBUGGER

// --------------------------------------------------------------------------------------------------------------------
