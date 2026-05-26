#include "SCkDebug_SelectableLabel.h"

#include "Widgets/Input/SEditableText.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

// --------------------------------------------------------------------------------------------------------------------
// Modifier-aware editable text. Plain left-click returns Unhandled so the
// event bubbles up to any clickable parent (SButton, etc.) — meaning a
// SelectableLabel placed inside a button label or a clickable row no longer
// traps the click. Ctrl- or Shift-left-click activates normal text selection;
// right-click still opens the built-in Copy / Select All context menu.
//
// Cursor query mirrors the click policy: on plain hover the cursor stays as
// the default arrow (matches the click pass-through behavior); only when a
// selection modifier (Ctrl/Shift) is held does the text-edit I-beam appear.
// Without this, users see the I-beam, expect drag-select to "just work", and
// get surprised when their plain click activates the parent button instead.
//
// This is the project-wide policy for selectable text in debugger UIs: copy-
// paste should be possible without breaking clickable parents. Hold Ctrl (or
// Shift) when you actually want to drag-select. Browsers handle this with a
// click-then-drag model that's incompatible with click-fires-button semantics;
// our model trades plain-click selection for parent-click safety.
// --------------------------------------------------------------------------------------------------------------------

namespace
{
    class SCkDebug_ClickThroughEditableText : public SEditableText
    {
    public:
        virtual auto
            OnMouseButtonDown(
                const FGeometry& InGeometry,
                const FPointerEvent& InMouseEvent)
            -> FReply override
        {
            const auto IsLeftButton  = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
            const auto ModifierHeld  = InMouseEvent.IsControlDown() || InMouseEvent.IsShiftDown();

            if (IsLeftButton && NOT ModifierHeld)
            {
                // Pass through to any parent button / clickable row.
                return FReply::Unhandled();
            }

            // Ctrl/Shift left-click (selection start), right-click (context
            // menu), middle-click, etc. — defer to standard SEditableText.
            return SEditableText::OnMouseButtonDown(InGeometry, InMouseEvent);
        }

        virtual auto
            OnMouseButtonDoubleClick(
                const FGeometry& InGeometry,
                const FPointerEvent& InMouseEvent)
            -> FReply override
        {
            // Double-click word-select also gated on a modifier so plain
            // double-clicks on buttons (which sometimes consume DoubleClick
            // events) keep working too. Ctrl+double-click selects the word.
            const auto IsLeftButton  = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
            const auto ModifierHeld  = InMouseEvent.IsControlDown() || InMouseEvent.IsShiftDown();

            if (IsLeftButton && NOT ModifierHeld)
            {
                return FReply::Unhandled();
            }

            return SEditableText::OnMouseButtonDoubleClick(InGeometry, InMouseEvent);
        }

        virtual auto
            OnCursorQuery(
                const FGeometry& InGeometry,
                const FPointerEvent& InCursorEvent)
            const -> FCursorReply override
        {
            // SEditableText returns the text-edit I-beam cursor on hover, which
            // misleads the user into thinking a plain click will select text —
            // but our OnMouseButtonDown deliberately passes plain clicks through
            // to the clickable parent. Match the cursor to the actual behavior:
            // show the default arrow until a selection-modifier key is held.
            //
            // We read modifier state from FSlateApplication rather than the
            // InCursorEvent because cursor-query events are synthesized from
            // mouse-move position and don't always carry up-to-date keyboard
            // modifier state.
            const auto& ModKeys = FSlateApplication::Get().GetModifierKeys();
            const auto ModifierHeld = ModKeys.IsControlDown() || ModKeys.IsShiftDown();

            if (NOT ModifierHeld)
            {
                return FCursorReply::Cursor(EMouseCursor::Default);
            }

            return SEditableText::OnCursorQuery(InGeometry, InCursorEvent);
        }
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_SelectableLabel::Construct(const FArguments& InArgs) -> void
{
    ChildSlot
    [
        SAssignNew(_Editable, SCkDebug_ClickThroughEditableText)
        .Text(InArgs._Text)
        .Font(InArgs._Font)
        .ColorAndOpacity(InArgs._ColorAndOpacity)
        .IsReadOnly(true)
    ];
}

auto SCkDebug_SelectableLabel::SetText(const FText& InText) -> void
{
    if (_Editable.IsValid())
    {
        _Editable->SetText(InText);
    }
}
