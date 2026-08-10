#pragma once

#include "CkCore/Enums/CkEnums.h"

#include "CkDebuggerCommon/Markers/CkDebug_PmgGizmoSet.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_Transform : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_IconName() const -> FName override { return TEXT("Transform"); }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("Transform"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8B93A1"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 10; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;

private:
    // Persistent PMG axis triad on the inspected entity — created on first Tick,
    // moved thereafter, destroyed on deactivation (selection change / panel teardown).
    FCkDebug_PmgGizmoSet _Gizmos;

    // Interpolation is an optional sibling feature, so its rows are a STRUCTURAL section: Tick asks for
    // a rebuild only when its presence flips, never per frame.
    bool _HadInterpolation = false;

    // Pending arguments for the edit rows. Shared boxes, not plain members: the row lambdas capture
    // them BY VALUE so a widget that briefly outlives this inspector during panel teardown still reads
    // a live object (the idiom FCkInspector_Probes established for its debug-draw box).
    //
    // _EditSpace is shared by the three setters and the two offset verbs, which is what makes one
    // "Space:" dropdown honest for all of them.
    TSharedRef<ECk_LocalWorld> _EditSpace       = MakeShared<ECk_LocalWorld>(ECk_LocalWorld::World);
    TSharedRef<FVector>        _LocationOffset  = MakeShared<FVector>(FVector::ZeroVector);
    TSharedRef<FRotator>       _RotationOffset  = MakeShared<FRotator>(FRotator::ZeroRotator);
    TSharedRef<FVector>        _InterpGoalLoc   = MakeShared<FVector>(FVector::ZeroVector);
    TSharedRef<FRotator>       _InterpGoalRot   = MakeShared<FRotator>(FRotator::ZeroRotator);
};