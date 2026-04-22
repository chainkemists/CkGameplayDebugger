#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"

class FMenuBuilder;
class SWidget;
class UToolMenu;

// --------------------------------------------------------------------------------------------------------------------
// Shared "Copy <text>" context-menu helpers used across the CK debugger modules.
//
// Three call shapes are supported:
//
// 1) Right-click on a Slate widget (e.g. an SBorder wrapping a selector SButton):
//      .OnMouseButtonDown_Lambda([this, Text](const FGeometry&, const FPointerEvent& Evt)
//      {
//          return ck::DebugCopyMenu::Handle_RightClickToCopy(SharedThis(this), Evt, Text);
//      })
//
// 2) Inside an SListView/STreeView::OnContextMenuOpening callback (FMenuBuilder):
//      ck::DebugCopyMenu::AddCopyEntry(MenuBuilder, NSLOCTEXT("...","Copy Name","Copy Name"), Tooltip, NameStr);
//
// 3) Inside a UEdGraphSchema::GetContextMenuActions override (UToolMenu):
//      ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu, "CopyText", LabelText, Tooltip, NodeTitleStr);
//
// All variants copy via FPlatformApplicationMisc::ClipboardCopy and use the standard
// "GenericCommands.Copy" icon.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::DebugCopyMenu
{
    // Show a single-entry "Copy Text" popup menu at the given screen position.
    // Use this when you need full control over presentation; most callers should
    // use Handle_RightClickToCopy below.
    CKDEBUGGERCOMMON_API auto Push_CopyTextMenu(
        const TSharedRef<SWidget>& InSourceWidget,
        const FVector2D& InScreenSpacePosition,
        const FString& InTextToCopy) -> FReply;

    // Drop-in for SBorder::OnMouseButtonDown / SWidget::OnMouseButtonUp lambdas.
    // Returns FReply::Handled() and shows a "Copy Text" menu when the event is a
    // right-click; returns FReply::Unhandled() otherwise (so other buttons keep
    // bubbling normally).
    CKDEBUGGERCOMMON_API auto Handle_RightClickToCopy(
        const TSharedRef<SWidget>& InSourceWidget,
        const FPointerEvent& InMouseEvent,
        const FString& InTextToCopy) -> FReply;

    // Append a "Copy <Label>" entry that copies InTextToCopy to the clipboard.
    // For use inside FMenuBuilder-based context menus (e.g. SListView::OnContextMenuOpening).
    CKDEBUGGERCOMMON_API auto AddCopyEntry(
        FMenuBuilder& InMenuBuilder,
        const FText& InLabel,
        const FText& InTooltip,
        const FString& InTextToCopy) -> void;

    // Append a "Copy" section with a single entry to a UToolMenu. For use inside
    // UEdGraphSchema::GetContextMenuActions overrides.
    CKDEBUGGERCOMMON_API auto AddCopyEntryToToolMenu(
        UToolMenu* InMenu,
        FName InEntryName,
        const FText& InLabel,
        const FText& InTooltip,
        const FString& InTextToCopy) -> void;
}
