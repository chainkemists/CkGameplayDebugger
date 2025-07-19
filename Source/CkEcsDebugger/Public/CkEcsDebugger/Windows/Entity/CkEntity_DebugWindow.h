#pragma once

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_EntityBasics_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

protected:
    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;

private:
    static auto
    Get_SectionHeaderColor() -> ImU32;

    static auto
    Get_ValueColor_Entity() -> ImU32;

    static auto
    Get_ValueColor_Vector() -> ImU32;

    static auto
    Get_ValueColor_Number() -> ImU32;

    static auto
    Get_ValueColor_Enum() -> ImU32;

    static auto
    Get_ValueColor_Unknown() -> ImU32;

    static auto
    Get_ValueColor_None() -> ImU32;

    static auto
    Request_RenderTableRow_Entity(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_Vector(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_Number(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_Enum(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_Unknown(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_None(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderTableRow_Default(const char* Label, const FString& Value) -> void;

    static auto
    Request_RenderEntityInformation(const FCk_Handle& SelectionEntity) -> void;

    static auto
    Request_RenderTransformSection(const FCk_Handle& SelectionEntity) -> void;

    static auto
    Request_RenderNetworkSection(const FCk_Handle& SelectionEntity) -> void;

    static auto
    Request_RenderRelationshipsSection(const FCk_Handle& SelectionEntity) -> void;
};

// --------------------------------------------------------------------------------------------------------------------