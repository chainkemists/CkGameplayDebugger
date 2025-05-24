#pragma once

#include "CkEcs/Handle/CkHandle.h"

struct ImGuiTextFilter;

using FCogWindowEntityContextMenuFunction = TFunction<void(const FCk_Handle& InEntity)>;

class CKECSDEBUGGER_API FCkEcs_DebugWindowWidgets
{
public:
    static auto
    EntitiesList(
        const UWorld& World,
        bool OnlyRootEntities,
        const ImGuiTextFilter* Filter = nullptr,
        const FCogWindowEntityContextMenuFunction& ContextMenuFunction = nullptr) -> bool;

    static auto
    EntitiesListWithFilters(
        const UWorld& World,
        bool OnlyRootEntities,
        ImGuiTextFilter* Filter,
        const FCogWindowEntityContextMenuFunction& ContextMenuFunction = nullptr) -> bool;
};