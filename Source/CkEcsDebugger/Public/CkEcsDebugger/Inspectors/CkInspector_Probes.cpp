#include "CkInspector_Probes.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkSpatialQuery/Probe/CkProbe_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Probes)

// =====================================================================================================================

auto FCkInspector_Probes::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Probe"));
}

auto FCkInspector_Probes::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Probe_UE::Has(Entity);
}

auto FCkInspector_Probes::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildProbeGrid(Entity);
}

// =====================================================================================================================

auto FCkInspector_Probes::BuildProbeGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    auto Probe = UCk_Utils_Probe_UE::Cast(MutableEntity);
    if (ck::Is_NOT_Valid(Probe))
    {
        return Builder.Build(Entity);
    }

    const auto CapturedProbe = Probe;

    const auto ProbeName = UCk_Utils_Probe_UE::Get_Name(Probe);
    const auto ProbeNameStr = ProbeName.IsValid() ? ProbeName.GetTagName().ToString() : TEXT("Unnamed");

    // Name — a gameplay tag, so it takes the shared tag value role.
    Builder.AddRow(
        FText::FromString(TEXT("Name:")),
        [ProbeNameStr](const FCk_Handle& E) { return FText::FromString(ProbeNameStr); },
        CkStyle::Value_Tag());

    // Enabled/Disabled state
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda([CapturedProbe]()
        {
            if (ck::Is_NOT_Valid(CapturedProbe)) { return FText::GetEmpty(); }
            const auto State = UCk_Utils_Probe_UE::Get_IsEnabledDisabled(CapturedProbe);
            return FText::FromString(State == ECk_EnableDisable::Enable ? TEXT("Enabled") : TEXT("Disabled"));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedProbe]()
        {
            if (ck::Is_NOT_Valid(CapturedProbe)) { return ECk_Tone::Neutral; }
            return UCk_Utils_Probe_UE::Get_IsEnabledDisabled(CapturedProbe) == ECk_EnableDisable::Enable
                ? ECk_Tone::Ok : ECk_Tone::Neutral;
        }));

    // Overlaps — live badge box stored for in-place updates
    {
        auto OverlapHandles = TArray<FCk_Handle>{};
        if (ck::IsValid(Probe))
        {
            for (const auto& Info : UCk_Utils_Probe_UE::Get_CurrentOverlaps(Probe))
            {
                if (ck::IsValid(Info.Get_OtherEntity()))
                {
                    OverlapHandles.Add(Info.Get_OtherEntity());
                }
            }
        }
        _LastOverlapCount = OverlapHandles.Num();
        _OverlapsBox = FCkInspectorWidgetBuilder::MakeBadgeBox(OverlapHandles);
        Builder.AddWidgetRow(FText::FromString(TEXT("Overlaps:")), _OverlapsBox.ToSharedRef());
    }

    // Response policy
    Builder.AddRow(
        FText::FromString(TEXT("Response:")),
        [](const FCk_Handle& E)
        {
            auto MutableE = E;
            const auto P = UCk_Utils_Probe_UE::Cast(MutableE);
            if (ck::Is_NOT_Valid(P)) { return FText::GetEmpty(); }
            const auto Policy = UCk_Utils_Probe_UE::Get_ResponsePolicy(P);
            return FText::FromString(Policy == ECk_ProbeResponse_Policy::Notify ? TEXT("Notify") : TEXT("Silent"));
        },
        CkStyle::State_Config());

    // Motion type
    Builder.AddRow(
        FText::FromString(TEXT("Motion:")),
        [](const FCk_Handle& E)
        {
            auto MutableE = E;
            const auto P = UCk_Utils_Probe_UE::Cast(MutableE);
            if (ck::Is_NOT_Valid(P)) { return FText::GetEmpty(); }
            const auto MotionType = UCk_Utils_Probe_UE::Get_MotionType(P);
            switch (MotionType)
            {
            case ECk_MotionType::Static:    return FText::FromString(TEXT("Static"));
            case ECk_MotionType::Kinematic: return FText::FromString(TEXT("Kinematic"));
            case ECk_MotionType::Dynamic:   return FText::FromString(TEXT("Dynamic"));
            default:                        return FText::FromString(TEXT("Unknown"));
            }
        },
        CkStyle::State_Config());

    // Motion quality
    Builder.AddRow(
        FText::FromString(TEXT("Quality:")),
        [](const FCk_Handle& E)
        {
            auto MutableE = E;
            const auto P = UCk_Utils_Probe_UE::Cast(MutableE);
            if (ck::Is_NOT_Valid(P)) { return FText::GetEmpty(); }
            const auto Quality = UCk_Utils_Probe_UE::Get_MotionQuality(P);
            return FText::FromString(Quality == ECk_MotionQuality::Discrete ? TEXT("Discrete") : TEXT("LinearCast (CCD)"));
        },
        CkStyle::State_Config());

    // Filter
    Builder.AddRow(
        FText::FromString(TEXT("Filter:")),
        [](const FCk_Handle& E)
        {
            auto MutableE = E;
            const auto P = UCk_Utils_Probe_UE::Cast(MutableE);
            if (ck::Is_NOT_Valid(P)) { return FText::GetEmpty(); }
            const auto Filter = UCk_Utils_Probe_UE::Get_Filter(P);
            if (Filter.IsEmpty()) { return FText::FromString(TEXT("(Empty)")); }
            return FText::FromString(Filter.ToString());
        },
        CkStyle::TextDim());

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Probes::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    auto MutableEntity = Entity;
    if (auto Probe = UCk_Utils_Probe_UE::Cast(MutableEntity); ck::IsValid(Probe))
    {
        UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(Probe, ECk_EnableDisable::Enable, {});
        LastInspectedEntity = Entity;

        if (_OverlapsBox.IsValid())
        {
            const auto CurrentCount = static_cast<int32>(UCk_Utils_Probe_UE::Get_CurrentOverlaps(Probe).Num());
            if (CurrentCount != _LastOverlapCount)
            {
                _LastOverlapCount = CurrentCount;

                auto OverlapHandles = TArray<FCk_Handle>{};
                for (const auto& Info : UCk_Utils_Probe_UE::Get_CurrentOverlaps(Probe))
                {
                    if (ck::IsValid(Info.Get_OtherEntity()))
                    {
                        OverlapHandles.Add(Info.Get_OtherEntity());
                    }
                }
                FCkInspectorWidgetBuilder::PopulateBadgeBox(*_OverlapsBox, OverlapHandles);
            }
        }
    }
}

auto FCkInspector_Probes::OnDeactivated() -> void
{
    DisableDebugDraw();
}

auto FCkInspector_Probes::DisableDebugDraw() -> void
{
    if (ck::IsValid(LastInspectedEntity) && UCk_Utils_Probe_UE::Has(LastInspectedEntity))
    {
        auto Probe = UCk_Utils_Probe_UE::Cast(LastInspectedEntity);
        if (ck::IsValid(Probe))
        {
            UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(Probe, ECk_EnableDisable::Disable, {});
        }
    }
    LastInspectedEntity = FCk_Handle();
}
