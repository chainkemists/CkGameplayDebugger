#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkJoltDebugger/Settings/CkJoltDebuggerSettings.h"
#include "CkJoltDebugger/Viewport/SCkJoltDebugger_3dViewport.h"
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
            _ShowProbeResults = _Settings->ShowProbeResults;
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
            _Settings->ShowProbeResults = _ShowProbeResults;
            _Settings->RunawayVelocityCmS = _RunawayVelocityCmS;
            _Settings->CameraBookmarks = _CameraBookmarks;

            // Restoring the CDO is not enough: every toggle this spec drives calls SaveConfig(), so the
            // developer's REAL ini already holds the test values by the time this runs — a bookmark in slot 3
            // among them. Saving the restored CDO is what puts the file back (P8-D74/F7).
            _Settings->SaveConfig();
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
        bool _ShowProbeResults = false;
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
    // preset apply, which frames against a target with no content yet.
    const auto Window = SNew(SCkJoltDebuggerWindow);
    Window->SlatePrepass();

    TestTrue(TEXT("the window constructs ensure-free with non-default preferences"),
        Window->GetDesiredSize().Y > 0.0f);

    /*
     * The restore has to LAND, not merely leave the ini alone (P7-D71/F10). Every assertion below this line
     * reads the window's own state and the facility target it configured; asserting only that the preference
     * survived would pass just as well against a restore pass that never ran at all.
     */
    TestEqual(TEXT("the restored draw flags reached the facility target"),
        static_cast<int32>(Window->Get_TargetDrawFlags()), NonDefaultDrawFlags);
    TestEqual(TEXT("the restored colour mode reached the facility target"),
        static_cast<int32>(Window->Get_TargetColorMode()),
        static_cast<int32>(ECk_Jolt_DebugDrawColorMode::ShapeType));
    TestTrue(TEXT("the restored isolate state reached the window"), Window->Get_IsolateActive());
    TestTrue(TEXT("the restored follow state reached the window"), Window->Get_FollowSelection());
    TestFalse(TEXT("the restored grid state reached the window"), Window->Get_ShowGrid());

    // A grid the user turned off pushes NOTHING into its retained channel — the toggle is the push, not a
    // per-frame visibility test the capture would have to make.
    TestEqual(TEXT("a grid restored OFF leaves its channel empty"), Window->Get_NumGridLines(), 0);

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

    // A second window, with the grid restored ON: the same restore path pushes the lattice into its channel
    // once, which is the whole of the grid's cost.
    Settings->ShowGrid = true;

    const auto GridWindow = SNew(SCkJoltDebuggerWindow);
    GridWindow->SlatePrepass();

    TestTrue(TEXT("the restored grid state reached the second window"), GridWindow->Get_ShowGrid());

    // 20 m of 1 m cells either side of the origin is 41 lines per axis, both axes: the shape is a constant,
    // and a grid that quietly grew or shrank would change what "1 m cell" means to the eye reading it.
    TestEqual(TEXT("a grid restored ON pushes its lattice once"), GridWindow->Get_NumGridLines(), 82);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

/*
 * Camera bookmarks end to end (P8-D59), through the REAL hotkeys: Ctrl+digit stores, a bare digit recalls, and
 * both live in the viewport client's InputKey. Driven through the widget's own input entry points, so what is
 * pinned is the binding as well as the storage.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerSettings_CameraBookmarksStoreAndRecall,
    "Ck.JoltDebugger.Settings.CameraBookmarksStoreAndRecall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerSettings_CameraBookmarksStoreAndRecall::RunTest(const FString&) -> bool
{
    const auto Preferences = ck_jolt_debugger_settings_spec::FScopedPreferences{};
    auto* Settings = Preferences.Get_Settings();

    if (NOT TestNotNull(TEXT("the Jolt debugger settings object exists"), Settings))
    { return false; }

    Settings->CameraBookmarks.Reset();

    const auto Viewport = SNew(SCkJoltDebugger_3dViewport);
    Viewport->SlatePrepass();

    const auto Pan = [&Viewport]()
    {
        Viewport->Input_Key(EKeys::MiddleMouseButton, IE_Pressed);
        Viewport->Input_MouseAxis(EKeys::MouseX, 60.0f);
        Viewport->Input_Key(EKeys::MiddleMouseButton, IE_Released);
    };

    Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::Perspective);
    Pan();

    const auto StoredLocation = Viewport->Get_ViewLocation();
    const auto StoredRotation = Viewport->Get_ViewRotation();

    Viewport->Input_Key(EKeys::LeftControl, IE_Pressed);
    Viewport->Input_Key(EKeys::Three, IE_Pressed);
    Viewport->Input_Key(EKeys::Three, IE_Released);
    Viewport->Input_Key(EKeys::LeftControl, IE_Released);

    if (NOT TestTrue(TEXT("Ctrl+3 stores a pose in slot 3"),
        Settings->CameraBookmarks.IsValidIndex(3) && Settings->CameraBookmarks[3].IsSet))
    { return false; }

    TestFalse(TEXT("a perspective pose is not stored as an orthographic one"),
        Settings->CameraBookmarks[3].IsOrthographic);

    // Somewhere else entirely: a different projection AND a different eye, so the recall has to put both back.
    Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::Top);
    Pan();

    Viewport->Input_Key(EKeys::Three, IE_Pressed);

    TestEqual(TEXT("a bare 3 recalls the stored projection"),
        static_cast<int32>(Viewport->Get_ProjectionMode()),
        static_cast<int32>(ECameraProjectionMode::Perspective));
    TestTrue(TEXT("and the stored eye"), Viewport->Get_ViewLocation().Equals(StoredLocation, 0.01));
    TestTrue(TEXT("and the stored rotation"), Viewport->Get_ViewRotation().Equals(StoredRotation, 0.01f));

    // A slot nobody ever stored is INERT: the slots are dense, so an untouched one holds a default pose that
    // would otherwise snap the camera to the origin.
    const auto LocationBeforeInert = Viewport->Get_ViewLocation();

    Viewport->Input_Key(EKeys::Seven, IE_Pressed);

    TestTrue(TEXT("an unstored slot recalls nothing"),
        Viewport->Get_ViewLocation().Equals(LocationBeforeInert));

    // Ctrl+Alt+3 is not a bookmark gesture: only Ctrl stores, so a modified combination stays available to
    // whatever else binds it.
    const auto BookmarkBefore = Settings->CameraBookmarks[3];

    Pan();

    Viewport->Input_Key(EKeys::LeftControl, IE_Pressed);
    Viewport->Input_Key(EKeys::LeftAlt, IE_Pressed);
    Viewport->Input_Key(EKeys::Three, IE_Pressed);
    Viewport->Input_Key(EKeys::Three, IE_Released);
    Viewport->Input_Key(EKeys::LeftAlt, IE_Released);
    Viewport->Input_Key(EKeys::LeftControl, IE_Released);

    TestTrue(TEXT("Ctrl+Alt+3 does not overwrite the bookmark"),
        Settings->CameraBookmarks[3].Location.Equals(BookmarkBefore.Location));

    return true;
}

#endif
