#pragma once

#include "CogCommonConfig.h"

#include "CkCore/Enums/CkEnums.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkInteractionResolver_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

struct FCk_Handle_InteractionResolver;

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_InteractionResolver_DebugWindowConfig : public UCogCommonConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_InteractionResolver_DebugWindowConfig);

public:
    auto
    Reset() -> void override;

    auto
    Get_IntentColor(bool InIsActive) const -> const FVector4f&;

    auto
    Get_TargetColor(bool InIsInBestTargets) const -> const FVector4f&;

private:
    inline static FVector4f _ActiveIntentColor{0.0f, 1.0f, 0.5f, 1.0f};      // Green
    inline static FVector4f _InactiveIntentColor{0.6f, 0.6f, 0.6f, 1.0f};    // Gray
    inline static FVector4f _BestTargetColor{1.0f, 0.95f, 0.0f, 1.0f};       // Yellow
    inline static FVector4f _AvailableTargetColor{0.0f, 0.8f, 1.0f, 1.0f};   // Blue

public:
    UPROPERTY(Config)
    bool SortByName = false;

    UPROPERTY(Config)
    bool ShowActiveIntents = true;

    UPROPERTY(Config)
    bool ShowInactiveIntents = true;

    UPROPERTY(Config)
    bool ShowBestTargets = true;

    UPROPERTY(Config)
    bool ShowAvailableTargets = true;

    UPROPERTY(Config)
    bool ShowTargetDetails = false;

    UPROPERTY(Config)
    bool ShowParameters = true;

    UPROPERTY(Config)
    FVector4f ActiveIntentColor = _ActiveIntentColor;

    UPROPERTY(Config)
    FVector4f InactiveIntentColor = _InactiveIntentColor;

    UPROPERTY(Config)
    FVector4f BestTargetColor = _BestTargetColor;

    UPROPERTY(Config)
    FVector4f AvailableTargetColor = _AvailableTargetColor;
};

// --------------------------------------------------------------------------------------------------------------------