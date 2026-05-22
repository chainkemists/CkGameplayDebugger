#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SVerticalBox;
class STextBlock;

// ====================================================================================================================
// SCkGoapDebugger_Breadcrumb (U11.7-C)
//
// Top-of-center widget. One row per top-level Planner on the currently
// selected entity, each row showing that Planner's runtime active chain as a
// horizontal trail of pills. Clicking a pill selects the corresponding
// Planner (or no-op if the step is an atomic Action that isn't a Planner).
//
// Layout per row:
//   "ACTIVE CHAIN ·"  [T0 AliveBrain]  ▸  [T1 Engage]  ▸  [T2 LightAttacks]
//
// Multiple top-level Planners stack vertically.
//
// Refresh discipline (CkDebuggerCommon CLAUDE.md):
//   - Leaf widgets cached at Construct (SVerticalBox of rows).
//   - Structural hash gate per RefreshFromViewModel; rebuild only on change.
//   - Refresh-gate (Should_RefreshNow) is enforced at the Window level — every
//     OnChanged broadcast already passed the gate.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_Breadcrumb : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_Breadcrumb) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    ~SCkGoapDebugger_Breadcrumb();

    // Called by parent on ViewModel::OnChanged.
    auto RefreshFromViewModel() -> void;

private:
    auto RebuildRows() -> void;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    // Vertical stack of per-top-level-Planner rows.
    TSharedPtr<SVerticalBox> _RowsBox;

    // Structural hash — handles in every top-level chain + selected planner.
    uint32 _LastHash        = 0;
    bool   _HasMaterialized = false;
};

// ====================================================================================================================
