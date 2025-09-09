#include "CkInteractionResolver_DebugConfig.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    UCk_InteractionResolver_DebugWindowConfig::
    Reset()
    -> void
{
    Super::Reset();

    SortByName = false;
    ShowActiveIntents = true;
    ShowInactiveIntents = true;
    ShowBestTargets = true;
    ShowAvailableTargets = true;
    ShowTargetDetails = false;
    ShowParameters = true;

    ActiveIntentColor = _ActiveIntentColor;
    InactiveIntentColor = _InactiveIntentColor;
    BestTargetColor = _BestTargetColor;
    AvailableTargetColor = _AvailableTargetColor;
}

auto
    UCk_InteractionResolver_DebugWindowConfig::
    Get_IntentColor(bool InIsActive) const
    -> const FVector4f&
{
    return InIsActive ? ActiveIntentColor : InactiveIntentColor;
}

auto
    UCk_InteractionResolver_DebugWindowConfig::
    Get_TargetColor(bool InIsInBestTargets) const
    -> const FVector4f&
{
    return InIsInBestTargets ? BestTargetColor : AvailableTargetColor;
}

//--------------------------------------------------------------------------------------------------------------------------