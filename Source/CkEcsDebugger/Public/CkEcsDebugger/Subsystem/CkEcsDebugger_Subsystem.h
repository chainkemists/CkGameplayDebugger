#pragma once

#include "CoreMinimal.h"
#include "CkCore/Subsystems/GameWorldSubsytem/CkGameWorldSubsystem.h"
#include "CkEcsDebugger_Subsystem.generated.h"

class SCkDebuggerWindow_Main;
class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class SDockTab;

UCLASS(DisplayName = "CkSubsystem_EcsDebuggerSlate")
class CKECSDEBUGGER_API UCkEcsDebugger_Subsystem : public UCk_Game_WorldSubsystem_Base_UE
{
    GENERATED_BODY()

public:
    auto Initialize(FSubsystemCollectionBase& InCollection) -> void override;
    auto Deinitialize() -> void override;
    auto OnWorldBeginPlay(UWorld& InWorld) -> void override;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

    auto Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>;
    auto Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>;

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto RegisterTabSpawner() -> void;
    auto UnregisterTabSpawner() -> void;

private:
    static auto ConsoleCommand_EcsDebugger(const TArray<FString>& InArgs, UWorld* InWorld) -> void;

    TSharedPtr<SCkDebuggerWindow_Main> DebuggerWindow;
    TSharedPtr<SDockTab> DebuggerTab;

    static const FName DebuggerTabName;
};