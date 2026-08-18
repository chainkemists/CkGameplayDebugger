#pragma once

#include "CkInputHudOverlay/Style/CkInputHud_RenderStyle.h"

#include "CoreMinimal.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FCk_InputHud_Model;
class SBorder;
class SBox;
class SCkInputHud_Ribbon;
class SWidget;

// --------------------------------------------------------------------------------------------------------------------
// The on-screen input HUD. Pure presentation: every content binding is a TAttribute lambda over a TWeakPtr to the
// model, so the widget never owns a refresh method and the producer never touches Slate.
//
// HitTestInvisible by construction — this sits in the game viewport over live gameplay and must never eat a click.
//
// The panel FADES OUT WHOLE when the model has nothing to say. A HUD that stays drawn over an idle player is chrome
// the player did not ask for; the first event snaps it straight back to full rather than easing in, so nothing is
// ever half-visible at the moment it becomes interesting.
// --------------------------------------------------------------------------------------------------------------------

class CKINPUTHUDOVERLAY_API SCkInputHud_Root : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInputHud_Root)
        : _Corner(1)
        , _Scale(1.0f)
        , _Mode(1)
        , _Opacity(1.0f)
    {}
        // Co-owned with the subsystem. A null/expired model renders the HUD empty rather than crashing.
        SLATE_ARGUMENT(TWeakPtr<FCk_InputHud_Model>, Model)
        // 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right. Read live so a cvar edit moves the HUD
        // without a rebuild.
        SLATE_ATTRIBUTE(int32, Corner)
        SLATE_ATTRIBUTE(float, Scale)
        // 1 = keyboard only, 2 = follow the active device. Device presses arrive as ordinary chips either way; the
        // mode only decides whether the gamepad's stick numerics are worth the row.
        SLATE_ATTRIBUTE(int32, Mode)
        // Base render opacity of the whole panel, multiplied UNDER the idle fade so the fade curve is unchanged.
        SLATE_ATTRIBUTE(float, Opacity)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto Tick(
        const FGeometry& InAllottedGeometry,
        const double     InCurrentTime,
        const float      InDeltaTime) -> void override;

private:
    auto Get_ShowSticks() const -> bool;

private:
    TWeakPtr<FCk_InputHud_Model> _Model;

    TAttribute<int32> _Corner;
    TAttribute<float> _Scale;
    TAttribute<int32> _Mode;
    TAttribute<float> _Opacity;

    TSharedPtr<SBox>    _AnchorBox;
    TSharedPtr<SBorder> _Panel;
    TSharedPtr<SBorder> _PanelFill;
    TSharedPtr<SCkInputHud_Ribbon> _Ribbon;

    // Cached so Tick only touches Slate when the value actually moved.
    int32 _AppliedCorner = INDEX_NONE;
    float _AppliedScale  = -1.0f;

    // The idle-fade state, normalized 0..1 — the base opacity multiplies in only at apply time.
    float _PanelOpacity = 0.0f;
    float _AppliedOpacityProduct = -1.0f;
    FVector2f _AppliedPanelPadding = FVector2f{-1.0f, -1.0f};
    ECk_InputHud_BrushShape _AppliedPanelBrushShape = ECk_InputHud_BrushShape::Square;
    FLinearColor _AppliedPanelFillTint = FLinearColor::Transparent;
    FLinearColor _AppliedPanelOutlineTint = FLinearColor::Transparent;
    uint32 _AppliedSettingsRevision = MAX_uint32;
};

// --------------------------------------------------------------------------------------------------------------------
