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
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
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
#include "CkJolt/Constraint/CkJoltConstraint_Fragment.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkSpatialQuery/Probe/CkProbe_Utils.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

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

    /*
     * The retained External sub-channel the drag line lives in (P5-D61/S3). Named, because the channel is
     * OWNED by this contributor: the capture re-emits it every pass without clearing it, and Clear_External
     * on this one name is the only thing that ever empties it.
     */
    static auto Get_DragChannel() -> FName
    { return FName{TEXT("JoltDebugger.Drag")}; }

    /// The retained External sub-channel the selected probe's overlaps live in (P8-D56 + P5-D61/S3).
    static auto Get_ProbeResultsChannel() -> FName
    { return FName{TEXT("JoltDebugger.ProbeResults")}; }

    /*
     * The retained External sub-channel the ground grid lives in (P8-D59). Pushed ONCE — the capture re-emits
     * a retained channel every pass, so a grid that never moves costs one push for the life of the window.
     */
    static auto Get_GridChannel() -> FName
    { return FName{TEXT("JoltDebugger.Grid")}; }

    // The grid's shape, in centimetres: metre cells over a 20 m half-extent, with a heavier line every ten so
    // the eye can count distance without measuring it.
    static constexpr float GridCellSize   = 100.0f;
    static constexpr float GridExtent     = 2000.0f;
    static constexpr int32 GridMajorEvery = 10;

    // Dimming factor for a minor grid line. Multiplied into the colour rather than expressed as alpha: the
    // debug lines go through Jolt's own colour path, which is not a translucency budget this window owns.
    static constexpr float GridMinorDim = 0.45f;

    // How far a contact normal is drawn, and how big a contact point's marker is. Both in centimetres, both
    // sized to read beside a human-scale body rather than to be geometrically meaningful.
    static constexpr float ProbeContactPointRadius = 6.0f;
    static constexpr float ProbeContactNormalLength = 40.0f;
    static constexpr float DirectionGlyphScaleMin = 0.25f;
    static constexpr float DirectionGlyphScaleMax = 4.0f;

    /*
     * Where a cursor ray meets a plane. Unset when the ray is parallel to the plane, and when the hit is
     * BEHIND the eye — a drag that snapped to a point behind the camera would throw the body across the map.
     */
    static auto TryIntersect_Plane(
        const FVector& InRayOrigin,
        const FVector& InRayDirection,
        const FVector& InPlanePoint,
        const FVector& InPlaneNormal) -> TOptional<FVector>
    {
        const auto Denominator = FVector::DotProduct(InRayDirection, InPlaneNormal);

        if (FMath::IsNearlyZero(Denominator, UE_KINDA_SMALL_NUMBER))
        { return {}; }

        const auto Distance = FVector::DotProduct(InPlanePoint - InRayOrigin, InPlaneNormal) / Denominator;

        if (Distance <= 0.0)
        { return {}; }

        return InRayOrigin + InRayDirection * Distance;
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

    static auto Get_RenderMode(
        ECkJoltDebugger_RenderModePref InPreference) -> ECk_Jolt_DebugDraw_RenderMode
    {
        switch (InPreference)
        {
            case ECkJoltDebugger_RenderModePref::SensorWireframe:
                return ECk_Jolt_DebugDraw_RenderMode::SensorWireframe;
            case ECkJoltDebugger_RenderModePref::Wireframe:
                return ECk_Jolt_DebugDraw_RenderMode::Wireframe;
            default:
                return ECk_Jolt_DebugDraw_RenderMode::Solid;
        }
    }

    static auto Get_RenderModePref(
        ECk_Jolt_DebugDraw_RenderMode InRenderMode) -> ECkJoltDebugger_RenderModePref
    {
        switch (InRenderMode)
        {
            case ECk_Jolt_DebugDraw_RenderMode::SensorWireframe:
                return ECkJoltDebugger_RenderModePref::SensorWireframe;
            case ECk_Jolt_DebugDraw_RenderMode::Wireframe:
                return ECkJoltDebugger_RenderModePref::Wireframe;
            default:
                return ECkJoltDebugger_RenderModePref::Solid;
        }
    }

    static auto Get_NextRenderMode(
        ECk_Jolt_DebugDraw_RenderMode InRenderMode) -> ECk_Jolt_DebugDraw_RenderMode
    {
        switch (InRenderMode)
        {
            case ECk_Jolt_DebugDraw_RenderMode::Solid:
                return ECk_Jolt_DebugDraw_RenderMode::SensorWireframe;
            case ECk_Jolt_DebugDraw_RenderMode::SensorWireframe:
                return ECk_Jolt_DebugDraw_RenderMode::Wireframe;
            default:
                return ECk_Jolt_DebugDraw_RenderMode::Solid;
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
        || InCandidate.Has<ck::FFragment_JoltCharacter_Current>()
        // The fifth clause (P8-D55). A constraint entity draws nothing of its own, but it IS a Jolt entity this
        // window lists and can select — so an ECS "Open In" and a viewport pick both have to reach it.
        || InCandidate.Has<ck::FFragment_JoltConstraint_Current>();
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
        .OnStepOnce(FSimpleDelegate::CreateSP(this, &SCkJoltDebuggerWindow::HandleStepOnce))
        .OnToggleIsolate(FSimpleDelegate::CreateSP(this, &SCkJoltDebuggerWindow::HandleToggleIsolate))
        .OnDragArm(FOnCkJoltDebugger_DragArm::CreateSP(this, &SCkJoltDebuggerWindow::HandleDragArm))
        .OnDragRay(FOnCkJoltDebugger_DragRay::CreateSP(this, &SCkJoltDebuggerWindow::HandleDragRay))
        .OnDragPlaneShift(FOnCkJoltDebugger_DragPlaneShift::CreateSP(this, &SCkJoltDebuggerWindow::HandleDragPlaneShift))
        .OnDragRelease(FSimpleDelegate::CreateSP(this, &SCkJoltDebuggerWindow::HandleDragRelease))
        .OnBodyHovered(FOnCkJoltDebugger_BodyHovered::CreateSP(this, &SCkJoltDebuggerWindow::HandleViewportBodyHovered));

    DoCreateDebugDrawTarget();

    _OutlinerPanel = SNew(SCkJoltDebugger_OutlinerPanel)
        .OnRowSelected(FOnCkJoltDebugger_RowSelected::CreateSP(this, &SCkJoltDebuggerWindow::HandleOutlinerRowSelected));

    _DetailPanel = SNew(SCkJoltDebugger_DetailPanel)
        .GetSelection(FOnCkJoltDebugger_GetSelection::CreateSP(this, &SCkJoltDebuggerWindow::Get_Selection))
        .GetSelectionFacts(FOnCkJoltDebugger_GetSelectionFacts::CreateSP(this, &SCkJoltDebuggerWindow::Get_SelectionFacts))
        .OnContactSelected(FOnCkJoltDebugger_ContactSelected::CreateSP(this, &SCkJoltDebuggerWindow::HandleContactSelected));

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

    if (const auto Adapter = _Viewport->Get_CommonAdapter(); Adapter.IsValid())
    {
        Adapter->Set_ShowGrid(_ShowGrid);
        Adapter->Set_OnRenderModeChanged([this](ECk_Jolt_DebugDraw_RenderMode InMode)
        { Set_RenderMode(InMode); });
        Adapter->Set_OnGridChanged([this](bool InIsEnabled)
        { Set_ShowGrid(InIsEnabled); });
        Adapter->Set_OnLabelsChanged([this](bool InIsEnabled)
        { Set_DrawFlag(ECk_Jolt_DebugDrawFlags::Labels, InIsEnabled); });
        Adapter->Set_OnDirectionGlyphScaleChanged([this](float InScale)
        { Set_DirectionGlyphScale(InScale); });
        Adapter->Set_OnIsolatedKeysChanged([this](const TArray<uint64>& InKeys)
        {
            const auto IsActive = NOT InKeys.IsEmpty();
            if (_IsolateActive == IsActive) { return; }
            _IsolateActive = IsActive;
            auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
            Settings->IsolateActive = IsActive;
            Settings->SaveConfig();
        });
    }
    _Viewport->Set_IsolateSelection(_IsolateActive);
}

// --------------------------------------------------------------------------------------------------------------------

SCkJoltDebuggerWindow::~SCkJoltDebuggerWindow()
{
    // FIRST, while the world selector still points at the world whose subsystem holds the spring: a debugger
    // that closes mid-drag must not leave a body attached to a constraint nothing will ever release
    // (P7-D71/F3). Idempotent, so it costs nothing when no drag was live.
    HandleDragRelease();

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
    _SelectionAll.Reset();
    _SelectionFacts.Reset();
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

        // The authority answer can change under this window (the world selector moves between a PIE server
        // and its client), so it is pushed every tick rather than read once at construct.
        _Viewport->Set_DragEnabled(Get_IsAuthorityWorld());
    }

    // Ungated, like the selection bounds above it: a drag line that only moved at the refresh cadence would
    // lag the body it is attached to, and the user is holding the mouse down while they watch it.
    DoUpdateDragLine();

    // Also ungated: the label rides the selection bounds, and a label lagging the body it names reads worse
    // than no label at all.
    DoUpdateViewportLabels();

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
    // Before anything else (P7-D71/F3 + F2): end whatever drag was live and take its line down. Idempotent,
    // and it is the ONE place the drag's local state is dropped — a world change that reset the flags by hand
    // was how the retained "JoltDebugger.Drag" channel came to outlive the world it described.
    HandleDragRelease();

    DoUnregisterDebugDrawTarget();

    if (_DebugDrawTarget.IsValid())
    {
        _DebugDrawTarget->Set_IsDesired(false);
        _DebugDrawTarget->Set_HighlightedBody(TOptional<uint64>{});
    }

    if (_ViewportPicker.IsValid())
    { _ViewportPicker->Deactivate(); }

    if (_Viewport.IsValid())
    {
        _Viewport->Set_SelectionBounds(TOptional<FBox>{});
        _Viewport->Set_SelectionKeys({});
    }

    if (_OutlinerPanel.IsValid())
    { _OutlinerPanel->Clear(); }

    // The snapshots and the selection are where this window's PIE handles live — they die here, in the
    // one reset both the world switch and the session invalidation route through.
    _Collector.Reset();
    _Selection.Reset();
    _SelectionAll.Reset();
    _SelectionFacts.Reset();
    _PendingTarget.Reset();

    if (_DetailPanel.IsValid())
    { _DetailPanel->Refresh_Contacts(); }

    // The isolation set named bodies in the world that just went away (P7-D71/F1). Left standing, the facility
    // keeps drawing ONLY those keys — and every one of them is dead, so the viewport goes blank under a lit
    // Isolate toggle, which reads as a broken draw rather than as a stale set. The selection is already empty
    // here, so this clears rather than re-pushes.
    DoApplyIsolation();

    // The reference-frames flag is DERIVED from "the user's own preference OR a constraint is selected"
    // (P5-D61/S8). The selection is already empty here, so re-deriving restores exactly the preference the
    // user chose — left standing, a world change made while a constraint was selected would leave the whole
    // next world's frames drawn by a flag nobody set (P8-D74/F1).
    DoApplyConstraintReferenceFrames();

    // The probe channel described a probe in the world that just went away. Cleared here rather than left for
    // the next refresh, which cannot happen at all until a world is selected again.
    if (_DebugDrawTarget.IsValid() && _ProbeResultsSignature.IsSet())
    { _DebugDrawTarget->Clear_External(ck_jolt_debugger::Get_ProbeResultsChannel()); }

    _ProbeResultsSignature.Reset();

    if (_DebugDrawTarget.IsValid())
    { _DebugDrawTarget->Set_HoveredBody(TOptional<uint64>{}); }

    if (_Viewport.IsValid())
    {
        _Viewport->Set_HoverLabel(FText::GetEmpty());
        _Viewport->Set_PrimaryLabel({});
    }

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

    // Between the ECS pass and the outliner: the rows are the registry's and the flags are the capture's, and
    // the panel has to receive one reconciled snapshot rather than two halves arriving a frame apart.
    DoApplyProblemThresholds(InWorld);
    _Collector.Apply_ProblemFlags(_DebugDrawTarget.IsValid()
        ? _DebugDrawTarget->Get_ProblemBodies()
        : TMap<uint64, ECk_Jolt_DebugDraw_ProblemFlags>{});

    if (_OutlinerPanel.IsValid())
    {
        _OutlinerPanel->Refresh(_Collector.Get_Bodies());

        // The panel prunes selected rows whose entities left the world and promotes a survivor to primary. The
        // window's own set has to follow it here, before any sink reads it (P7-D71/F8).
        DoSyncSelectionFromOutliner();
    }

    DoApplyPendingTarget(InWorld);
    DoRefreshSelectionFacts();
    DoUpdateProbeResults();
}

auto
    SCkJoltDebuggerWindow::
    DoApplyProblemThresholds(
        UWorld* InWorld)
    -> void
{
    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    if (NOT ck_jolt_debugger::Get_IsInspectable(InWorld))
    {
        _DebugDrawTarget->Set_ProblemThresholds({});
        return;
    }

    const auto* Settings = GetDefault<UCkJoltDebuggerSettings>();

    // KillZ is the world's, not the facility's: a preview world has no kill plane at all, and the number the
    // user cares about is the one the world being INSPECTED enforces.
    const auto* WorldSettings = InWorld->GetWorldSettings();
    const auto KillZ = WorldSettings != nullptr
        ? static_cast<float>(WorldSettings->KillZ)
        : -TNumericLimits<float>::Max();

    const auto Current = _DebugDrawTarget->Get_ProblemThresholds();

    // Re-pushed only on CHANGE: the setter drops the last verdict, so pushing an identical pair every refresh
    // would blank the problem set between every capture and make the chip flicker.
    if (Current.IsSet() &&
        Current->Get_RunawayVelocityCmS() == Settings->RunawayVelocityCmS &&
        Current->Get_KillZ() == KillZ)
    { return; }

    _DebugDrawTarget->Set_ProblemThresholds(
        FCk_Jolt_DebugDraw_ProblemThresholds{Settings->RunawayVelocityCmS, KillZ});
}

auto
    SCkJoltDebuggerWindow::
    DoSyncSelectionFromOutliner()
    -> void
{
    if (NOT _OutlinerPanel.IsValid())
    { return; }

    auto All = _OutlinerPanel->Get_SelectedAll();

    // Compared by row IDENTITY rather than by snapshot: every field but the handle and the population is
    // re-collected each pass, so a value compare would re-apply the whole selection on every velocity change.
    const auto IsUnchanged = All.Num() == _SelectionAll.Num() &&
        [&]()
        {
            for (auto Index = 0; Index < All.Num(); ++Index)
            {
                if (All[Index].Handle != _SelectionAll[Index].Handle ||
                    All[Index].Population != _SelectionAll[Index].Population)
                { return false; }
            }

            return true;
        }();

    if (IsUnchanged)
    { return; }

    DoApplySelectionSet(
        MoveTemp(All),
        _OutlinerPanel->Get_Selection(),
        ECkJoltDebugger_SelectionSource::OutlinerPrune);
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
    {
        _SelectionFacts.Reset();

        if (_DetailPanel.IsValid())
        { _DetailPanel->Refresh_Contacts(); }

        return;
    }

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

    // Every fact below comes from the facility's own capture, which sampled it in the physics pipeline's
    // async-safe window. Reading any of it off the live physics system from here would race the step.
    if (NOT _DebugDrawTarget.IsValid())
    {
        _SelectionFacts.Reset();

        if (_DetailPanel.IsValid())
        { _DetailPanel->Refresh_Contacts(); }

        return;
    }

    _SelectionFacts.BodySample      = _DebugDrawTarget->Get_BodySample();
    _SelectionFacts.CharacterSample = _DebugDrawTarget->Get_CharacterSample();
    _SelectionFacts.Contacts        = _DebugDrawTarget->Get_SelectionContacts();

    if (_DetailPanel.IsValid())
    { _DetailPanel->Refresh_Contacts(); }

    if (NOT _SelectionFacts.BodySample.IsSet())
    { return; }

    _Selection->LinearVelocity = _SelectionFacts.BodySample->Get_LinearVelocity();
    _Selection->HasLinearVelocity = true;
}

auto
    SCkJoltDebuggerWindow::
    DoApplySelection(
        TOptional<FCkJoltDebugger_BodySnapshot> InSnapshot,
        ECkJoltDebugger_SelectionSource InSource)
    -> void
{
    auto All = TArray<FCkJoltDebugger_BodySnapshot>{};

    if (InSnapshot.IsSet())
    { All.Emplace(*InSnapshot); }

    DoApplySelectionSet(MoveTemp(All), MoveTemp(InSnapshot), InSource);
}

auto
    SCkJoltDebuggerWindow::
    DoApplySelectionSet(
        TArray<FCkJoltDebugger_BodySnapshot> InAll,
        TOptional<FCkJoltDebugger_BodySnapshot> InPrimary,
        ECkJoltDebugger_SelectionSource InSource)
    -> void
{
    // A USER apply supersedes a route target still waiting for a row: left standing, it would land on the next
    // refresh and take the selection away from whoever just made one. A prune is nobody's act, so it leaves
    // the pending target alone — it runs on the very refresh the target is waiting for.
    if (InSource != ECkJoltDebugger_SelectionSource::External &&
        InSource != ECkJoltDebugger_SelectionSource::OutlinerPrune)
    { _PendingTarget.Reset(); }

    _Selection    = MoveTemp(InPrimary);
    _SelectionAll = MoveTemp(InAll);

    if (_DebugDrawTarget.IsValid())
    {
        // The FIRST key is the PRIMARY on the facility's side — it alone is sampled and asked for its
        // contacts — and the set arrives primary-first from the outliner's own store.
        auto Keys = TArray<uint64>{};
        Keys.Reserve(_SelectionAll.Num());

        for (const auto& Body : _SelectionAll)
        {
            if (Body.BodyKey.IsSet())
            { Keys.AddUnique(*Body.BodyKey); }

            // A constraint row draws NOTHING itself — what it highlights is the pair it joins (P8-D55). Body A
            // leads, so the facility's sample and the detail panel follow the constraint's own first body.
            for (const auto ConstraintBodyKey : Body.ConstraintBodyKeys)
            { Keys.AddUnique(ConstraintBodyKey); }
        }

        if (_Viewport.IsValid())
        { _Viewport->Set_SelectionKeys(Keys); }
        _DebugDrawTarget->Set_HighlightedBodies(MoveTemp(Keys));

        // The contacts query is a NarrowPhaseQuery::CollideShape the facility only runs while a consumer is
        // showing the result — which is exactly while something is selected.
        _DebugDrawTarget->Set_WantsSelectionContacts(_Selection.IsSet());
    }

    if (_Viewport.IsValid())
    {
        _Viewport->Set_SelectionBounds(_DebugDrawTarget.IsValid()
            ? _DebugDrawTarget->Get_HighlightedBodyBounds()
            : TOptional<FBox>{});
    }

    // The outliner is a sink for every source except the two that already drove its own store — re-stamping
    // those would fight the view's selection state, and a single-row stamp would collapse a multi-selection.
    const auto OutlinerOwnsThisApply = InSource == ECkJoltDebugger_SelectionSource::Outliner
        || InSource == ECkJoltDebugger_SelectionSource::ViewportAdditive
        || InSource == ECkJoltDebugger_SelectionSource::OutlinerPrune;

    if (_OutlinerPanel.IsValid() && NOT OutlinerOwnsThisApply)
    {
        if (_Selection.IsSet())
        { _OutlinerPanel->SelectByHandle(_Selection->Handle); }
        else
        { _OutlinerPanel->ClearSelection(); }
    }

    // Isolation names the SELECTED keys, so it has to be re-pushed whenever they change.
    DoApplyIsolation();

    DoApplyConstraintReferenceFrames();

    // The probe channel belongs to WHICHEVER probe is selected; a selection that left one has to take its
    // lines with it rather than leaving a retained channel describing a body nobody is looking at.
    DoUpdateProbeResults();

    const auto IsUserOriginated = InSource == ECkJoltDebugger_SelectionSource::Outliner
        || InSource == ECkJoltDebugger_SelectionSource::Viewport
        || InSource == ECkJoltDebugger_SelectionSource::ViewportAdditive;

    // Only the PRIMARY is broadcast: the rest of the suite is single-selection, and a set has no meaning there.
    if (IsUserOriginated && _Selection.IsSet())
    { ck::DebugSelectionSync::Broadcast(_Selection->Handle, TabId); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    HandleOutlinerRowSelected(
        TOptional<FCkJoltDebugger_BodySnapshot> InPrimary,
        TArray<FCkJoltDebugger_BodySnapshot> InAll)
    -> void
{
    DoApplySelectionSet(MoveTemp(InAll), MoveTemp(InPrimary), ECkJoltDebugger_SelectionSource::Outliner);
}

auto
    SCkJoltDebuggerWindow::
    TryFind_RowForBodyKey(
        uint64 InBodyKey) const
    -> const FCkJoltDebugger_BodySnapshot*
{
    const auto* Found = _Collector.Get_Bodies().FindByPredicate(
        [InBodyKey](const FCkJoltDebugger_BodySnapshot& InBody)
        { return InBody.BodyKey.IsSet() && *InBody.BodyKey == InBodyKey; });

    if (Found != nullptr)
    { return Found; }

    // A baked actor contributes many bodies and ONE row, whose key is only the first of them. Every other
    // baked body is just as pickable and resolves to its actor through the collector's owner index.
    const auto* Owner = _Collector.Get_BakedBodyOwners().Find(InBodyKey);

    if (Owner == nullptr)
    { return nullptr; }

    const auto OwnerHandle = *Owner;

    return _Collector.Get_Bodies().FindByPredicate(
        [&OwnerHandle](const FCkJoltDebugger_BodySnapshot& InBody) { return InBody.Handle == OwnerHandle; });
}

auto
    SCkJoltDebuggerWindow::
    HandleViewportBodyPicked(
        TOptional<uint64> InBodyKey,
        bool InIsAdditive)
    -> void
{
    if (NOT InBodyKey.IsSet())
    {
        // A Ctrl+click on empty space adds nothing rather than clearing what the user was building up.
        if (NOT InIsAdditive)
        { DoApplySelection({}, ECkJoltDebugger_SelectionSource::Viewport); }

        return;
    }

    const auto* Picked = TryFind_RowForBodyKey(*InBodyKey);

    // A pickable instance the outliner has no row for is a body the collector cannot attribute to an entity.
    // Leave the selection alone rather than clearing it — the click did hit something.
    if (Picked == nullptr)
    { return; }

    if (NOT InIsAdditive)
    {
        DoApplySelectionSet(
            TArray<FCkJoltDebugger_BodySnapshot>{*Picked},
            TOptional<FCkJoltDebugger_BodySnapshot>{*Picked},
            ECkJoltDebugger_SelectionSource::Viewport);
        return;
    }

    // The OUTLINER is the multi-selection store — a Ctrl+click in the viewport is the same act as a
    // Ctrl+click on its row, and routing it through the panel is what keeps the two from diverging.
    if (NOT _OutlinerPanel.IsValid())
    { return; }

    const auto Guard = ck::DebugSelectionSync::FApplyGuard{};

    if (NOT _OutlinerPanel->Add_ToSelection(Picked->Handle).IsSet())
    { return; }

    DoApplySelectionSet(
        _OutlinerPanel->Get_SelectedAll(),
        _OutlinerPanel->Get_Selection(),
        ECkJoltDebugger_SelectionSource::ViewportAdditive);
}

/*
 * The subdued twin of a pick (P8-D58). The facility owns the overlay; this window owns the NAME, because the
 * facility keys bodies and only the collector can turn a key back into an entity.
 */
auto
    SCkJoltDebuggerWindow::
    HandleViewportBodyHovered(
        TOptional<uint64> InBodyKey)
    -> void
{
    if (_DebugDrawTarget.IsValid())
    { _DebugDrawTarget->Set_HoveredBody(InBodyKey); }

    if (NOT _Viewport.IsValid())
    { return; }

    if (NOT InBodyKey.IsSet())
    {
        _Viewport->Set_HoverLabel(FText::GetEmpty());
        return;
    }

    const auto* Hovered = TryFind_RowForBodyKey(*InBodyKey);

    // A drawn body the collector cannot attribute to an entity still HIGHLIGHTS — it just has no name to show,
    // and inventing one from the key would read as an entity that does not exist.
    _Viewport->Set_HoverLabel(Hovered != nullptr
        ? FText::FromString(Hovered->DisplayName)
        : FText::GetEmpty());
}

// --------------------------------------------------------------------------------------------------------------------

/*
 * A contacts-row click. It is the same key -> row resolution a viewport pick does, and it is just as
 * user-driven, so it takes the same source and re-broadcasts.
 */
auto
    SCkJoltDebuggerWindow::
    HandleContactSelected(
        uint64 InOtherBodyKey)
    -> void
{
    HandleViewportBodyPicked(TOptional<uint64>{InOtherBodyKey}, false);
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
        // Pause and Step act on the world the viewport is showing, so they belong beside it rather than in a
        // context lane — they are the two controls a physics debugger is opened to reach.
        FCkDebug_CommandGroup::Primary(
            TEXT("JoltSim"),
            FText::FromString(TEXT("Jolt simulation")),
            BuildSimGroup()),
        // Drag is specialized Jolt state. Common owns isolate and follow presentation.
        FCkDebug_CommandGroup::Primary(
            TEXT("JoltSelection"),
            FText::FromString(TEXT("Jolt selection")),
            BuildSelectionGroup()),
        FCkDebug_CommandGroup::Context(
            TEXT("JoltTarget"),
            FText::FromString(TEXT("Jolt world selection")),
            BuildTargetGroup()),
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
    Get_IsAuthorityWorld() const
    -> bool
{
    auto* World = _WorldModel.IsValid() ? _WorldModel->Get_SelectedWorld() : nullptr;

    return ck_jolt_debugger::Get_IsInspectable(World) && World->GetNetMode() != NM_Client;
}

/*
 * Isolation is a SET on the facility, re-pushed from the current selection. An empty selection CLEARS it
 * instead of isolating nothing: a viewport that went blank while the toggle stayed lit is indistinguishable
 * from a broken draw, and the user's next act would be to turn Isolate off and back on to no effect.
 */
auto
    SCkJoltDebuggerWindow::
    DoApplyIsolation()
    -> void
{
    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    if (NOT _IsolateActive)
    {
        _DebugDrawTarget->Clear_Isolation();
        return;
    }

    auto Keys = TSet<uint64>{};
    Keys.Reserve(_SelectionAll.Num());

    for (const auto& Body : _SelectionAll)
    {
        if (Body.BodyKey.IsSet())
        { Keys.Emplace(*Body.BodyKey); }

        // A constraint row carries no body key of its own — what it names is the pair it joins (P8-D55).
        // Gathered here for the same reason `DoApplySelectionSet` gathers it for the highlight: isolating on
        // a constraint-only selection would otherwise find no keys and CLEAR the isolation, so selecting a
        // constraint while Isolate was lit turned isolation off (P8-D74/F2).
        for (const auto ConstraintBodyKey : Body.ConstraintBodyKeys)
        { Keys.Emplace(ConstraintBodyKey); }
    }

    if (Keys.IsEmpty())
    {
        _DebugDrawTarget->Clear_Isolation();
        return;
    }

    _DebugDrawTarget->Set_IsolatedBodies(MoveTemp(Keys));
}

auto
    SCkJoltDebuggerWindow::
    Set_IsolateActive(
        bool InIsActive)
    -> void
{
    _IsolateActive = InIsActive;

    if (_Viewport.IsValid())
    { _Viewport->Set_IsolateSelection(InIsActive); }

    DoApplyIsolation();

    auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
    Settings->IsolateActive = InIsActive;
    Settings->SaveConfig();
}

auto
    SCkJoltDebuggerWindow::
    Set_FollowSelection(
        bool InIsActive)
    -> void
{
    _FollowSelection = InIsActive;

    if (_Viewport.IsValid())
    { _Viewport->Set_FollowSelection(InIsActive); }

    auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
    Settings->FollowSelection = InIsActive;
    Settings->SaveConfig();
}

auto
    SCkJoltDebuggerWindow::
    HandleToggleIsolate()
    -> void
{
    Set_IsolateActive(NOT _IsolateActive);
}

// --------------------------------------------------------------------------------------------------------------------
// Debug drag (P7-D54). The VIEWPORT owns the cursor and the deprojection; this window owns the world, the
// subsystem and the drag plane. Every facility call here is behind the same #if the facility's own drag API
// is behind — it is the one sim-MUTATING thing this module does.
// --------------------------------------------------------------------------------------------------------------------

/*
 * The drag opens ON THE PRESS, at the exact point the pick ray met the body (P7-D70/i). The viewport resolves
 * the press through `TryPick_BodyHit` — one pick, which both selects the body and hands its surface point over
 * — so nothing here has to wait a capture for a sample or guess a depth from a bounds centre.
 *
 * Three refusals, and the first is the one that matters: the arm is keyed on the PICKED body, so a Ctrl+click
 * on empty space (or on a body that is not the primary the pick just made) opens no drag at all instead of
 * grabbing whatever was selected before (P7-D71/F4).
 */
auto
    SCkJoltDebuggerWindow::
    HandleDragArm(
        TOptional<uint64> InPickedKey,
        FVector InGrabPointWorld)
    -> void
{
#if !UE_BUILD_SHIPPING
    // A gesture whose release never arrived must not leak into this one.
    HandleDragRelease();

    if (NOT InPickedKey.IsSet() || NOT Get_IsAuthorityWorld())
    { return; }

    // The press picked and applied the selection before arming, so the primary IS the picked body — unless the
    // pick hit something the outliner has no row for, in which case there is nothing to drag by.
    if (NOT _Selection.IsSet() || NOT _Selection->BodyKey.IsSet() || *_Selection->BodyKey != *InPickedKey)
    { return; }

    // Only DYNAMIC bodies can be dragged — the facility drops anything else at Verbose, and arming on a
    // static body would leave a gesture that eats the mouse and silently does nothing.
    if (NOT _Selection->HasSimulationState || _Selection->MotionType != ECk_MotionType::Dynamic)
    { return; }

    auto* Subsystem = Get_SelectedJoltSubsystem();

    if (ck::Is_NOT_Valid(Subsystem))
    { return; }

    // The plane is camera-parallel THROUGH the grab point, captured once here. Ctrl+wheel slides it along its
    // own normal from then on.
    _DragPlaneNormal = _Viewport.IsValid()
        ? _Viewport->Get_ViewRotation().Vector()
        : FVector::ForwardVector;
    _DragPlanePoint = InGrabPointWorld;

    Subsystem->Request_BeginDrag(*InPickedKey, InGrabPointWorld);
    _DragBodyKey    = *InPickedKey;
    _DragSubsystem  = Subsystem;
#endif
}

auto
    SCkJoltDebuggerWindow::
    HandleDragRay(
        FVector InRayOrigin,
        FVector InRayDirection)
    -> void
{
#if !UE_BUILD_SHIPPING
    if (NOT _DragBodyKey.IsSet())
    { return; }

    auto* Subsystem = Get_SelectedJoltSubsystem();

    if (ck::Is_NOT_Valid(Subsystem))
    { return; }

    const auto TargetPoint = ck_jolt_debugger::TryIntersect_Plane(
        InRayOrigin, InRayDirection, _DragPlanePoint, _DragPlaneNormal);

    if (TargetPoint.IsSet())
    { Subsystem->Request_UpdateDrag(*TargetPoint); }
#endif
}

auto
    SCkJoltDebuggerWindow::
    HandleDragPlaneShift(
        float InDirection)
    -> void
{
#if !UE_BUILD_SHIPPING
    if (NOT _DragBodyKey.IsSet())
    { return; }

    // Scaled by how far the plane already is, so one wheel notch means the same thing on a body at arm's
    // length and on one across a streamed cell.
    const auto EyeLocation = _Viewport.IsValid() ? _Viewport->Get_ViewLocation() : FVector::ZeroVector;
    const auto Distance    = FMath::Max(FVector::Dist(EyeLocation, _DragPlanePoint), 1.0);

    _DragPlanePoint += _DragPlaneNormal * (InDirection * Distance * 0.1);
#endif
}

auto
    SCkJoltDebuggerWindow::
    HandleDragRelease()
    -> void
{
#if !UE_BUILD_SHIPPING
    // Ended on the subsystem the drag BEGAN on, captured weakly at arm (P8-D73). `HandleWorldChanged` calls
    // this AFTER the selector has already re-pointed, so `Get_SelectedJoltSubsystem()` there answers with the
    // new world's subsystem — which never had this drag — and the old world's body stayed on its spring until
    // FJoltWorld shutdown. A live server↔client selector switch never reaches that shutdown at all.
    // Weak because the world the drag began in can die first; the `Get_IsDragging()` gate stays, against THAT
    // subsystem, so a second call is still a no-op.
    if (auto* Subsystem = _DragSubsystem.Get();
        ck::IsValid(Subsystem) && _DragBodyKey.IsSet() && Subsystem->Get_IsDragging())
    { Subsystem->Request_EndDrag(); }

    _DragSubsystem.Reset();
    _DragBodyKey.Reset();
    _DragLineGrab.Reset();
    _DragLineAnchor.Reset();

    if (_DebugDrawTarget.IsValid())
    { _DebugDrawTarget->Clear_External(ck_jolt_debugger::Get_DragChannel()); }
#endif
}

/*
 * The drag line lives in a RETAINED named External sub-channel (P5-D61/S3): the capture re-emits it every
 * pass without clearing it, so it is pushed only when the grab point or the anchor actually MOVED. Moving it
 * means clearing that one channel and re-pushing — Draw_External* appends, it does not replace.
 */
auto
    SCkJoltDebuggerWindow::
    DoUpdateDragLine()
    -> void
{
#if !UE_BUILD_SHIPPING
    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    auto* Subsystem = Get_SelectedJoltSubsystem();
    const auto IsDragging = ck::IsValid(Subsystem) && Subsystem->Get_IsDragging();

    if (NOT IsDragging)
    {
        if (_DragLineAnchor.IsSet())
        {
            _DebugDrawTarget->Clear_External(ck_jolt_debugger::Get_DragChannel());
            _DragLineGrab.Reset();
            _DragLineAnchor.Reset();
        }

        return;
    }

    const auto State = Subsystem->Get_DragState();

    if (NOT State.IsSet())
    { return; }

    const auto GrabPoint   = State->Get_GrabPointWorld();
    const auto AnchorPoint = State->Get_AnchorPointWorld();

    constexpr auto MovedTolerance = 0.1;

    const auto IsUnchanged = _DragLineGrab.IsSet() && _DragLineAnchor.IsSet()
        && _DragLineGrab->Equals(GrabPoint, MovedTolerance)
        && _DragLineAnchor->Equals(AnchorPoint, MovedTolerance);

    if (IsUnchanged)
    { return; }

    _DebugDrawTarget->Clear_External(ck_jolt_debugger::Get_DragChannel());
    _DebugDrawTarget->Draw_ExternalLine(
        ck_jolt_debugger::Get_DragChannel(), GrabPoint, AnchorPoint, FLinearColor::Yellow);

    _DragLineGrab   = GrabPoint;
    _DragLineAnchor = AnchorPoint;
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    DoApplyConstraintReferenceFrames()
    -> void
{
    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    const auto IsConstraintSelected = _SelectionAll.ContainsByPredicate(
        [](const FCkJoltDebugger_BodySnapshot& InBody)
        { return InBody.Population == ECkJoltDebugger_Population::Constraint; });

    // Derived from the PERSISTED intent rather than remembered: the user's own toggle is the saved bit, the
    // selection forces it on while it holds, and turning the selection off restores exactly what they chose.
    const auto* Settings = GetDefault<UCkJoltDebuggerSettings>();
    const auto UserWantsFrames = EnumHasAnyFlags(
        static_cast<ECk_Jolt_DebugDrawFlags>(Settings->DrawFlags),
        ECk_Jolt_DebugDrawFlags::ConstraintReferenceFrames);

    auto Flags = _DebugDrawTarget->Get_DrawFlags();

    if (UserWantsFrames || IsConstraintSelected)
    { EnumAddFlags(Flags, ECk_Jolt_DebugDrawFlags::ConstraintReferenceFrames); }
    else
    { EnumRemoveFlags(Flags, ECk_Jolt_DebugDrawFlags::ConstraintReferenceFrames); }

    if (Flags == _DebugDrawTarget->Get_DrawFlags())
    { return; }

    // Not persisted: this is selection state, not a preference. Set_DrawFlag is what the user's own toggle uses.
    _DebugDrawTarget->Set_DrawFlags(Flags);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    Set_ShowProbeResults(
        bool InIsEnabled)
    -> void
{
    _ShowProbeResults = InIsEnabled;

    auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
    Settings->ShowProbeResults = InIsEnabled;
    Settings->SaveConfig();

    DoUpdateProbeResults();
}

auto
    SCkJoltDebuggerWindow::
    Set_DirectionGlyphScale(
        float InScale)
    -> void
{
    const auto ClampedScale = FMath::Clamp(
        InScale, ck_jolt_debugger::DirectionGlyphScaleMin, ck_jolt_debugger::DirectionGlyphScaleMax);

    if (NOT FMath::IsNearlyEqual(_DirectionGlyphScale, ClampedScale))
    {
        _DirectionGlyphScale = ClampedScale;

        if (_DebugDrawTarget.IsValid())
        { _DebugDrawTarget->Set_DirectionGlyphScale(ClampedScale); }

        // The retained channel contains the old arrow lengths. Rebuild it now rather than waiting for contacts to
        // change, and include the scale in its signature as a second guard against a stale retained drawing.
        _ProbeResultsSignature.Reset();
        DoUpdateProbeResults();
    }

    auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();

    if (NOT FMath::IsNearlyEqual(Settings->DirectionGlyphScale, ClampedScale))
    {
        Settings->DirectionGlyphScale = ClampedScale;
        Settings->SaveConfig();
    }
}

auto
    SCkJoltDebuggerWindow::
    Set_ShowGrid(
        bool InIsEnabled)
    -> void
{
    _ShowGrid = InIsEnabled;

    auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
    Settings->ShowGrid = InIsEnabled;
    Settings->SaveConfig();

    DoApplyGrid();
}

/*
 * The ground grid (P8-D59): a metre lattice at Z=0 pushed ONCE into its own retained External sub-channel.
 * The capture re-emits a retained channel every pass without clearing it (P5-D61/S3), so a grid that never
 * moves costs one push for the life of the window — which is why this is a push/clear rather than anything
 * that runs on Tick.
 *
 * Colours come off the shared style: the two lines through the origin ARE the world axes and are drawn in the
 * suite's own axis colours, so the grid doubles as the ground truth the orientation gizmo is claiming. No hex
 * anywhere.
 */
auto
    SCkJoltDebuggerWindow::
    DoApplyGrid()
    -> void
{
    using namespace ck_jolt_debugger;

    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    const auto Channel = Get_GridChannel();

    _DebugDrawTarget->Clear_External(Channel);

    if (NOT _ShowGrid)
    { return; }

    const auto MajorColor = CkStyle::TextMute();
    const auto MinorColor = MajorColor * GridMinorDim;

    const auto NumCells = FMath::FloorToInt(GridExtent / GridCellSize);

    for (auto Cell = -NumCells; Cell <= NumCells; ++Cell)
    {
        const auto Offset = static_cast<double>(Cell) * GridCellSize;
        const auto IsMajor = Cell % GridMajorEvery == 0;
        const auto LineColor = IsMajor ? MajorColor : MinorColor;

        // A line at x = Offset runs ALONG Y, so the one through the origin is the Y axis — and the other way
        // round. Naming them the wrong way is the one mistake a grid can make that misleads rather than looks
        // wrong.
        _DebugDrawTarget->Draw_ExternalLine(Channel,
            FVector{Offset, -GridExtent, 0.0}, FVector{Offset, GridExtent, 0.0},
            Cell == 0 ? CkStyle::AxisY() : LineColor);

        _DebugDrawTarget->Draw_ExternalLine(Channel,
            FVector{-GridExtent, Offset, 0.0}, FVector{GridExtent, Offset, 0.0},
            Cell == 0 ? CkStyle::AxisX() : LineColor);
    }
}

auto
    SCkJoltDebuggerWindow::
    Get_NumGridLines() const
    -> int32
{
    return _DebugDrawTarget.IsValid()
        ? _DebugDrawTarget->Get_NumExternalLines(ck_jolt_debugger::Get_GridChannel())
        : 0;
}

auto
    SCkJoltDebuggerWindow::
    Get_TargetDrawFlags() const
    -> ECk_Jolt_DebugDrawFlags
{
    return _DebugDrawTarget.IsValid()
        ? _DebugDrawTarget->Get_DrawFlags()
        : ECk_Jolt_DebugDrawFlags::None;
}

auto
    SCkJoltDebuggerWindow::
    Get_TargetColorMode() const
    -> ECk_Jolt_DebugDrawColorMode
{
    return Get_ColorMode();
}

auto
    SCkJoltDebuggerWindow::
    Get_TargetRenderMode() const
    -> ECk_Jolt_DebugDraw_RenderMode
{
    return _DebugDrawTarget.IsValid()
        ? _DebugDrawTarget->Get_RenderMode()
        : ECk_Jolt_DebugDraw_RenderMode::Solid;
}

auto
    SCkJoltDebuggerWindow::
    Get_TargetDirectionGlyphScale() const
    -> float
{
    return _DebugDrawTarget.IsValid() ? _DebugDrawTarget->Get_DirectionGlyphScale() : 1.0f;
}

auto
    SCkJoltDebuggerWindow::
    DoUpdateProbeResults()
    -> void
{
    using namespace ck_jolt_debugger;

    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    const auto DoClearChannel = [this]()
    {
        if (NOT _ProbeResultsSignature.IsSet())
        { return; }

        _ProbeResultsSignature.Reset();
        _DebugDrawTarget->Clear_External(Get_ProbeResultsChannel());
    };

    if (NOT _ShowProbeResults || NOT _Selection.IsSet() || ck::Is_NOT_Valid(_Selection->Handle))
    {
        DoClearChannel();
        return;
    }

    const auto SelectedHandle = _Selection->Handle;

    const auto IsProbe = SelectedHandle.Has<ck::FFragment_Probe_Current>();
    const auto IsProbeTrace = SelectedHandle.Has<ck::FFragment_ProbeTrace_WorldContacts>();

    if (NOT IsProbe && NOT IsProbeTrace)
    {
        DoClearChannel();
        return;
    }

    // Where the lines START. The probe's own drawn bounds rather than its transform: the sensor body is what
    // the viewport is showing, and a line leaving from anywhere else would not touch it.
    const auto Bounds = _DebugDrawTarget->Get_HighlightedBodyBounds();
    const auto Origin = Bounds.IsSet() && Bounds->IsValid != 0
        ? Bounds->GetCenter()
        : UCk_Utils_Transform_UE::Has(SelectedHandle)
            ? UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentLocation(SelectedHandle)
            : FVector::ZeroVector;

    // Gathered first, hashed, and only then drawn: Get_CurrentOverlaps copies a whole TSet, so the channel is
    // rebuilt when the RESULT changes rather than every time this runs (P5-D61/S3).
    struct FProbeLink
    {
        FVector          _OtherLocation = FVector::ZeroVector;
        bool             _HasOtherLocation = false;
        TArray<FVector>  _ContactPoints;
        FVector          _ContactNormal = FVector::ZeroVector;
    };

    auto Links = TArray<FProbeLink>{};
    auto Signature = uint32{0};

    const auto NoteOther = [&Signature](const FCk_Handle& InOther, FProbeLink& OutLink)
    {
        Signature = HashCombine(Signature, GetTypeHash(InOther));

        if (ck::Is_NOT_Valid(InOther) || NOT UCk_Utils_Transform_UE::Has(InOther))
        { return; }

        OutLink._OtherLocation = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentLocation(InOther);
        OutLink._HasOtherLocation = true;

        // The POSITION, not just the identity (P8-D74/F5). The overlap set of a probe resting against a body
        // that is moving never changes membership, so an identity-only digest froze the lines where the two
        // entities were when the selection landed.
        Signature = HashCombine(Signature, GetTypeHash(OutLink._OtherLocation));
    };

    if (IsProbe)
    {
        const auto Probe = UCk_Utils_Probe_UE::CastChecked(SelectedHandle);

        for (const auto& Overlap : UCk_Utils_Probe_UE::Get_CurrentOverlaps(Probe))
        {
            auto Link = FProbeLink{};
            NoteOther(Overlap.Get_OtherEntity(), Link);

            Link._ContactPoints = Overlap.Get_ContactPoints();
            Link._ContactNormal = Overlap.Get_ContactNormal();

            Signature = HashCombine(Signature, static_cast<uint32>(Link._ContactPoints.Num()));
            // A resting overlap can keep both its members and contact points while its normal changes (for example,
            // when a touched shape rotates). The retained arrow must follow that direction too.
            Signature = HashCombine(Signature, GetTypeHash(Link._ContactNormal));

            for (const auto& Point : Link._ContactPoints)
            { Signature = HashCombine(Signature, GetTypeHash(Point)); }

            Links.Emplace(MoveTemp(Link));
        }
    }
    else
    {
        // A ProbeTrace records WHICH entities it hit and nothing else — FFragment_ProbeTrace_WorldContacts
        // holds a TSet<FCk_Handle> with no positions in it — so this half can only ever draw the lines.
        for (const auto& Other : SelectedHandle.Get<ck::FFragment_ProbeTrace_WorldContacts>()._Entities)
        {
            auto Link = FProbeLink{};
            NoteOther(Other, Link);
            Links.Emplace(MoveTemp(Link));
        }
    }

    Signature = HashCombine(Signature, static_cast<uint32>(Links.Num()));
    Signature = HashCombine(Signature, GetTypeHash(SelectedHandle));
    Signature = HashCombine(Signature, GetTypeHash(_DirectionGlyphScale));

    // Every line STARTS at the origin, so a probe that moved while its overlap set held still moves the whole
    // drawing (P8-D74/F5).
    Signature = HashCombine(Signature, GetTypeHash(Origin));

    if (_ProbeResultsSignature.IsSet() && *_ProbeResultsSignature == Signature)
    { return; }

    _ProbeResultsSignature = Signature;
    _DebugDrawTarget->Clear_External(Get_ProbeResultsChannel());

    const auto Channel = Get_ProbeResultsChannel();
    const auto LinkColor = ck::debug_axes::Get_CategoricalColor(2);
    const auto PointColor = ck::debug_axes::Get_CategoricalColor(0);
    const auto NormalColor = ck::debug_axes::Get_CategoricalColor(1);

    for (const auto& Link : Links)
    {
        if (Link._HasOtherLocation)
        { _DebugDrawTarget->Draw_ExternalLine(Channel, Origin, Link._OtherLocation, LinkColor); }

        for (const auto& Point : Link._ContactPoints)
        {
            _DebugDrawTarget->Draw_ExternalSphere(Channel, Point, ProbeContactPointRadius, PointColor);

            if (Link._ContactNormal.IsNearlyZero())
            { continue; }

            _DebugDrawTarget->Draw_ExternalArrow(Channel, Point,
                Point + Link._ContactNormal.GetSafeNormal() * ProbeContactNormalLength * _DirectionGlyphScale,
                NormalColor);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    DoUpdateViewportLabels()
    -> void
{
    if (NOT _Viewport.IsValid())
    { return; }

    if (NOT _Selection.IsSet() || NOT _DebugDrawTarget.IsValid())
    {
        _Viewport->Set_PrimaryLabel({});
        return;
    }

    const auto Bounds = _DebugDrawTarget->Get_HighlightedBodyBounds();

    if (NOT Bounds.IsSet() || Bounds->IsValid == 0)
    {
        _Viewport->Set_PrimaryLabel({});
        return;
    }

    auto Label = FCkJoltDebugger_ViewportLabel{};

    // Sat on TOP of the selection rather than at its centre, so the text does not sit inside the very shape it
    // is naming.
    Label.WorldPosition = Bounds->GetCenter() + FVector{0.0, 0.0, Bounds->GetExtent().Z};
    Label.Text          = _Selection->DisplayName;
    Label.Color         = _DebugDrawTarget->Get_Palette().Get_HighlightColor();

    _Viewport->Set_PrimaryLabel(MoveTemp(Label));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    BuildSelectionGroup()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            /*
             * The drag is armed by the WORLD, not by the user, so this reads as state rather than as a
             * control: its inner check box is always disabled, and it is merely lit while the selected world
             * is the authority.
             *
             * The live explanation binds on the TOGGLE itself (P7-D71/F11). `SCkDebug_IconToggle` takes its
             * own ToolTip as a static FText and hands it to the disabled check box inside — which shows none
             * — while the compound widget around it stays enabled, so the base ToolTipText attribute lands
             * exactly where the hover resolves it. A Ctrl+LMB that silently did nothing on a client would read
             * as a broken debugger rather than as a refused sim mutation, so the reason has to be reachable.
             */
            SNew(SCkDebug_IconToggle)
            .IconId(TEXT("Hand"))
            .Label(FText::FromString(TEXT("Drag")))
            .IsEnabled(false)
            .IsOn_Lambda([this]() { return Get_IsAuthorityWorld(); })
            .ToolTipText_Lambda([this]() -> FText
            {
                return Get_IsAuthorityWorld()
                    ? FText::FromString(TEXT("Ctrl+LMB drags a DYNAMIC body by a spring, grabbing it at the point you clicked; Ctrl+wheel moves the drag plane along the view; release drops it."))
                    : FText::FromString(TEXT("Dragging is disabled: the selected world is a CLIENT. A drag here would move a body the server corrects on its next replication."));
            })
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
    Cycle_RenderMode()
    -> void
{
    Set_RenderMode(ck_jolt_debugger::Get_NextRenderMode(Get_TargetRenderMode()));
}

#if WITH_DEV_AUTOMATION_TESTS
auto
    SCkJoltDebuggerWindow::
    Cycle_RenderModeForTest()
    -> void
{
    Cycle_RenderMode();
}
#endif

auto
    SCkJoltDebuggerWindow::
    Set_RenderMode(
        ECk_Jolt_DebugDraw_RenderMode InRenderMode)
    -> void
{
    if (NOT _DebugDrawTarget.IsValid())
    { return; }

    _DebugDrawTarget->Set_RenderMode(InRenderMode);

    auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
    Settings->RenderMode = ck_jolt_debugger::Get_RenderModePref(InRenderMode);
    Settings->SaveConfig();
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

            if (Flag == ECk_Jolt_DebugDrawFlags::Labels)
            {
                continue;
            }

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

        if (Actions.IsEmpty())
        {
            continue;
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

    // NOT a facility draw flag, and deliberately in the same lane as the ones that are: from the user's side
    // "show me what the probe is touching" is the same kind of question as "show me velocities" (P8-D56).
    Lane->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
    [
        SNew(STextBlock)
        .Font_Static(&ck_jolt_debugger::Font_RowLabel)
        .ColorAndOpacity(CkStyle::TextMute())
        .Text(FText::FromString(TEXT("Probe")))
    ];

    Lane->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
    [
        SNew(SCkDebug_IconToggle)
        .IconId(TEXT("Probe"))
        .Label(FText::FromString(TEXT("Probe results")))
        .ToolTip(FText::FromString(TEXT(
            "Draw what the SELECTED probe is currently overlapping: a line to each overlapping entity, its "
            "contact points, and their normals.\n\n"
            "A persistent probe TRACE records only WHICH entities it hit - its world-contacts fragment holds "
            "no hit positions at all - so a trace selection draws the lines and nothing else.\n\n"
            "Read for the selection only: the overlap query returns a full copy of the probe's overlap set.")))
        .IsOn_Lambda([this]() { return _ShowProbeResults; })
        .OnStateChanged_Lambda([this](const bool InIsEnabled) { Set_ShowProbeResults(InIsEnabled); })
    ];

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
        _DebugDrawTarget->Set_RenderMode(ck_jolt_debugger::Get_RenderMode(Settings->RenderMode));

        _DebugDrawTarget->Set_DrawFlags(static_cast<ECk_Jolt_DebugDrawFlags>(Settings->DrawFlags));

        // Before the population visibility, never after: Set_ColorMode clears the visibility mask, so a mode
        // restored second would wipe the very toggles this pass just put back.
        _DebugDrawTarget->Set_ColorMode(ck_jolt_debugger::Get_ColorMode(Settings->ColorMode));

        DoApplyPopulationVisibility();
    }

    DoRebuildLegend();

    // Isolate is restored as STATE, not as an act: with nothing selected yet it pushes no isolation set, and
    // the first selection the user makes arms it through DoApplySelectionSet.
    _IsolateActive     = Settings->IsolateActive;
    _FollowSelection   = Settings->FollowSelection;
    _ShowProbeResults  = Settings->ShowProbeResults;
    _DirectionGlyphScale = FMath::Clamp(
        Settings->DirectionGlyphScale,
        ck_jolt_debugger::DirectionGlyphScaleMin,
        ck_jolt_debugger::DirectionGlyphScaleMax);
    _DebugDrawTarget->Set_DirectionGlyphScale(_DirectionGlyphScale);
    _ShowGrid          = Settings->ShowGrid;

    DoApplyIsolation();

    // Pushed once, here: the grid is retained by the capture from now on and only a toggle ever touches it.
    DoApplyGrid();

    if (_Viewport.IsValid())
    {
        _Viewport->Set_FollowSelection(_FollowSelection);
        _Viewport->ApplyPreset(ck_jolt_debugger::Get_CameraPreset(Settings->CameraPreset));
    }
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

                // The health count (P8-D57). Present only when there IS something wrong: a badge reading "0"
                // is a permanent alarm nobody can silence, and an absent one is the good news.
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SBox)
                    .ToolTipText(FText::FromString(TEXT(
                        "Bodies the facility's health scan flagged this capture: NaN transform or velocity, a "
                        "runaway linear speed, an AABB under the world's KillZ, or a shape with no extent. "
                        "Use the outliner's Problems chip to narrow to them.")))
                    .Visibility_Lambda([this]() -> EVisibility
                    {
                        return _Collector.Get_NumProblemRows() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    [
                        SNew(SCkDebug_CountBadge)
                        .ValueText_Lambda([this]() -> FText
                        {
                            return FText::AsNumber(_Collector.Get_NumProblemRows());
                        })
                        .SuffixText(FText::FromString(TEXT("problems")))
                        .ValueColor(ck::debug_axes::Get_HeatColor(1.0f))
                    ]
                ]

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

        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
                [
                    SNew(SCkDebug_StatPair)
                    .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    .ValueColor(FSlateColor{CkStyle::Value_Numeric()})
                    .Value_Lambda([this]() { return _Stats.HasWorld
                        ? ck_jolt_debugger::MillisecondsText(_Stats.LastStepMs)
                        : FText::FromString(TEXT("--")); })
                    .Label(FText::FromString(TEXT("LAST STEP")))
                ]

                + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
                [
                    SNew(SCkDebug_StatPair)
                    .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    .ValueColor(FSlateColor{CkStyle::Value_Numeric()})
                    .Value_Lambda([this]() { return _Stats.HasWorld
                        ? FText::AsNumber(_Stats.NumActiveRigidBodies)
                        : FText::FromString(TEXT("--")); })
                    .Label(FText::FromString(TEXT("ACTIVE")))
                ]

                + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
                [
                    SNew(SCkDebug_StatPair)
                    .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    .ValueColor(FSlateColor{CkStyle::Value_Numeric()})
                    .Value_Lambda([this]() { return _Stats.HasWorld
                        ? FText::AsNumber(_Stats.ContactPairsLastStep)
                        : FText::FromString(TEXT("--")); })
                    .Label(FText::FromString(TEXT("CONTACTS")))
                ]

                + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
                [
                    SNew(SCkDebug_StatPair)
                    .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    .ValueColor(FSlateColor{CkStyle::Value_Numeric()})
                    .Value_Lambda([this]() { return _Stats.HasWorld
                        ? FText::AsNumber(_Stats.NumBodies)
                        : FText::FromString(TEXT("--")); })
                    .Label(FText::FromString(TEXT("BODIES")))
                ]
            ]

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
                    [ MakeStatRow(TEXT("Active Soft Bodies:"), TAttribute<FText>::CreateLambda([this]()
                        { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumActiveSoftBodies) : FText::FromString(TEXT("--")); })) ]

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
