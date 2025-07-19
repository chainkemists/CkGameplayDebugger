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
    DisplayEntitiesList(bool InRequiresUpdate) -> bool;

    auto
    DisplayEntitiesListWithFilters(bool InRequiresUpdate) -> bool;

    auto
    RenderEntityTree(const TArray<FCk_Handle>& Entities, bool InRequiresUpdate) -> bool;

    auto
    RenderEntityNode(const FCk_Handle& Entity, const TArray<FCk_Handle>& SelectedEntities, const FCk_Handle& TransientEntity, bool OpenAllChildren) -> bool;

    auto
    GetEntityChildren(const FCk_Handle& Entity, const TArray<FCk_Handle>& AllEntities) -> TArray<FCk_Handle>;

private:
    TObjectPtr<UCk_DebugWindowConfig_EntitySelection> Config;

    ImGuiTextFilter _Filter;

    mutable FCk_Time LastUpdateTime;
    mutable TArray<FCk_Handle> CachedSelectedEntities;
    mutable bool CachedShouldRenderEntityTree = false;
};

// --------------------------------------------------------------------------------------------------------------------