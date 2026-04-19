#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"
#include "CkGoapDebugger/Window/MacroNodes/CkGoapDebug_ActionCategorizer.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkGoapDebugActionRowClicked, FString /*ActionClassName*/);

// ====================================================================================================================
// One row in the macro panel: colored left border (category) + dot + name +
// pre-count badge + cost badge. Clickable. Does NOT own the action info —
// holds the class name and forwards it via the click delegate; the parent
// re-resolves the action pointer from the ViewModel snapshot to avoid
// dangling pointers on data refresh.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebug_ActionRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebug_ActionRow) {}
		SLATE_ARGUMENT(FCkGoapDebugger_ActionInfo, Action)
		SLATE_ARGUMENT(ECkGoapDebug_ActionCategory, Category)
		SLATE_EVENT(FOnCkGoapDebugActionRowClicked, OnClicked)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

private:
	auto OnButtonClicked() -> FReply;

private:
	FString _ClassName;
	FOnCkGoapDebugActionRowClicked _OnClicked;
};

// ====================================================================================================================
