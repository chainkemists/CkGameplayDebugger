#include "CkEntityDebugOverlay_Module.h"
#include "CkEntityDebugOverlay_Log.h"

#include "Picker/CkDebugOverlay_PickerCards.h"

#include "CkDebuggerCommon/Picker/CkDebug_PickerOverlayCards.h"
#include "CkDebuggerCommon/Settings/CkDebuggerUserSettingsMigration.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"

#define LOCTEXT_NAMESPACE "FCkEntityDebugOverlayModule"

// --------------------------------------------------------------------------------------------------------------------

void FCkEntityDebugOverlayModule::StartupModule()
{
	ck::debugger_settings::Migrate_EditorUserSettingsIfNeeded(GetMutableDefault<UCk_DebugOverlay_InputSettings>());

    // Fill CkDebuggerCommon's picker overlay-card slot so the shared viewport
    // picker (FCkDebug_ViewportPicker) shows this module's focus card + world
    // tags. Low-tier module owns the slot, this module fills it — same pattern
    // as ck::DebugNav.
    ck::DebugPickerCards::Register_Factory(
        []() -> TSharedPtr<ICkDebug_PickerOverlayCards>
        {
            return MakeShared<FCkDebugOverlay_PickerCards>();
        });
}

void FCkEntityDebugOverlayModule::ShutdownModule()
{
    ck::DebugPickerCards::Register_Factory({});
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkEntityDebugOverlayModule, CkEntityDebugOverlay)
