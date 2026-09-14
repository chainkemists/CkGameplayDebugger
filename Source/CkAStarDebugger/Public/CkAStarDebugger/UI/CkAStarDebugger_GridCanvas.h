#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"

class SCkAStarDebugger_GridView;

class FCkAStarDebugger_GridCanvas final
{
public:
    static auto Register(
        FCkUiWidgetRegistry& InRegistry,
        TWeakPtr<SCkAStarDebugger_GridView> InGrid) -> FCkUiLoadResult;
};
