#pragma once

#include "CkCore/Enums/CkEnums.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_Timer : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_IconName() const -> FName override { return TEXT("Timer"); }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("Timer"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C98500"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 50; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override {}

private:
    // Pending arguments for the Jump / Consume verbs — the timer itself stores none of these, so the
    // rows display what was last typed rather than a live value.
    //
    // Shared boxes rather than plain members: the row lambdas capture them BY VALUE, so a widget that
    // briefly outlives this inspector during panel teardown still reads a live object (the idiom
    // FCkInspector_Probes established for its debug-draw box).
    TSharedRef<float>                _JumpSeconds    = MakeShared<float>(1.0f);
    TSharedRef<ECk_RelativeAbsolute> _JumpMode       = MakeShared<ECk_RelativeAbsolute>(ECk_RelativeAbsolute::Relative);
    TSharedRef<float>                _ConsumeSeconds = MakeShared<float>(1.0f);
};
