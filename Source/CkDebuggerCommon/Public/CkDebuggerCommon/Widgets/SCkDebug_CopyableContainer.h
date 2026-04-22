#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// Generic wrapper that adds right-click → "Copy Text" to any child widget.
// Uses ck::DebugCopyMenu::Handle_RightClickToCopy under the hood, so behaviour
// is identical to the inline pattern used by SCkDebug_HistoryRow / SCkDebug_NodePill.
//
// Use when:
//   - You have a custom widget assembly (not HistoryRow / NodePill) that should
//     respond to right-click with a Copy menu.
//   - The text to copy is computed at construction (a row, a label group, etc.).
//
// Note: any inner SButton inside Content still consumes left-click; right-click
// bubbles past SButton to this wrapper. If your child is itself a transparent
// container, no special handling is needed.
// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_CopyableContainer : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_CopyableContainer) {}
        SLATE_DEFAULT_SLOT(FArguments, Content)
        // Required: the text put on the clipboard when the user picks "Copy Text".
        SLATE_ARGUMENT(FString, CopyText)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};
