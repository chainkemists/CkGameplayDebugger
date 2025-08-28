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
    Get_EntitiesForList(bool InRequiresUpdate, bool InRefreshCache) const -> TArray<FCk_Handle>;

    auto
    ApplyFilterToEntitiesAndCache(const TArray<FCk_Handle>& Entities) const -> const TArray<FCk_Handle>&;

    auto
    RenderEntitiesWithFilters(bool InRequiresUpdate, bool InRefreshCache) -> void;

    auto
    RenderEntityTree(const TArray<FCk_Handle>& Entities) -> void;

    // Returns next index
    auto
    RenderEntityNode(const TArray<FCk_Handle>& Entities, int32 CurrentIndex, const TArray<FCk_Handle>& SelectedEntities, const FCk_Handle& TransientEntity, bool OpenAllChildren) -> int32;

private:
    static auto
    BaseSortingFunction(const FCk_Handle& InA, const FCk_Handle& InB, TObjectPtr<const UCk_DebugWindowConfig_EntitySelection> InConfig) -> bool;

    static auto
    HierarchicalSortingFunction(const FCk_Handle& InA, const FCk_Handle& InB, TObjectPtr<const UCk_DebugWindowConfig_EntitySelection> InConfig) -> bool;

private:
    TObjectPtr<UCk_DebugWindowConfig_EntitySelection> Config;

    ImGuiTextFilter _Filter;

    bool IsFirstUpdate = true;

    mutable FCk_Time LastUpdateTime;
    mutable TArray<FCk_Handle> CachedSelectedEntities;
    mutable TArray<FCk_Handle> CachedFilteredSelectedEntities;

    struct FCk_EntitySelectionComparisonFunc
    {
        FCk_EntitySelectionComparisonFunc() = default;
        FCk_EntitySelectionComparisonFunc(const UCk_DebugWindowConfig_EntitySelection* Config)
            : Config(Config)
        {}

        TObjectPtr<const UCk_DebugWindowConfig_EntitySelection> Config;
        auto operator()(const FCk_Handle& InA, const FCk_Handle& InB) const -> bool
        {
            if (ck::IsValid(Config))
            { return HierarchicalSortingFunction(InA, InB, Config); }
            return false;
        }
    };

    mutable std::set<FCk_Handle, FCk_EntitySelectionComparisonFunc> CachedSelectedEntitiesSet;
};

// --------------------------------------------------------------------------------------------------------------------