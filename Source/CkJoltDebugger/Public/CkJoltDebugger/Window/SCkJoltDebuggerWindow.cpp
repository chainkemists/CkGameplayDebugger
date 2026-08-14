#include "CkJoltDebugger/Window/SCkJoltDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
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
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"

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

    // RowDensity lands live on every stat row — the slot padding is a Slate attribute, so the axis
    // moves already-built rows without a rebuild.
    static auto Get_StatRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, 0.0f});
    }

    static auto FindGameWorld() -> UWorld*
    {
        if (NOT GEngine)
        { return nullptr; }

        for (const auto& Context : GEngine->GetWorldContexts())
        {
            auto* World = Context.World();

            if (ck::Is_NOT_Valid(World))
            { continue; }

            if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
            { continue; }

            if (NOT World->HasBegunPlay())
            { continue; }

            return World;
        }

        return nullptr;
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

    // ---- Summary (world) ----

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

    // ---- Body ----

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome).WindowId(Get_WindowId()).ToolTabId(TEXT("CkJoltDebugger"))
        .ShowRefreshControls(true)
        .CommandGroups({
            FCkDebug_CommandGroup::Primary(
                TEXT("JoltDebugDraw"),
                FText::FromString(TEXT("Jolt debug drawing")),
                SNew(SCkDebug_IconToolbar)
            .Actions({
                FCkDebug_IconToggleAction{
                    TEXT("JoltDebugDraw"),
                    TEXT("Cube"),
                    FText::FromString(TEXT("Debug Draw")),
                    FText::FromString(TEXT("Draw Jolt physics bodies in the selected world.")),
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
                    FText::FromString(TEXT("Velocity Vectors")),
                    FText::FromString(TEXT("Draw linear-velocity vectors for active Jolt bodies.")),
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
            }))})
        .Content()
        [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::Bg1())
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                [ Summary ]

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f)
                [ ck::debug_axes::Make_AxisSeparator() ]

            + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SScrollBox)

                    + SScrollBox::Slot().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                        [ MakeSectionHeader(TEXT("World")) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("Async Physics Update:"), TAttribute<FText>::CreateLambda([this]()
                            { return FText::FromString(_Stats.HasWorld ? (_Stats.AsyncPhysics ? TEXT("Yes") : TEXT("No")) : TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("Parallel Physics:"), TAttribute<FText>::CreateLambda([this]()
                            { return FText::FromString(_Stats.HasWorld ? (_Stats.ParallelPhysics ? TEXT("Yes") : TEXT("No")) : TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("Physics Threads:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.ThreadCount) : FText::FromString(TEXT("--")); })) ]

                    + SScrollBox::Slot().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                        [ MakeSectionHeader(TEXT("Rigid Bodies")) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("JoltBody Entities:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumBodies) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("Dynamic:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumDynamic) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("Kinematic:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumKinematic) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("Static:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumStatic) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("Awake:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumAwake) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("Asleep:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumAsleep) : FText::FromString(TEXT("--")); })) ]

                    + SScrollBox::Slot().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                        [ MakeSectionHeader(TEXT("Characters")) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("JoltCharacter Entities:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumCharacters) : FText::FromString(TEXT("--")); })) ]

                    + SScrollBox::Slot().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                        [ MakeSectionHeader(TEXT("Static World")) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("JoltStaticActor Entities:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumStaticActors) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("Static Bodies:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumStaticBodies) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger::Get_StatRowPadding))
                        [ MakeStatRow(TEXT("Unique Shapes:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumUniqueShapes) : FText::FromString(TEXT("--")); })) ]
                ]
        ]
        ]
    ];
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

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    DoRefreshStats();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebuggerWindow::
    DoRefreshStats()
    -> void
{
    auto Stats = FCkJoltDebugger_Stats{};

    auto* World = ck_jolt_debugger::FindGameWorld();

    if (World == nullptr)
    {
        _Stats = Stats;
        return;
    }

    Stats.HasWorld   = true;
    Stats.WorldLabel = ck::Format_UE(TEXT("{} ({})"), World->GetName(), ck_jolt_debugger::NetModeLabel(World));

    if (auto* JoltSubsystem = World->GetSubsystem<UCk_Jolt_Subsystem>())
    {
        Stats.AsyncPhysics    = JoltSubsystem->Get_AsyncPhysicsUpdate();
        Stats.ParallelPhysics = JoltSubsystem->Get_ParallelPhysicsEnabled();
        Stats.ThreadCount     = JoltSubsystem->Get_PhysicsThreadCount();
    }

    if (auto* StaticWorldSubsystem = World->GetSubsystem<UCk_JoltStaticWorld_Subsystem_UE>())
    {
        Stats.NumStaticBodies = StaticWorldSubsystem->Get_NumStaticBodies();
        Stats.NumUniqueShapes = StaticWorldSubsystem->Get_NumUniqueShapes();
    }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(World);

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
    return SNew(SHorizontalBox)

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
            ];
}

// --------------------------------------------------------------------------------------------------------------------
