#include "CkProbes_DebugConfig.h"

#include "CkSpatialQuery/Public/CkSpatialQuery/Probe/CkProbe_Fragment_Data.h"
#include "CkSpatialQuery/Public/CkSpatialQuery/Probe/CkProbe_Utils.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    UCk_Probes_DebugWindowConfig::
    Reset()
    -> void
{
    Super::Reset();

    _SortByName = false;
    _ShowEnabled = true;
    _ShowDisabled = true;
    _ShowOverlapping = true;
    _ShowNotOverlapping = true;
    _ShowNotifyPolicy = true;
    _ShowSilentPolicy = true;

    EnabledColor = _EnabledColor;
    DisabledColor = _DisabledColor;
    OverlappingColor = _OverlappingColor;
    NotifyColor = _NotifyColor;
    SilentColor = _SilentColor;
}

auto
    UCk_Probes_DebugWindowConfig::
    Get_ProbeColor(
        const FCk_Handle_Probe& InProbe) const
    -> FVector4f
{
    // Priority: Disabled > Overlapping > Response Policy > Enabled
    if (UCk_Utils_Probe_UE::Get_IsEnabledDisabled(InProbe) == ECk_EnableDisable::Disable)
    {
        return DisabledColor;
    }

    if (UCk_Utils_Probe_UE::Get_IsOverlapping(InProbe))
    {
        return OverlappingColor;
    }

    if (UCk_Utils_Probe_UE::Get_ResponsePolicy(InProbe) == ECk_ProbeResponse_Policy::Notify)
    {
        return NotifyColor;
    }

    if (UCk_Utils_Probe_UE::Get_ResponsePolicy(InProbe) == ECk_ProbeResponse_Policy::Silent)
    {
        return SilentColor;
    }

    return EnabledColor;
}

//--------------------------------------------------------------------------------------------------------------------------