#include "CkInspector_ProbeTraces.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkSpatialQuery/Probe/CkProbeTrace_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ProbeTraces)

static const FLinearColor Color_Trace = FLinearColor(0.6f, 0.4f, 1.0f);

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
            FCkDebuggerStyle::Color_State_Config);
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
            FCkDebuggerStyle::Color_State_Config);
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
        FCkDebuggerStyle::Color_State_Config);

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
            FCkDebuggerStyle::Color_State_Config);
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

    // ---- Overlaps — live badge box stored for in-place updates
    {
        auto OverlapHandles = TArray<FCk_Handle>{};
        if (Entity.Has<TSet<FCk_Probe_OverlapInfo>>())
        {
            for (const auto& Info : Entity.Get<TSet<FCk_Probe_OverlapInfo>>())
            {
                if (ck::IsValid(Info.Get_OtherEntity()))
                {
                    OverlapHandles.Add(Info.Get_OtherEntity());
                }
            }
        }
        _LastOverlapCount = OverlapHandles.Num();
        _OverlapsBox = FCkInspectorWidgetBuilder::MakeBadgeBox(OverlapHandles, SelectionModel);
        Builder.AddWidgetRow(FText::FromString(TEXT("Overlaps:")), _OverlapsBox.ToSharedRef());
    }

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

        if (_OverlapsBox.IsValid())
        {
            const auto CurrentCount = Entity.Has<TSet<FCk_Probe_OverlapInfo>>()
                ? static_cast<int32>(Entity.Get<TSet<FCk_Probe_OverlapInfo>>().Num())
                : 0;
            if (CurrentCount != _LastOverlapCount)
            {
                _LastOverlapCount = CurrentCount;

                auto OverlapHandles = TArray<FCk_Handle>{};
                if (Entity.Has<TSet<FCk_Probe_OverlapInfo>>())
                {
                    for (const auto& Info : Entity.Get<TSet<FCk_Probe_OverlapInfo>>())
                    {
                        if (ck::IsValid(Info.Get_OtherEntity()))
                        {
                            OverlapHandles.Add(Info.Get_OtherEntity());
                        }
                    }
                }
                FCkInspectorWidgetBuilder::PopulateBadgeBox(*_OverlapsBox, OverlapHandles, SelectionModel);
            }
        }
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
