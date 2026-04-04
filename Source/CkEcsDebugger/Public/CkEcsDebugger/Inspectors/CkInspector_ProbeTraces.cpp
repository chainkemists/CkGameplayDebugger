#include "CkInspector_ProbeTraces.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkSpatialQuery/Probe/CkProbeTrace_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ProbeTraces)

static const FLinearColor Color_Trace = FLinearColor(0.6f, 0.4f, 1.0f);
static const FLinearColor Color_Trace_Config = FLinearColor(1.0f, 0.8f, 0.01f);
static const FLinearColor Color_Trace_Overlapping = FLinearColor(1.0f, 0.95f, 0.0f);

// =====================================================================================================================

auto FCkInspector_ProbeTraces::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Probe Trace"));
}

auto FCkInspector_ProbeTraces::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_ProbeTrace_UE::Has(Entity);
}

auto FCkInspector_ProbeTraces::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildTraceGrid(Entity);
}

// =====================================================================================================================

auto FCkInspector_ProbeTraces::BuildTraceGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    const auto IsRayCast = Entity.Has<ck::FFragment_ProbeTrace_RayCast>();
    const auto IsShapeCast = Entity.Has<ck::FFragment_ProbeTrace_ShapeCast>();

    if (NOT IsRayCast && NOT IsShapeCast)
    {
        return Builder.Build(Entity);
    }

    // ---- Type

    Builder.AddRow(
        FText::FromString(TEXT("Type:")),
        [IsRayCast](const FCk_Handle& E)
        {
            return FText::FromString(IsRayCast ? TEXT("RayCast") : TEXT("ShapeCast"));
        },
        Color_Trace);

    // ---- Direction

    if (IsRayCast)
    {
        Builder.AddRow(
            FText::FromString(TEXT("Direction:")),
            [](const FCk_Handle& E)
            {
                const auto& Settings = E.Get<ck::FFragment_ProbeTrace_RayCast>();
                return FText::FromString(Settings.Get_DirectionAndLength().ToString());
            },
            Color_Trace_Config);
    }
    else
    {
        Builder.AddRow(
            FText::FromString(TEXT("Direction:")),
            [](const FCk_Handle& E)
            {
                const auto& Settings = E.Get<ck::FFragment_ProbeTrace_ShapeCast>();
                return FText::FromString(Settings.Get_DirectionAndLength().ToString());
            },
            Color_Trace_Config);
    }

    // ---- Trace Policy

    Builder.AddRow(
        FText::FromString(TEXT("Policy:")),
        [IsRayCast](const FCk_Handle& E)
        {
            const auto Policy = IsRayCast
                ? E.Get<ck::FFragment_ProbeTrace_RayCast>().Get_TracePolicy()
                : E.Get<ck::FFragment_ProbeTrace_ShapeCast>().Get_TracePolicy();
            return FText::FromString(Policy == ECk_ProbeTrace_Policy::Single ? TEXT("Single") : TEXT("Multi"));
        },
        Color_Trace_Config);

    // ---- Shape (ShapeCast only)

    if (IsShapeCast)
    {
        Builder.AddRow(
            FText::FromString(TEXT("Shape:")),
            [](const FCk_Handle& E)
            {
                const auto& Settings = E.Get<ck::FFragment_ProbeTrace_ShapeCast>();
                switch (Settings.Get_Shape().Get_ShapeType())
                {
                case ECk_Shape_Type::Box:      return FText::FromString(TEXT("Box"));
                case ECk_Shape_Type::Sphere:   return FText::FromString(TEXT("Sphere"));
                case ECk_Shape_Type::Capsule:  return FText::FromString(TEXT("Capsule"));
                case ECk_Shape_Type::Cylinder: return FText::FromString(TEXT("Cylinder"));
                default:                       return FText::FromString(TEXT("Unknown"));
                }
            },
            Color_Trace_Config);
    }

    // ---- Filter

    Builder.AddRow(
        FText::FromString(TEXT("Filter:")),
        [IsRayCast](const FCk_Handle& E)
        {
            const auto Filter = IsRayCast
                ? E.Get<ck::FFragment_ProbeTrace_RayCast>().Get_Filter()
                : E.Get<ck::FFragment_ProbeTrace_ShapeCast>().Get_Filter();
            if (Filter.IsEmpty()) { return FText::FromString(TEXT("(Empty)")); }
            return FText::FromString(Filter.ToString());
        },
        FCkDebuggerStyle::Color_Text_Secondary);

    // ---- Overlaps

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Overlaps:")),
        [](const FCk_Handle& E)
        {
            if (NOT E.Has<TSet<FCk_Probe_OverlapInfo>>())
            { return FText::FromString(TEXT("None")); }
            const auto& Overlaps = E.Get<TSet<FCk_Probe_OverlapInfo>>();
            if (Overlaps.IsEmpty())
            { return FText::FromString(TEXT("None")); }
            return FText::FromString(ck::Format_UE(TEXT("{} entities"), Overlaps.Num()));
        },
        [](const FCk_Handle& E)
        {
            if (E.Has<TSet<FCk_Probe_OverlapInfo>>() && NOT E.Get<TSet<FCk_Probe_OverlapInfo>>().IsEmpty())
            { return Color_Trace_Overlapping; }
            return FCkDebuggerStyle::Color_None;
        });

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_ProbeTraces::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::IsValid(Entity) && UCk_Utils_ProbeTrace_UE::Has(Entity))
    {
        auto MutableEntity = Entity;
        MutableEntity.AddOrGet<ck::FTag_ProbeTrace_DebugDraw>();
        LastInspectedEntity = Entity;
    }
}

auto FCkInspector_ProbeTraces::OnDeactivated() -> void
{
    DisableDebugDraw();
}

auto FCkInspector_ProbeTraces::DisableDebugDraw() -> void
{
    if (ck::IsValid(LastInspectedEntity) && UCk_Utils_ProbeTrace_UE::Has(LastInspectedEntity))
    {
        LastInspectedEntity.Try_Remove<ck::FTag_ProbeTrace_DebugDraw>();
    }
    LastInspectedEntity = FCk_Handle();
}
