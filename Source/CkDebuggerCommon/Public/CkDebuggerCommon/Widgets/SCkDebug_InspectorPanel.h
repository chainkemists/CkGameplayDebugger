#pragma once

#include "SCkDebug_StatusPill.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

// ====================================================================================================================
// Right-rail / dockable panel primitive. Uppercase header, optional count
// badge, optional status pill in the header, chevron that toggles a
// collapsible body. Consumers provide a single SLATE_NAMED_SLOT for body.
// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkDebugInspectorToggled, bool /*bIsExpanded*/);

class CKDEBUGGERCOMMON_API SCkDebug_InspectorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_InspectorPanel)
		: _Title(FText::GetEmpty())
		, _CountText(FText::GetEmpty())
		, _StatusPillText(FText::GetEmpty())
		, _StatusPillTone(ECkDebug_Tone::Neutral)
		, _StartExpanded(true)
	{}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, CountText)
		SLATE_ARGUMENT(FText, StatusPillText)
		SLATE_ARGUMENT(ECkDebug_Tone, StatusPillTone)
		SLATE_ARGUMENT(bool, StartExpanded)
		SLATE_EVENT(FOnCkDebugInspectorToggled, OnToggled)

		SLATE_NAMED_SLOT(FArguments, Body)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	auto Set_Expanded(bool InExpanded) -> void;
	auto Is_Expanded() const -> bool { return _IsExpanded; }

	// Update the header pill / count dynamically (data binding).
	auto Set_CountText(const FText& InText) -> void;
	auto Set_StatusPill(const FText& InText, ECkDebug_Tone InTone) -> void;

private:
	auto OnHeaderClicked() -> FReply;
	auto RebuildHeader() -> void;

private:
	bool _IsExpanded = true;
	FOnCkDebugInspectorToggled _OnToggled;

	TSharedPtr<class SBorder> _BodyBorder;
	TSharedPtr<class STextBlock> _CountBadge;
	TSharedPtr<class STextBlock> _ChevronText;

	TSharedPtr<class SHorizontalBox> _HeaderRow;
	TSharedPtr<SCkDebug_StatusPill> _StatusPill;

	FText _Title;
	FText _CountText;
	FText _StatusPillText;
	ECkDebug_Tone _StatusPillTone = ECkDebug_Tone::Neutral;
};

// ====================================================================================================================
