#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkUiCollection;
class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_AudioAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_AudioAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_AudioAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_TracksCollection() const -> TSharedPtr<FCkUiCollection> { return _Tracks; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsTrackAvailable() const -> bool;
    auto Get_IsDirectorAvailable() const -> bool;
    auto Get_TrackStateText() const -> FString;
    auto Get_TrackVolumeText() const -> FString;
    auto Get_TrackFadeSpeedText() const -> FString;
    auto Get_TrackPlaybackText() const -> FString;
    auto Get_TrackVirtualizedText() const -> FString;
    auto Get_DirectorActiveTracksText() const -> FString;
    auto Get_DirectorPriorityText() const -> FString;
    auto Get_DirectorAllFinishedText() const -> FString;
    auto Get_CanRequestTrack() const -> bool;
    auto Get_CanRequestDirector() const -> bool;
    auto Get_CanDebugDraw() const -> bool;
    auto Get_RequestDisabledReasonTrack() const -> FString;
    auto Get_RequestDisabledReasonDirector() const -> FString;
    auto Get_DebugDrawDisabledReason() const -> FString;
    auto Get_IsDebugDrawEnabled() const -> bool;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Get_DiffColor(const FString& InLabel) const -> FLinearColor;

private:
    auto Refresh_Tracks() -> bool;
    auto Build_AuthoredView() -> bool;
    auto Request_Play() -> void;
    auto Request_Stop() -> void;
    auto Commit_Volume(float InVolume) -> void;
    auto Set_DebugDraw(bool InEnabled) -> void;
    auto Request_StopAll() -> void;
    auto Request_StopTrack(const FString& InTrackKey) -> void;

private:
    FCk_Handle _Entity;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiCollection> _Tracks;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Audio : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Audio() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Audio; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4FB3BF"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 95; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_AudioAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
