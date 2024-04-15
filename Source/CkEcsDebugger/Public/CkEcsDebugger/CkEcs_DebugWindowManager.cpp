#include "CkEcs_DebugWindowManager.h"

#include "CkEcsDebugger/Attribute/CkAttribute_DebugWindow.h"
#include "CkEcsDebugger/Entity/CkEntity_DebugWindow.h"
#include "CkEcsDebugger/Timer/CkTimer_DebugWindow.h"

#include "CogEngineWindow_CollisionTester.h"
#include "CogEngineWindow_CollisionViewer.h"
#include "CogEngineWindow_CommandBindings.h"
#include "CogEngineWindow_DebugSettings.h"
#include "CogEngineWindow_ImGui.h"
#include "CogEngineWindow_Selection.h"

#include <GameFramework/Character.h>
#include <GameFramework/GameModeBase.h>
#include <GameFramework/GameStateBase.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    ACk_Ecs_DebugWindowManager::
    BeginPlay()
    -> void
{
    Super::BeginPlay();

#if ENABLE_COG
    _CogWindowManager = NewObject<UCogWindowManager>(this);
    _CogWindowManagerRef = _CogWindowManager;

    // Add a custom window
    _CogWindowManager->AddWindow<FCk_EntityBasics_DebugWindow>("Ck.Entity");
    _CogWindowManager->AddWindow<FCk_FloatAttribute_DebugWindow>("Ck.Attribute");
    _CogWindowManager->AddWindow<FCk_Timer_DebugWindow>("Ck.Timer");

    _CogWindowManager->AddWindow<FCogEngineWindow_CollisionTester>("Engine.Collision Tester");
    _CogWindowManager->AddWindow<FCogEngineWindow_CollisionViewer>("Engine.Collision Viewer");
    _CogWindowManager->AddWindow<FCogEngineWindow_CommandBindings>("Engine.Command Bindings");
    _CogWindowManager->AddWindow<FCogEngineWindow_DebugSettings>("Engine.Debug Settings");
    _CogWindowManager->AddWindow<FCogEngineWindow_ImGui>("Engine.ImGui");

    auto* SelectionWindow = _CogWindowManager->AddWindow<FCogEngineWindow_Selection>("Engine.Selection");
    SelectionWindow->SetActorClasses({ ACharacter::StaticClass(), AActor::StaticClass(), AGameModeBase::StaticClass(), AGameStateBase::StaticClass() });
    SelectionWindow->SetTraceType(UEngineTypes::ConvertToTraceType(ECC_Pawn));
#endif
}

auto
    ACk_Ecs_DebugWindowManager::
    Tick(
        float DeltaSeconds)
    -> void
{
    Super::Tick(DeltaSeconds);

#if ENABLE_COG
    _CogWindowManager->Tick(DeltaSeconds);
#endif //ENABLE_COG
}

// --------------------------------------------------------------------------------------------------------------------
