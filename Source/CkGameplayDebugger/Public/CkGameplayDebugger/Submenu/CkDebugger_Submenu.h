#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkGameplayDebugger/CkDebugger_Common.h"
#include "CkGameplayDebugger/Submenu/CkDebugger_Submenu_Data.h"

#include <InputCoreTypes.h>

#include "CkDebugger_Submenu.generated.h"

#define CK_GAMEPLAY_DEBUGGER_INVALID_SUBMENU_NAME TEXT("[NAMELESS SUBMENU]")

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Abstract, Blueprintable)
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_DebugSubmenu_UE : public UObject
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameplayDebugger_DebugSubmenu_UE);

public:
    auto ActivateSubmenu() -> void;
    auto DeactivateSubmenu() -> void;
    auto DrawData(const FCk_GameplayDebugger_DrawSubmenuData_Params& InParams) -> void;
    auto ToggleShowState() -> ECk_GameplayDebugger_DebugSubmenu_ShowState;
    auto Get_HasValidSubmenuName() const -> bool;

protected:
    UFUNCTION(BlueprintNativeEvent,
              Category = "Ck|GameplayDebugger|Submenu",
              meta     = (DisplayName = "ActivateSubmenu"))
    void
    DoActivateSubmenu();

    UFUNCTION(BlueprintNativeEvent,
              Category = "Ck|GameplayDebugger|Submenu",
              meta     = (DisplayName = "DeactivateSubmenu"))
    void
    DoDeactivateSubmenu();

    UFUNCTION(BlueprintNativeEvent,
              Category = "Ck|GameplayDebugger|Submenu",
              meta     = (DisplayName = "DrawData"))
    FCk_GameplayDebugger_DrawSubmenuData_Result
    DoDrawData(
        const FCk_GameplayDebugger_DrawSubmenuData_Params& InParams);

private:
    // Name of the submenu displayed on screen when it is active
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger|Submenu|Params", meta = (AllowPrivateAccess = true))
    FName _MenuName = CK_GAMEPLAY_DEBUGGER_INVALID_SUBMENU_NAME;

    // "Key to press to show/hide the submenu information on screen
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger|Submenu|Params", meta = (AllowPrivateAccess = true))
    FKey  _KeyToShowMenu;

    // Font to use when drawing the text. The value points to the font file set in engine settings
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger|Submenu|Params", meta = (AllowPrivateAccess = true))
    ECk_GameplayDebugger_DrawText_FontSize _TextFontSize = ECk_GameplayDebugger_DrawText_FontSize::Tiny;

    // Starting Show State
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger|Submenu|Params", meta = (AllowPrivateAccess = true))
    ECk_GameplayDebugger_DebugSubmenu_ShowState _ShowState = ECk_GameplayDebugger_DebugSubmenu_ShowState::Visible;

public:
    CK_PROPERTY_GET(_MenuName);
    CK_PROPERTY_GET(_KeyToShowMenu);
    CK_PROPERTY_GET(_TextFontSize);
    CK_PROPERTY_GET(_ShowState);
};

// --------------------------------------------------------------------------------------------------------------------
