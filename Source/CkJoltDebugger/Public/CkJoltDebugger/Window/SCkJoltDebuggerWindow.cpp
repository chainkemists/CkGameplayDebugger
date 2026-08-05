#include "CkJoltDebugger/Window/SCkJoltDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

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

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Local style + helpers (module-unique namespace name — unity builds concatenate TUs).
// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_debugger
{
    static constexpr auto Pad_S = 4.0f;
    static constexpr auto Pad_M = 8.0f;

    static const auto Bg_Dark   = FLinearColor(0.01f, 0.01f, 0.01f);
    static const auto Bg_Medium = FLinearColor(0.025f, 0.025f, 0.025f);

    static const auto Text_Primary   = FLinearColor(0.85f, 0.85f, 0.85f);
    static const auto Text_Secondary = FLinearColor(0.6f, 0.6f, 0.6f);
    static const auto Text_Highlight = FLinearColor(0.95f, 0.95f, 0.95f);

    static auto Normal(int32 InSize = 9) -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Regular", InSize); }
    static auto Bold(int32 InSize = 9)   -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Bold", InSize); }
    static auto Mono(int32 InSize = 9)   -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Mono", InSize); }

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

    // ---- Toolbar ----

    auto Toolbar =
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(ck_jolt_debugger::Bg_Dark)
        .Padding(FMargin(ck_jolt_debugger::Pad_M, ck_jolt_debugger::Pad_S))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Font(ck_jolt_debugger::Bold(11))
                    .ColorAndOpacity(ck_jolt_debugger::Text_Highlight)
                    .Text(FText::FromString(TEXT("Jolt Physics")))
                ]

            + SHorizontalBox::Slot().FillWidth(1.0f)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SCkDebugger_RefreshControls)
                        .WindowId(SCkJoltDebuggerWindow::WindowId)
                ]
        ];

    // ---- Summary (world) ----

    auto Summary =
        SNew(STextBlock)
        .Font(ck_jolt_debugger::Bold(10))
        .ColorAndOpacity(ck_jolt_debugger::Text_Primary)
        .Text_Lambda([this]() -> FText
        {
            return _Stats.HasWorld
                ? FText::FromString(ck::Format_UE(TEXT("World: {}"), _Stats.WorldLabel))
                : FText::FromString(TEXT("No active world. Start PIE to see Jolt stats."));
        });

    // ---- Body ----

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome).WindowId(Get_WindowId()).ToolTabId(TEXT("CkJoltDebugger")).DisplayName(Get_WindowDisplayName()).Content()
        [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(ck_jolt_debugger::Bg_Medium)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()
                [ Toolbar ]

            + SVerticalBox::Slot().AutoHeight().Padding(ck_jolt_debugger::Pad_M, ck_jolt_debugger::Pad_S)
                [ Summary ]

            + SVerticalBox::Slot().AutoHeight().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                [ SNew(SSeparator).Thickness(1.0f) ]

            + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SScrollBox)

                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, ck_jolt_debugger::Pad_S)
                        [ MakeSectionHeader(TEXT("World")) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("Async Physics Update:"), TAttribute<FText>::CreateLambda([this]()
                            { return FText::FromString(_Stats.HasWorld ? (_Stats.AsyncPhysics ? TEXT("Yes") : TEXT("No")) : TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("Parallel Physics:"), TAttribute<FText>::CreateLambda([this]()
                            { return FText::FromString(_Stats.HasWorld ? (_Stats.ParallelPhysics ? TEXT("Yes") : TEXT("No")) : TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("Physics Threads:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.ThreadCount) : FText::FromString(TEXT("--")); })) ]

                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, ck_jolt_debugger::Pad_S)
                        [ MakeSectionHeader(TEXT("Rigid Bodies")) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("JoltBody Entities:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumBodies) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("Dynamic:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumDynamic) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("Kinematic:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumKinematic) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("Static:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumStatic) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("Awake:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumAwake) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("Asleep:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumAsleep) : FText::FromString(TEXT("--")); })) ]

                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, ck_jolt_debugger::Pad_S)
                        [ MakeSectionHeader(TEXT("Characters")) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("JoltCharacter Entities:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumCharacters) : FText::FromString(TEXT("--")); })) ]

                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, ck_jolt_debugger::Pad_S)
                        [ MakeSectionHeader(TEXT("Static World")) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("JoltStaticActor Entities:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumStaticActors) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
                        [ MakeStatRow(TEXT("Static Bodies:"), TAttribute<FText>::CreateLambda([this]()
                            { return _Stats.HasWorld ? FText::AsNumber(_Stats.NumStaticBodies) : FText::FromString(TEXT("--")); })) ]
                    + SScrollBox::Slot().Padding(ck_jolt_debugger::Pad_M, 0.0f)
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
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

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
    return SNew(STextBlock)
        .Font(ck_jolt_debugger::Bold(10))
        .ColorAndOpacity(ck_jolt_debugger::Text_Highlight)
        .Text(FText::FromString(InText));
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
                .Font(ck_jolt_debugger::Normal())
                .ColorAndOpacity(ck_jolt_debugger::Text_Secondary)
                .Text(FText::FromString(InLabel))
            ]

        + SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font(ck_jolt_debugger::Mono())
                .ColorAndOpacity(ck_jolt_debugger::Text_Primary)
                .Text(InValue)
            ];
}

// --------------------------------------------------------------------------------------------------------------------
