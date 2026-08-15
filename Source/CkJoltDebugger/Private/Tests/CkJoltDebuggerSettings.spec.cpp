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
            _DrawFlags = _Settings->DrawFlags;
            _ColorMode = _Settings->ColorMode;
            _IsolateActive = _Settings->IsolateActive;
            _FollowSelection = _Settings->FollowSelection;
            _ShowGrid = _Settings->ShowGrid;
            _RunawayVelocityCmS = _Settings->RunawayVelocityCmS;
            _CameraBookmarks = _Settings->CameraBookmarks;
        }

        ~FScopedPreferences()
        {
            if (_Settings == nullptr)
            { return; }

            _Settings->RenderMode = _RenderMode;
            _Settings->CameraPreset = _CameraPreset;
            _Settings->ShowSensors = _ShowSensors;
            _Settings->DrawFlags = _DrawFlags;
            _Settings->ColorMode = _ColorMode;
            _Settings->IsolateActive = _IsolateActive;
            _Settings->FollowSelection = _FollowSelection;
            _Settings->ShowGrid = _ShowGrid;
            _Settings->RunawayVelocityCmS = _RunawayVelocityCmS;
            _Settings->CameraBookmarks = _CameraBookmarks;
        }

        FScopedPreferences(const FScopedPreferences&) = delete;
        auto operator=(const FScopedPreferences&) -> FScopedPreferences& = delete;

        auto Get_Settings() const -> UCkJoltDebuggerSettings* { return _Settings; }

    private:
        UCkJoltDebuggerSettings* _Settings = nullptr;
        ECkJoltDebugger_RenderModePref _RenderMode = ECkJoltDebugger_RenderModePref::Solid;
        ECkJoltDebugger_CameraPref _CameraPreset = ECkJoltDebugger_CameraPref::Perspective;
        bool _ShowSensors = true;
        int32 _DrawFlags = 0;
        ECkJoltDebugger_ColorModePref _ColorMode = ECkJoltDebugger_ColorModePref::BodyClass;
        bool _IsolateActive = false;
        bool _FollowSelection = false;
        bool _ShowGrid = true;
        float _RunawayVelocityCmS = 5000.0f;
        TArray<FCkJoltDebugger_CameraBookmark> _CameraBookmarks;
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

    const auto NonDefaultDrawFlags = static_cast<int32>(
        ECk_Jolt_DebugDrawFlags::Shape | ECk_Jolt_DebugDrawFlags::BoundingBox | ECk_Jolt_DebugDrawFlags::Labels);

    auto Bookmark = FCkJoltDebugger_CameraBookmark{};
    Bookmark.Location = FVector{100.0, -200.0, 300.0};
    Bookmark.Rotation = FRotator{-15.0, 30.0, 0.0};
    Bookmark.OrthoWidth = 1234.0f;
    Bookmark.IsOrthographic = true;

    Settings->RenderMode = ECkJoltDebugger_RenderModePref::Wireframe;
    Settings->CameraPreset = ECkJoltDebugger_CameraPref::Top;
    Settings->ShowSensors = false;
    Settings->DrawFlags = NonDefaultDrawFlags;
    Settings->ColorMode = ECkJoltDebugger_ColorModePref::ShapeType;
    Settings->IsolateActive = true;
    Settings->FollowSelection = true;
    Settings->ShowGrid = false;
    Settings->RunawayVelocityCmS = 1234.0f;
    Settings->CameraBookmarks = {Bookmark};

    // Construct with NON-DEFAULT preferences, which is what drives the restore path — including the camera
    // preset apply, which frames against a target with no content yet. That the preferences visibly land on
    // the facility target is `[EDITOR-VERIFY]`: the target is the window's own, with no read surface here.
    const auto Window = SNew(SCkJoltDebuggerWindow);
    Window->SlatePrepass();

    TestTrue(TEXT("the window constructs ensure-free with non-default preferences"),
        Window->GetDesiredSize().Y > 0.0f);

    // Restoring must not WRITE: every preference the window read is still the value the test set, so a restore
    // that quietly re-saved a default over the developer's own choice would show up here rather than in an ini.
    TestEqual(TEXT("the draw flags survive the restore pass"), Settings->DrawFlags, NonDefaultDrawFlags);
    TestEqual(TEXT("the colour mode survives the restore pass"),
        static_cast<int32>(Settings->ColorMode), static_cast<int32>(ECkJoltDebugger_ColorModePref::ShapeType));
    TestTrue(TEXT("the isolate preference survives the restore pass"), Settings->IsolateActive);
    TestTrue(TEXT("the follow-selection preference survives the restore pass"), Settings->FollowSelection);
    TestFalse(TEXT("the grid preference survives the restore pass"), Settings->ShowGrid);
    TestEqual(TEXT("the runaway-velocity threshold survives the restore pass"),
        Settings->RunawayVelocityCmS, 1234.0f);

    if (TestEqual(TEXT("the camera bookmarks survive the restore pass"), Settings->CameraBookmarks.Num(), 1))
    {
        TestTrue(TEXT("the bookmark keeps its pose"),
            Settings->CameraBookmarks[0].Location.Equals(FVector{100.0, -200.0, 300.0}) &&
            Settings->CameraBookmarks[0].IsOrthographic);
    }

    return true;
}

#endif
