#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SCkGoapDebugger_PlanStrip;
class SVerticalBox;
class STextBlock;
class SBox;

// ====================================================================================================================
// SCkGoapDebugger_PrimaryPane — the big inspector pane in the centre column.
//
// Vertical stack:
//   1. Header bar — action name, role, status, cost, attempts, last replan
//   2. Plan strip — horizontal cards (via SCkGoapDebugger_PlanStrip)
//   3. Goal section — list of goal conditions, sat/unsat against resolved WS
//   4. Drilldown — visible when SelectedAction != header action; preconditions,
//      effects, cost columns
//   5. (Right rail in the horizontal pair) — WS source, parent, invalid goals,
//      dependency cycles, OR failure-variant when PlanFailed
//
// The pane subscribes to ViewModel::OnChanged via RefreshFromViewModel().
// All sub-content lives inside _LeftBody / _RightRail SVerticalBox slots that
// are cleared and re-populated on each refresh.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_PrimaryPane : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_PrimaryPane) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    ~SCkGoapDebugger_PrimaryPane();

    // Called by the parent window on ViewModel::OnChanged. Rebuilds the
    // header + left body + right rail. Plan strip refreshes itself.
    auto RefreshFromViewModel() -> void;

private:
    // -----------------------------------------------------------------------------------------------------------------
    // Build helpers — operate on the current ViewModel snapshot. Each returns
    // a fresh widget tree that lives inside the corresponding host slot.
    // -----------------------------------------------------------------------------------------------------------------

    auto BuildEmptyState() -> TSharedRef<SWidget>;
    auto BuildHeader(const FCkGoapDebugger_ActionInfo& InAction) -> TSharedRef<SWidget>;
    auto BuildGoalSection(const FCkGoapDebugger_ActionSetInfo& InAs, const FCkGoapDebugger_ActionInfo& InAction) -> TSharedRef<SWidget>;
    auto BuildDrilldown(const FCkGoapDebugger_ActionSetInfo& InAs, const FCkGoapDebugger_ActionInfo& InSelected) -> TSharedRef<SWidget>;

    auto BuildRightRail_Normal(const FCkGoapDebugger_ActionSetInfo& InAs, const FCkGoapDebugger_ActionInfo& InAction) -> TSharedRef<SWidget>;
    auto BuildRightRail_Failure(const FCkGoapDebugger_ActionSetInfo& InAs, const FCkGoapDebugger_ActionInfo& InAction) -> TSharedRef<SWidget>;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    // Host slots — refreshed by RefreshFromViewModel.
    TSharedPtr<SBox>          _HeaderHost;
    TSharedPtr<SVerticalBox>  _LeftBody;
    TSharedPtr<SBox>          _RightRailHost;

    // Plan strip — its own widget; just call RefreshFromViewModel on it.
    TSharedPtr<SCkGoapDebugger_PlanStrip> _PlanStrip;
};

// ====================================================================================================================
