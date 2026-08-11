#pragma once

#include "CoreMinimal.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkDebug_ViewportPicker;
class SMenuAnchor;

// ====================================================================================================================
// The one toolbar surface for a debugger's viewport picker: the Pick toggle
// button plus the gear popover with the shared picker options (ignore self,
// meshes first, cull radius, billboard size, marker max depth). Owning a
// FCkDebug_ViewportPicker and dropping this widget into a toolbar is all a
// debugger needs to gain click-to-select.
//
//   SNew(SCkDebug_ViewportPickerControls)
//       .Picker(_ViewportPicker)
//       .PickTooltip(FText::FromString(TEXT("Pick a GOAP agent in the viewport")))
//
// ExtraSettingsContent appends host-specific rows below the shared ones in the
// popover (the ECS debugger adds its overlay-attribute filter there).
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_ViewportPickerControls : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_ViewportPickerControls)
        {}
        // Required. The host window owns the picker; this widget only binds to it.
        SLATE_ARGUMENT(TSharedPtr<FCkDebug_ViewportPicker>, Picker)

        // Tooltip shown on the Pick button when pick mode can start. Empty = a
        // generic "click an entity" default; specialized pickers should say what
        // is pickable ("Pick a GOAP agent…").
        SLATE_ARGUMENT(FText, PickTooltip)

        // Optional host-specific rows appended below the shared picker rows in
        // the gear popover.
        SLATE_NAMED_SLOT(FArguments, ExtraSettingsContent)
    SLATE_END_ARGS()

    auto
    Construct(
        const FArguments& InArgs) -> void;

private:
    auto
    Build_SettingsPopover() -> TSharedRef<SWidget>;

    TSharedPtr<FCkDebug_ViewportPicker> _Picker;
    FText                               _PickTooltip;
    TSharedPtr<SWidget>                 _ExtraSettingsContent;
    TSharedPtr<SMenuAnchor>             _SettingsAnchor;
};

// ====================================================================================================================
