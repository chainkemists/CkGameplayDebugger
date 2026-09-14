#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"

/** Typed retained page strip. Ordered keys are topology; label/count/warning fields remain live. */
class CKDEBUGGERCOMMON_API FCkDebug_UiTabs final
{
public:
    static auto Register(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult;
};
