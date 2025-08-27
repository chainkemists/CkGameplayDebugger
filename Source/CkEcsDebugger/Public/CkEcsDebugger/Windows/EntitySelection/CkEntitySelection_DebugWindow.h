#pragma once

#include "CogCommonConfig.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"
#include "CkEntitySelection_DebugConfig.h"

// --------------------------------------------------------------------------------------------------------------------

struct ImGuiTextFilter;
using FCogWindowEntityContextMenuFunction = TFunction<void(const FCk_Handle& InEntity)>;

//--------------------------------------------------------------------------------------------------------------------------

class FCk_EntitySelection_DebugWindow : public FCk_Ecs_DebugWindow
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

    auto
    RenderTick(
        float InDeltaT) -> void override;

private:
    auto
    Get_CurrentTime() const -> FCk_Time;

    auto
    Get_EntitiesForList(bool InRequiresUpdate) const -> TArray<FCk_Handle>;

    auto
    BuildCaches(const TArray<FCk_Handle>& Entities) const -> void;

    auto
    UpdateFilterCache() const -> void;

    auto
    DisplayEntitiesList(bool InRequiresUpdate) -> bool;

    auto
    DisplayEntitiesListWithFilters(bool InRequiresUpdate) -> bool;

    auto
    RenderEntityTree(const TArray<FCk_Handle>& Entities) -> bool;

    auto
    RenderEntityNode(
        const FCk_Handle& Entity,
        const TArray<FCk_Handle>& SelectedEntities,
        const FCk_Handle& TransientEntity,
        bool OpenAllChildren) -> bool;

private:
    TObjectPtr<UCk_DebugWindowConfig_EntitySelection> Config;

    ImGuiTextFilter _Filter;

    mutable FCk_Time LastUpdateTime;
    mutable TArray<FCk_Handle> CachedSelectedEntities;

    // Performance optimization caches
    mutable TMap<FCk_Handle, TArray<FCk_Handle>> CachedParentToChildren;
    mutable TMap<FCk_Handle, FString> CachedDebugNames;
    mutable TMap<FCk_Handle, bool> CachedFilterResults;
    mutable TSet<FCk_Handle> CachedRootEntities;
};

// --------------------------------------------------------------------------------------------------------------------