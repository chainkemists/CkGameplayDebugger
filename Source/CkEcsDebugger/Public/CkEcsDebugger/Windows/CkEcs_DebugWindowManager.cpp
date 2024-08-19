#include "CkEcs_DebugWindowManager.h"

#include "CkEcsDebugger/Windows/Ability/CkAbilities_DebugWindow.h"
#include "CkEcsDebugger/Windows/Ability/CkAbilityOwnerTags_DebugWindow.h"
#include "CkEcsDebugger/Windows/Attribute/CkAttribute_DebugWindow.h"
#include "CkEcsDebugger/Windows/Entity/CkEntity_DebugWindow.h"
#include "CkEcsDebugger/Windows/Timer/CkTimer_DebugWindow.h"
#include "CkEcsDebugger/Windows/EntityCollection/CkEntityCollection_DebugWindow.h"
#include "CkEcsDebugger/Windows/OverlapBody/CkOverlapBody_DebugWindow.h"
#include "CkEcsDebugger/Windows/World/CkWorld_DebugWindow.h"

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

ACk_Ecs_DebugWindowManager_UE::
    ACk_Ecs_DebugWindowManager_UE()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = false;
}

auto
    ACk_Ecs_DebugWindowManager_UE::
    BeginPlay()
    -> void
{
    Super::BeginPlay();

#if ENABLE_COG
    _CogWindowManager = NewObject<UCogWindowManager>(this);
    _CogWindowManagerRef = _CogWindowManager;

    CK_ENSURE_IF_NOT(ck::IsValid(_CogWindowManager), TEXT("[{}] failed to create Cog Windowe Manager!"))
    { return; }

    // Add a custom window
    _CogWindowManager->AddWindow<FCk_World_DebugWindow>("Ck.World");
    _CogWindowManager->AddWindow<FCk_EntityBasics_DebugWindow>("Ck.Entity");
    _CogWindowManager->AddWindow<FCk_EntityCollection_DebugWindow>("Ck.EntityCollection");
    _CogWindowManager->AddWindow<FCk_Timer_DebugWindow>("Ck.Timer");

    _CogWindowManager->AddWindow<FCk_ByteAttribute_DebugWindow>("Ck.Attribute.Byte");
    _CogWindowManager->AddWindow<FCk_FloatAttribute_DebugWindow>("Ck.Attribute.Float");
    _CogWindowManager->AddWindow<FCk_VectorAttribute_DebugWindow>("Ck.Attribute.Vector");

    _CogWindowManager->AddWindow<FCk_AbilityOwnerTags_DebugWindow>("Ck.AbilityOwnerTags");
    _CogWindowManager->AddWindow<FCk_Abilities_DebugWindow>("Ck.Abilities");

    _CogWindowManager->AddWindow<FCk_Marker_DebugWindow>("Ck.OverlapBody.Marker");
    _CogWindowManager->AddWindow<FCk_Sensor_DebugWindow>("Ck.OverlapBody.Sensor");

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
    ACk_Ecs_DebugWindowManager_UE::
    Tick(
        float DeltaSeconds)
    -> void
{
    Super::Tick(DeltaSeconds);

#if ENABLE_COG
    if (ck::IsValid(_CogWindowManager))
    {
        _CogWindowManager->Tick(DeltaSeconds);
    }
#endif //ENABLE_COG
}

// --------------------------------------------------------------------------------------------------------------------
