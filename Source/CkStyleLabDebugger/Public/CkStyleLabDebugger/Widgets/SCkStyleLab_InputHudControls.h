#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

enum class ECk_InputHud_ColorRole : uint8;

DECLARE_DELEGATE(FOnCkStyleLab_InputHudChanged);

// ====================================================================================================================

/**
 * Feature-local Input HUD visual/readout controls. The Style Lab owns the preview surface; CkInputHudOverlay owns
 * every persisted value. This widget is only an apply bridge and never mirrors settings state.
 */
class SCkStyleLab_InputHudControls : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkStyleLab_InputHudControls) {}
        SLATE_EVENT(FOnCkStyleLab_InputHudChanged, OnChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Build_PaletteRow() -> TSharedRef<SWidget>;
    auto Build_DensityRow() -> TSharedRef<SWidget>;
    auto Build_CornerRow() -> TSharedRef<SWidget>;
    auto Build_MetadataRow() -> TSharedRef<SWidget>;
    auto Build_FrameNotationRow() -> TSharedRef<SWidget>;
    auto Build_NumericRows() -> TSharedRef<SWidget>;
    auto Build_ColorRows() -> TSharedRef<SWidget>;
    auto
        Build_ColorRow(
            const FText&             InLabel,
            ECk_InputHud_ColorRole   InRole)
        -> TSharedRef<SWidget>;

    auto OnCyclePalette(int32 InDirection)       -> FReply;
    auto OnCycleDensity(int32 InDirection)       -> FReply;
    auto OnCycleCorner(int32 InDirection)        -> FReply;
    auto OnCycleMetadata(int32 InDirection)      -> FReply;
    auto OnCycleFrameNotation(int32 InDirection) -> FReply;
    auto OnResetVisuals()                        -> FReply;

    auto OnColorMouseDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent,
        ECk_InputHud_ColorRole InRole) -> FReply;
    auto OnColorCommitted(FLinearColor InColor, ECk_InputHud_ColorRole InRole) -> void;
    auto Get_Color(ECk_InputHud_ColorRole InRole) const -> FLinearColor;
    auto NotifyChanged() -> void;

    FOnCkStyleLab_InputHudChanged _OnChanged;
};

// ====================================================================================================================
