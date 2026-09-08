#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"

// ====================================================================================================================
// Immutable authored-widget definitions shared by debugger surfaces. The registry has no debugger state: callers bind
// their own attributes when constructing FCkUiView.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API FCkDebug_UiRegistry final
{
public:
    /** Creates the complete immutable registry atomically. OutSnapshot is unchanged when registration fails. */
    static auto TryCreate(TSharedPtr<const FCkUiWidgetRegistrySnapshot>& OutSnapshot) -> FCkUiLoadResult;
};

// ====================================================================================================================
