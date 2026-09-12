#pragma once

#include "CkGoap/Action/CkGoap_Action_Fragment_Data.h" // FCk_Handle_Goap_Action — stored by value below
#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class FCkUiView;
class SBox;
class STextBlock;
class SCkDebug_SelectableLabel;
class SCkDebug_GraphCanvas;
class FCkGoapRuntimeGraphModel;

// ====================================================================================================================
// SCkGoapDebugger_GraphPane — bottom-of-center-column action graph view.
//
// Owns a runtime-safe graph model plus the shared Slate canvas.  The same
// widget path is used by editor and packaged Development builds.
//   - Header row with status text + Fit / 1:1 / Hide-dimmed buttons (stubs).
//
// Lifecycle:
//   - Subscribes to ViewModel->OnChanged and rebuilds the graph from the
//     currently selected Planner snapshot.
//   - Restores selection by Action handle when the previously-selected
//     node still exists after a rebuild.
//   - Propagates canvas selection changes back to the ViewModel.
//   - Reset_ForWorldChange clears the graph so FCk_Handle copies inside
//     node snapshots release while the registry is still alive.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_GraphPane : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_GraphPane) {}
    SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    SCkGoapDebugger_GraphPane();
    virtual ~SCkGoapDebugger_GraphPane() override;

    auto Construct(const FArguments& InArgs) -> void;

    // Called by SCkGoapDebuggerWindow when the ViewModel publishes a change.
    // Also safe to call directly to force a rebuild after a world reset.
    auto RefreshFromViewModel() -> void;

    // Drops all graph nodes so handles release while the ECS registry is live.
    auto Reset_ForWorldChange() -> void;

    // Rebind after the ViewModel has published its teardown reset. The next
    // ordinary ViewModel change can then mount the next world's authored shell.
    auto Resume_AfterWorldChange() -> void;

    // Exposes the runtime graph's max-name-depth so the main window's
    // toolbar +/- buttons can clamp their cycle to the longest class-name
    // segment count actually present in the current snapshot.
    auto Get_MaxNameDepth() const -> int32;

    // Recomputes fixed canvas geometry after an axis revision while preserving
    // the graph model, selection, viewport transform, and cached card widgets.
    auto Refresh_ForStyleChange() -> void;

    /** Test-only inspection of the retained production authored shell. */
    auto Get_AuthoredView() const -> TSharedPtr<FCkUiView> { return _AuthoredView; }
    auto Get_AuthoredLoadFailure() const -> const FString& { return _AuthoredLoadFailure; }

  private:
    auto BuildHeader() -> TSharedRef<SWidget>;
    auto OnGraphSelectionChanged(const TSet<uint64>& InSelection) -> void;
    auto OnGraphNodeMoved(uint64 InNodeId, const FVector2D& InPosition) -> void;
    auto RebuildCanvasScene() -> void;
    auto ResetManualNodePositions() -> void;
    auto Request_SetHideDimmed(bool InHideDimmed) -> void;
    auto ResetGraphState() -> void;
    auto TryActivateAuthoredView() -> void;
    auto ActivateNativeFallback() -> void;
    auto ClearAuthoredNativePort() -> void;

  private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    TSharedPtr<FCkGoapRuntimeGraphModel> _Graph;
    TSharedPtr<SCkDebug_GraphCanvas> _GraphCanvas;
    TSharedPtr<SCkDebug_SelectableLabel> _HeaderText;
    TSharedPtr<SBox> _ContentHost;
    TSharedPtr<SWidget> _NativeContent;
    TSharedPtr<SBox> _GraphBodyPort;
    TSharedPtr<FCkUiView> _AuthoredView;
    FString _AuthoredLoadFailure;
    uint64 _AuthoredGeneration = 0;
    TMap<uint64, TSharedPtr<SWidget>> _CardWidgets;
    TMap<uint64, FVector2D> _NodePositionOverrides;
    uint64 _ManualPositionScopeId = 0;

    FDelegateHandle _OnChangedHandle;

    // Suppress the round-trip echo when we programmatically restore selection
    // after a rebuild — otherwise OnSelectionChanged would push it back into
    // the ViewModel as if the user clicked it.
    bool _SuppressSelectionEcho = false;

    // Topology hash of the last graph we built. Lets RefreshFromViewModel
    // skip the destructive RebuildFromSnapshot when only mutable per-tick
    // state changed (plan membership / selection / failure flag) — those go
    // through the runtime model's cheap UpdateRuntimeState path instead.
    // Setting this back to 0 (via Reset_ForWorldChange) forces a rebuild.
    uint32 _LastTopologyHash = 0;
    uint32 _LastEffectiveGoalHash = 0;
    int32 _LastNameDepth = INDEX_NONE;
    bool _HideDimmed = false;

    // Track selection identity separately from the topology hash. A selection
    // change inside an otherwise-stable topology still needs the in-place
    // node update so the highlighted-node tint follows the user's click.
    FCk_Handle_Goap_Action _LastSelectedAction;
};

// ====================================================================================================================
