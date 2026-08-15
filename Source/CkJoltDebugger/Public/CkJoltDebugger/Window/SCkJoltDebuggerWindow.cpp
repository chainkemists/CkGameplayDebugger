#include "CkJoltDebugger/Window/SCkJoltDebuggerWindow.h"

#include "CkJoltDebugger/Settings/CkJoltDebuggerSettings.h"
#include "CkJoltDebugger/Viewport/SCkJoltDebugger_3dViewport.h"
#include "CkJoltDebugger/Window/SCkJoltDebugger_DetailPanel.h"
#include "CkJoltDebugger/Window/SCkJoltDebugger_OutlinerPanel.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkJolt/Subsystem/CkJolt_Subsystem.h"
#include "CkJolt/StaticWorld/CkJoltStaticWorld_Subsystem.h"
#include "CkJolt/Body/CkJoltBody_Fragment.h"
#include "CkJolt/Character/CkJoltCharacter_Fragment.h"
#include "CkJolt/StaticWorld/CkJoltStaticActor_Fragment.h"

#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

#include "HAL/IConsoleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#include "Framework/Docking/TabManager.h"

// --------------------------------------------------------------------------------------------------------------------
// Local style + helpers (module-unique namespace name — unity builds concatenate TUs).
// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_debugger
{
    // TextScale-aware counterparts of CkStyle::RegularFont / BoldFont / MonoFont. Bound through
    // .Font_Static below so a Style Lab flip resizes text that was built long before the flip.
    static auto Normal(int32 InSize) -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Regular", InSize); }
    static auto Bold(int32 InSize)   -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Bold", InSize); }
    static auto Mono(int32 InSize)   -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Mono", InSize); }

    static auto Font_Summary()  -> FSlateFontInfo { return Bold(CkStyle::FontSizeH3()); }
    static auto Font_RowLabel() -> FSlateFontInfo { return Normal(CkStyle::FontSizeSmall()); }
    static auto Font_RowValue() -> FSlateFontInfo { return Mono(CkStyle::FontSizeSmall()); }

    // RowDensity lands live on every stat row through its registered SBox padding attribute, so the
    // axis moves already-built rows without binding an unsupported SScrollPanel slot attribute.
    static auto Get_StatRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, 0.0f});
    }

    static auto MillisecondsText(float InMilliseconds) -> FText
    {
        auto Options = FNumberFormattingOptions{};
        Options.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);

        return FText::Format(INVTEXT("{0} ms"), FText::AsNumber(InMilliseconds, &Options));
    }

    static auto NetModeLabel(const UWorld* InWorld) -> FString
    {
        switch (InWorld->GetNetMode())
        {
            case NM_DedicatedServer: return TEXT("Dedicated Server");
            case NM_ListenServer:    return TEXT("Listen Server");
            case NM_Client:          return TEXT("Client");
            case NM_Standalone:      return TEXT("Standalone");
            default:                 return TEXT("Unknown");
        }
    }

    static auto GetDebugCVarBool(const TCHAR* InName) -> bool
    {
        const auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName);
        return CVar != nullptr && CVar->GetInt() != 0;
    }

    static auto SetDebugCVarBool(const TCHAR* InName, const bool InEnabled) -> void
    {
        if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName))
        {
            CVar->Set(InEnabled ? 1 : 0, ECVF_SetByConsole);
        }
    }

    // The world is only inspectable once it has begun play — GetSubsystem on a not-yet-begun world crashes.
    static auto Get_IsInspectable(UWorld* InWorld) -> bool
    {
        return ck::IsValid(InWorld) && InWorld->HasBegunPlay();
    }

    // One definition of what a population toggle IS: its chrome, the colour classes it drives, and the
    // preference it persists into. Both the toolbar that builds the toggles and the restore-at-construct pass
    // read this, so a toggle and its saved value can never describe different classes.
    struct FPopulationGroup
    {
        FName _IconId;
        FString _Label;
        FString _ToolTip;
        TArray<ECk_Jolt_DebugDraw_ColorClass> _ColorClasses;
        bool UCkJoltDebuggerSettings::* _Preference = nullptr;
    };

    static auto Get_PopulationGroups() -> TArray<FPopulationGroup>
    {
        return
        {
            FPopulationGroup{
                TEXT("Jolt"),
                TEXT("Jolt Bodies"),
                TEXT("Show rigid bodies composed through CkJoltBody — static, kinematic, and dynamic (awake or asleep)."),
                {
                    ECk_Jolt_DebugDraw_ColorClass::Static,
                    ECk_Jolt_DebugDraw_ColorClass::Kinematic,
                    ECk_Jolt_DebugDraw_ColorClass::Dynamic_Awake,
                    ECk_Jolt_DebugDraw_ColorClass::Dynamic_Sleeping
                },
                &UCkJoltDebuggerSettings::ShowJoltBodies},
            FPopulationGroup{
                TEXT("World"),
                TEXT("Baked Static World"),
                TEXT("Show the baked level geometry extracted into the Jolt static world."),
                {ECk_Jolt_DebugDraw_ColorClass::BakedStatic},
                &UCkJoltDebuggerSettings::ShowBakedStaticWorld},
            FPopulationGroup{
                TEXT("Probe"),
                TEXT("Sensors"),
                TEXT("Show sensor bodies — the trigger volumes behind CkSpatialQuery probes."),
                {ECk_Jolt_DebugDraw_ColorClass::Sensor},
                &UCkJoltDebuggerSettings::ShowSensors},
            FPopulationGroup{
                TEXT("Person"),
                TEXT("Characters"),
                TEXT("Show CkJoltCharacter capsules. Characters have no broadphase body — they are drawn from their own shape."),
                {ECk_Jolt_DebugDraw_ColorClass::Character},
                &UCkJoltDebuggerSettings::ShowCharacters}
        };
    }

    // One definition of a draw-flag toggle, and of which of D39's four questions it answers. Both the toolbar
    // builder and the restore pass read this table, so a toggle and the bit it persists cannot drift apart.
    struct FDrawFlagToggle
    {
        FName _IconId;
        FString _Label;
        FString _ToolTip;
        ECk_Jolt_DebugDrawFlags _Flag = ECk_Jolt_DebugDrawFlags::None;
    };

    struct FDrawFlagGroup
    {
        FString _Label;
        TArray<FDrawFlagToggle> _Toggles;
    };

    static auto Get_DrawFlagGroups() -> TArray<FDrawFlagGroup>
    {
        return
        {
            FDrawFlagGroup{
                TEXT("Bodies"),
                {
                    {TEXT("Cube"), TEXT("Shapes"),
                        TEXT("Draw each body's collision shape as instanced geometry. Turning it off leaves only the line extras."),
                        ECk_Jolt_DebugDrawFlags::Shape},
                    {TEXT("ArrowProjectile"), TEXT("Velocity"),
                        TEXT("Draw a linear-velocity arrow per active body."),
                        ECk_Jolt_DebugDrawFlags::Velocity},
                    {TEXT("Wheel"), TEXT("Angular Velocity"),
                        TEXT("Draw an angular-velocity arrow per active body."),
                        ECk_Jolt_DebugDrawFlags::AngularVelocity},
                    {TEXT("Transform"), TEXT("World Transform"),
                        TEXT("Draw each body's world-transform axes."),
                        ECk_Jolt_DebugDrawFlags::WorldTransform},
                    {TEXT("Target"), TEXT("Centre of Mass"),
                        TEXT("Draw each body's centre-of-mass transform axes."),
                        ECk_Jolt_DebugDrawFlags::CenterOfMassTransform},
                    {TEXT("Crate"), TEXT("Bounding Box"),
                        TEXT("Draw each body's world-space AABB."),
                        ECk_Jolt_DebugDrawFlags::BoundingBox},
                    {TEXT("Scale"), TEXT("Mass + Inertia"),
                        TEXT("Draw the inertia wire box. Its numeric mass needs the Labels toggle as well."),
                        ECk_Jolt_DebugDrawFlags::MassAndInertia}
                }},
            FDrawFlagGroup{
                TEXT("Constraints"),
                {
                    {TEXT("Anchor"), TEXT("Constraints"),
                        TEXT("Draw every constraint's anchors and axes."),
                        ECk_Jolt_DebugDrawFlags::Constraints},
                    {TEXT("Gate"), TEXT("Limits"),
                        TEXT("Draw each constraint's configured limits."),
                        ECk_Jolt_DebugDrawFlags::ConstraintLimits},
                    {TEXT("Compass"), TEXT("Reference Frames"),
                        TEXT("Draw each constraint's per-body reference frames."),
                        ECk_Jolt_DebugDrawFlags::ConstraintReferenceFrames}
                }},
            FDrawFlagGroup{
                TEXT("Contacts"),
                {
                    {TEXT("Crosshair"), TEXT("Contact Points"),
                        TEXT("Draw the solve's contact points. CONTACT FLAGS ARE PROCESS-WIDE: Jolt's contact draw switches are statics, so this arms contact emission for every world and every debugger at once."),
                        ECk_Jolt_DebugDrawFlags::ContactPoints},
                    {TEXT("Needle"), TEXT("Contact Normals"),
                        TEXT("Draw the solve's manifold normals. Process-wide, like every contact flag."),
                        ECk_Jolt_DebugDrawFlags::ContactNormals},
                    {TEXT("Shield"), TEXT("Supporting Faces"),
                        TEXT("Draw the supporting faces the solver resolved each contact against. Process-wide, like every contact flag."),
                        ECk_Jolt_DebugDrawFlags::SupportingFaces}
                }},
            FDrawFlagGroup{
                TEXT("Labels"),
                {
                    {TEXT("Note"), TEXT("Labels"),
                        TEXT("Collect the capture's text labels. Today the only label is the numeric mass beside the Mass + Inertia box, so both toggles are needed to see it."),
                        ECk_Jolt_DebugDrawFlags::Labels}
                }}
        };
    }

    static auto Get_ColorMode(
        ECkJoltDebugger_ColorModePref InPreference) -> ECk_Jolt_DebugDrawColorMode
    {
        switch (InPreference)
        {
            case ECkJoltDebugger_ColorModePref::SleepState:  return ECk_Jolt_DebugDrawColorMode::SleepState;
            case ECkJoltDebugger_ColorModePref::ObjectLayer: return ECk_Jolt_DebugDrawColorMode::ObjectLayer;
            case ECkJoltDebugger_ColorModePref::ShapeType:   return ECk_Jolt_DebugDrawColorMode::ShapeType;
            default:                                         return ECk_Jolt_DebugDrawColorMode::BodyClass;
        }
    }

    static auto Get_ColorModePref(
        ECk_Jolt_DebugDrawColorMode InColorMode) -> ECkJoltDebugger_ColorModePref
    {
        switch (InColorMode)
        {
            case ECk_Jolt_DebugDrawColorMode::SleepState:  return ECkJoltDebugger_ColorModePref::SleepState;
            case ECk_Jolt_DebugDrawColorMode::ObjectLayer: return ECkJoltDebugger_ColorModePref::ObjectLayer;
            case ECk_Jolt_DebugDrawColorMode::ShapeType:   return ECkJoltDebugger_ColorModePref::ShapeType;
            default:                                       return ECkJoltDebugger_ColorModePref::BodyClass;
        }
    }

    static auto Get_CameraPreset(
        ECkJoltDebugger_CameraPref InPreference) -> ECkJoltDebugger_CameraPreset
    {
        switch (InPreference)
        {
            case ECkJoltDebugger_CameraPref::Top:    return ECkJoltDebugger_CameraPreset::Top;
            case ECkJoltDebugger_CameraPref::Bottom: return ECkJoltDebugger_CameraPreset::Bottom;
            case ECkJoltDebugger_CameraPref::Left:   return ECkJoltDebugger_CameraPreset::Left;
            case ECkJoltDebugger_CameraPref::Right:  return ECkJoltDebugger_CameraPreset::Right;
            case ECkJoltDebugger_CameraPref::Front:  return ECkJoltDebugger_CameraPreset::Front;
            case ECkJoltDebugger_CameraPref::Back:   return ECkJoltDebugger_CameraPreset::Back;
            default:                                 return ECkJoltDebugger_CameraPreset::Perspective;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

const FName SCkJoltDebuggerWindow::WindowId = FName(TEXT("JoltDebugger"));
const FName SCkJoltDebuggerWindow::TabId = FName(TEXT("CkJoltDebugger"));

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    Is_JoltDebuggerEntity(
        const FCk_Handle& InCandidate)
    -> bool
{
    if (ck::Is_NOT_Valid(InCandidate))
    { return false; }

    return InCandidate.Has<ck::FFragment_JoltBody_Current>()
        || InCandidate.Has<ck::FFragment_JoltStaticActor_Current>()
        || InCandidate.Has<ck::FFragment_Probe_Current>()
        || InCandidate.Has<ck::FFragment_JoltCharacter_Current>();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();
    _Viewport = SNew(SCkJoltDebugger_3dViewport)
        .OnBodyPicked(FOnCkJoltDebugger_BodyPicked::CreateSP(this, &SCkJoltDebuggerWindow::HandleViewportBodyPicked))
        .OnTogglePause(FSimpleDelegate::CreateSP(this, &SCkJoltDebuggerWindow::HandleTogglePause))
        .OnStepOnce(FSimpleDelegate::CreateSP(this, &SCkJoltDebuggerWindow::HandleStepOnce));

    DoCreateDebugDrawTarget();

    _OutlinerPanel = SNew(SCkJoltDebugger_OutlinerPanel)
        .OnRowSelected(FOnCkJoltDebugger_RowSelected::CreateSP(this, &SCkJoltDebuggerWindow::HandleOutlinerRowSelected));

    _DetailPanel = SNew(SCkJoltDebugger_DetailPanel)
        .GetSelection(FOnCkJoltDebugger_GetSelection::CreateSP(this, &SCkJoltDebuggerWindow::Get_Selection));

    // The shared game-viewport picker, specialized to the four body-backing features. The pick routes
    // through this module's entity-target route, so a pick and an ECS "Open In" land on the same row.
    _ViewportPicker = MakeShared<FCkDebug_ViewportPicker>();
    {
        auto PickerParams = FCkDebug_ViewportPicker::FParams{};
        PickerParams.Get_TargetWorld =
            [WeakWorldModel = TWeakPtr<FCkDebuggerModel_WorldSelector>{_WorldModel}]() -> UWorld*
            {
                const auto Pinned = WeakWorldModel.Pin();
                return Pinned.IsValid() ? Pinned->Get_SelectedWorld() : nullptr;
            };
        PickerParams.TargetFilter =
            [](const FCk_Handle& InCandidate) { return Is_JoltDebuggerEntity(InCandidate); };
        PickerParams.OnEntityPicked =
            [](const FCk_Handle& InPicked)
            {
                ck::DebugSelectionSync::Broadcast(InPicked, TabId);
                FCkDebug_EntityTargetRegistry::Get().TryOpenAndTarget(TabId, InPicked);
            };
        _ViewportPicker->Construct(MoveTemp(PickerParams));
    }

    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddSP(this, &SCkJoltDebuggerWindow::HandleWorldChanged);
    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkJoltDebuggerWindow::HandleSessionInvalidated);
    _SelectionSyncHandle = ck::DebugSelectionSync::Get_OnSelection().AddSP(
        this, &SCkJoltDebuggerWindow::HandleGlobalSelectionSync);
    _TabForegroundedHandle = FGlobalTabmanager::Get()->OnTabForegrounded_Subscribe(
        FOnActiveTabChanged::FDelegate::CreateSP(this, &SCkJoltDebuggerWindow::HandleTabForegrounded));

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome).WindowId(Get_WindowId()).ToolTabId(TabId)
        .ShowRefreshControls(true)
        .CommandGroups(BuildCommandGroups())
        .Content()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(CkStyle::Bg1())
            [
                SNew(SSplitter).Orientation(Orient_Horizontal)

                + SSplitter::Slot().Value(0.22f)
                [ _OutlinerPanel.ToSharedRef() ]

                + SSplitter::Slot().Value(0.53f)
                [ _Viewport.ToSharedRef() ]

                + SSplitter::Slot().Value(0.25f)
                [ BuildRightRail() ]
            ]
        ]
    ];

    // After the tree, not before: the toggles read their state back off the target, so the restore has to be
    // the last word on it rather than something a freshly-built control overwrites.
    DoApplySavedPreferences();
}

// --------------------------------------------------------------------------------------------------------------------

SCkJoltDebuggerWindow::~SCkJoltDebuggerWindow()
{
    // Demand off BEFORE unregistering: the capture processor reads demand, and a target that is dropped while
    // still desired leaves its last instances standing in the preview world.
    if (_DebugDrawTarget.IsValid())
    { _DebugDrawTarget->Set_IsDesired(false); }

    DoUnregisterDebugDrawTarget();

    if (_WorldModel.IsValid() && _WorldChangedHandle.IsValid())
    { _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle); }

    if (_SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }

    if (_SelectionSyncHandle.IsValid())
    { ck::DebugSelectionSync::Get_OnSelection().Remove(_SelectionSyncHandle); }

    if (_TabForegroundedHandle.IsValid())
    { FGlobalTabmanager::Get()->OnTabForegrounded_Unsubscribe(_TabForegroundedHandle); }

    if (_ViewportPicker.IsValid())
    { _ViewportPicker->Deactivate(); }

    _Selection.Reset();
    _Collector.Reset();
    _DebugDrawTarget.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    // The picker drives the GAME viewport, not this one — it must keep ticking at the display rate even
    // when the user has throttled this window's refresh gate.
    if (_ViewportPicker.IsValid() && _ViewportPicker->IsActive())
    { _ViewportPicker->Tick(InDeltaTime); }

    _WorldModel->Ensure_AutoSelect();
    auto* World = _WorldModel->Get_SelectedWorld();

    DoSyncDebugDrawTarget(World);

    if (_Viewport.IsValid())
    {
        _Viewport->Set_SelectionBounds(_DebugDrawTarget.IsValid()
            ? _DebugDrawTarget->Get_HighlightedBodyBounds()
            : TOptional<FBox>{});
    }

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    DoRefreshStats(World);
    DoRefreshBodies(World);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    DoCreateDebugDrawTarget()
    -> void
{
    auto* PreviewWorld = _Viewport->Get_PreviewWorld();

    const auto PreviewWorldIsValid = ck::IsValid(PreviewWorld);
    CK_ENSURE_IF_NOT(PreviewWorldIsValid, TEXT("The Jolt debugger viewport produced no preview world; there is nothing to draw into"))
    { return; }

    _DebugDrawTarget = MakeShared<FCk_Jolt_DebugDrawTarget>(PreviewWorld);
    _Viewport->Set_Target(_DebugDrawTarget);
}

auto
    SCkJoltDebuggerWindow::
    DoSyncDebugDrawTarget(
        UWorld* InWorld)
    -> void
{
    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    const auto WorldIsInspectable = ck_jolt_debugger::Get_IsInspectable(InWorld);

    _DebugDrawTarget->Set_IsDesired(WorldIsInspectable && FCkDebuggerRefreshGate::Is_WindowVisible(WindowId));

    auto* DesiredSubsystem = WorldIsInspectable ? InWorld->GetSubsystem<UCk_Jolt_Subsystem>() : nullptr;
    auto* CurrentSubsystem = _RegisteredSubsystem.Get();

    if (DesiredSubsystem == CurrentSubsystem)
    { return; }

    DoUnregisterDebugDrawTarget();

    if (ck::Is_NOT_Valid(DesiredSubsystem))
    { return; }

    DesiredSubsystem->Register_DebugDrawTarget(_DebugDrawTarget.ToSharedRef());
    _RegisteredSubsystem = DesiredSubsystem;
}

auto
    SCkJoltDebuggerWindow::
    DoUnregisterDebugDrawTarget()
    -> void
{
    auto* CurrentSubsystem = _RegisteredSubsystem.Get();

    if (ck::IsValid(CurrentSubsystem) && _DebugDrawTarget.IsValid())
    { CurrentSubsystem->Unregister_DebugDrawTarget(_DebugDrawTarget.ToSharedRef()); }

    _RegisteredSubsystem.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    HandleWorldChanged(
        UWorld*)
    -> void
{
    DoUnregisterDebugDrawTarget();

    if (_DebugDrawTarget.IsValid())
    {
        _DebugDrawTarget->Set_IsDesired(false);
        _DebugDrawTarget->Set_HighlightedBody(TOptional<uint64>{});
    }

    if (_ViewportPicker.IsValid())
    { _ViewportPicker->Deactivate(); }

    if (_Viewport.IsValid())
    { _Viewport->Set_SelectionBounds(TOptional<FBox>{}); }

    if (_OutlinerPanel.IsValid())
    { _OutlinerPanel->Clear(); }

    // The snapshots and the selection are where this window's PIE handles live — they die here, in the
    // one reset both the world switch and the session invalidation route through.
    _Collector.Reset();
    _Selection.Reset();
    _PendingTarget.Reset();

    _Stats = FCkJoltDebugger_Stats{};
}

auto
    SCkJoltDebuggerWindow::
    HandleSessionInvalidated()
    -> void
{
    if (_WorldModel.IsValid() && _WorldModel->Get_SelectedWorld() != nullptr)
    {
        _WorldModel->Set_SelectedWorld(nullptr);
        return;
    }

    HandleWorldChanged(nullptr);
}

auto
    SCkJoltDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    // Everything else here is attribute-bound and has already moved; the outliner is the one surface with
    // generated ROW widgets, whose STableRow style is resolved at generation time.
    if (_OutlinerPanel.IsValid())
    { _OutlinerPanel->Rebuild_ForStyleChange(); }
}

auto
    SCkJoltDebuggerWindow::
    HandleTabForegrounded(
        TSharedPtr<SDockTab>,
        TSharedPtr<SDockTab>)
    -> void
{
    // The tab well updates its foreground index before broadcasting, so the refresh gate's visibility answer is
    // already the post-switch one for both directions.
    DoSyncDebugDrawTarget(_WorldModel.IsValid() ? _WorldModel->Get_SelectedWorld() : nullptr);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    DoRefreshStats(
        UWorld* InWorld)
    -> void
{
    auto Stats = FCkJoltDebugger_Stats{};

    if (NOT ck_jolt_debugger::Get_IsInspectable(InWorld))
    {
        _Stats = Stats;
        return;
    }

    Stats.HasWorld   = true;
    Stats.WorldLabel = ck::Format_UE(TEXT("{} ({})"), InWorld->GetName(), ck_jolt_debugger::NetModeLabel(InWorld));

    if (auto* JoltSubsystem = InWorld->GetSubsystem<UCk_Jolt_Subsystem>())
    {
        Stats.AsyncPhysics    = JoltSubsystem->Get_AsyncPhysicsUpdate();
        Stats.ParallelPhysics = JoltSubsystem->Get_ParallelPhysicsEnabled();
        Stats.ThreadCount     = JoltSubsystem->Get_PhysicsThreadCount();

        // The world's own three: the capture pushes two of them onto the target, but only while something is
        // capturing, and the pause state is not on the target at all.
        Stats.IsPaused             = JoltSubsystem->Get_IsDebugPaused();
        Stats.LastStepMs           = JoltSubsystem->Get_LastStepDurationMs();
        Stats.ContactPairsLastStep = JoltSubsystem->Get_ContactPairsLastStep();
    }

    if (_DebugDrawTarget.IsValid())
    {
        const auto& WorldStats = _DebugDrawTarget->Get_WorldStats();

        Stats.HasSampledStats = WorldStats.Get_HasSample();
        Stats.SampleAge       = WorldStats.Get_SampleAge();

        Stats.NumActiveRigidBodies = WorldStats.Get_NumActiveRigidBodies();
        Stats.NumActiveSoftBodies  = WorldStats.Get_NumActiveSoftBodies();

        Stats.SampledNumBodies             = WorldStats.Get_NumBodies();
        Stats.SampledMaxBodies             = WorldStats.Get_MaxBodies();
        Stats.SampledStaticBodies          = WorldStats.Get_NumStaticBodies();
        Stats.SampledDynamicBodies         = WorldStats.Get_NumDynamicBodies();
        Stats.SampledActiveDynamicBodies   = WorldStats.Get_NumActiveDynamicBodies();
        Stats.SampledKinematicBodies       = WorldStats.Get_NumKinematicBodies();
        Stats.SampledActiveKinematicBodies = WorldStats.Get_NumActiveKinematicBodies();
        Stats.SampledSoftBodies            = WorldStats.Get_NumSoftBodies();
        Stats.SampledConstraints           = WorldStats.Get_NumConstraints();
    }

    if (auto* StaticWorldSubsystem = InWorld->GetSubsystem<UCk_JoltStaticWorld_Subsystem_UE>())
    {
        Stats.NumStaticBodies = StaticWorldSubsystem->Get_NumStaticBodies();
        Stats.NumUniqueShapes = StaticWorldSubsystem->Get_NumUniqueShapes();
    }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);

    if (ck::IsValid(TransientEntity))
    {
        TransientEntity.View<ck::FFragment_JoltBody_Current>().ForEach(
            [&Stats, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_JoltBody_Current&)
            {
                ++Stats.NumBodies;

                auto Handle = ck::MakeHandle(InEntity, TransientEntity);

                if (Handle.Has<ck::FTag_JoltBody_Sleeping>()) { ++Stats.NumAsleep; }
                else                                          { ++Stats.NumAwake; }

                if (Handle.Has<ck::FTag_JoltBody_MotionType_Dynamic>())        { ++Stats.NumDynamic; }
                else if (Handle.Has<ck::FTag_JoltBody_MotionType_Kinematic>()) { ++Stats.NumKinematic; }
                else if (Handle.Has<ck::FTag_JoltBody_MotionType_Static>())    { ++Stats.NumStatic; }
            });

        TransientEntity.View<ck::FFragment_JoltCharacter_Current>().ForEach(
            [&Stats](FCk_Entity, const ck::FFragment_JoltCharacter_Current&)
            {
                ++Stats.NumCharacters;
            });

        TransientEntity.View<ck::FFragment_JoltStaticActor_Current>().ForEach(
            [&Stats](FCk_Entity, const ck::FFragment_JoltStaticActor_Current&)
            {
                ++Stats.NumStaticActors;
            });
    }

    _Stats = Stats;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    DoRefreshBodies(
        UWorld* InWorld)
    -> void
{
    _Collector.Collect(InWorld);

    if (_OutlinerPanel.IsValid())
    { _OutlinerPanel->Refresh(_Collector.Get_Bodies()); }

    DoApplyPendingTarget(InWorld);
    DoRefreshSelectionFacts();
}

auto
    SCkJoltDebuggerWindow::
    DoApplyPendingTarget(
        UWorld* InWorld)
    -> void
{
    if (NOT _PendingTarget.IsSet() || NOT _OutlinerPanel.IsValid())
    { return; }

    if (_PendingTarget->World.Get() != InWorld)
    {
        _PendingTarget.Reset();
        return;
    }

    const auto PendingEntity = _PendingTarget->Entity;
    const auto Guard = ck::DebugSelectionSync::FApplyGuard{};
    const auto Matched = _OutlinerPanel->SelectByEntity(PendingEntity);

    if (Matched.IsSet())
    {
        _PendingTarget.Reset();
        DoApplySelection(Matched, ECkJoltDebugger_SelectionSource::External);
        return;
    }

    // One retry, not a standing claim: once a pass HAS produced rows and none of them is the target, the
    // target is not in this world's collection, and a pending target that never expires would keep stealing
    // the next selection the user makes.
    if (NOT _Collector.Get_Bodies().IsEmpty())
    { _PendingTarget.Reset(); }
}

auto
    SCkJoltDebuggerWindow::
    DoRefreshSelectionFacts()
    -> void
{
    if (NOT _Selection.IsSet())
    { return; }

    // Keyed by the ENTITY, not the body key: a body key can be unset (a baked actor whose bodies are gone)
    // and a re-baked actor keeps its entity while every one of its body ids changes.
    const auto SelectedHandle = _Selection->Handle;
    const auto SelectedPopulation = _Selection->Population;

    const auto* Refreshed = _Collector.Get_Bodies().FindByPredicate(
        [&SelectedHandle, SelectedPopulation](const FCkJoltDebugger_BodySnapshot& InBody)
        { return InBody.Handle == SelectedHandle && InBody.Population == SelectedPopulation; });

    if (Refreshed == nullptr)
    {
        // The selected body left the world. Drop the selection rather than keeping a row that no longer
        // has anything behind it — the highlight would keep pointing at a released slot.
        DoApplySelection({}, ECkJoltDebugger_SelectionSource::External);
        return;
    }

    _Selection = *Refreshed;

    // Velocity comes from the facility's own capture, which sampled it in the physics pipeline's async-safe
    // window. Reading it off the live physics system from here would race the step.
    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    const auto Sample = _DebugDrawTarget->Get_BodySample();

    if (NOT Sample.IsSet())
    { return; }

    _Selection->LinearVelocity = Sample->Get_LinearVelocity();
    _Selection->HasLinearVelocity = true;
}

auto
    SCkJoltDebuggerWindow::
    DoApplySelection(
        TOptional<FCkJoltDebugger_BodySnapshot> InSnapshot,
        ECkJoltDebugger_SelectionSource InSource)
    -> void
{
    // Anything but an external apply supersedes a route target still waiting for a row: left standing, it
    // would land on the next refresh and take the selection away from whoever just made one.
    if (InSource != ECkJoltDebugger_SelectionSource::External)
    { _PendingTarget.Reset(); }

    _Selection = MoveTemp(InSnapshot);

    if (_DebugDrawTarget.IsValid())
    {
        _DebugDrawTarget->Set_HighlightedBody(_Selection.IsSet()
            ? _Selection->BodyKey
            : TOptional<uint64>{});
    }

    if (_Viewport.IsValid())
    {
        _Viewport->Set_SelectionBounds(_DebugDrawTarget.IsValid()
            ? _DebugDrawTarget->Get_HighlightedBodyBounds()
            : TOptional<FBox>{});
    }

    // The outliner is a sink for every source except itself — re-stamping the row the user just clicked
    // would fight the view's own selection state.
    if (_OutlinerPanel.IsValid() && InSource != ECkJoltDebugger_SelectionSource::Outliner)
    {
        if (_Selection.IsSet())
        { _OutlinerPanel->SelectByHandle(_Selection->Handle); }
        else
        { _OutlinerPanel->ClearSelection(); }
    }

    const auto IsUserOriginated = InSource == ECkJoltDebugger_SelectionSource::Outliner
        || InSource == ECkJoltDebugger_SelectionSource::Viewport;

    if (IsUserOriginated && _Selection.IsSet())
    { ck::DebugSelectionSync::Broadcast(_Selection->Handle, TabId); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    HandleOutlinerRowSelected(
        TOptional<FCkJoltDebugger_BodySnapshot> InSnapshot)
    -> void
{
    DoApplySelection(MoveTemp(InSnapshot), ECkJoltDebugger_SelectionSource::Outliner);
}

auto
    SCkJoltDebuggerWindow::
    HandleViewportBodyPicked(
        TOptional<uint64> InBodyKey)
    -> void
{
    if (NOT InBodyKey.IsSet())
    {
        DoApplySelection({}, ECkJoltDebugger_SelectionSource::Viewport);
        return;
    }

    const auto PickedKey = InBodyKey.GetValue();
    const auto* Picked = _Collector.Get_Bodies().FindByPredicate(
        [PickedKey](const FCkJoltDebugger_BodySnapshot& InBody)
        { return InBody.BodyKey.IsSet() && *InBody.BodyKey == PickedKey; });

    if (Picked == nullptr)
    {
        // A baked actor contributes many bodies and ONE row, whose key is only the first of them. Every other
        // baked body is just as pickable and resolves to its actor through the collector's owner index.
        const auto* Owner = _Collector.Get_BakedBodyOwners().Find(PickedKey);

        if (Owner == nullptr)
        { return; }

        const auto OwnerHandle = *Owner;
        Picked = _Collector.Get_Bodies().FindByPredicate(
            [&OwnerHandle](const FCkJoltDebugger_BodySnapshot& InBody) { return InBody.Handle == OwnerHandle; });
    }

    // A pickable instance the outliner has no row for is a body the collector cannot attribute to an entity.
    // Leave the selection alone rather than clearing it — the click did hit something.
    if (Picked == nullptr)
    { return; }

    DoApplySelection(TOptional<FCkJoltDebugger_BodySnapshot>{*Picked}, ECkJoltDebugger_SelectionSource::Viewport);
}

auto
    SCkJoltDebuggerWindow::
    HandleGlobalSelectionSync(
        const FCk_Handle& InSelected,
        FName InSource)
    -> void
{
    if (InSource == TabId)
    { return; }

    const auto Resolved = ck::DebugSelectionSync::Resolve_ClosestLineageMatch(
        InSelected,
        [](const FCk_Handle& InCandidate) { return Is_JoltDebuggerEntity(InCandidate); });

    if (ck::Is_NOT_Valid(Resolved) || NOT _OutlinerPanel.IsValid())
    { return; }

    const auto Guard = ck::DebugSelectionSync::FApplyGuard{};
    const auto Matched = _OutlinerPanel->SelectByHandle(Resolved);

    if (NOT Matched.IsSet())
    { return; }

    DoApplySelection(Matched, ECkJoltDebugger_SelectionSource::External);
}

auto
    SCkJoltDebuggerWindow::
    TargetEntity(
        const FCk_Handle& InEntity)
    -> void
{
    if (ck::Is_NOT_Valid(InEntity))
    { return; }

    if (_OutlinerPanel.IsValid())
    {
        const auto Guard = ck::DebugSelectionSync::FApplyGuard{};
        const auto Matched = _OutlinerPanel->SelectByHandle(InEntity);

        if (Matched.IsSet())
        {
            DoApplySelection(Matched, ECkJoltDebugger_SelectionSource::External);
            return;
        }
    }

    // The tab may have just opened, with no collector pass behind it yet — retry on the next refresh, against
    // the world this entity actually lives in.
    _PendingTarget = FCkJoltDebugger_PendingTarget{
        InEntity.Get_Entity(),
        UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InEntity)};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    BuildCommandGroups()
    -> TArray<FCkDebug_CommandGroup>
{
    return
    {
        // Primary lane = what acts on THIS viewport. The CVar toggles paint the game viewport instead, so they
        // sit in their own context group rather than reading as controls for the pane above them.
        FCkDebug_CommandGroup::Primary(
            TEXT("JoltRender"),
            FText::FromString(TEXT("Jolt viewport render mode")),
            BuildRenderGroup()),
        // Pause and Step act on the world the viewport is showing, so they belong beside it rather than in a
        // context lane — they are the two controls a physics debugger is opened to reach.
        FCkDebug_CommandGroup::Primary(
            TEXT("JoltSim"),
            FText::FromString(TEXT("Jolt simulation")),
            BuildSimGroup()),
        FCkDebug_CommandGroup::Context(
            TEXT("JoltTarget"),
            FText::FromString(TEXT("Jolt world selection")),
            BuildTargetGroup()),
        FCkDebug_CommandGroup::Context(
            TEXT("JoltCamera"),
            FText::FromString(TEXT("Jolt viewport camera")),
            BuildCameraGroup()),
        FCkDebug_CommandGroup::Context(
            TEXT("JoltDraw"),
            FText::FromString(TEXT("Jolt debug draw")),
            BuildDrawGroup()),
        FCkDebug_CommandGroup::Context(
            TEXT("JoltInWorldDraw"),
            FText::FromString(TEXT("In-world draw")),
            BuildInWorldDrawToggles()),
        FCkDebug_CommandGroup::Context(
            TEXT("JoltPopulations"),
            FText::FromString(TEXT("Jolt body populations")),
            BuildPopulationGroup()),
        FCkDebug_CommandGroup::Context(
            TEXT("JoltLegend"),
            FText::FromString(TEXT("Jolt colour legend")),
            BuildLegendGroup())
    };
}

auto
    SCkJoltDebuggerWindow::
    BuildInWorldDrawToggles() const
    -> TSharedRef<SWidget>
{
    // Everything below the master gate is inert while the gate is closed — the subsystem resets its own draw
    // flags when the in-world draw is switched off, so a toggle that looked live there would be a lie.
    const auto MakeGatedCVarToggle = [](
        FName InId,
        FName InIconId,
        const TCHAR* InLabel,
        const TCHAR* InToolTip,
        const TCHAR* InCVarName) -> FCkDebug_IconToggleAction
    {
        const auto CVarName = FString{InCVarName};

        return FCkDebug_IconToggleAction{
            InId,
            InIconId,
            FText::FromString(InLabel),
            FText::FromString(InToolTip),
            TAttribute<bool>::CreateLambda([CVarName]()
            {
                return ck_jolt_debugger::GetDebugCVarBool(*CVarName);
            }),
            FOnCkDebug_IconToggleChanged::CreateLambda([CVarName](const bool InIsEnabled)
            {
                ck_jolt_debugger::SetDebugCVarBool(*CVarName, InIsEnabled);
            }),
            TAttribute<bool>::CreateLambda([]()
            {
                return ck_jolt_debugger::GetDebugCVarBool(TEXT("ck.Jolt.DebugDraw.Enabled"));
            })};
    };

    return SNew(SCkDebug_IconToolbar)
        .Actions({
            FCkDebug_IconToggleAction{
                TEXT("JoltDebugDraw"),
                TEXT("Cube"),
                FText::FromString(TEXT("In-world Debug Draw")),
                FText::FromString(TEXT("Draw Jolt physics bodies in the GAME viewport, not this one. Affects the selected world's own debug draw (ck.Jolt.DebugDraw.Enabled).")),
                TAttribute<bool>::CreateLambda([]()
                {
                    return ck_jolt_debugger::GetDebugCVarBool(TEXT("ck.Jolt.DebugDraw.Enabled"));
                }),
                FOnCkDebug_IconToggleChanged::CreateLambda([](const bool InIsEnabled)
                {
                    ck_jolt_debugger::SetDebugCVarBool(TEXT("ck.Jolt.DebugDraw.Enabled"), InIsEnabled);
                })},
            MakeGatedCVarToggle(
                TEXT("JoltInWorldSleepColoring"),
                TEXT("Moon"),
                TEXT("In-world Sleep Colouring"),
                TEXT("Colour the GAME viewport's bodies by sleep state instead of by body class (ck.Jolt.DebugDraw.SleepColoring). This is the in-world draw's colour MODE, not this viewport's."),
                TEXT("ck.Jolt.DebugDraw.SleepColoring")),
            MakeGatedCVarToggle(
                TEXT("JoltInWorldVelocity"),
                TEXT("ArrowProjectile"),
                TEXT("In-world Velocity Vectors"),
                TEXT("Draw linear AND angular velocity arrows for active Jolt bodies in the GAME viewport, not this one (ck.Jolt.DebugDraw.Velocity)."),
                TEXT("ck.Jolt.DebugDraw.Velocity")),
            MakeGatedCVarToggle(
                TEXT("JoltInWorldWorldTransform"),
                TEXT("Transform"),
                TEXT("In-world World Transforms"),
                TEXT("Draw each body's world-transform axes in the GAME viewport, not this one (ck.Jolt.DebugDraw.WorldTransform)."),
                TEXT("ck.Jolt.DebugDraw.WorldTransform")),
            MakeGatedCVarToggle(
                TEXT("JoltInWorldConstraints"),
                TEXT("Anchor"),
                TEXT("In-world Constraints"),
                TEXT("Draw constraint anchors, axes and limits in the GAME viewport, not this one (ck.Jolt.DebugDraw.Constraints)."),
                TEXT("ck.Jolt.DebugDraw.Constraints")),
            MakeGatedCVarToggle(
                TEXT("JoltInWorldContacts"),
                TEXT("Crosshair"),
                TEXT("In-world Contacts"),
                TEXT("Draw contact points and manifold normals in the GAME viewport, not this one (ck.Jolt.DebugDraw.Contacts). PROCESS-WIDE: Jolt's contact draw switches are statics, so this arms contact emission for every world at once."),
                TEXT("ck.Jolt.DebugDraw.Contacts"))
        });
}

auto
    SCkJoltDebuggerWindow::
    BuildTargetGroup()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [ SNew(SCkDebug_WorldSelector, _WorldModel).ShowHeaderLabel(false) ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SCkDebug_ViewportPickerControls)
            .Picker(_ViewportPicker)
            .PickTooltip(FText::FromString(TEXT("Enter pick mode: click a physics body in the GAME viewport to select it here.\nOnly Jolt bodies, baked static actors, sensors and characters (and their owning entity) are shown and pickable.")))
        ];
}

auto
    SCkJoltDebuggerWindow::
    BuildCameraGroup()
    -> TSharedRef<SWidget>
{
    const auto MakeCameraButton = [this](
        FName InIconId,
        const TCHAR* InToolTip,
        ECkJoltDebugger_CameraPreset InPreset,
        TOptional<ECkJoltDebugger_CameraPref> InPreference) -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ToolTipText(FText::FromString(InToolTip))
            .ContentPadding(FMargin{4.0f, 1.0f})
            .OnClicked_Lambda([this, InPreset, InPreference]() -> FReply
            {
                if (_Viewport.IsValid())
                { _Viewport->ApplyPreset(InPreset); }

                // Only the ORIENTATION presets are a state to come back to; framing is an action against
                // whatever happens to be in the world at the time.
                if (InPreference.IsSet())
                {
                    auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
                    Settings->CameraPreset = *InPreference;
                    Settings->SaveConfig();
                }

                return FReply::Handled();
            })
            [
                SNew(SImage).Image(FCkDebuggerCommonStyle::Get_IconBrush(InIconId))
            ];
    };

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewPerspective"), TEXT("Perspective camera"), ECkJoltDebugger_CameraPreset::Perspective, ECkJoltDebugger_CameraPref::Perspective) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewTop"), TEXT("Top orthographic camera"), ECkJoltDebugger_CameraPreset::Top, ECkJoltDebugger_CameraPref::Top) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewBottom"), TEXT("Bottom orthographic camera"), ECkJoltDebugger_CameraPreset::Bottom, ECkJoltDebugger_CameraPref::Bottom) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewLeft"), TEXT("Left orthographic camera"), ECkJoltDebugger_CameraPreset::Left, ECkJoltDebugger_CameraPref::Left) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewRight"), TEXT("Right orthographic camera"), ECkJoltDebugger_CameraPreset::Right, ECkJoltDebugger_CameraPref::Right) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewFront"), TEXT("Front orthographic camera"), ECkJoltDebugger_CameraPreset::Front, ECkJoltDebugger_CameraPref::Front) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewBack"), TEXT("Back orthographic camera"), ECkJoltDebugger_CameraPreset::Back, ECkJoltDebugger_CameraPref::Back) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("FrameActor"), TEXT("Frame every drawn Jolt body (Home)"), ECkJoltDebugger_CameraPreset::FrameAll, {}) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ToolTipText(FText::FromString(TEXT("Frame the selected body (F)")))
            .ContentPadding(FMargin{4.0f, 1.0f})
            .IsEnabled_Lambda([this]() { return _Selection.IsSet(); })
            .OnClicked_Lambda([this]() -> FReply
            {
                if (_Viewport.IsValid())
                { _Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::FrameSelection); }

                return FReply::Handled();
            })
            [
                SNew(SImage).Image(FCkDebuggerCommonStyle::Get_IconBrush(TEXT("SelectInViewport")))
            ]
        ];
}

auto
    SCkJoltDebuggerWindow::
    BuildRenderGroup()
    -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_IconToggle)
        .IconId(TEXT("Grid"))
        .Label(FText::FromString(TEXT("Wireframe")))
        .ToolTip(FText::FromString(TEXT("Draw the Jolt bodies as wireframe instead of solid. Materials swap on the same instances — no geometry is rebuilt.")))
        .IsOn_Lambda([this]()
        {
            return _DebugDrawTarget.IsValid() &&
                _DebugDrawTarget->Get_RenderMode() == ECk_Jolt_DebugDraw_RenderMode::Wireframe;
        })
        .OnStateChanged_Lambda([this](const bool InIsWireframe)
        {
            if (NOT _DebugDrawTarget.IsValid())
            { return; }

            _DebugDrawTarget->Set_RenderMode(InIsWireframe
                ? ECk_Jolt_DebugDraw_RenderMode::Wireframe
                : ECk_Jolt_DebugDraw_RenderMode::Solid);

            auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
            Settings->RenderMode = InIsWireframe
                ? ECkJoltDebugger_RenderModePref::Wireframe
                : ECkJoltDebugger_RenderModePref::Solid;
            Settings->SaveConfig();
        });
}

auto
    SCkJoltDebuggerWindow::
    Get_SelectedJoltSubsystem() const
    -> UCk_Jolt_Subsystem*
{
    auto* World = _WorldModel.IsValid() ? _WorldModel->Get_SelectedWorld() : nullptr;

    return ck_jolt_debugger::Get_IsInspectable(World)
        ? World->GetSubsystem<UCk_Jolt_Subsystem>()
        : nullptr;
}

auto
    SCkJoltDebuggerWindow::
    Get_HasCommandableWorld() const
    -> bool
{
    return ck::IsValid(Get_SelectedJoltSubsystem());
}

auto
    SCkJoltDebuggerWindow::
    HandleTogglePause()
    -> void
{
    auto* Subsystem = Get_SelectedJoltSubsystem();

    if (ck::Is_NOT_Valid(Subsystem))
    { return; }

    Subsystem->Request_SetDebugPaused(NOT Subsystem->Get_IsDebugPaused());
}

auto
    SCkJoltDebuggerWindow::
    HandleStepOnce()
    -> void
{
    auto* Subsystem = Get_SelectedJoltSubsystem();

    if (ck::Is_NOT_Valid(Subsystem))
    { return; }

    Subsystem->Request_StepOnce();
}

auto
    SCkJoltDebuggerWindow::
    BuildSimGroup()
    -> TSharedRef<SWidget>
{
    const auto HasWorld = TAttribute<bool>::CreateSP(this, &SCkJoltDebuggerWindow::Get_HasCommandableWorld);

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SCkDebug_IconToggle)
            .IconId(TEXT("Hourglass"))
            .Label(FText::FromString(TEXT("Pause")))
            .ToolTip(FText::FromString(TEXT("Freeze the JOLT world of the selected game world — the engine keeps running, physics does not (Space). Needs a world that has begun play.")))
            .IsEnabled(HasWorld)
            .IsOn_Lambda([this]()
            {
                const auto* Subsystem = Get_SelectedJoltSubsystem();
                return ck::IsValid(Subsystem) && Subsystem->Get_IsDebugPaused();
            })
            .OnStateChanged_Lambda([this](const bool InIsPaused)
            {
                if (auto* Subsystem = Get_SelectedJoltSubsystem())
                { Subsystem->Request_SetDebugPaused(InIsPaused); }
            })
        ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ToolTipText(FText::FromString(TEXT("Advance the Jolt world by exactly one step, then re-pause (Enter). Ignored while the world is not paused.")))
            .ContentPadding(FMargin{4.0f, 1.0f})
            .IsEnabled(HasWorld)
            .OnClicked_Lambda([this]() -> FReply
            {
                HandleStepOnce();
                return FReply::Handled();
            })
            [
                SNew(SImage).Image(FCkDebuggerCommonStyle::Get_IconBrush(TEXT("Footprint")))
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    Get_ColorMode() const
    -> ECk_Jolt_DebugDrawColorMode
{
    return _DebugDrawTarget.IsValid()
        ? _DebugDrawTarget->Get_ColorMode()
        : ECk_Jolt_DebugDrawColorMode::BodyClass;
}

auto
    SCkJoltDebuggerWindow::
    Set_ColorMode(
        ECk_Jolt_DebugDrawColorMode InColorMode)
    -> void
{
    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    _DebugDrawTarget->Set_ColorMode(InColorMode);
    DoApplyPopulationVisibility();

    auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
    Settings->ColorMode = ck_jolt_debugger::Get_ColorModePref(InColorMode);
    Settings->SaveConfig();

    DoRebuildLegend();
}

auto
    SCkJoltDebuggerWindow::
    Set_DrawFlag(
        ECk_Jolt_DebugDrawFlags InFlag,
        bool InIsEnabled)
    -> void
{
    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    auto Flags = _DebugDrawTarget->Get_DrawFlags();

    if (InIsEnabled)
    { EnumAddFlags(Flags, InFlag); }
    else
    { EnumRemoveFlags(Flags, InFlag); }

    _DebugDrawTarget->Set_DrawFlags(Flags);

    auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
    Settings->DrawFlags = static_cast<int32>(Flags);
    Settings->SaveConfig();
}

auto
    SCkJoltDebuggerWindow::
    BuildDrawGroup()
    -> TSharedRef<SWidget>
{
    auto Lane = SNew(SHorizontalBox);

    for (const auto& Group : ck_jolt_debugger::Get_DrawFlagGroups())
    {
        auto Actions = TArray<FCkDebug_IconToggleAction>{};

        for (const auto& Toggle : Group._Toggles)
        {
            const auto Flag = Toggle._Flag;

            Actions.Emplace(FCkDebug_IconToggleAction{
                FName{ck::Format_UE(TEXT("JoltDraw.{}"), Toggle._Label)},
                Toggle._IconId,
                FText::FromString(Toggle._Label),
                FText::FromString(Toggle._ToolTip),
                TAttribute<bool>::CreateLambda([this, Flag]()
                {
                    return _DebugDrawTarget.IsValid() && _DebugDrawTarget->Get_IsDrawFlagSet(Flag);
                }),
                FOnCkDebug_IconToggleChanged::CreateLambda([this, Flag](const bool InIsEnabled)
                {
                    Set_DrawFlag(Flag, InIsEnabled);
                })});
        }

        Lane->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(STextBlock)
            .Font_Static(&ck_jolt_debugger::Font_RowLabel)
            .ColorAndOpacity(CkStyle::TextMute())
            .Text(FText::FromString(Group._Label))
        ];

        Lane->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [ SNew(SCkDebug_IconToolbar).Actions(Actions) ];
    }

    using FColorModeControl = SSegmentedControl<ECk_Jolt_DebugDrawColorMode>;

    const auto ModeControl =
        SNew(FColorModeControl)
        .Value_Lambda([this]() { return Get_ColorMode(); })
        .OnValueChanged_Lambda([this](ECk_Jolt_DebugDrawColorMode InColorMode) { Set_ColorMode(InColorMode); })
        + FColorModeControl::Slot(ECk_Jolt_DebugDrawColorMode::BodyClass)
            .Text(FText::FromString(TEXT("Class")))
            .ToolTip(FText::FromString(TEXT("Colour by body class — static, kinematic, awake, asleep, sensor, baked, character. The only mode the population toggles apply in.")))
        + FColorModeControl::Slot(ECk_Jolt_DebugDrawColorMode::SleepState)
            .Text(FText::FromString(TEXT("Sleep")))
            .ToolTip(FText::FromString(TEXT("Colour by sleep state. Statics and kinematics collapse to one colour each; the only distinction drawn is awake vs asleep.")))
        + FColorModeControl::Slot(ECk_Jolt_DebugDrawColorMode::ObjectLayer)
            .Text(FText::FromString(TEXT("Layer")))
            .ToolTip(FText::FromString(TEXT("Colour by Jolt object layer, named after the project's own collision channels where they are known.")))
        + FColorModeControl::Slot(ECk_Jolt_DebugDrawColorMode::ShapeType)
            .Text(FText::FromString(TEXT("Shape")))
            .ToolTip(FText::FromString(TEXT("Colour by shape sub-type — box, sphere, capsule, convex hull, mesh, height field and the rest.")));

    Lane->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
    [
        SNew(STextBlock)
        .Font_Static(&ck_jolt_debugger::Font_RowLabel)
        .ColorAndOpacity(CkStyle::TextMute())
        .Text(FText::FromString(TEXT("Colour")))
    ];

    Lane->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [ ModeControl ];

    return Lane;
}

auto
    SCkJoltDebuggerWindow::
    BuildPopulationGroup()
    -> TSharedRef<SWidget>
{
    auto Toggles = SNew(SHorizontalBox);

    for (const auto& Group : ck_jolt_debugger::Get_PopulationGroups())
    {
        Toggles->AddSlot().AutoWidth().VAlign(VAlign_Center)
        [ MakePopulationToggle(Group) ];
    }

    return Toggles;
}

auto
    SCkJoltDebuggerWindow::
    MakePopulationToggle(
        const ck_jolt_debugger::FPopulationGroup& InGroup) const
    -> TSharedRef<SWidget>
{
    const auto HasColorClasses = NOT InGroup._ColorClasses.IsEmpty();
    CK_ENSURE_IF_NOT(HasColorClasses,
        TEXT("Population toggle [{}] was given no colour classes to drive"), InGroup._Label)
    { return SNullWidget::NullWidget; }

    // The whole group tracks its first class: they are only ever flipped together from here.
    const auto RepresentativeClass = InGroup._ColorClasses[0];
    const auto ColorClasses = InGroup._ColorClasses;
    const auto Preference = InGroup._Preference;

    // The visibility mask is indexed by the CURRENT mode's class indices, and Set_ColorMode clears it outright
    // because index 5 is BakedStatic in one mode and Cylinder in another. So these toggles are meaningful in
    // BodyClass mode and nowhere else — disabled rather than silently hiding the wrong population.
    const auto ToolTip = ck::Format_UE(
        TEXT("{}\n\nAvailable only while colouring by Class: the visibility mask is indexed by the current colour mode's classes, and switching mode clears it."),
        InGroup._ToolTip);

    return SNew(SCkDebug_IconToggle)
        .IconId(InGroup._IconId)
        .Label(FText::FromString(InGroup._Label))
        .ToolTip(FText::FromString(ToolTip))
        .IsEnabled_Lambda([this]()
        {
            return Get_ColorMode() == ECk_Jolt_DebugDrawColorMode::BodyClass;
        })
        .IsOn_Lambda([this, RepresentativeClass]()
        {
            return _DebugDrawTarget.IsValid() && _DebugDrawTarget->Get_IsClassVisible(
                ck::jolt::debug_draw::Get_ClassIndex(RepresentativeClass));
        })
        .OnStateChanged_Lambda([this, ColorClasses, Preference](const bool InIsVisible)
        {
            if (NOT _DebugDrawTarget.IsValid())
            { return; }

            for (const auto& ColorClass : ColorClasses)
            {
                _DebugDrawTarget->Set_ClassVisibility(
                    ck::jolt::debug_draw::Get_ClassIndex(ColorClass), InIsVisible);
            }

            if (Preference == nullptr)
            { return; }

            auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
            Settings->*Preference = InIsVisible;
            Settings->SaveConfig();
        });
}

auto
    SCkJoltDebuggerWindow::
    DoApplySavedPreferences()
    -> void
{
    const auto* Settings = GetDefault<UCkJoltDebuggerSettings>();

    if (_DebugDrawTarget.IsValid())
    {
        _DebugDrawTarget->Set_RenderMode(Settings->RenderMode == ECkJoltDebugger_RenderModePref::Wireframe
            ? ECk_Jolt_DebugDraw_RenderMode::Wireframe
            : ECk_Jolt_DebugDraw_RenderMode::Solid);

        _DebugDrawTarget->Set_DrawFlags(static_cast<ECk_Jolt_DebugDrawFlags>(Settings->DrawFlags));

        // Before the population visibility, never after: Set_ColorMode clears the visibility mask, so a mode
        // restored second would wipe the very toggles this pass just put back.
        _DebugDrawTarget->Set_ColorMode(ck_jolt_debugger::Get_ColorMode(Settings->ColorMode));

        DoApplyPopulationVisibility();
    }

    DoRebuildLegend();

    if (_Viewport.IsValid())
    { _Viewport->ApplyPreset(ck_jolt_debugger::Get_CameraPreset(Settings->CameraPreset)); }
}

/*
 * The saved population toggles, pushed onto the target's BodyClass visibility mask. Runs on restore AND every
 * time the mode returns to BodyClass, because Set_ColorMode clears the mask — without the second call the
 * toggles would read "everything visible" while the preferences say otherwise.
 */
auto
    SCkJoltDebuggerWindow::
    DoApplyPopulationVisibility()
    -> void
{
    if (NOT _DebugDrawTarget.IsValid() ||
        _DebugDrawTarget->Get_ColorMode() != ECk_Jolt_DebugDrawColorMode::BodyClass)
    { return; }

    const auto* Settings = GetDefault<UCkJoltDebuggerSettings>();

    for (const auto& Group : ck_jolt_debugger::Get_PopulationGroups())
    {
        if (Group._Preference == nullptr)
        { continue; }

        const auto IsVisible = Settings->*Group._Preference;

        for (const auto& ColorClass : Group._ColorClasses)
        {
            _DebugDrawTarget->Set_ClassVisibility(
                ck::jolt::debug_draw::Get_ClassIndex(ColorClass), IsVisible);
        }
    }
}

auto
    SCkJoltDebuggerWindow::
    BuildLegendGroup()
    -> TSharedRef<SWidget>
{
    _LegendBox = SNew(SHorizontalBox);
    DoRebuildLegend();

    return _LegendBox.ToSharedRef();
}

/*
 * The legend is the ONE surface here that is not attribute-bound, because a colour mode does not just recolour
 * the classes — it changes how many there are and what they are called. Rebuilding is therefore correct, and it
 * happens from the mode control's handler, never from Tick.
 */
auto
    SCkJoltDebuggerWindow::
    DoRebuildLegend()
    -> void
{
    if (NOT _LegendBox.IsValid())
    { return; }

    _LegendBox->ClearChildren();

    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    const auto ColorMode = _DebugDrawTarget->Get_ColorMode();

    for (const auto& Entry : _DebugDrawTarget->Get_LegendEntries(ColorMode))
    {
        const auto ClassIndex = Entry.Get_ClassIndex();

        _LegendBox->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(10.0f).HeightOverride(10.0f)
                [
                    SNew(SImage)
                    .Image(FAppStyle::GetBrush("WhiteBrush"))
                    // The colour still binds: the palette can move under a fixed set of classes (Style Lab,
                    // Set_Palette) without the mode changing, and that must not need a rebuild.
                    .ColorAndOpacity_Lambda([this, ColorMode, ClassIndex]() -> FSlateColor
                    {
                        return _DebugDrawTarget.IsValid()
                            ? FSlateColor{_DebugDrawTarget->Get_Palette().Get_Color(ColorMode, ClassIndex)}
                            : CkStyle::TextMute();
                    })
                ]
            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Font_Static(&ck_jolt_debugger::Font_RowLabel)
                .ColorAndOpacity(CkStyle::TextDim())
                .Text(Entry.Get_Name())
            ]
        ];
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    BuildRightRail()
    -> TSharedRef<SWidget>
{
    return SNew(SSplitter).Orientation(Orient_Vertical)

        + SSplitter::Slot().Value(0.62f)
        [ BuildStatRail() ]

        + SSplitter::Slot().Value(0.38f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [ _DetailPanel.ToSharedRef() ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    BuildStatRail() const
    -> TSharedRef<SWidget>
{
    auto Summary =
        SNew(STextBlock)
        .Font_Static(&ck_jolt_debugger::Font_Summary)
        .ColorAndOpacity(CkStyle::Text())
        .Text_Lambda([this]() -> FText
        {
            return _Stats.HasWorld
                ? FText::FromString(ck::Format_UE(TEXT("World: {}"), _Stats.WorldLabel))
                : FText::FromString(TEXT("No active world. Start PIE to see Jolt stats."));
        });

    return SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [ Summary ]

                // Paused is the one piece of state that changes what every other number below MEANS, so it
                // rides the summary line rather than waiting to be found in a section.
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_StatusPill)
                    .Text_Lambda([this]() -> FText
                    {
                        return FText::FromString(_Stats.IsPaused ? TEXT("PAUSED") : TEXT("LIVE"));
                    })
                    .Tone_Lambda([this]() -> ECk_Tone
                    {
                        if (NOT _Stats.HasWorld) { return ECk_Tone::Neutral; }
                        return _Stats.IsPaused ? ECk_Tone::Warn : ECk_Tone::Ok;
                    })
                ]
            ]

        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f)
            [ ck::debug_axes::Make_AxisSeparator() ]

        + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SNew(SScrollBox)

                + SScrollBox::Slot().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                    [ MakeSectionHeader(TEXT("World")) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Async Physics Update:"), TAttribute<FText>::CreateLambda([this]()
                        { return FText::FromString(_Stats.HasWorld ? (_Stats.AsyncPhysics ? TEXT("Yes") : TEXT("No")) : TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Parallel Physics:"), TAttribute<FText>::CreateLambda([this]()
                        { return FText::FromString(_Stats.HasWorld ? (_Stats.ParallelPhysics ? TEXT("Yes") : TEXT("No")) : TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Physics Threads:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.ThreadCount) : FText::FromString(TEXT("--")); })) ]

                + SScrollBox::Slot().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                    [ MakeSectionHeader(TEXT("Simulation")) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Jolt Paused:"), TAttribute<FText>::CreateLambda([this]()
                        { return FText::FromString(_Stats.HasWorld ? (_Stats.IsPaused ? TEXT("Yes") : TEXT("No")) : TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Last Step:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld
                            ? ck_jolt_debugger::MillisecondsText(_Stats.LastStepMs)
                            : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Active Rigid Bodies:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumActiveRigidBodies) : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Active Soft Bodies:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumActiveSoftBodies) : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Contact Pairs (last step):"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.ContactPairsLastStep) : FText::FromString(TEXT("--")); })) ]

                // Everything below is refreshed every 30th capture, not every frame: GetBodyStats walks every
                // body and GetConstraints copies the constraint array. The label is the disclosure — a
                // throttled count is allowed to be LATE, never wrong.
                + SScrollBox::Slot()
                    [ MakeSampledStatRow(TEXT("Bodies:"), TAttribute<FText>::CreateLambda([this]()
                        { return Get_SampledStatText(_Stats.SampledNumBodies); })) ]
                + SScrollBox::Slot()
                    [ MakeSampledStatRow(TEXT("Body Budget:"), TAttribute<FText>::CreateLambda([this]()
                        { return Get_SampledStatText(_Stats.SampledMaxBodies); })) ]
                + SScrollBox::Slot()
                    [ MakeSampledStatRow(TEXT("Static Bodies:"), TAttribute<FText>::CreateLambda([this]()
                        { return Get_SampledStatText(_Stats.SampledStaticBodies); })) ]
                + SScrollBox::Slot()
                    [ MakeSampledStatRow(TEXT("Dynamic Bodies:"), TAttribute<FText>::CreateLambda([this]()
                        { return Get_SampledStatText(_Stats.SampledDynamicBodies); })) ]
                + SScrollBox::Slot()
                    [ MakeSampledStatRow(TEXT("Active Dynamic:"), TAttribute<FText>::CreateLambda([this]()
                        { return Get_SampledStatText(_Stats.SampledActiveDynamicBodies); })) ]
                + SScrollBox::Slot()
                    [ MakeSampledStatRow(TEXT("Kinematic Bodies:"), TAttribute<FText>::CreateLambda([this]()
                        { return Get_SampledStatText(_Stats.SampledKinematicBodies); })) ]
                + SScrollBox::Slot()
                    [ MakeSampledStatRow(TEXT("Active Kinematic:"), TAttribute<FText>::CreateLambda([this]()
                        { return Get_SampledStatText(_Stats.SampledActiveKinematicBodies); })) ]
                + SScrollBox::Slot()
                    [ MakeSampledStatRow(TEXT("Soft Bodies:"), TAttribute<FText>::CreateLambda([this]()
                        { return Get_SampledStatText(_Stats.SampledSoftBodies); })) ]
                + SScrollBox::Slot()
                    [ MakeSampledStatRow(TEXT("Constraints:"), TAttribute<FText>::CreateLambda([this]()
                        { return Get_SampledStatText(_Stats.SampledConstraints); })) ]

                + SScrollBox::Slot().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                    [ MakeSectionHeader(TEXT("Rigid Bodies")) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("JoltBody Entities:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumBodies) : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Dynamic:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumDynamic) : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Kinematic:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumKinematic) : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Static:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumStatic) : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Awake:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumAwake) : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Asleep:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumAsleep) : FText::FromString(TEXT("--")); })) ]

                + SScrollBox::Slot().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                    [ MakeSectionHeader(TEXT("Characters")) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("JoltCharacter Entities:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumCharacters) : FText::FromString(TEXT("--")); })) ]

                + SScrollBox::Slot().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                    [ MakeSectionHeader(TEXT("Static World")) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("JoltStaticActor Entities:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumStaticActors) : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Static Bodies:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumStaticBodies) : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Unique Shapes:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumUniqueShapes) : FText::FromString(TEXT("--")); })) ]

                + SScrollBox::Slot().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                    [ MakeSectionHeader(TEXT("Viewport")) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Drawn Instances:"), TAttribute<FText>::CreateLambda([this]()
                        { return _DebugDrawTarget.IsValid()
                            ? FText::AsNumber(_DebugDrawTarget->Get_NumInstances())
                            : FText::FromString(TEXT("--")); })) ]
                + SScrollBox::Slot()
                    [ MakeStatRow(TEXT("Buckets:"), TAttribute<FText>::CreateLambda([this]()
                        { return _DebugDrawTarget.IsValid()
                            ? FText::AsNumber(_DebugDrawTarget->Get_NumBuckets())
                            : FText::FromString(TEXT("--")); })) ]
            ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    Get_SampledStatText(
        int32 InValue) const
    -> FText
{
    if (NOT _Stats.HasWorld || NOT _Stats.HasSampledStats)
    { return FText::FromString(TEXT("--")); }

    return FText::AsNumber(InValue);
}

// The "(sampled)" suffix is part of the LABEL, not the value: it describes the row's cadence, which does not
// change, while the value it qualifies changes every 30th capture.
auto
    SCkJoltDebuggerWindow::
    MakeSampledStatRow(
        const FString&    InLabel,
        TAttribute<FText> InValue) const
    -> TSharedRef<SWidget>
{
    return MakeStatRow(ck::Format_UE(TEXT("{} (sampled)"), InLabel), MoveTemp(InValue));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    MakeSectionHeader(
        const FString& InText) const
    -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_SectionHeader)
        .Label(FText::FromString(InText));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    MakeStatRow(
        const FString&     InLabel,
        TAttribute<FText>  InValue) const
    -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().FillWidth(0.6f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font_Static(&ck_jolt_debugger::Font_RowLabel)
                .ColorAndOpacity(CkStyle::TextDim())
                .Text(FText::FromString(InLabel))
            ]

            + SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font_Static(&ck_jolt_debugger::Font_RowValue)
                .ColorAndOpacity(CkStyle::Text())
                .Text(InValue)
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------
