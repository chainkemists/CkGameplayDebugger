#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "CkEcs/Handle/CkHandle.h"

class STextBlock;

// ====================================================================================================================
// Single-line clickable entity reference widget.
//
// Renders an FCk_Handle around the canonical "{ID}|{Version}({Raw})" string.
// Left-click opens the CK ECS Debugger and selects this entity (via
// ck::DebugNav::Goto_Entity — see CkDebug_Navigator.h).
// Right-click opens a "Copy Text" menu with the formatted string.
//
// Drop this in anywhere a debugger refers to an entity — instead of hand-rolling
// STextBlock + Format_UE call sites.
//
// Behaviour:
// - Empty / invalid / tombstone handles render with CkStyle::None() color
//   and the click is disabled.
// - When no entity navigator is registered (e.g. in a build that has no
//   CkEcsDebugger module), the click is a no-op but the widget still renders
//   and right-click → copy still works.
// - The Entity argument is a SLATE_ATTRIBUTE so callers can bind a lambda for
//   dynamic values (matches the rest of the inspector widget builders).
//
// Two orthogonal axes drive the look, both re-read every frame so a Style Lab flip lands live on
// every existing pill without a rebuild:
// - EntityIdStyle composes the TEXT (ck::debug_axes::Make_EntityIdText). NameOnly is the one option
//   that DROPS the identifier from the visible text, so the pill moves the id into its tooltip
//   instead; every other option leaves the tooltip untouched.
// - EntityRefStyle picks the TREATMENT. Flat is the shipped look (transparent chip, zero padding,
//   EntityId ink). Pill fills a rounded chip with a hash-derived hue washed behind the same hue as
//   ink. OutlinePill rings that hue instead of filling it. Monochrome drops the accent entirely for
//   TextDim ink in a monospace face. Invalid handles ignore all of it and stay a muted "None".
//
// A clickable reference brightens its ink while hovered; a non-clickable one never changes.
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

        // STYLE LAB ONLY. The Lab renders with no world loaded, and an invalid handle collapses
        // every treatment onto the same muted "None" — which would make the preview lie about all
        // four of them. Supplying an id string substitutes the two composition inputs (and seeds the
        // hash hue from that string) so the treatments render for real. Never set these from a
        // debugger surface: a preview never navigates, whatever the navigator says.
        SLATE_ARGUMENT(FString, PreviewName)
        SLATE_ARGUMENT(FString, PreviewIdText)
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
    auto Get_ChipColor() const -> FSlateColor;
    auto Get_ChipPadding() const -> FMargin;
    auto Get_ChipBrush() const -> const FSlateBrush*;
    auto Get_Font() const -> FSlateFontInfo;
    auto Get_Tooltip() const -> FText;
    auto Get_TooltipIdPrefix() const -> FString;
    auto Get_Accent() const -> FLinearColor;
    auto Get_IdText() const -> FString;
    auto Is_Clickable() const -> bool;
    auto Is_Preview() const -> bool;
    auto Has_Subject() const -> bool;

    TAttribute<FCk_Handle> _Entity;
    TAttribute<FSlateFontInfo> _Font;
    bool _ShowName = false;
    FText _CustomTooltip;
    FString _PreviewName;
    FString _PreviewIdText;
};

// ====================================================================================================================
