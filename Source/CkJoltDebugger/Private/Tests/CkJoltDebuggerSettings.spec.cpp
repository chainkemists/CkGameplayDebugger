#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkJoltDebugger/Settings/CkJoltDebuggerSettings.h"
#include "CkJoltDebugger/Window/SCkJoltDebuggerWindow.h"

namespace ck_jolt_debugger_settings_spec
{
    // The CDO is the developer's REAL preference store, and a failing prepass throws past any hand-written
    // restore — leaving test values on it that the next toggle would save into the actual ini.
    struct FScopedPreferences
    {
        FScopedPreferences()
            : _Settings(GetMutableDefault<UCkJoltDebuggerSettings>())
        {
            if (_Settings == nullptr)
            { return; }

            _RenderMode = _Settings->RenderMode;
            _CameraPreset = _Settings->CameraPreset;
            _ShowSensors = _Settings->ShowSensors;
        }

        ~FScopedPreferences()
        {
            if (_Settings == nullptr)
            { return; }

            _Settings->RenderMode = _RenderMode;
            _Settings->CameraPreset = _CameraPreset;
            _Settings->ShowSensors = _ShowSensors;
        }

        FScopedPreferences(const FScopedPreferences&) = delete;
        auto operator=(const FScopedPreferences&) -> FScopedPreferences& = delete;

        auto Get_Settings() const -> UCkJoltDebuggerSettings* { return _Settings; }

    private:
        UCkJoltDebuggerSettings* _Settings = nullptr;
        ECkJoltDebugger_RenderModePref _RenderMode = ECkJoltDebugger_RenderModePref::Solid;
        ECkJoltDebugger_CameraPref _CameraPreset = ECkJoltDebugger_CameraPref::Perspective;
        bool _ShowSensors = true;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerSettings_ConstructRestoresPreferences,
    "Ck.JoltDebugger.Settings.ConstructRestoresPreferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerSettings_ConstructRestoresPreferences::RunTest(const FString&) -> bool
{
    const auto Preferences = ck_jolt_debugger_settings_spec::FScopedPreferences{};
    auto* Settings = Preferences.Get_Settings();

    if (NOT TestNotNull(TEXT("the Jolt debugger settings object exists"), Settings))
    { return false; }

    TestEqual(TEXT("the settings are presented under the Editor container, not Project Settings"),
        Settings->GetContainerName(), FName{TEXT("Editor")});

    Settings->RenderMode = ECkJoltDebugger_RenderModePref::Wireframe;
    Settings->CameraPreset = ECkJoltDebugger_CameraPref::Top;
    Settings->ShowSensors = false;

    // Construct with NON-DEFAULT preferences, which is what drives the restore path — including the camera
    // preset apply, which frames against a target with no content yet. That the preferences visibly land on
    // the facility target is `[EDITOR-VERIFY]`: the target is the window's own, with no read surface here.
    const auto Window = SNew(SCkJoltDebuggerWindow);
    Window->SlatePrepass();

    TestTrue(TEXT("the window constructs ensure-free with non-default preferences"),
        Window->GetDesiredSize().Y > 0.0f);

    return true;
}

#endif
