#pragma once

#include "CkDebuggerCommon/Viewport/SCkDebug_3dPreviewViewport.h"
#include "CkJoltEditor/Cook/CkJoltCook_MeshShapeAudit.h"

class FCk_DebugScene_Target;

namespace ck::jolt_bake_inspector
{
auto Get_CookedPreviewLabel(const ck::jolt::cook::FCk_Jolt_MeshShapeAuditResult& InAudit) -> const TCHAR*;
}

class FCkJoltBakeInspectorPreviewAdapter final : public ICkDebug3dPreviewAdapter
{
public:
    auto Set_Target(TSharedPtr<FCk_DebugScene_Target> InTarget) -> void;
    auto Reconcile(const ck::jolt::cook::FCk_Jolt_MeshShapeAuditResult& InAudit) -> void;
    auto Reset() -> void;

    auto Get_FrameBounds(ECkDebug3dFrameTarget InTarget) const -> FBox override;
    auto Get_SelectionCenter() const -> TOptional<FVector> override;
    auto Get_Capabilities() const -> ECkDebug3dViewportCapability override;
    auto On_Pick(const FCkDebug3dCursorRay&) -> void override {}
    auto On_ViewportTeardown() -> void override;

private:
    TSharedPtr<FCk_DebugScene_Target> _Target;
};
