#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

class SHorizontalBox;
class SScrollBox;

// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkGoapDebugMacroActionClicked, FString /*ActionClassName*/);

// ====================================================================================================================
// The macro view: tiered columns (discovered from age-like gates) containing
// category groups of action rows, plus a goals strip at the bottom.
//
// Rebuilt only when the underlying action/goal set changes (content hash).
// Click a row → fires OnActionClicked with the class name.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebug_MacroNodesPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebug_MacroNodesPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
		SLATE_EVENT(FOnCkGoapDebugMacroActionClicked, OnActionClicked)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	auto RebuildPanel() -> void;
	auto ComputeContentHash() const -> uint32;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
	FOnCkGoapDebugMacroActionClicked _OnActionClicked;

	TSharedPtr<SHorizontalBox> _ColumnsBox;
	TSharedPtr<SHorizontalBox> _GoalsBox;

	uint32 _LastContentHash = 0;

	// Transient per-session: which tier indices the user has collapsed.
	TSet<int32> _CollapsedTiers;
};

// ====================================================================================================================
