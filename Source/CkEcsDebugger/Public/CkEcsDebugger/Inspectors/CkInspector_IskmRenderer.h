#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class SCkInspector_IskmRendererAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_IskmRendererAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(bool, RendererDataDiffMarked)
        SLATE_ARGUMENT(bool, AnimCollectionDiffMarked)
        SLATE_ARGUMENT(bool, SequencesDiffMarked)
        SLATE_ARGUMENT(bool, SubmeshesDiffMarked)
        SLATE_ARGUMENT(bool, DefaultAnimInstanceDiffMarked)
        SLATE_ARGUMENT(bool, CustomDataSlotsDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_IskmRendererAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_RendererDataText() const -> FString;
    auto Get_AnimCollectionText() const -> FString;
    auto Get_SequencesText() const -> FString;
    auto Get_SubmeshesText() const -> FString;
    auto Get_DefaultAnimInstanceText() const -> FString;
    auto Get_CustomDataSlotsText() const -> FString;
    auto Is_RendererDataDiffMarked() const -> bool { return _RendererDataDiffMarked; }
    auto Is_AnimCollectionDiffMarked() const -> bool { return _AnimCollectionDiffMarked; }
    auto Is_SequencesDiffMarked() const -> bool { return _SequencesDiffMarked; }
    auto Is_SubmeshesDiffMarked() const -> bool { return _SubmeshesDiffMarked; }
    auto Is_DefaultAnimInstanceDiffMarked() const -> bool { return _DefaultAnimInstanceDiffMarked; }
    auto Is_CustomDataSlotsDiffMarked() const -> bool { return _CustomDataSlotsDiffMarked; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _RendererDataDiffMarked = false;
    bool _AnimCollectionDiffMarked = false;
    bool _SequencesDiffMarked = false;
    bool _SubmeshesDiffMarked = false;
    bool _DefaultAnimInstanceDiffMarked = false;
    bool _CustomDataSlotsDiffMarked = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_IskmRenderer : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_IskmRenderer() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::IsmRenderer; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9A648"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 55; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_IskmRendererAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
