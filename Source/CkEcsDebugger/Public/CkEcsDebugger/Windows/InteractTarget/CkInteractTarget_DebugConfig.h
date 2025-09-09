#pragma once

#include "CogCommonConfig.h"

#include "CkCore/Enums/CkEnums.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkInteractTarget_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

struct FCk_Handle_InteractTarget;

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_InteractTarget_DebugWindowConfig : public UCogCommonConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_InteractTarget_DebugWindowConfig);

public:
    auto
    Reset() -> void override;

    auto
    Get_TargetColor(bool InIsEnabled) const -> const FVector4f&;

    auto
    Get_InteractionColor(bool InIsActive) const -> const FVector4f&;

private:
    inline static FVector4f _EnabledTargetColor{0.0f, 1.0f, 0.5f, 1.0f};       // Green
    inline static FVector4f _DisabledTargetColor{0.6f, 0.6f, 0.6f, 1.0f};     // Gray
    inline static FVector4f _ActiveInteractionColor{1.0f, 0.95f, 0.0f, 1.0f}; // Yellow
    inline static FVector4f _InactiveInteractionColor{0.0f, 0.8f, 1.0f, 1.0f}; // Blue

public:
    UPROPERTY(Config)
    bool SortByName = false;

    UPROPERTY(Config)
    bool ShowEnabledTargets = true;

    UPROPERTY(Config)
    bool ShowDisabledTargets = true;

    UPROPERTY(Config)
    bool ShowActiveInteractions = true;

    UPROPERTY(Config)
    bool ShowInteractionDetails = false;

    UPROPERTY(Config)
    bool ShowParameters = true;

    UPROPERTY(Config)
    FVector4f EnabledTargetColor = _EnabledTargetColor;

    UPROPERTY(Config)
    FVector4f DisabledTargetColor = _DisabledTargetColor;

    UPROPERTY(Config)
    FVector4f ActiveInteractionColor = _ActiveInteractionColor;

    UPROPERTY(Config)
    FVector4f InactiveInteractionColor = _InactiveInteractionColor;
};

// --------------------------------------------------------------------------------------------------------------------