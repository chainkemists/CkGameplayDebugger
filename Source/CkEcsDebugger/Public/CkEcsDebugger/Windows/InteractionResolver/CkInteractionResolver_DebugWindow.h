#pragma once

#include "CkInteractionResolver_DebugConfig.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

#include "CkInteraction/InteractionResolver/CkInteractionResolver_Fragment_Data.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_InteractionResolver_DebugWindow : public FCk_Ecs_DebugWindow
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
    RenderEntityInteractionResolverSection(
        const FCk_Handle& InEntity) -> void;

    auto
    RenderInteractionResolverInfo(
        const FCk_Handle_InteractionResolver& InResolver) -> void;

    auto
    RenderParametersSection(
        const FCk_Handle_InteractionResolver& InResolver) -> void;

    auto
    RenderActiveIntentsSection(
        const FCk_Handle_InteractionResolver& InResolver) -> void;

    auto
    RenderTargetsSection(
        const FCk_Handle_InteractionResolver& InResolver) -> void;

    auto
    RenderTargetDetails(
        const FCk_Handle_InteractTarget& InTarget) -> void;

    auto
    RenderInteractionResolverControls(
        const FCk_Handle_InteractionResolver& InResolver) -> void;

    // Helper functions for rendering table rows with colors
    static auto
    Request_RenderTableRow_Default(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_Intent(const char* Label, const FString& Value, bool InIsActive) -> void;

    static auto
    Request_RenderTableRow_Target(const char* Label, const FString& Value, bool InIsInBestTargets) -> void;

    static auto
    Request_RenderTableRow_Number(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_Enum(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_None(const char* Label, const FString& Value) -> void;

    // Color helpers
    static auto
    Get_ValueColor_Intent(bool InIsActive) -> ImU32;

    static auto
    Get_ValueColor_Target(bool InIsInBestTargets) -> ImU32;

    static auto
    Get_ValueColor_Number() -> ImU32;

    static auto
    Get_ValueColor_Enum() -> ImU32;

    static auto
    Get_ValueColor_None() -> ImU32;

private:
    ImGuiTextFilter _Filter;
    TObjectPtr<UCk_InteractionResolver_DebugWindowConfig> _Config;
};

// --------------------------------------------------------------------------------------------------------------------
