#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

// ====================================================================================================================

DECLARE_DELEGATE(FOnCkStyleLab_SelectionChanged);

// --------------------------------------------------------------------------------------------------------------------

/**
 * One axis row in the always-visible HUD. Discovered by reflecting over
 * FCkDebuggerStyleSelection. The reflected property remains the source of value and declaration
 * order; CkStyleLab_AxisMetadata owns its packaged-safe label and tooltip contract.
 */
struct FCkStyleLab_AxisRow
{
    const FProperty* Property = nullptr;
    FText            DisplayName;
    FText            ToolTip;

    // Option values and their labels are snapshotted at construction — the enum's shape cannot
    // change while the editor runs, and snapshotting keeps a UObject pointer out of the row.
    TArray<int64>    Options;
    TArray<FText>    OptionLabels;
};

// ====================================================================================================================
// The Style Lab's RIGHT pane: every axis, the curated profiles, and reset.
//
// Writes go straight to UCkDebuggerStyleSettings (SaveConfig + NotifyChanged), which is what makes
// an already-open ECS tree or overlay follow the change. Manual axis edits flip the active profile
// name to "Custom".
// ====================================================================================================================

class SCkStyleLab_ControlsPane : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkStyleLab_ControlsPane) {}
        SLATE_EVENT(FOnCkStyleLab_SelectionChanged, OnSelectionChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Build_AxisRows() -> TSharedRef<SWidget>;
    auto Build_AxisRow(const TSharedPtr<FCkStyleLab_AxisRow>& InAxis) -> TSharedRef<SWidget>;
    auto Build_ProfileControls() -> TSharedRef<SWidget>;

    auto Get_AxisValueLabel(TSharedPtr<FCkStyleLab_AxisRow> InAxis) const -> FText;
    auto OnCycleAxis(TSharedPtr<FCkStyleLab_AxisRow> InAxis, int32 InDirection) -> FReply;
    auto OnResetToClassic() -> FReply;

    auto Apply_Profile(int32 InProfileIndex) -> void;
    auto Notify_SelectionChanged() -> void;

    TArray<TSharedPtr<FCkStyleLab_AxisRow>> _Axes;
    TArray<TSharedPtr<int32>>               _ProfileItems;

    TSharedPtr<SComboBox<TSharedPtr<int32>>> _ProfileCombo;

    FOnCkStyleLab_SelectionChanged _OnSelectionChanged;
};

// ====================================================================================================================
