#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

// --------------------------------------------------------------------------------------------------------------------
// Inspector for the ECS Camera feature (CkCamera/Camera). Lights up for the director entity
// (FFragment_Camera_Current) and for any selected layer child entity (FFragment_CameraLayer_*).
//
// Sections: Director (intention / dominant / composed-profile summary / final ViewInfo), Layer Stack
// (one live row per record entry with a blend-weight bar; the persistent base layer is marked), POV pipeline
// (the FPov_State intermediates), and Layer detail.
//
// Tick draws the live camera rig in-world (boom, gizmo+forward, look-at, collision push-in).
// --------------------------------------------------------------------------------------------------------------------

class FCkInspector_Camera : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_IconName() const -> FName override { return TEXT("Camera"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("B8A1E3"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 62; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    // Rebuild the stack section when the modifier count changes (modifiers are added / pruned over time).
    int32 _LastModifierCount = -1;
};
