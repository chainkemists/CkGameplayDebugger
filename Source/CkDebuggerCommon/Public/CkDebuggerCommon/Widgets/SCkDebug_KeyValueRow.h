#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Mono-font key/value row used for world state, preconditions, any debug
// key-value pair. Optional leading marker color + optional trailing status pill.
//
// Value semantics:
//   ValueTone::Bool     — value text rendered green if "true", red otherwise.
//   ValueTone::Custom   — value text rendered in CustomValueColor.
// ====================================================================================================================

enum class ECkDebug_KeyValueTone : uint8
{
	Bool,
	Custom,
};

class CKDEBUGGERCOMMON_API SCkDebug_KeyValueRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_KeyValueRow)
		: _KeyText(FText::GetEmpty())
		, _ValueText(FText::GetEmpty())
		, _Tone(ECkDebug_KeyValueTone::Bool)
		, _ShowMarker(false)
		, _MarkerColor(FLinearColor::White)
		, _CustomValueColor(FLinearColor::White)
		, _BackgroundColor(FLinearColor::Transparent)
	{}
		SLATE_ARGUMENT(FText, KeyText)
		SLATE_ARGUMENT(FText, ValueText)
		SLATE_ARGUMENT(ECkDebug_KeyValueTone, Tone)
		SLATE_ARGUMENT(bool, ShowMarker)
		SLATE_ARGUMENT(FLinearColor, MarkerColor)
		SLATE_ARGUMENT(FLinearColor, CustomValueColor)
		SLATE_ARGUMENT(FLinearColor, BackgroundColor)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
