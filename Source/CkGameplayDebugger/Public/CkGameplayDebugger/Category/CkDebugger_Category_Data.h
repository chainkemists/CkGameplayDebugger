#pragma once
#include "CkCore/Macros/CkMacros.h"

#include "CkDebugger_Category_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class AGameplayDebuggerCategoryReplicator;
class FGameplayDebuggerCanvasContext;

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

public:
    CK_PROPERTY_GET(_OwnerPC);
    CK_PROPERTY_GET(_DebugActor);
    CK_PROPERTY_GET(_Replicator);
};

DECLARE_DYNAMIC_DELEGATE_OneParam(
    FCk_Delegate_GameplayDebugger_OnCollectData,
    const FCk_Payload_GameplayDebugger_OnCollectData&,
    InPayload);

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
    FGameplayDebuggerCanvasContext* _CanvasContext = nullptr;

public:
    CK_PROPERTY_GET(_OwnerPC);
    CK_PROPERTY_GET(_Replicator);
    CK_PROPERTY_GET(_CanvasContext);
};

DECLARE_DYNAMIC_DELEGATE_OneParam(
    FCk_Delegate_GameplayDebugger_OnDrawData,
    const FCk_Payload_GameplayDebugger_OnDrawData&,
    InPayload);

// --------------------------------------------------------------------------------------------------------------------
