#pragma once

#include "CkObjective/Objective/CkObjective_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"

#include "CogCommonConfig.h"

#include "CkObjective_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_Objective_DebugWindowConfig : public UCogCommonConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Objective_DebugWindowConfig);

public:
    auto
    Reset() -> void override;

    auto
    Get_ObjectiveColor(
        const FCk_Handle_Objective& InObjective) const -> FVector4f;

private:
    inline static FVector4f _NotStartedColor{0.8f, 0.8f, 0.8f, 1.0f};
    inline static FVector4f _ActiveColor{0.0f, 1.0f, 0.5f, 1.0f};
    inline static FVector4f _CompletedColor{0.0f, 0.8f, 1.0f, 1.0f};
    inline static FVector4f _FailedColor{1.0f, 0.5f, 0.5f, 1.0f};

public:
    UPROPERTY(Config)
    bool SortByName = false;

    UPROPERTY(Config)
    bool ShowNotStarted = true;

    UPROPERTY(Config)
    bool ShowActive = true;

    UPROPERTY(Config)
    bool ShowCompleted = true;

    UPROPERTY(Config)
    bool ShowFailed = true;

    UPROPERTY(Config)
    bool ShowSubObjectives = true;

    UPROPERTY(Config)
    FVector4f NotStartedColor = _NotStartedColor;

    UPROPERTY(Config)
    FVector4f ActiveColor = _ActiveColor;

    UPROPERTY(Config)
    FVector4f CompletedColor = _CompletedColor;

    UPROPERTY(Config)
    FVector4f FailedColor = _FailedColor;
};

//--------------------------------------------------------------------------------------------------------------------------