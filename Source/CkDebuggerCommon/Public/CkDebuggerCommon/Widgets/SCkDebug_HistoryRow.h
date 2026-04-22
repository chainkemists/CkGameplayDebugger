#pragma once

#include "SCkDebug_StatusPill.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// A compact row for history/timeline lists: colored status dot + title + meta
// + right-aligned frame / code, plus an optional subtitle line.
// Selectable (shows a left-border accent when selected).
//
// Designed to be reused: plan history, SM transition history, anything with
// a sequence of "what happened" entries.
// ====================================================================================================================

DECLARE_DELEGATE(FOnCkDebugHistoryRowClicked);

class CKDEBUGGERCOMMON_API SCkDebug_HistoryRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_HistoryRow)
		: _Tone(ECkDebug_Tone::Neutral)
		, _TitleText(FText::GetEmpty())
		, _MetaText(FText::GetEmpty())
		, _RightText(FText::GetEmpty())
		, _SubtitleText(FText::GetEmpty())
		, _IsSelected(false)
	{}
		SLATE_ARGUMENT(ECkDebug_Tone, Tone)
		SLATE_ARGUMENT(FText, TitleText)
		SLATE_ARGUMENT(FText, MetaText)
		SLATE_ARGUMENT(FText, RightText)
		SLATE_ARGUMENT(FText, SubtitleText)
		SLATE_ARGUMENT(bool, IsSelected)
		// When non-empty, right-click on the row shows a "Copy Text" menu that
		// puts this string on the clipboard. Default off.
		SLATE_ARGUMENT(FString, CopyText)
		SLATE_EVENT(FOnCkDebugHistoryRowClicked, OnClicked)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

private:
	auto OnClicked() -> FReply;

private:
	FOnCkDebugHistoryRowClicked _OnClicked;
};

// ====================================================================================================================
