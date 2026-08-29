#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class SDockTab;

// --------------------------------------------------------------------------------------------------------------------
// The one sanctioned way for CK surfaces (launcher, in-window Debuggers menu, ck.* console
// commands) to open a debugger tab.
//
//   1. A live tab with this id → focused, never duplicated.
//   2. DockNewDebuggersIntoExistingWindow ON and a live registered CK debugger tab of the SAME
//      ECkDebuggerToolCategory exists and this tool registered a tab factory → the new tab docks
//      into that live tab's stack. Same-category ONLY: there is no cross-category fallback, so a
//      category's window never grows past the handful of tools that belong to it.
//   3. Otherwise → the plain nomad invoke (saved layout decides placement, usually a new window).
//      This includes a tab id with no catalog descriptor (the launcher, the suite host).
//
// The docked path inserts an UNMANAGED tab, which the engine assigns an instanced layout id —
// such tabs are not persisted across editor restarts; the plain path is unchanged.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::debugger_tabs
{
    // Shared identity: Common owns the launcher tab id so any debugger chrome
    // can invoke it without depending on the DeveloperTool launcher module.
    CKDEBUGGERCOMMON_API extern const FName LauncherTabId;

    // Same reasoning for the one-window suite host: the rail's "Debugger Suite" button lives in
    // widget code that must not include the launcher module's root header.
    CKDEBUGGERCOMMON_API extern const FName SuiteTabId;

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
