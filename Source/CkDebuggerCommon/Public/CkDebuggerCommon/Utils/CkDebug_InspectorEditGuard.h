#pragma once

#include "Containers/Set.h"
#include "Templates/SharedPointer.h"

// ====================================================================================================================
// The one thing that makes an inspector safe to make INTERACTIVE: a panel-scoped registry of
// "an edit is in flight right now".
//
// An inspector panel rebuilds its widget tree on selection change, on a filter keystroke, and
// (potentially) on a structural RequestRebuild. Any of those destroys the very SEditableTextBox
// the user is typing into. The guard makes that a DEFERRAL rather than a data loss: while any row
// reports an active edit the panel holds the dirty flag, and consumes it the moment the edit ends.
//
// SCOPE: one guard per inspector panel INSTANCE — never a static/global. Two ECS debugger windows
// must not block each other's rebuilds. The panel owns it and hands a TSharedPtr down
// panel -> inspector -> FCkInspectorWidgetBuilder -> row.
//
// THREADING: game thread only, like every Slate surface that uses it.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API FCkInspectorEditGuard : public TSharedFromThis<FCkInspectorEditGuard>
{
public:
    // Opaque per-guard identity for one interactive control. Never reused, so a stale release from a
    // widget that outlived a Clear_AllEdits cannot resurrect somebody else's edit.
    using FEditKey = uint64;

public:
    auto Claim_EditKey() -> FEditKey;

    // Idempotent by construction (the active set is a TSet, not a counter): a double-begin or a
    // double-end from a widget with quirky focus events cannot leave the guard permanently stuck.
    auto Set_EditActive(FEditKey InKey, bool InIsActive) -> void;

    auto Get_HasActiveEdit() const -> bool;
    auto Get_ActiveEditCount() const -> int32;

    // Called when the panel tears its rows down: every key those rows held is meaningless now.
    auto Clear_AllEdits() -> void;

public:
    // ----- Rebuild deferral ------------------------------------------------------------------
    // DEFERS, never drops. Request_Rebuild while an edit is in flight keeps the request pending;
    // Consume_PendingRebuild hands it back exactly once, and only once no edit is active.

    auto Request_Rebuild() -> void;
    auto Get_HasPendingRebuild() const -> bool;
    auto Consume_PendingRebuild() -> bool;

private:
    TSet<FEditKey> _ActiveEdits;
    FEditKey       _NextKey = 1;
    bool           _HasPendingRebuild = false;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * RAII lifetime for one control's edit state. Rows hold this as a TSharedPtr captured BY VALUE in
 * their Slate delegates, so the scope dies with the widget — a control destroyed mid-type (panel
 * rebuild, PIE end, world switch) releases its edit instead of wedging the guard forever.
 *
 * A null guard is a valid, inert scope: builders used outside an inspector panel need no wiring.
 */
class CKDEBUGGERCOMMON_API FCkInspectorEditScope
{
public:
    explicit FCkInspectorEditScope(TSharedPtr<FCkInspectorEditGuard> InGuard);
    ~FCkInspectorEditScope();

    FCkInspectorEditScope(const FCkInspectorEditScope&) = delete;
    auto operator=(const FCkInspectorEditScope&) -> FCkInspectorEditScope& = delete;

public:
    auto Set_Active(bool InIsActive) -> void;
    auto Get_IsActive() const -> bool { return _IsActive; }

private:
    TWeakPtr<FCkInspectorEditGuard> _Guard;
    FCkInspectorEditGuard::FEditKey _Key = 0;
    bool                            _IsActive = false;
};

// ====================================================================================================================
