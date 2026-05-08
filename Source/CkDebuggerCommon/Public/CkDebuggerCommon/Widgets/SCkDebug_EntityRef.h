#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "CkEcs/Handle/CkHandle.h"

class STextBlock;

// ====================================================================================================================
// Single-line clickable entity reference widget.
//
// Renders an FCk_Handle as the canonical "{ID}|{Version}({Raw})" string in
// CkDebugStyle::EntityId() color. Left-click opens the CK ECS Debugger and
// selects this entity (via ck::DebugNav::Goto_Entity — see CkDebug_Navigator.h).
// Right-click opens a "Copy Text" menu with the formatted string.
//
// Drop this in anywhere a debugger refers to an entity — instead of hand-rolling
// STextBlock + Format_UE call sites.
//
// Behaviour:
// - Empty / invalid / tombstone handles render with CkDebugStyle::None() color
//   and the click is disabled.
// - When no entity navigator is registered (e.g. in a build that has no
//   CkEcsDebugger module), the click is a no-op but the widget still renders
//   and right-click → copy still works.
// - The Entity argument is a SLATE_ATTRIBUTE so callers can bind a lambda for
//   dynamic values (matches the rest of the inspector widget builders).
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_EntityRef : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_EntityRef)
        : _Entity(FCk_Handle{})
        , _ShowName(false)
        , _Tooltip(FText::GetEmpty())
        {}
        SLATE_ATTRIBUTE(FCk_Handle, Entity)
        SLATE_ATTRIBUTE(FSlateFontInfo, Font)
        SLATE_ARGUMENT(bool, ShowName)
        SLATE_ARGUMENT(FText, Tooltip)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) -> FReply override;

    virtual auto OnCursorQuery(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) const -> FCursorReply override;

private:
    auto Get_DisplayText() const -> FText;
    auto Get_TextColor() const -> FSlateColor;
    auto Get_Tooltip() const -> FText;
    auto Is_Clickable() const -> bool;

    TAttribute<FCk_Handle> _Entity;
    bool _ShowName = false;
    FText _CustomTooltip;
};

// ====================================================================================================================
