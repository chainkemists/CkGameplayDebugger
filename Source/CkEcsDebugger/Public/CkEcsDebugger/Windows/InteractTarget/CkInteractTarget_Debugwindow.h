#pragma once

#include "CkInteractTarget_DebugConfig.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

#include "CkInteraction/InteractTarget/CkInteractTarget_Fragment_Data.h"
#include "CkInteraction/Interaction/CkInteraction_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_InteractTarget_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

protected:
    auto
    ResetConfig() -> void override;

    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;

    auto
    RenderMenu() -> void;

private:
    auto
    RenderEntityInteractTargetSection(
        const FCk_Handle& InEntity) -> void;

    auto
    RenderInteractTargetInfo(
        const FCk_Handle_InteractTarget& InTarget) -> void;

    auto
    RenderParametersSection(
        const FCk_Handle_InteractTarget& InTarget) -> void;

    auto
    RenderCurrentInteractionsSection(
        const FCk_Handle_InteractTarget& InTarget) -> void;

    auto
    RenderInteractionDetails(
        const FCk_Handle_Interaction& InInteraction) -> void;

    auto
    RenderInteractTargetControls(
        const FCk_Handle_InteractTarget& InTarget) -> void;

    // Helper functions for rendering table rows with colors
    static auto
    Request_RenderTableRow_Default(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_Target(const char* Label, const FString& Value, bool InIsEnabled) -> void;

    static auto
    Request_RenderTableRow_Interaction(const char* Label, const FString& Value, bool InIsActive) -> void;

    static auto
    Request_RenderTableRow_Number(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_Enum(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_None(const char* Label, const FString& Value) -> void;

    // Color helpers
    static auto
    Get_ValueColor_Target(bool InIsEnabled) -> ImU32;

    static auto
    Get_ValueColor_Interaction(bool InIsActive) -> ImU32;

    static auto
    Get_ValueColor_Number() -> ImU32;

    static auto
    Get_ValueColor_Enum() -> ImU32;

    static auto
    Get_ValueColor_None() -> ImU32;

private:
    ImGuiTextFilter _Filter;
    TObjectPtr<UCk_InteractTarget_DebugWindowConfig> _Config;
};

// --------------------------------------------------------------------------------------------------------------------