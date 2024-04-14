#include "CkCogWindow_EntityBasicInfo.h"

#include "CogWindowManager.h"

#include "CogEngineWindow_CollisionTester.h"
#include "CogEngineWindow_CollisionViewer.h"
#include "CogEngineWindow_CommandBindings.h"
#include "CogEngineWindow_DebugSettings.h"
#include "CogEngineWindow_ImGui.h"
#include "CogEngineWindow_Selection.h"

#include "CkCore/Actor/CkActor_Utils.h"

#include "CkEcs/Entity/CkEntity_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"

//--------------------------------------------------------------------------------------------------------------------------
void FCkCogWindow_EntityBasicInfo::Initialize()
{
    Super::Initialize();
}

//--------------------------------------------------------------------------------------------------------------------------
void FCkCogWindow_EntityBasicInfo::RenderHelp()
{
    ImGui::Text("A window part of the sample project.");
}

//--------------------------------------------------------------------------------------------------------------------------
void FCkCogWindow_EntityBasicInfo::RenderContent()
{
    Super::RenderContent();

    if (ck::Is_NOT_Valid(GetSelection()))
    { return; }

    if (NOT UCk_Utils_OwningActor_UE::Get_IsActorEcsReady(GetSelection()))
    {
        ImGui::Text("ACTOR NOT ECS READY");
        return;
    }

    auto Entity = UCk_Utils_OwningActor_UE::Get_ActorEntityHandle(GetSelection());

    if (ck::Is_NOT_Valid(Entity))
    { return; }

    ImGui::Text("EntityID %d", Entity.Get_Entity().Get_ID());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ACkCogWindowManager::
    BeginPlay()
    -> void
{
    Super::BeginPlay();

#if ENABLE_COG
    _CogWindowManager = NewObject<UCogWindowManager>(this);
    _CogWindowManagerRef = _CogWindowManager;

    // Add a custom window
    _CogWindowManager->AddWindow<FCkCogWindow_EntityBasicInfo>("Ck.Entity");

    _CogWindowManager->AddWindow<FCogEngineWindow_CollisionTester>("Engine.Collision Tester");
    _CogWindowManager->AddWindow<FCogEngineWindow_CollisionViewer>("Engine.Collision Viewer");
    _CogWindowManager->AddWindow<FCogEngineWindow_CommandBindings>("Engine.Command Bindings");
    _CogWindowManager->AddWindow<FCogEngineWindow_DebugSettings>("Engine.Debug Settings");
    _CogWindowManager->AddWindow<FCogEngineWindow_ImGui>("Engine.ImGui");

    auto* SelectionWindow = _CogWindowManager->AddWindow<FCogEngineWindow_Selection>("Engine.Selection");
    SelectionWindow->SetActorClasses({ACharacter::StaticClass(), AActor::StaticClass(), AGameModeBase::StaticClass(), AGameStateBase::StaticClass() });
    SelectionWindow->SetTraceType(UEngineTypes::ConvertToTraceType(ECC_Pawn));
#endif
}

auto
    ACkCogWindowManager::
    Tick(
        float DeltaSeconds)
    -> void
{
    Super::Tick(DeltaSeconds);

#if ENABLE_COG
    _CogWindowManager->Tick(DeltaSeconds);
#endif //ENABLE_COG
}
