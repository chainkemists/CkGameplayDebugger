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
    // Hierarchical sorting cache structure
    struct FHierarchicalSortKey
    {
        TArray<FCk_Handle> ParentChain;  // Root to entity path
        FString CachedName;               // For alphabetical sorting
        int32 Depth;                      // Hierarchy depth
        uint32 EntityNumber;              // Cached for number sorting
        uint32 VersionNumber;             // Cached for number sorting
    };

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

    // Unified sorting functions
    auto
    CompareEntities(
        const FCk_Handle& InA,
        const FCk_Handle& InB,
        bool bUseHierarchicalSort) const -> bool;

    auto
    CompareEntitiesDirect(
        const FCk_Handle& InA,
        const FCk_Handle& InB,
        const FHierarchicalSortKey* OptionalKeyA = nullptr,
        const FHierarchicalSortKey* OptionalKeyB = nullptr) const -> bool;

    auto
    SortEntitiesArray(
        TArray<FCk_Handle>& InOutEntities,
        bool bUseHierarchicalSort) const -> void;

private:
    TObjectPtr<UCk_DebugWindowConfig_EntitySelection> Config;

    ImGuiTextFilter _Filter;

    mutable FCk_Time LastUpdateTime;
    mutable TArray<FCk_Handle> CachedSelectedEntities;

    // Performance optimization caches
    mutable TMap<FCk_Handle, TArray<FCk_Handle>> CachedParentToChildren;
    mutable TMap<FCk_Handle, FString> CachedDebugNames;
    mutable TMap<FCk_Handle, bool> CachedFilterResults;
    mutable TSet<FCk_Handle> CachedFilterDirectMatches;  // Entities that directly match the filter
    mutable TSet<FCk_Handle> CachedRootEntities;
    
    // Hierarchical sorting cache
    mutable TMap<FCk_Handle, FHierarchicalSortKey> CachedSortKeys;
    
    // Track if caches need rebuild
    mutable ECkDebugger_EntitiesListFragmentFilteringTypes LastFragmentFilter = ECkDebugger_EntitiesListFragmentFilteringTypes::None;
    mutable ECkDebugger_EntitiesListDisplayPolicy LastDisplayPolicy = ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy;
    mutable ECkDebugger_EntitiesListSortingPolicy LastSortingPolicy = ECkDebugger_EntitiesListSortingPolicy::Alphabetical;
};

// --------------------------------------------------------------------------------------------------------------------