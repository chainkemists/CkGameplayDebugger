#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "CkJoltEditor/Cook/CkJoltCook_MeshShapeAudit.h"

class FCk_DebugScene_Target;
class FCkJoltBakeInspectorPreviewAdapter;
class SCkDebug_3dPreviewViewport;
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

private:
    auto Teardown() -> void;

    TSharedPtr<SCkDebug_3dPreviewViewport> _Viewport;
    TSharedPtr<FCkJoltBakeInspectorPreviewAdapter> _Adapter;
    TSharedPtr<FCk_DebugScene_Target> _Target;
    TSharedPtr<STextBlock> _SourceLabel;
    TSharedPtr<STextBlock> _NormalizedLabel;
    TSharedPtr<STextBlock> _CookedLabel;
};
