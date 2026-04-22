#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

class SCheckBox;
class SCkDebug_DualSearchBar;

// ====================================================================================================================

class SCkGoapDebugger_WorldStatePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebugger_WorldStatePanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	struct FChangeRecord
	{
		bool NewValue = false;
		double ChangeTime = 0.0;
	};

	auto RebuildWorldState() -> void;
	auto OnFilterTextChanged(const FString& InText) -> void;
	auto OnHighlightTextChanged(const FString& InText) -> void;
	auto OnShowOnlyTrueChanged(ECheckBoxState InState) -> void;
	auto GetRowBackground(FGameplayTag InKey) const -> FSlateColor;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
	TSharedPtr<SVerticalBox> _StateListBox;
	TSharedPtr<SCkDebug_DualSearchBar> _SearchBar;
	TSharedPtr<SCheckBox> _ShowOnlyTrueCheckBox;

	uint32 _LastWorldStateHash = 0;
	FString _FilterString;
	FString _HighlightString;
	bool _ShowOnlyTrue = false;

	TMap<FGameplayTag, bool> _PrevValues;
	TMap<FGameplayTag, FChangeRecord> _TagChanges;
};

// ====================================================================================================================
