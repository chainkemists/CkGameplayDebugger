#include "CkDebugger_Category.h"

#include "CkGameplayDebugger/Engine/CkDebugger_GameplayDebuggerCategoryReplicator.h"
#include "CkGameplayDebugger/Engine/CkDebugger_GameplayDebuggerTypes.h"
#include "CkGameplayDebugger/Settings/CkDebugger_Settings.h"

#if WITH_GAMEPLAY_DEBUGGER
#include <EngineUtils.h>
#include <Engine/Engine.h>
#include <Engine/Canvas.h>
#endif

// --------------------------------------------------------------------------------------------------------------------

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
        FGameplayDebuggerCanvasContext& InCanvasContext)
    -> void
{
    InCanvasContext.FontRenderInfo.bEnableShadow = true;

    const auto& fontSizeToUse = [&]() -> ECk_Engine_TextFontSize
    {
        const auto& userOverrideFont = UCk_Utils_GameplayDebugger_UserSettings_UE::Get_UserOverride_FontSize();

        if (ck::IsValid(userOverrideFont))
        { return *userOverrideFont; }

        return UCk_Utils_GameplayDebugger_ProjectSettings_UE::Get_ProjectDefault_FontSize();
    }();

    InCanvasContext.Font = UCk_Utils_IO_UE::Get_Engine_DefaultTextFont(fontSizeToUse);

    _OnDrawDataDelegate.ExecuteIfBound(FCk_Payload_GameplayDebugger_OnDrawData{InOwnerPC, &InCanvasContext, GetReplicator()});
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