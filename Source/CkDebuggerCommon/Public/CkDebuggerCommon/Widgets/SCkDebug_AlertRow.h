#pragma once

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Full-width alert strip row — tone-dim background, glyph + bold lead + body
// text, an optional right-aligned "fix hint", and an optional action button.
//
//   ⚠  Fallback plan active — no affordable path…      Check the Decision panel   [Pop layer]
//
// Stack rows in an SVerticalBox under the window chrome; give each alert a
// stable identity so the strip doesn't flicker (rebuild only when the alert
// SET changes).
// ====================================================================================================================

DECLARE_DELEGATE(FOnCkDebug_AlertAction);

class CKDEBUGGERCOMMON_API SCkDebug_AlertRow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_AlertRow)
        : _Tone(ECk_Tone::Warn)
        , _Glyph(FText::GetEmpty())
        , _LeadText(FText::GetEmpty())
        , _BodyText(FText::GetEmpty())
        , _FixText(FText::GetEmpty())
        , _ActionText(FText::GetEmpty())
    {}
        SLATE_ARGUMENT(ECk_Tone, Tone)
        // Short glyph, e.g. "⚠" / "🧪". Empty = none.
        SLATE_ARGUMENT(FText, Glyph)
        // Bold lead fragment ("Fallback plan active").
        SLATE_ARGUMENT(FText, LeadText)
        // Regular continuation (" — no affordable path to the goal…").
        SLATE_ATTRIBUTE(FText, BodyText)
        // Muted right-aligned hint. Empty = none.
        SLATE_ARGUMENT(FText, FixText)
        // Outlined action button label. Empty = no button.
        SLATE_ARGUMENT(FText, ActionText)
        SLATE_EVENT(FOnCkDebug_AlertAction, OnAction)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
