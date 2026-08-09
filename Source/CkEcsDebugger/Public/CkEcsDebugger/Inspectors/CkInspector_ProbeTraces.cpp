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

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ProbeTraces)

// =====================================================================================================================

namespace ck_inspector_probetraces
{
    static auto Get_DirectionAndLength(const FCk_Handle& InEntity) -> FVector
    {
        if (ck::Is_NOT_Valid(InEntity))
        { return FVector::ZeroVector; }

        if (InEntity.Has<ck::FFragment_ProbeTrace_RayCast>())
        { return InEntity.Get<ck::FFragment_ProbeTrace_RayCast>().Get_DirectionAndLength(); }

        if (InEntity.Has<ck::FFragment_ProbeTrace_ShapeCast>())
        { return InEntity.Get<ck::FFragment_ProbeTrace_ShapeCast>().Get_DirectionAndLength(); }

        return FVector::ZeroVector;
    }

    // Three fixed-precision components in X/Y/Z order — same 3 decimals FVector::ToString printed —
    // so the direction row's axis coloring lines up with the Transform inspector.
    static auto Make_AxisComponents(
        const FCk_Handle& InEntity)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InEntity, Axis]()
            {
                return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), Get_DirectionAndLength(InEntity)[Axis]));
            }));
        }

        return Components;
    }
}

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
        CkStyle::Value_Enum());

    // ---- Direction

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Direction:")),
        ck_inspector_probetraces::Make_AxisComponents(Entity));

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
        CkStyle::State_Config());

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
            CkStyle::State_Config());
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
        CkStyle::TextDim());

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
        _OverlapsBox = FCkInspectorWidgetBuilder::MakeBadgeBox(OverlapHandles);
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
                FCkInspectorWidgetBuilder::PopulateBadgeBox(*_OverlapsBox, OverlapHandles);
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
