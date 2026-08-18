#pragma once

#include "CkTween/CkTween_Fragment_Data.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_Tween : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Tween; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FBFE8"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 120; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    // Pending argument for the Stop verb — the tween stores no such field, so the dropdown holds what
    // the next Stop will carry. Shared box, not a plain member: the row lambdas capture it BY VALUE so a
    // widget that briefly outlives this inspector during panel teardown still reads a live object.
    TSharedRef<ECk_TweenStopBehavior> _StopBehavior =
        MakeShared<ECk_TweenStopBehavior>(ECk_TweenStopBehavior::DoNothing);
};
