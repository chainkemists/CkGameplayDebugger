#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class SDockTab;

// --------------------------------------------------------------------------------------------------------------------
// The one sanctioned way for CK surfaces (launcher, in-window Debuggers menu, ck.* console
// commands) to open a debugger tab.
//
//   1. A live tab with this id → focused, never duplicated.
//   2. DockNewDebuggersIntoExistingWindow ON and another registered CK debugger tab is live and
//      this tool registered a tab factory → the new tab docks into that live tab's stack.
//   3. Otherwise → the plain nomad invoke (saved layout decides placement, usually a new window).
//
// The docked path inserts an UNMANAGED tab, which the engine assigns an instanced layout id —
// such tabs are not persisted across editor restarts; the plain path is unchanged.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::debugger_tabs
{
    // Shared identity: Common owns the launcher tab id so any debugger chrome
    // can invoke it without depending on the DeveloperTool launcher module.
    CKDEBUGGERCOMMON_API extern const FName LauncherTabId;

    CKDEBUGGERCOMMON_API auto Invoke_DebuggerTab(FName InTabId) -> TSharedPtr<SDockTab>;

    /**
     * Detaches a module-owned tab from callbacks and content before releasing the final local reference.
     * Request closing only while Slate is live; engine-exit teardown must detach without calling SharedThis.
     */
    CKDEBUGGERCOMMON_API auto Release_DebuggerTab(
        TSharedPtr<SDockTab>& InOutTab,
        bool InRequestClose) -> void;
}

// --------------------------------------------------------------------------------------------------------------------
