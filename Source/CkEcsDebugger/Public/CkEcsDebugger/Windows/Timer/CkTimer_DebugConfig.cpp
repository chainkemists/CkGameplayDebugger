#include "CkTimer_DebugConfig.h"

#include "CkTimer/CkTimer_Utils.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    UCk_Timer_DebugWindowConfig::
    Reset()
    -> void
{
    Super::Reset();

    SortByName = true;
    ShowOnlyRunningTimers = false;

    RunningColor = FVector4f{1.0f, 1.0f, 1.0f, 1.0f};
    PausedColor = FVector4f{0.5f, 0.5f, 0.5f, 1.0f};
}

auto
    UCk_Timer_DebugWindowConfig::
    Get_TimerColor(
        const FCk_Handle_Timer& InTimer) const
    -> const FVector4f&
{
    return UCk_Utils_Timer_UE::Get_CurrentState(InTimer) == ECk_Timer_State::Running ? RunningColor : PausedColor;
}

//--------------------------------------------------------------------------------------------------------------------------
