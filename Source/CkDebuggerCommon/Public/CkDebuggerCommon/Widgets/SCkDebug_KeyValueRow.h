#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Mono-font key/value row used for world state, preconditions, any debug
// key-value pair. Optional leading marker color + optional trailing status pill.
//
// Value semantics:
//   ValueTone::Bool     — value text rendered green if "true", red otherwise.
//                         Only evaluated at Construct — use Custom + attribute for dynamic values.
//   ValueTone::Custom   — value text rendered in CustomValueColor (attribute, can be dynamic).
//
// Advanced:
//   OnKeyClicked — when bound, the key column renders as a clickable button tinted with
//                  the Selection color (used by the ECS inspector for entity-navigation rows).
//   ValueWidget  — named slot; when set, replaces the value text entirely. Use for rows
//                  where the value column is a custom inline widget (badge box, button, etc.).
// ====================================================================================================================

enum class ECkDebug_KeyValueTone : uint8
{
	Bool,
	Custom,
};

DECLARE_DELEGATE(FOnCkDebugKeyValueRow_KeyClicked);

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
		SLATE_ATTRIBUTE(FText, ValueText)
		SLATE_ARGUMENT(ECkDebug_KeyValueTone, Tone)
		SLATE_ARGUMENT(bool, ShowMarker)
		SLATE_ARGUMENT(FLinearColor, MarkerColor)
		SLATE_ATTRIBUTE(FLinearColor, CustomValueColor)
		SLATE_ARGUMENT(FLinearColor, BackgroundColor)
		SLATE_EVENT(FOnCkDebugKeyValueRow_KeyClicked, OnKeyClicked)
		SLATE_NAMED_SLOT(FArguments, ValueWidget)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
