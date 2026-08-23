#pragma once

#include "CkStyleLabDebugger/Styles/CkStyleLab_AxisMetadata.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SCkStyleLab_SamplePane;

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
    ECkStyleLab_Group Group = ECkStyleLab_Group::WorkbenchSurfaces;

    // Option values and their labels are snapshotted at construction — the enum's shape cannot
    // change while the editor runs, and snapshotting keeps a UObject pointer out of the row.
    TArray<int64>    Options;
    TArray<FText>    OptionLabels;
};

// ====================================================================================================================
// The Style Lab's single grouped document: curated profiles, every generic axis, and feature-local
// controls. Each group owns the focused preview that demonstrates its settings.
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

    auto RequestPreviewRebuilds() -> void;
    auto Get_ShowAllTones() const -> bool { return _ShowAllTones; }
    auto Set_ShowAllTones(bool InShowAllTones) -> void;

    auto Get_AxisCount() const -> int32 { return _Axes.Num(); }
    auto Get_GroupPreviewCount() const -> int32 { return _GroupPreviews.Num(); }

private:
    auto Build_GroupedAxes() -> TSharedRef<SWidget>;
    auto Build_AxisGroup(const FCkStyleLab_GroupMetadata& InGroup) -> TSharedRef<SWidget>;
    auto Build_InputHudGroup(const FCkStyleLab_GroupMetadata& InGroup) -> TSharedRef<SWidget>;
    auto Build_AxisRow(const TSharedPtr<FCkStyleLab_AxisRow>& InAxis) -> TSharedRef<SWidget>;
    auto Build_ProfileControls() -> TSharedRef<SWidget>;

    auto Get_AxisValueLabel(TSharedPtr<FCkStyleLab_AxisRow> InAxis) const -> FText;
    auto OnCycleAxis(TSharedPtr<FCkStyleLab_AxisRow> InAxis, int32 InDirection) -> FReply;

    auto Apply_Profile(int32 InProfileIndex) -> void;
    auto Notify_SelectionChanged() -> void;

    TArray<TSharedPtr<FCkStyleLab_AxisRow>> _Axes;
    TArray<TSharedPtr<SCkStyleLab_SamplePane>> _GroupPreviews;

    FOnCkStyleLab_SelectionChanged _OnSelectionChanged;
    bool _ShowAllTones = false;
};

// ====================================================================================================================
