#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "CkInteraction/InteractTarget/CkInteractTarget_Fragment_Data.h"

#include "Widgets/Layout/SWrapBox.h"

class FCkInspector_InteractTarget : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Interaction; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D95926"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 61; }
    auto IsMultiSection() const -> bool override { return true; }
    auto Get_InspectorSections(const FCk_Handle& Entity) -> TArray<FInspectorSection> override;
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;

private:
    auto BuildTargetWidget(const FCk_Handle_InteractTarget& InTarget) -> TSharedRef<SWidget>;

    struct FTargetState
    {
        FCk_Handle_InteractTarget Handle;
        TSharedPtr<SWrapBox> InteractionsBox;
        int32 LastInteractionCount = 0;
    };

    TArray<FTargetState> _TargetStates;
    int32 _LastTargetCount = 0;
};
