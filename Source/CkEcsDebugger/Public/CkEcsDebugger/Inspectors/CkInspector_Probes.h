#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment_Data.h"

#include "Widgets/Layout/SWrapBox.h"

class FCkInspector_Probes : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_IconName() const -> FName override { return TEXT("Probe"); }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("Probe"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D55181"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 70; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;

private:
    auto BuildProbeGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
    auto DisableDebugDraw() -> void;

    FCk_Handle LastInspectedEntity;
    TSharedPtr<SWrapBox> _OverlapsBox;
    int32 _LastOverlapCount = -1;

    // The intent behind the "Debug Draw:" toggle row. Tick re-asserts it on the inspected probe (the
    // request is idempotent), so the row IS the switch that used to be an unconditional Enable.
    // Sticky across selections on purpose: it is a viewing preference, not per-entity state.
    //
    // A shared box rather than a plain bool: the row's attribute lambdas capture it BY VALUE, so a
    // widget that briefly outlives this inspector during panel teardown reads a live object.
    TSharedRef<bool> _DebugDrawEnabled = MakeShared<bool>(true);
};
