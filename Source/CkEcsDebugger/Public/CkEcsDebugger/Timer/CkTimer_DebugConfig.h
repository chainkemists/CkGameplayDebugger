#pragma once

#include "CkTimer/CkTimer_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"

#include "CogWindowConfig.h"

#include "CkTimer_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_Timer_DebugWindowConfig : public UCogWindowConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Timer_DebugWindowConfig);

public:
    auto Reset() -> void override;

public:
    UPROPERTY(Config)
    bool SortByName = true;

    UPROPERTY(Config)
    bool ShowOnlyRunningTimers = false;

    // TODO: GroupBySource (EntityExtensions)
};

//--------------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_Timer_DebugWindowConfigDisplay : public UCogWindowConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Timer_DebugWindowConfigDisplay);

public:
    auto
    Reset() -> void override;

    auto
    Get_TimerColor(
        const FCk_Handle_Timer& InTimer) const -> FVector4f;

public:
    UPROPERTY(Config)
    FVector4f RunningColor = FVector4f{1.0f, 1.0f, 1.0f, 1.0f};

    UPROPERTY(Config)
    FVector4f PausedColor = FVector4f{0.5f, 0.5f, 0.5f, 1.0f};
};

//--------------------------------------------------------------------------------------------------------------------------
