#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_FogOfWar : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::FogOfWar; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 66; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    // Scratch payload for the "Reveal Here" button. A reveal request carries a location + radius that
    // exist nowhere in fog state, so there is no live value for those rows to read back — the
    // inspector owns them.
    //
    // Shared boxes rather than plain members: the rows' attribute lambdas capture them BY VALUE, so a
    // widget that briefly outlives this inspector during panel teardown still reads a live object.
    // Sticky across selections on purpose — it is a probe position, not per-entity state.
    TSharedRef<FVector> _RevealLocation = MakeShared<FVector>(FVector::ZeroVector);

    // 0 = use the fog params' own RevealRadius (CkFogOfWar_Fragment_Data.h:181).
    TSharedRef<float> _RevealRadius = MakeShared<float>(0.0f);
};
