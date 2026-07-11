#pragma once

#include "CkDebuggerCommon/Markers/CkDebug_PmgGizmoSet.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/Layout/SWrapBox.h"

class FCkInspector_SceneNode : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_IconName() const -> FName override { return TEXT("SceneNode"); }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("SceneNode"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("199E70"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 22; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;

private:
    TSharedPtr<SWrapBox> _SiblingsBox;
    int32 _LastSiblingCount = -1;

    // Persistent PMG axis triads: one on the node, one on its parent. The parent
    // key is remembered so a re-parent doesn't strand a stale gizmo.
    FCkDebug_PmgGizmoSet _Gizmos;
    FCk_Handle _LastParentGizmoKey;
};
