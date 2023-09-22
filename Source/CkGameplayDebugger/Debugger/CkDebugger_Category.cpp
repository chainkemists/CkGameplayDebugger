#include "CkDebugger_Category.h"

#include "CkGameplayDebugger/Engine/CkDebugger_GameplayDebuggerCategoryReplicator.h"
#include "CkGameplayDebugger/Engine/CkDebugger_GameplayDebuggerTypes.h"

#if WITH_GAMEPLAY_DEBUGGER

#include <EngineUtils.h>
#include <Engine/Engine.h>
#include <Engine/Canvas.h>

#endif

// --------------------------------------------------------------------------------------------------------------------

FCk_Payload_GameplayDebugger_OnCollectData::
    FCk_Payload_GameplayDebugger_OnCollectData(
        APlayerController* InOwnerPC,
        AActor* InDebugActor,
        AGameplayDebuggerCategoryReplicator* InReplicator)
    : _OwnerPC(InOwnerPC)
    , _DebugActor(InDebugActor)
    , _Replicator(InReplicator)
{
}

// --------------------------------------------------------------------------------------------------------------------

FCk_Payload_GameplayDebugger_OnDrawData::
    FCk_Payload_GameplayDebugger_OnDrawData(
        APlayerController* InOwnerPC,
        FGameplayDebuggerCanvasContext* InCanvasContext,
        AGameplayDebuggerCategoryReplicator* InReplicator)
    : _OwnerPC(InOwnerPC)
    , _CanvasContext(InCanvasContext)
    , _Replicator(InReplicator)
{
}

#if WITH_GAMEPLAY_DEBUGGER

// --------------------------------------------------------------------------------------------------------------------

TSharedPtr<FCk_GameplayDebugger_Category> FCk_GameplayDebugger_Category::_This;

// --------------------------------------------------------------------------------------------------------------------


FCk_GameplayDebugger_Category::
    FCk_GameplayDebugger_Category()
{
    bShowOnlyWithDebugActor = false;
}

auto
    FCk_GameplayDebugger_Category::
    MakeInstance()
    -> TSharedRef<FGameplayDebuggerCategory>
{
    if (_This.IsValid())
    { return _This.ToSharedRef(); }

    _This = MakeShareable(new FCk_GameplayDebugger_Category());
    return _This.ToSharedRef();
}

auto
    FCk_GameplayDebugger_Category::
    Get_Instance()
    -> TSharedPtr<FCk_GameplayDebugger_Category>
{
    return _This;
}

auto
    FCk_GameplayDebugger_Category::
    CollectData(
        APlayerController* InOwnerPC,
        AActor* InDebugActor)
    -> void
{
    _OnCollectDataDelegate.ExecuteIfBound(FCk_Payload_GameplayDebugger_OnCollectData{InOwnerPC, InDebugActor, GetReplicator()});
}

auto
    FCk_GameplayDebugger_Category::
    DrawData(
        APlayerController* InOwnerPC,
        FGameplayDebuggerCanvasContext& CanvasContext)
    -> void
{
    // TODO: Pull this from user settings
    CanvasContext.FontRenderInfo.bEnableShadow = true;
    CanvasContext.Font = GEngine->GetTinyFont();

    _OnDrawDataDelegate.ExecuteIfBound(FCk_Payload_GameplayDebugger_OnDrawData{InOwnerPC, &CanvasContext, GetReplicator()});
}

auto
    FCk_GameplayDebugger_Category::
    OnGameplayDebuggerActivated()
    -> void
{
    std::ignore = _OnActivatedDelegate.ExecuteIfBound();
}

auto
    FCk_GameplayDebugger_Category::
    OnGameplayDebuggerDeactivated()
    -> void
{
    std::ignore = _OnDeactivatedDelegate.ExecuteIfBound();
}

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_GAMEPLAY_DEBUGGER