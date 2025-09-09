#include "CkInteractTarget_DebugConfig.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    UCk_InteractTarget_DebugWindowConfig::
    Reset()
    -> void
{
    Super::Reset();

    SortByName = false;
    ShowEnabledTargets = true;
    ShowDisabledTargets = true;
    ShowActiveInteractions = true;
    ShowInteractionDetails = false;
    ShowParameters = true;

    EnabledTargetColor = _EnabledTargetColor;
    DisabledTargetColor = _DisabledTargetColor;
    ActiveInteractionColor = _ActiveInteractionColor;
    InactiveInteractionColor = _InactiveInteractionColor;
}

auto
    UCk_InteractTarget_DebugWindowConfig::
    Get_TargetColor(bool InIsEnabled) const
    -> const FVector4f&
{
    return InIsEnabled ? EnabledTargetColor : DisabledTargetColor;
}

auto
    UCk_InteractTarget_DebugWindowConfig::
    Get_InteractionColor(bool InIsActive) const
    -> const FVector4f&
{
    return InIsActive ? ActiveInteractionColor : InactiveInteractionColor;
}

//--------------------------------------------------------------------------------------------------------------------------