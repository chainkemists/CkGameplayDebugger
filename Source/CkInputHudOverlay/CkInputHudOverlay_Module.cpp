#include "CkInputHudOverlay_Module.h"

#include "CkInputHudOverlay_Log.h"
#include "CkInputHudOverlay/Settings/CkInputHud_UserSettings.h"

#include "CkDebuggerCommon/Settings/CkDebuggerUserSettingsMigration.h"

#define LOCTEXT_NAMESPACE "FCkInputHudOverlayModule"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInputHudOverlayModule::
    StartupModule()
    -> void
{
    auto* Settings = GetMutableDefault<UCk_InputHud_UserSettings>();
    ck::debugger_settings::Migrate_EditorUserSettingsIfNeeded(Settings);
    Settings->Migrate_VisualSettingsIfNeeded();
}

auto
    FCkInputHudOverlayModule::
    ShutdownModule()
    -> void
{
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkInputHudOverlayModule, CkInputHudOverlay)
