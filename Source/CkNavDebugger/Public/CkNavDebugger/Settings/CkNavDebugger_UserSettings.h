#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Format/CkFormat.h"

#include "CkSettings/UserSettings/CkUserSettings.h"

#include "CkNavDebugger_UserSettings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_NavDebugger_DrawMode : uint8
{
    None,       // No agents drawn
    Selected,   // Only the selected agent (FTag_NavDebugger_AgentSelected) is drawn
    All         // Every nav agent is drawn; the selected one uses the highlight color
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_NavDebugger_DrawMode);

// --------------------------------------------------------------------------------------------------------------------

// Per-user editor settings for the CK Navigation Debugger. All fields are
// backed by CVars (`Ck.NavDebugger.*`) so they can also be tweaked at runtime
// from the console. Persisted in EditorPerProjectUserSettings — never
// committed to source.
//
// Reads the active values via UCk_Utils_NavDebugger_Settings_UE accessors;
// the runtime processors and panel widgets all go through that BPFL so the
// settings class is the single source of truth.
UCLASS(meta = (DisplayName = "Nav Debugger"))
class CKNAVDEBUGGER_API UCk_NavDebugger_UserSettings_UE : public UCk_Plugin_UserSettings_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_NavDebugger_UserSettings_UE);

    auto GetCategoryName() const -> FName override { return TEXT("CkFoundation"); }
    auto GetSectionName()  const -> FName override { return TEXT("Nav Debugger"); }

public:
    friend class UCk_Utils_NavDebugger_Settings_UE;

private:
    // ----------------------------------------------------------------------------------------------------------------
    // Capsule overlay
    // ----------------------------------------------------------------------------------------------------------------

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
              Category = "Capsule",
              meta = (AllowPrivateAccess = true,
                     ConsoleVariable = "Ck.NavDebugger.AgentCapsuleMode",
                     ToolTip = "Which agents get a capsule visual: None, Selected, or All."))
    ECk_NavDebugger_DrawMode _AgentCapsuleMode = ECk_NavDebugger_DrawMode::All;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
              Category = "Capsule",
              meta = (AllowPrivateAccess = true,
                     ToolTip = "Color used for the capsule of unselected agents (or all agents when Mode=All and nothing's selected)."))
    FLinearColor _AgentCapsuleColor = FLinearColor(0.231f, 0.510f, 0.965f, 0.35f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
              Category = "Capsule",
              meta = (AllowPrivateAccess = true,
                     ToolTip = "Color used for the capsule of the agent currently selected in the debugger panel."))
    FLinearColor _AgentCapsuleColor_Selected = FLinearColor(1.0f, 0.55f, 0.1f, 0.55f);

    // ----------------------------------------------------------------------------------------------------------------
    // Path overlay
    // ----------------------------------------------------------------------------------------------------------------

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
              Category = "Path",
              meta = (AllowPrivateAccess = true,
                     ConsoleVariable = "Ck.NavDebugger.AgentPathMode",
                     ToolTip = "Which agents get path-line visuals: None, Selected, or All."))
    ECk_NavDebugger_DrawMode _AgentPathMode = ECk_NavDebugger_DrawMode::All;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
              Category = "Path",
              meta = (AllowPrivateAccess = true,
                     ToolTip = "Color of path lines for an agent whose last query is Ready."))
    FLinearColor _AgentPathColor_Ready = FLinearColor(0.133f, 0.773f, 0.369f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
              Category = "Path",
              meta = (AllowPrivateAccess = true,
                     ToolTip = "Color of path lines when the last query returned Partial."))
    FLinearColor _AgentPathColor_Partial = FLinearColor(0.961f, 0.620f, 0.043f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
              Category = "Path",
              meta = (AllowPrivateAccess = true,
                     ToolTip = "Color of path lines when the last query Failed (the line still shows the previous waypoints)."))
    FLinearColor _AgentPathColor_Failed = FLinearColor(0.937f, 0.267f, 0.267f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
              Category = "Path",
              meta = (AllowPrivateAccess = true,
                     ToolTip = "Highlight color used for the selected agent's path, regardless of status. Lets you visually pick out the path you're inspecting from a busy crowd."))
    FLinearColor _AgentPathColor_Selected = FLinearColor(1.0f, 0.55f, 0.1f);

    // ----------------------------------------------------------------------------------------------------------------
    // Misc overlays (kept for parity with the previous CVar set)
    // ----------------------------------------------------------------------------------------------------------------

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
              Category = "Misc",
              meta = (AllowPrivateAccess = true,
                     ConsoleVariable = "Ck.NavDebugger.DrawProjections",
                     ToolTip = "Draw vertical lines from agent location to its projected-onto-navmesh location. Makes 'agent floating above navmesh' setup mistakes immediately obvious."))
    bool _DrawProjections = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly,
              Category = "Misc",
              meta = (AllowPrivateAccess = true,
                     ConsoleVariable = "Ck.NavDebugger.DrawNavmeshBounds",
                     ToolTip = "Draw the navmesh AABB as a wireframe box."))
    bool _DrawNavmeshBounds = false;

public:
    CK_PROPERTY_GET(_AgentCapsuleMode);
    CK_PROPERTY_GET(_AgentCapsuleColor);
    CK_PROPERTY_GET(_AgentCapsuleColor_Selected);
    CK_PROPERTY_GET(_AgentPathMode);
    CK_PROPERTY_GET(_AgentPathColor_Ready);
    CK_PROPERTY_GET(_AgentPathColor_Partial);
    CK_PROPERTY_GET(_AgentPathColor_Failed);
    CK_PROPERTY_GET(_AgentPathColor_Selected);
    CK_PROPERTY_GET(_DrawProjections);
    CK_PROPERTY_GET(_DrawNavmeshBounds);
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKNAVDEBUGGER_API UCk_Utils_NavDebugger_Settings_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_NavDebugger_Settings_UE);

public:
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|NavDebugger|Settings")
    static ECk_NavDebugger_DrawMode Get_AgentCapsuleMode();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|NavDebugger|Settings")
    static FLinearColor Get_AgentCapsuleColor();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|NavDebugger|Settings")
    static FLinearColor Get_AgentCapsuleColor_Selected();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|NavDebugger|Settings")
    static ECk_NavDebugger_DrawMode Get_AgentPathMode();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|NavDebugger|Settings")
    static FLinearColor Get_AgentPathColor_Ready();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|NavDebugger|Settings")
    static FLinearColor Get_AgentPathColor_Partial();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|NavDebugger|Settings")
    static FLinearColor Get_AgentPathColor_Failed();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|NavDebugger|Settings")
    static FLinearColor Get_AgentPathColor_Selected();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|NavDebugger|Settings")
    static bool Get_DrawProjections();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|NavDebugger|Settings")
    static bool Get_DrawNavmeshBounds();

    // ----------------------------------------------------------------------------------------------------------------
    // Setters — write to the CDO and persist via SaveConfig so the next editor
    // session picks up the value. The matching CVar (where one exists) is also
    // refreshed so any console listeners stay in sync.
    // ----------------------------------------------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|NavDebugger|Settings")
    static void Set_AgentCapsuleMode(ECk_NavDebugger_DrawMode InNewMode);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|NavDebugger|Settings")
    static void Set_AgentPathMode(ECk_NavDebugger_DrawMode InNewMode);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|NavDebugger|Settings")
    static void Set_DrawProjections(bool InNewValue);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|NavDebugger|Settings")
    static void Set_DrawNavmeshBounds(bool InNewValue);
};
