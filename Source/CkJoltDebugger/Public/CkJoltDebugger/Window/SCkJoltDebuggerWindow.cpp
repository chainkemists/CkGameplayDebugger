#include "CkJoltDebugger/Window/SCkJoltDebuggerWindow.h"

#include "CkJoltDebugger/Viewport/SCkJoltDebugger_3dViewport.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkJolt/Subsystem/CkJolt_Subsystem.h"
#include "CkJolt/StaticWorld/CkJoltStaticWorld_Subsystem.h"
#include "CkJolt/Body/CkJoltBody_Fragment.h"
#include "CkJolt/Character/CkJoltCharacter_Fragment.h"
#include "CkJolt/StaticWorld/CkJoltStaticActor_Fragment.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

#include "HAL/IConsoleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
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

    static auto Get_ColorClassLabel(ECk_Jolt_DebugDraw_ColorClass InColorClass) -> FString
    {
        switch (InColorClass)
        {
            case ECk_Jolt_DebugDraw_ColorClass::Static:           return TEXT("Static");
            case ECk_Jolt_DebugDraw_ColorClass::Kinematic:        return TEXT("Kinematic");
            case ECk_Jolt_DebugDraw_ColorClass::Dynamic_Awake:    return TEXT("Awake");
            case ECk_Jolt_DebugDraw_ColorClass::Dynamic_Sleeping: return TEXT("Asleep");
            case ECk_Jolt_DebugDraw_ColorClass::Sensor:           return TEXT("Sensor");
            case ECk_Jolt_DebugDraw_ColorClass::BakedStatic:      return TEXT("Baked");
            case ECk_Jolt_DebugDraw_ColorClass::Character:        return TEXT("Character");
            default:                                              return TEXT("Unknown");
        }
    }

    static auto Get_AllColorClasses() -> TArray<ECk_Jolt_DebugDraw_ColorClass>
    {
        return
        {
            ECk_Jolt_DebugDraw_ColorClass::Static,
            ECk_Jolt_DebugDraw_ColorClass::Kinematic,
            ECk_Jolt_DebugDraw_ColorClass::Dynamic_Awake,
            ECk_Jolt_DebugDraw_ColorClass::Dynamic_Sleeping,
            ECk_Jolt_DebugDraw_ColorClass::Sensor,
            ECk_Jolt_DebugDraw_ColorClass::BakedStatic,
            ECk_Jolt_DebugDraw_ColorClass::Character
        };
    }
}

// --------------------------------------------------------------------------------------------------------------------

const FName SCkJoltDebuggerWindow::WindowId = FName(TEXT("JoltDebugger"));

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();
    _Viewport = SNew(SCkJoltDebugger_3dViewport);

    DoCreateDebugDrawTarget();

    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddSP(this, &SCkJoltDebuggerWindow::HandleWorldChanged);
    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkJoltDebuggerWindow::HandleSessionInvalidated);
    _TabForegroundedHandle = FGlobalTabmanager::Get()->OnTabForegrounded_Subscribe(
        FOnActiveTabChanged::FDelegate::CreateSP(this, &SCkJoltDebuggerWindow::HandleTabForegrounded));

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome).WindowId(Get_WindowId()).ToolTabId(TEXT("CkJoltDebugger"))
        .ShowRefreshControls(true)
        .CommandGroups(BuildCommandGroups())
        .Content()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(CkStyle::Bg1())
            [
                SNew(SSplitter).Orientation(Orient_Horizontal)

                + SSplitter::Slot().Value(0.72f)
                [ _Viewport.ToSharedRef() ]

                + SSplitter::Slot().Value(0.28f)
                [ BuildStatRail() ]
            ]
        ]
    ];
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

    if (_TabForegroundedHandle.IsValid())
    { FGlobalTabmanager::Get()->OnTabForegrounded_Unsubscribe(_TabForegroundedHandle); }

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

    _WorldModel->Ensure_AutoSelect();
    auto* World = _WorldModel->Get_SelectedWorld();

    DoSyncDebugDrawTarget(World);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    DoRefreshStats(World);
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
    { _DebugDrawTarget->Set_IsDesired(false); }

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
        FCkDebug_CommandGroup::Context(
            TEXT("JoltTarget"),
            FText::FromString(TEXT("Jolt world selection")),
            BuildTargetGroup()),
        FCkDebug_CommandGroup::Context(
            TEXT("JoltCamera"),
            FText::FromString(TEXT("Jolt viewport camera")),
            BuildCameraGroup()),
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
            FCkDebug_IconToggleAction{
                TEXT("JoltVelocityVectors"),
                TEXT("ArrowProjectile"),
                FText::FromString(TEXT("In-world Velocity Vectors")),
                FText::FromString(TEXT("Draw linear-velocity vectors for active Jolt bodies in the GAME viewport, not this one (ck.Jolt.DebugDraw.Velocity).")),
                TAttribute<bool>::CreateLambda([]()
                {
                    return ck_jolt_debugger::GetDebugCVarBool(TEXT("ck.Jolt.DebugDraw.Velocity"));
                }),
                FOnCkDebug_IconToggleChanged::CreateLambda([](const bool InIsEnabled)
                {
                    ck_jolt_debugger::SetDebugCVarBool(TEXT("ck.Jolt.DebugDraw.Velocity"), InIsEnabled);
                }),
                TAttribute<bool>::CreateLambda([]()
                {
                    return ck_jolt_debugger::GetDebugCVarBool(TEXT("ck.Jolt.DebugDraw.Enabled"));
                })}
        });
}

auto
    SCkJoltDebuggerWindow::
    BuildTargetGroup()
    -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_WorldSelector, _WorldModel).ShowHeaderLabel(false);
}

auto
    SCkJoltDebuggerWindow::
    BuildCameraGroup()
    -> TSharedRef<SWidget>
{
    const auto MakeCameraButton = [this](
        FName InIconId,
        const TCHAR* InToolTip,
        ECkJoltDebugger_CameraPreset InPreset) -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ToolTipText(FText::FromString(InToolTip))
            .ContentPadding(FMargin{4.0f, 1.0f})
            .OnClicked_Lambda([this, InPreset]() -> FReply
            {
                if (_Viewport.IsValid())
                { _Viewport->ApplyPreset(InPreset); }

                return FReply::Handled();
            })
            [
                SNew(SImage).Image(FCkDebuggerCommonStyle::Get_IconBrush(InIconId))
            ];
    };

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewPerspective"), TEXT("Perspective camera"), ECkJoltDebugger_CameraPreset::Perspective) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewTop"), TEXT("Top orthographic camera"), ECkJoltDebugger_CameraPreset::Top) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewBottom"), TEXT("Bottom orthographic camera"), ECkJoltDebugger_CameraPreset::Bottom) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewLeft"), TEXT("Left orthographic camera"), ECkJoltDebugger_CameraPreset::Left) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewRight"), TEXT("Right orthographic camera"), ECkJoltDebugger_CameraPreset::Right) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewFront"), TEXT("Front orthographic camera"), ECkJoltDebugger_CameraPreset::Front) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("ViewBack"), TEXT("Back orthographic camera"), ECkJoltDebugger_CameraPreset::Back) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCameraButton(TEXT("FrameActor"), TEXT("Frame every drawn Jolt body (Home)"), ECkJoltDebugger_CameraPreset::FrameAll) ];
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
        });
}

auto
    SCkJoltDebuggerWindow::
    BuildPopulationGroup()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            MakePopulationToggle(
                TEXT("Jolt"),
                TEXT("Jolt Bodies"),
                TEXT("Show rigid bodies composed through CkJoltBody — static, kinematic, and dynamic (awake or asleep)."),
                {
                    ECk_Jolt_DebugDraw_ColorClass::Static,
                    ECk_Jolt_DebugDraw_ColorClass::Kinematic,
                    ECk_Jolt_DebugDraw_ColorClass::Dynamic_Awake,
                    ECk_Jolt_DebugDraw_ColorClass::Dynamic_Sleeping
                })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            MakePopulationToggle(
                TEXT("World"),
                TEXT("Baked Static World"),
                TEXT("Show the baked level geometry extracted into the Jolt static world."),
                { ECk_Jolt_DebugDraw_ColorClass::BakedStatic })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            MakePopulationToggle(
                TEXT("Probe"),
                TEXT("Sensors"),
                TEXT("Show sensor bodies — the trigger volumes behind CkSpatialQuery probes."),
                { ECk_Jolt_DebugDraw_ColorClass::Sensor })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            MakePopulationToggle(
                TEXT("Person"),
                TEXT("Characters"),
                TEXT("Show CkJoltCharacter capsules. Characters have no broadphase body — they are drawn from their own shape."),
                { ECk_Jolt_DebugDraw_ColorClass::Character })
        ];
}

auto
    SCkJoltDebuggerWindow::
    MakePopulationToggle(
        FName InIconId,
        const FString& InLabel,
        const FString& InToolTip,
        TArray<ECk_Jolt_DebugDraw_ColorClass> InColorClasses) const
    -> TSharedRef<SWidget>
{
    const auto HasColorClasses = NOT InColorClasses.IsEmpty();
    CK_ENSURE_IF_NOT(HasColorClasses, TEXT("Population toggle [{}] was given no colour classes to drive"), InLabel)
    { return SNullWidget::NullWidget; }

    // The whole group tracks its first class: they are only ever flipped together from here.
    const auto RepresentativeClass = InColorClasses[0];

    return SNew(SCkDebug_IconToggle)
        .IconId(InIconId)
        .Label(FText::FromString(InLabel))
        .ToolTip(FText::FromString(InToolTip))
        .IsOn_Lambda([this, RepresentativeClass]()
        {
            return _DebugDrawTarget.IsValid() && _DebugDrawTarget->Get_IsClassVisible(RepresentativeClass);
        })
        .OnStateChanged_Lambda([this, InColorClasses](const bool InIsVisible)
        {
            if (NOT _DebugDrawTarget.IsValid())
            { return; }

            for (const auto& ColorClass : InColorClasses)
            { _DebugDrawTarget->Set_ClassVisibility(ColorClass, InIsVisible); }
        });
}

auto
    SCkJoltDebuggerWindow::
    BuildLegendGroup() const
    -> TSharedRef<SWidget>
{
    auto Legend = SNew(SHorizontalBox);

    for (const auto& ColorClass : ck_jolt_debugger::Get_AllColorClasses())
    {
        Legend->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(10.0f).HeightOverride(10.0f)
                [
                    SNew(SImage)
                    .Image(FAppStyle::GetBrush("WhiteBrush"))
                    .ColorAndOpacity_Lambda([this, ColorClass]() -> FSlateColor
                    {
                        return _DebugDrawTarget.IsValid()
                            ? FSlateColor{_DebugDrawTarget->Get_Palette().Get_Color(ColorClass)}
                            : CkStyle::TextMute();
                    })
                ]
            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Font_Static(&ck_jolt_debugger::Font_RowLabel)
                .ColorAndOpacity(CkStyle::TextDim())
                .Text(FText::FromString(ck_jolt_debugger::Get_ColorClassLabel(ColorClass)))
            ]
        ];
    }

    return Legend;
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
            [ Summary ]

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
