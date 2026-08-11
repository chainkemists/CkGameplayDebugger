#include "CkEntityDebugOverlay_Module.h"
#include "CkEntityDebugOverlay_Log.h"

#include "CkDebuggerCommon/Settings/CkDebuggerUserSettingsMigration.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"

#define LOCTEXT_NAMESPACE "FCkEntityDebugOverlayModule"

// --------------------------------------------------------------------------------------------------------------------

void FCkEntityDebugOverlayModule::StartupModule()
{
	ck::debugger_settings::Migrate_EditorUserSettingsIfNeeded(GetMutableDefault<UCk_DebugOverlay_InputSettings>());
}
void FCkEntityDebugOverlayModule::ShutdownModule() {}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkEntityDebugOverlayModule, CkEntityDebugOverlay)
