#pragma once

#include "CkCore/IO/CkIO_Utils.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkGameplayDebugger/CkDebugger_Common.h"

#include "CkSettings/ProjectSettings/CkProjectSettings.h"
#include "CkSettings/UserSettings/CkUserSettings.h"

#include <CoreMinimal.h>

#include "CkDebugger_Settings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(meta = (DisplayName = "Gameplay Debugger"))
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_UserSettings_UE : public UCk_EditorPerProject_UserSettings_UE
{
    GENERATED_BODY()

private:
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Profile",
              meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA> _UserOverride_DebugProfile;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Display",
              meta = (AllowPrivateAccess = true, InlineEditConditionToggle))
    bool _Has_UserOverride_FontSize = false;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Display",
              meta = (AllowPrivateAccess = true, EditCondition = "_Has_UserOverride_FontSize"))
    ECk_Engine_TextFontSize _UserOverride_FontSize = ECk_Engine_TextFontSize::Tiny;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Display",
              meta = (AllowPrivateAccess = true))
    bool _DisplayTranslucentBackground = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Display",
              meta = (AllowPrivateAccess = true, EditCondition = "_DisplayTranslucentBackground"))
    FLinearColor _BackgroundColor = FLinearColor{0.0f, 0.0f, 0.0f, 0.3f};

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Display",
              meta = (AllowPrivateAccess = true, EditCondition = "_DisplayTranslucentBackground"))
    FCk_GameplayDebugger_BackgroundWidth _BackgroundWidth = FCk_GameplayDebugger_BackgroundWidth::Half;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Display",
              meta = (AllowPrivateAccess = true))
    bool _EnableTextDropShadow = false;

public:
    CK_PROPERTY_GET(_UserOverride_DebugProfile);
    CK_PROPERTY_GET(_Has_UserOverride_FontSize);
    CK_PROPERTY_GET(_UserOverride_FontSize);
    CK_PROPERTY_GET(_DisplayTranslucentBackground);
    CK_PROPERTY_GET(_BackgroundColor);
    CK_PROPERTY_GET(_BackgroundWidth);
    CK_PROPERTY_GET(_EnableTextDropShadow);
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(defaultconfig, meta = (DisplayName = "Gameplay Debugger"))
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_ProjectSettings_UE : public UCk_Engine_ProjectSettings_UE
{
    GENERATED_BODY()

private:
    // Default Gameplay Debugger Debug Profile to use if there is no override in the Editor Preferences
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Profile",
              meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA> _ProjectDefault_DebugProfile;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Display",
              meta = (AllowPrivateAccess = true))
    ECk_Engine_TextFontSize _ProjectDefault_FontSize = ECk_Engine_TextFontSize::Tiny;

public:
    CK_PROPERTY_GET(_ProjectDefault_DebugProfile);
    CK_PROPERTY_GET(_ProjectDefault_FontSize);
};

// --------------------------------------------------------------------------------------------------------------------

class CKGAMEPLAYDEBUGGER_API UCk_Utils_GameplayDebugger_UserSettings_UE
{
public:
    static auto Get_UserOverride_DebugProfile() -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>;
    static auto Get_UserOverride_FontSize() -> TOptional<ECk_Engine_TextFontSize>;
    static auto Get_DisplayTranslucentBackground() -> bool;
    static auto Get_BackgroundColor() -> FLinearColor;
    static auto Get_BackgroundWidth() -> FCk_GameplayDebugger_BackgroundWidth;
    static auto Get_EnableTextDropShadow() -> bool;
};

// --------------------------------------------------------------------------------------------------------------------

class CKGAMEPLAYDEBUGGER_API UCk_Utils_GameplayDebugger_ProjectSettings_UE
{
public:
    static auto Get_ProjectDefault_DebugProfile() -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>;
    static auto Get_ProjectDefault_FontSize() -> ECk_Engine_TextFontSize;
};

// --------------------------------------------------------------------------------------------------------------------
