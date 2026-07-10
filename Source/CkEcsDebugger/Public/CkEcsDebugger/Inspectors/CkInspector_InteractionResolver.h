#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/Layout/SWrapBox.h"

class FCkInspector_InteractionResolver : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_IconName() const -> FName override { return TEXT("Interaction"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D95926"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 79; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    auto BuildResolverGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>;

    // Live-updating badge boxes — repopulated in Tick without full panel rebuild
    TArray<TSharedPtr<SWrapBox>> _BestTargetBoxes;
    TSharedPtr<SWrapBox>         _AvailableTargetsBox;

    int32 _LastTotalBestCount = -1;
    int32 _LastAvailableCount = -1;
};
