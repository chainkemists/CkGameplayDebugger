#pragma once
#include "GameplayTagContainer.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkDebugOverlay_Model.generated.h"

UENUM() enum class ECk_DebugOverlay_Severity : uint8 { Normal, Good, Warn, Bad };

USTRUCT()
struct FCk_DebugOverlay_Row
{
    GENERATED_BODY()
    UPROPERTY() FGameplayTag FieldTag;
    UPROPERTY() FText        Value;
    UPROPERTY() ECk_DebugOverlay_Severity Severity = ECk_DebugOverlay_Severity::Normal;
    UPROPERTY() TArray<FText> ExplicitHistory;
};

USTRUCT()
struct FCk_DebugOverlay_Section
{
    GENERATED_BODY()
    UPROPERTY() FGameplayTag ProviderTag;
    UPROPERTY() int32        SortPriority = 0;
    UPROPERTY() TArray<FCk_DebugOverlay_Row> Rows;
};

USTRUCT()
struct FCk_DebugOverlay_EntityModel
{
    GENERATED_BODY()
    FCk_Handle   Entity;
    UPROPERTY() FText Header;
    UPROPERTY() TArray<FCk_DebugOverlay_Section> Sections;
};
