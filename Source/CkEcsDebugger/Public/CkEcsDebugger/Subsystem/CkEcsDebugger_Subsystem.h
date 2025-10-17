#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Subsystems/GameWorldSubsytem/CkGameWorldSubsystem.h"

#include "CkEcsDebugger_Subsystem.generated.h"

class FCkDebuggerWindow_Main;
class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;

UCLASS(DisplayName = "CkSubsystem_EcsDebuggerSlate")
class CKECSDEBUGGER_API UCk_EcsDebugger_Subsystem_UE : public UCk_Game_WorldSubsystem_Base_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_EcsDebugger_Subsystem_UE);

    auto Initialize(FSubsystemCollectionBase& Collection) -> void override;
    auto Deinitialize() -> void override;

protected:
    auto OnWorldBeginPlay(UWorld& InWorld) -> void override;

public:
    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;

    auto Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>;
    auto Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>;

private:
    TSharedPtr<FCkDebuggerWindow_Main> DebuggerWindow;
};