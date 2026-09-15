#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"
#include "CkEntityDebugOverlay/Subsystem/CkDebugOverlay_Subsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS && WITH_CK_DEBUG_OVERLAY

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealEdGlobals.h"

namespace ck_debug_overlay_subsystem_lifecycle_spec
{
    constexpr auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
    constexpr auto ReadyTimeoutSeconds = 30.0;

    struct FPlaySettingsSnapshot
    {
        int32 NumClients = 1;
        EPlayNetMode NetMode = EPlayNetMode::PIE_Standalone;
        bool RunUnderOneProcess = true;
        bool LaunchSeparateServer = false;
    };

    struct FCVarSnapshot
    {
        FString Value;
        EConsoleVariableFlags Flags = ECVF_Default;
    };

    struct FState
    {
        FPlaySettingsSnapshot PlaySettings;
        FCVarSnapshot MasterCVar;
        TStrongObjectPtr<UCk_DebugOverlay_Subsystem> Subsystem;
        TSharedPtr<SCkDebugOverlay_Root> HeldRoot;
        FCk_DebugOverlay_Subsystem_TestState Activated;
        FCk_DebugOverlay_Subsystem_TestState Deactivated;
        FCk_DebugOverlay_Subsystem_TestState Deinitialized;
        bool HasSnapshot = false;
        bool ActivatedThroughViewport = false;
        bool SelectionPanelOpened = false;
        bool HeldRootReleased = false;
        bool DeactivatedCleanly = false;
        bool DeinitializedCleanly = false;
        bool PieEnded = false;
    };

    auto GetPieWorld() -> UWorld*
    {
        if (GEngine == nullptr) { return nullptr; }
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
            { return Context.World(); }
        }
        return nullptr;
    }

    auto GetPlayer(UWorld* InWorld) -> ULocalPlayer*
    {
        UGameInstance* Instance = IsValid(InWorld) ? InWorld->GetGameInstance() : nullptr;
        return IsValid(Instance) ? Instance->GetLocalPlayerByIndex(0) : nullptr;
    }

    auto IsEmpty(const FCk_DebugOverlay_Subsystem_TestState& InState) -> bool
    {
        return !InState.HasRootWidget && !InState.HasAttachedViewport && !InState.HasTicker && !InState.HasHistory &&
            !InState.HasInputProcessor && !InState.HasSelectionUi && !InState.HasSelectionPanel && !InState.HasSessionInvalidationCallback &&
            !InState.HasWorldInvalidationCallback && !InState.HasOwnedConsoleObjects;
    }

    class FStartPie final : public IAutomationLatentCommand
    {
    public:
        explicit FStartPie(const TSharedRef<FState>& InState) : State(InState) {}

        virtual bool Update() override
        {
            if (GUnrealEd == nullptr) { return true; }
            ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
            if (Settings == nullptr) { return true; }

            Settings->GetPlayNumberOfClients(State->PlaySettings.NumClients);
            Settings->GetPlayNetMode(State->PlaySettings.NetMode);
            Settings->GetRunUnderOneProcess(State->PlaySettings.RunUnderOneProcess);
            State->PlaySettings.LaunchSeparateServer = Settings->bLaunchSeparateServer;
            Settings->SetPlayNumberOfClients(1);
            Settings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
            Settings->SetRunUnderOneProcess(true);
            Settings->bLaunchSeparateServer = false;

            FRequestPlaySessionParams Params;
            Params.WorldType = EPlaySessionWorldType::PlayInEditor;
            GUnrealEd->RequestPlaySession(Params);
            return true;
        }

    private:
        TSharedRef<FState> State;
    };

    class FWaitForPie final : public IAutomationLatentCommand
    {
    public:
        explicit FWaitForPie(const TSharedRef<FState>& InState) : State(InState) {}

        virtual bool Update() override
        {
            if (StartTime < 0.0) { StartTime = FPlatformTime::Seconds(); }
            UWorld* World = GetPieWorld();
            ULocalPlayer* Player = GetPlayer(World);
            if (IsValid(World) && World->HasBegunPlay() && IsValid(Player) &&
                IsValid(Player->GetSubsystem<UCk_DebugOverlay_Subsystem>()))
            { return true; }
            return FPlatformTime::Seconds() - StartTime >= ReadyTimeoutSeconds;
        }

    private:
        TSharedRef<FState> State;
        double StartTime = -1.0;
    };

    class FExerciseLifecycle final : public IAutomationLatentCommand
    {
    public:
        explicit FExerciseLifecycle(const TSharedRef<FState>& InState) : State(InState) {}

        virtual bool Update() override
        {
            ULocalPlayer* Player = GetPlayer(GetPieWorld());
            UCk_DebugOverlay_Subsystem* Subsystem = IsValid(Player)
                ? Player->GetSubsystem<UCk_DebugOverlay_Subsystem>() : nullptr;
            IConsoleVariable* Master = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.DebugOverlay"));
            if (!IsValid(Subsystem) || Master == nullptr || !FSlateApplication::IsInitialized()) { return true; }

            State->Subsystem.Reset(Subsystem);
            State->MasterCVar = {Master->GetString(), static_cast<EConsoleVariableFlags>(Master->GetFlags())};
            State->HasSnapshot = true;
            Master->Set(1, ECVF_SetByConsole);
            FSlateApplication::Get().PumpMessages();
            FSlateApplication::Get().Tick();

            State->HeldRoot = Subsystem->Get_TestRootWidget();
            if (GEngine != nullptr) { GEngine->Exec(GetPieWorld(), TEXT("ck.DebugOverlay.Settings")); }
            FSlateApplication::Get().PumpMessages();
            FSlateApplication::Get().Tick();
            State->Activated = Subsystem->Get_TestState();
            State->ActivatedThroughViewport = State->HeldRoot.IsValid() && State->Activated.HasRootWidget &&
                State->Activated.HasAttachedViewport && State->Activated.HasTicker && State->Activated.HasHistory &&
                State->Activated.HasInputProcessor && State->Activated.HasSelectionUi &&
                State->Activated.HasSessionInvalidationCallback && State->Activated.HasWorldInvalidationCallback;
            State->SelectionPanelOpened = State->Activated.HasSelectionPanel;

            Master->Set(0, ECVF_SetByConsole);
            FSlateApplication::Get().PumpMessages();
            FSlateApplication::Get().Tick();
            State->Deactivated = Subsystem->Get_TestState();
            State->HeldRootReleased = State->HeldRoot.IsValid() && !State->HeldRoot->Get_AuthoredView().IsValid() &&
                !State->HeldRoot->Get_WorldTagPort().IsValid() && !State->HeldRoot->Get_CardPort().IsValid() &&
                State->HeldRoot->Get_AdmittedWorldTagKeys().IsEmpty();
            State->DeactivatedCleanly = !State->Deactivated.HasRootWidget && !State->Deactivated.HasAttachedViewport &&
                !State->Deactivated.HasTicker && !State->Deactivated.HasHistory && !State->Deactivated.HasSelectionUi &&
                !State->Deactivated.HasSelectionPanel &&
                // The global preprocessor and lifecycle delegates deliberately survive cvar-off so input can reactivate.
                State->Deactivated.HasInputProcessor && State->Deactivated.HasSessionInvalidationCallback &&
                State->Deactivated.HasWorldInvalidationCallback;

            return true;
        }

    private:
        TSharedRef<FState> State;
    };

    class FAssertLifecycle final : public IAutomationLatentCommand
    {
    public:
        FAssertLifecycle(FAutomationTestBase* InTest, const TSharedRef<FState>& InState) : Test(InTest), State(InState) {}

        virtual bool Update() override
        {
            if (Test == nullptr) { return true; }
            Test->TestTrue(TEXT("real LocalPlayer subsystem activated and attached both overlay surfaces to its viewport"),
                State->ActivatedThroughViewport);
            Test->TestTrue(TEXT("production settings command attaches a selection panel that cvar-off later owns and removes"),
                State->SelectionPanelOpened);
            Test->TestTrue(TEXT("held root is made inert before the production viewport detaches it"), State->HeldRootReleased);
            Test->TestTrue(TEXT("cvar deactivation clears active ticker, viewport, history, and selection UI while retaining reactivation routing"),
                State->DeactivatedCleanly);
            Test->TestTrue(TEXT("ending PIE drives the engine-owned LocalPlayer subsystem through deinitialization"), State->PieEnded);
            Test->TestTrue(TEXT("natural deinitialization clears input preprocessing, lifecycle callbacks, and owned console objects"),
                State->DeinitializedCleanly);
            return true;
        }

    private:
        FAutomationTestBase* Test = nullptr;
        TSharedRef<FState> State;
    };

    class FEndPie final : public IAutomationLatentCommand
    {
    public:
        explicit FEndPie(const TSharedRef<FState>& InState) : State(InState) {}

        virtual bool Update() override
        {
            if (StartTime < 0.0) { StartTime = FPlatformTime::Seconds(); }
            if (!Requested)
            {
                if (GUnrealEd != nullptr) { GUnrealEd->RequestEndPlayMap(); }
                Requested = true;
                return false;
            }

            if (UCk_DebugOverlay_Subsystem* Subsystem = State->Subsystem.Get(); IsValid(Subsystem))
            {
                const auto Current = Subsystem->Get_TestState();
                if (IsEmpty(Current))
                {
                    State->Deinitialized = Current;
                    State->DeinitializedCleanly = true;
                }
            }

            if (GetPieWorld() != nullptr && FPlatformTime::Seconds() - StartTime < ReadyTimeoutSeconds)
            { return false; }

            State->PieEnded = GetPieWorld() == nullptr;

            if (ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>())
            {
                Settings->SetPlayNumberOfClients(State->PlaySettings.NumClients);
                Settings->SetPlayNetMode(State->PlaySettings.NetMode);
                Settings->SetRunUnderOneProcess(State->PlaySettings.RunUnderOneProcess);
                Settings->bLaunchSeparateServer = State->PlaySettings.LaunchSeparateServer;
            }
            if (State->HasSnapshot)
            {
                if (IConsoleVariable* Master = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.DebugOverlay")))
                {
                    Master->Set(*State->MasterCVar.Value, ECVF_SetByConsole);
                    Master->SetFlags(State->MasterCVar.Flags);
                }
            }
            return true;
        }

    private:
        TSharedRef<FState> State;
        bool Requested = false;
        double StartTime = -1.0;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugOverlay_SubsystemLifecycle_Test,
    "Ck.DebugOverlay.Subsystem.Lifecycle",
    ck_debug_overlay_subsystem_lifecycle_spec::TestFlags)

bool FCkDebugOverlay_SubsystemLifecycle_Test::RunTest(const FString&)
{
    using namespace ck_debug_overlay_subsystem_lifecycle_spec;

    const TSharedRef<FState> State = MakeShared<FState>();
    ADD_LATENT_AUTOMATION_COMMAND(FStartPie(State));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPie(State));
    ADD_LATENT_AUTOMATION_COMMAND(FExerciseLifecycle(State));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPie(State));
    ADD_LATENT_AUTOMATION_COMMAND(FAssertLifecycle(this, State));
    return true;
}

#endif
