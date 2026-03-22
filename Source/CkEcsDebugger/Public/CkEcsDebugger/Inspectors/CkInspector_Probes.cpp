#include "CkInspector_Probes.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkSpatialQuery/Probe/CkProbe_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Probes)

static const FLinearColor Color_Probe = FLinearColor(0.0f, 0.8f, 1.0f);
static const FLinearColor Color_Probe_Enabled = FLinearColor(0.0f, 1.0f, 0.5f);
static const FLinearColor Color_Probe_Disabled = FLinearColor(1.0f, 0.5f, 0.5f);
static const FLinearColor Color_Probe_Overlapping = FLinearColor(1.0f, 0.95f, 0.0f);
static const FLinearColor Color_Probe_Config = FLinearColor(1.0f, 0.8f, 0.01f);

// =====================================================================================================================

auto FCkInspector_Probes::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Probes"));
}

auto FCkInspector_Probes::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Probe_UE::Has(Entity);
}

auto FCkInspector_Probes::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildProbeGrid(Entity, FString());
}

auto FCkInspector_Probes::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return BuildProbeGrid(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_Probes::BuildProbeGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    auto WeakSelectionModel = SelectionModel;

    auto MutableEntity = Entity;
    auto Probe = UCk_Utils_Probe_UE::Cast(MutableEntity);
    if (ck::Is_NOT_Valid(Probe))
    {
        return Builder.Build(Entity, InFilter);
    }

    const auto ProbeName = UCk_Utils_Probe_UE::Get_Name(Probe);
    const auto ProbeNameStr = ProbeName.IsValid() ? ProbeName.GetTagName().ToString() : TEXT("Unnamed");

    // Name
    Builder.AddRow(
        FText::FromString(TEXT("Name:")),
        [ProbeNameStr](const FCk_Handle& E) { return FText::FromString(ProbeNameStr); },
        Color_Probe);

    // Enabled/Disabled state
    Builder.AddConditionalRow(
        FText::FromString(TEXT("State:")),
        [](const FCk_Handle& E)
        {
            auto MutableE = E;
            const auto P = UCk_Utils_Probe_UE::Cast(MutableE);
            if (ck::Is_NOT_Valid(P)) { return FText::GetEmpty(); }
            const auto State = UCk_Utils_Probe_UE::Get_IsEnabledDisabled(P);
            return FText::FromString(State == ECk_EnableDisable::Enable ? TEXT("Enabled") : TEXT("Disabled"));
        },
        [](const FCk_Handle& E)
        {
            auto MutableE = E;
            const auto P = UCk_Utils_Probe_UE::Cast(MutableE);
            if (ck::Is_NOT_Valid(P)) { return FCkDebuggerStyle::Color_None; }
            return UCk_Utils_Probe_UE::Get_IsEnabledDisabled(P) == ECk_EnableDisable::Enable
                ? Color_Probe_Enabled : Color_Probe_Disabled;
        });

    // Overlap status
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Overlap:")),
        [](const FCk_Handle& E)
        {
            auto MutableE = E;
            const auto P = UCk_Utils_Probe_UE::Cast(MutableE);
            if (ck::Is_NOT_Valid(P)) { return FText::GetEmpty(); }
            const auto bOverlapping = UCk_Utils_Probe_UE::Get_IsOverlapping(P);
            if (NOT bOverlapping) { return FText::FromString(TEXT("None")); }
            const auto Overlaps = UCk_Utils_Probe_UE::Get_CurrentOverlaps(P);
            return FText::FromString(FString::Printf(TEXT("Yes (%d entities)"), Overlaps.Num()));
        },
        [](const FCk_Handle& E)
        {
            auto MutableE = E;
            const auto P = UCk_Utils_Probe_UE::Cast(MutableE);
            if (ck::Is_NOT_Valid(P)) { return FCkDebuggerStyle::Color_None; }
            return UCk_Utils_Probe_UE::Get_IsOverlapping(P)
                ? Color_Probe_Overlapping : FCkDebuggerStyle::Color_None;
        });

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
        Color_Probe_Config);

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
        Color_Probe_Config);

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
        Color_Probe_Config);

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
        FCkDebuggerStyle::Color_Text_Secondary);

    return Builder.Build(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_Probes::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // Enable debug drawing while the entity is inspected
    auto MutableEntity = const_cast<FCk_Handle&>(Entity);
    if (auto Probe = UCk_Utils_Probe_UE::Cast(MutableEntity); ck::IsValid(Probe))
    {
        UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(Probe, ECk_EnableDisable::Enable);
    }
}
