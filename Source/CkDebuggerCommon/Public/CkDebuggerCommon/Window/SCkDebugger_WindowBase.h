#pragma once

#include "Widgets/SCompoundWidget.h"

class SDockTab;

// ====================================================================================================================
// Base class every Ck debugger window inherits from. Thin by design — its job
// is to:
//   1. Expose a stable window id for settings lookup (Get_WindowId).
//   2. Plug into FCkDebuggerRefreshGate on construct, unplug on destruct.
//
// Child widgets that do expensive Tick work (inspector panels, graph canvases,
// entity trees, etc.) call `FCkDebuggerRefreshGate::Should_RefreshNow` with
// the owning window's id. The gate consults user settings + tab visibility +
// rate cap and returns true only when the child should actually refresh.
//
//   3. Poll the Layer-B style revision behind that same gate and hand the
//      change to OnStyleRevisionChanged (see "Style live-apply" below).
//
// This base's Tick does nothing expensive — it exists only for (3). Individual
// debuggers still own their top-level tick semantics; gating at the top level
// wouldn't stop per-child ticks anyway (Slate dispatches Tick to every widget
// independently).
//
// ====================================================================================================================
// STYLE LIVE-APPLY — what a derived window must do
// ====================================================================================================================
//
// Visual style axes need nothing: every shared widget and every axes-lib helper
// binds them through TAttribute lambdas, so a Style Lab flip moves already-built
// widgets on the next paint. STRUCTURAL axes (an option that adds or removes a
// row, a section, a column) are the exception — those need a rebuild, and that
// is what the revision watch is for.
//
//   a) A derived Tick override MUST call
//      `SCkDebugger_WindowBase::Tick(...)` as its super, NOT
//      `SCompoundWidget::Tick(...)`. Calling the grandparent silently skips the
//      watch and the window stops responding to profile changes — with no
//      compile error and no runtime warning.
//
//   b) Override `OnStyleRevisionChanged()` and route it to the window's existing
//      rebuild entry point. The default is a no-op, so a window that has no
//      structural axes needs nothing.
//
// The poll is gated by FCkDebuggerRefreshGate, so it costs one uint32 compare
// per authorised frame. Should_RefreshNow is per-frame idempotent per window, so
// polling here does not starve sibling panels of their own gate call.
//
// ====================================================================================================================
// DEBUGGER RENDERING POLICY — read this before authoring any panel / inspector
// ====================================================================================================================
//
// Slate widget tree RE-CREATION on the Tick path is banned in every Ck debugger.
// Two separate reasons:
//
//   (1) Slate widget construction is expensive. Tick-driven rebuilds burn
//       CPU proportional to the body size, every frame.
//
//   (2) Structural mutation DURING Tick (ClearChildren / SetContent /
//       AddSlot on a live tree) causes a one-frame "scrunch" where the new
//       sub-tree hasn't been prepassed yet and parent AutoHeight slots
//       collapse to zero. See the Gallery "Rebuild Storm" section for a
//       live A/B repro: only the Data-only strategy is scrunch-free — every
//       other strategy (SetContent, in-place ClearChildren, in-place with
//       Invalidate(Prepass)) scrunches at non-trivial refresh rates.
//
// The rules:
//
//   a) Build each panel's widget tree ONCE per logical context (tab open,
//      entity selected, window switched). Cache the widgets; the tree
//      structure is stable for the lifetime of that context.
//
//   b) Dynamic values flow via TAttribute<FText> / TAttribute<FSlateColor> /
//      TAttribute<FLinearColor> bindings. Slate re-evaluates these every
//      paint — cheap, scrunch-free, and no invalidation required.
//
//   c) Structural changes (new row, new section) are NEVER triggered from
//      Tick. Permitted paths:
//        - Context change (selection, tab open, mode switch) — full rebuild
//        - Explicit user action (Refresh button, filter change)
//        - In-place mutation of a widget's own stable sub-container, ONLY
//          when the widget's caching keeps scrunch hidden (e.g. SListView
//          with an observable item source, or a pre-allocated pool of
//          slots toggled via Visibility attributes)
//
//   d) If a panel has "dirty flag" / "needs rebuild" plumbing, the flag
//      must be ignored on the Tick path. Act on it only at the next
//      context change, or via an explicit user refresh.
//
// Reference implementation: SCkDebuggerPanel_Inspector (CkEcsDebugger) — its
// Tick is purely informational (calls each inspector's own Tick for in-place
// mutation) and never rebuilds structure.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebugger_WindowBase : public SCompoundWidget
{
public:
	virtual ~SCkDebugger_WindowBase();

	/** Stable id used to look up per-window settings. Must return the same value for the lifetime of the widget. */
	virtual auto Get_WindowId() const -> FName = 0;

	/** Human-readable name for the settings UI. Override to customise. */
	virtual auto Get_WindowDisplayName() const -> FText;

	/**
	 * Hand the base a weak ref to the SDockTab that hosts this widget. Called
	 * by the module after SNew() + SDockTab construction. The gate uses this
	 * for visibility queries.
	 */
	auto Set_OwningTab(TWeakPtr<SDockTab> InTab) -> void;

	virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

protected:
	/**
	 * Subclasses call this from Construct once they know their window id is stable.
	 * Caches the id so the destructor can unregister without a virtual call on
	 * a partially-destructed object. Also seeds the style-revision baseline, so the
	 * first poll after construction never reports a spurious change.
	 */
	auto Register_WithGate() -> void;

	/**
	 * The Layer-B style selection changed structurally (profile applied, axis cycled,
	 * Editor Preferences edit). Route this to the window's existing rebuild entry point.
	 * Default is a no-op — visual axes already apply live and need no rebuild.
	 */
	virtual auto OnStyleRevisionChanged() -> void;

	/**
	 * Runs the gated revision comparison and fires OnStyleRevisionChanged once per change.
	 * Called from Tick; exposed so a window whose refresh work lives off the Tick path
	 * (or which must poll before its own expensive pass) can drive the watch itself.
	 * Calling it more than once per frame is harmless — the second call sees no change.
	 */
	auto Poll_StyleRevision() -> void;

private:
	TWeakPtr<SDockTab> _OwningTab;
	FName              _CachedWindowId;
	bool               _RegisteredWithGate = false;

	// Transient, like the revision itself — resets with the widget, never serialized.
	uint32 _LastSeenStyleRevision = 0;
};

// ====================================================================================================================
