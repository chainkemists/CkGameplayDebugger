#pragma once

#include "CkStyleLabDebugger/Styles/CkStyleLab_AxisMetadata.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SCkStyleLab_SamplePane;
class FCkUiCollection;

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
    auto Get_ProfileView() const -> TSharedPtr<FCkUiView> { return _ProfileView; }
    /** The retained authored surface for every generic axis group and Input HUD native mount. */
    auto Get_ControlsView() const -> TSharedPtr<FCkUiView> { return _ControlsView; }

private:
    auto Build_GroupedAxes() -> TSharedRef<SWidget>;
    auto Configure_InputHudBindings(
        FCkUiView::FDataBindings& InOutData,
        FCkUiView::FActions& InOutActions,
        TWeakPtr<SCkStyleLab_ControlsPane> InWeakPane) -> void;
    auto Build_ProfileControls() -> TSharedRef<SWidget>;
    auto Publish_ProfileRecords() -> void;
    auto Publish_AxisRecords() -> void;
    auto Apply_ProfileByName(const FString& InProfileName) -> void;
    auto Get_ProfileLabel() const -> FText;
    auto Get_ProfileLayoutError() const -> FText;
    auto Get_ControlsLayoutError() const -> FText;
    auto Tick_ProfileFiles(double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType;

    auto Get_AxisValueLabel(TSharedPtr<FCkStyleLab_AxisRow> InAxis) const -> FText;
    auto OnCycleAxis(TSharedPtr<FCkStyleLab_AxisRow> InAxis, int32 InDirection) -> FReply;

    auto Apply_Profile(int32 InProfileIndex) -> void;
    auto Notify_SelectionChanged() -> void;

    TArray<TSharedPtr<FCkStyleLab_AxisRow>> _Axes;
    TMap<FString, TSharedPtr<FCkStyleLab_AxisRow>> _AxesByProperty;
    TMap<ECkStyleLab_Group, TSharedPtr<FCkUiCollection>> _AxisCollections;
    TArray<TSharedPtr<SCkStyleLab_SamplePane>> _GroupPreviews;
    TSharedPtr<FCkUiView> _ProfileView;
    TSharedPtr<FCkUiView> _ControlsView;
    TSharedPtr<FCkUiCollection> _ProfileCollection;
    FString _ProfilePublicationError;
    FString _ControlsPublicationError;
    FString _PublishedProfileName;
    FLinearColor _PublishedProfileAccent = FLinearColor::Transparent;
    FLinearColor _PublishedProfileText = FLinearColor::Transparent;
    bool _HasPublishedProfileRecords = false;

    FOnCkStyleLab_SelectionChanged _OnSelectionChanged;
    bool _ShowAllTones = false;
};

// ====================================================================================================================
