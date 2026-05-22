#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "CkGoap/Action/CkGoap_Action_Fragment_Data.h"  // FCk_Handle_Goap_Action — stored by value below

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SGraphEditor;
class SBox;
class STextBlock;
class UCkGoapDebugGraph;

// ====================================================================================================================
// SCkGoapDebugger_GraphPane — bottom-of-center-column action graph view.
//
// Owns:
//   - A UCkGoapDebugGraph (transient UObject, GC-rooted via AddToRoot in
//     Construct and RemoveFromRoot in destructor; mirrors CkSmDebugger).
//   - A SGraphEditor binding to that graph.
//   - Header row with status text + Fit / 1:1 / Hide-dimmed buttons (stubs).
//
// Lifecycle:
//   - Subscribes to ViewModel->OnChanged and rebuilds the graph from the
//     currently selected Planner snapshot.
//   - Restores selection by Action handle when the previously-selected
//     node still exists after a rebuild.
//   - Propagates SGraphEditor selection changes back to the ViewModel.
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

private:
    auto BuildHeader() -> TSharedRef<SWidget>;
    auto OnGraphSelectionChanged(const TSet<UObject*>& InSelection) -> void;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    UCkGoapDebugGraph*       _Graph       = nullptr;
    TSharedPtr<SGraphEditor> _GraphEditor;
    TSharedPtr<SBox>         _GraphHost;
    TSharedPtr<STextBlock>   _HeaderText;

    FDelegateHandle _OnChangedHandle;

    // Suppress the round-trip echo when we programmatically restore selection
    // after a rebuild — otherwise OnSelectionChanged would push it back into
    // the ViewModel as if the user clicked it.
    bool _SuppressSelectionEcho = false;

    // Topology hash of the last graph we built. Lets RefreshFromViewModel
    // skip the destructive RebuildFromSnapshot when only mutable per-tick
    // state changed (plan membership / selection / failure flag) — those go
    // through the cheap UCkGoapDebugGraph::UpdateRuntimeState path instead.
    // Setting this back to 0 (via Reset_ForWorldChange) forces a rebuild.
    uint32 _LastTopologyHash = 0;

    // Track selection identity separately from the topology hash. A selection
    // change inside an otherwise-stable topology still needs the in-place
    // node update so the highlighted-node tint follows the user's click.
    FCk_Handle_Goap_Action _LastSelectedAction;
};

// ====================================================================================================================
