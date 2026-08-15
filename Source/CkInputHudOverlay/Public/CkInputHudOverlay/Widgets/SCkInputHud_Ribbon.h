#pragma once

#include "CkInputHudOverlay/Model/CkInputHud_Model.h"

#include "CoreMinimal.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// The ribbon: one horizontal lane of per-press chips, drawn wholesale in a single custom OnPaint.
//
// A LEAF widget rather than a box of child widgets, deliberately: a chip appears and disappears on every press, and
// a per-chip child would churn Slate's widget tree at input rate. Nothing here is interactive, so there is nothing
// a child would have bought.
//
// Lane order is HELD first (pinned, never faded, never evicted), a 1px divider, then released chips newest-first.
// --------------------------------------------------------------------------------------------------------------------

class CKINPUTHUDOVERLAY_API SCkInputHud_Ribbon : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInputHud_Ribbon)
    {}
        // Co-owned with the subsystem. A null/expired model draws an empty lane rather than crashing.
        SLATE_ARGUMENT(TWeakPtr<FCk_InputHud_Model>, Model)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

    virtual auto OnPaint(
        const FPaintArgs&        InArgs,
        const FGeometry&         InAllottedGeometry,
        const FSlateRect&        InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32                    InLayerId,
        const FWidgetStyle&      InWidgetStyle,
        bool                     InParentEnabled) const -> int32 override;

private:
    // One chip's fully-derived presentation. Built once per paint (and once per desired-size query) so the paint
    // pass never re-derives a width it already measured.
    struct FChip
    {
        FString LabelText;
        FString DurationText;
        FString FrameText;

        ECk_InputHud_EventKind Kind = ECk_InputHud_EventKind::Press;

        float Width      = 0.0f;
        float BarWidth   = 0.0f;
        float Opacity    = 1.0f;

        bool Resolved = false;
        bool Modifier = false;
        bool Held     = false;
    };

    struct FLayout
    {
        TArray<FChip> Held;
        TArray<FChip> Released;

        float LabelRowHeight    = 0.0f;
        float DurationRowHeight = 0.0f;
        float FrameRowHeight    = 0.0f;

        float TotalWidth  = 0.0f;
        float TotalHeight = 0.0f;

        bool ShowFrameNumbers = true;
    };

private:
    auto Build_Layout() const -> FLayout;

private:
    TWeakPtr<FCk_InputHud_Model> _Model;
};

// --------------------------------------------------------------------------------------------------------------------
