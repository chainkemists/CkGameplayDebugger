#include "CkInputHudOverlay/Settings/CkInputHud_Settings.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_hud_settings
{
    auto
        Get_Defaults()
        -> const UCk_InputHud_Settings&
    {
        // GetDefault is the CDO, which is what Config=Game writes into — never cached, so a settings edit is live.
        return *GetDefault<UCk_InputHud_Settings>();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_InputHud_Settings::
    Get_HistoryCap()
    -> int32
{
    return FMath::Clamp(ck_input_hud_settings::Get_Defaults().HistoryCap, 3, 20);
}

auto
    UCk_InputHud_Settings::
    Get_FadeLifetimeSeconds()
    -> float
{
    return FMath::Clamp(ck_input_hud_settings::Get_Defaults().FadeLifetimeSeconds, 3.0f, 30.0f);
}

auto
    UCk_InputHud_Settings::
    Get_TapHoldThresholdMs()
    -> float
{
    return FMath::Clamp(ck_input_hud_settings::Get_Defaults().TapHoldThresholdMs, 50.0f, 2000.0f);
}

auto
    UCk_InputHud_Settings::
    Get_ShowFrameNumbers()
    -> bool
{
    return ck_input_hud_settings::Get_Defaults().ShowFrameNumbers;
}

auto
    UCk_InputHud_Settings::
    Get_HoldBarMaxPx()
    -> float
{
    return FMath::Clamp(ck_input_hud_settings::Get_Defaults().HoldBarMaxPx, 8.0f, 64.0f);
}

// --------------------------------------------------------------------------------------------------------------------
