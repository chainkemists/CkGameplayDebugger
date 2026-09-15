#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "CkJoltEditor/Cook/CkJoltCook_MeshShapeAudit.h"

class FCk_DebugScene_Target;
class FCkJoltBakeInspectorPreviewAdapter;
struct FCkJoltBakeInspectorAuthoredTestAccess;
class FCkUiView;
class SCkDebug_3dPreviewViewport;
class SBox;
class STextBlock;
class UWorld;

class SCkJoltBakeInspectorPreview final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkJoltBakeInspectorPreview) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkJoltBakeInspectorPreview() override;

    auto Show_Audit(const ck::jolt::cook::FCk_Jolt_MeshShapeAuditResult& InAudit) -> void;
    auto Clear() -> void;
    auto Get_PreviewWorld() const -> UWorld*;
    auto Get_RenderedBounds() const -> FBox;
    auto Teardown() -> void;

    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
    friend struct FCkJoltBakeInspectorAuthoredTestAccess;

    auto Build_AuthoredLegend() -> TSharedRef<SWidget>;
    auto Build_NativeLegendFallback() -> TSharedRef<SWidget>;
    auto Refresh_NativeLegendFallback() -> void;

    TSharedPtr<SCkDebug_3dPreviewViewport> _Viewport;
    TSharedPtr<FCkJoltBakeInspectorPreviewAdapter> _Adapter;
    TSharedPtr<FCk_DebugScene_Target> _Target;
    TSharedPtr<FCkUiView> _LegendView;
    TSharedPtr<SBox> _LegendHost;
    TSharedPtr<STextBlock> _SourceFallbackLabel;
    TSharedPtr<STextBlock> _NormalizedFallbackLabel;
    TSharedPtr<STextBlock> _CookedFallbackLabel;
    FText _SourceLegendText;
    FText _NormalizedLegendText;
    FText _CookedLegendText;
    double _NextLegendPollSeconds{0.0};
    bool _Released{false};
};
