#include "CkInspector_Shapes.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkShapes/Sphere/CkShapeSphere_Fragment.h"
#include "CkShapes/Box/CkShapeBox_Fragment.h"
#include "CkShapes/Capsule/CkShapeCapsule_Fragment.h"
#include "CkShapes/Cylinder/CkShapeCylinder_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Shapes)

// =====================================================================================================================

namespace ck_inspector_shapes
{
    // Three fixed-precision components in X/Y/Z order — same 3 decimals FVector::ToString printed —
    // so the extents row's axis coloring lines up with the Transform inspector. The handle is
    // captured by value and re-validated per read, like every other live inspector row.
    static auto Make_AxisComponents(
        const FCk_Handle& InEntity,
        TFunction<FVector(const FCk_Handle&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InEntity, InProjector, Axis]()
            {
                return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), InProjector(InEntity)[Axis]));
            }));
        }

        return Components;
    }

    static auto Get_BoxHalfExtents(const FCk_Handle& InEntity) -> FVector
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_ShapeBox_Current>())
        { return FVector::ZeroVector; }

        return InEntity.Get<ck::FFragment_ShapeBox_Current>().Get_Dimensions().Get_HalfExtents();
    }
}

// =====================================================================================================================

auto FCkInspector_Shapes::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Shapes"));
}

auto FCkInspector_Shapes::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_ShapeSphere_Current,
        ck::FFragment_ShapeBox_Current,
        ck::FFragment_ShapeCapsule_Current,
        ck::FFragment_ShapeCylinder_Current>();
}

// =====================================================================================================================

auto FCkInspector_Shapes::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- Sphere ----
    if (Entity.Has<ck::FFragment_ShapeSphere_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Sphere")));

        const auto& Dims = Entity.Get<ck::FFragment_ShapeSphere_Current>().Get_Dimensions();
        const auto  Radius = Dims.Get_Radius();

        Builder.AddRow(
            FText::FromString(TEXT("Radius:")),
            [Radius](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), Radius)); },
            CkStyle::Value_Numeric());
    }

    // ---- Box ----
    if (Entity.Has<ck::FFragment_ShapeBox_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Box")));

        const auto& Dims    = Entity.Get<ck::FFragment_ShapeBox_Current>().Get_Dimensions();
        const auto  ConvexR = Dims.Get_ConvexRadius();

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Half Extents:")),
            ck_inspector_shapes::Make_AxisComponents(Entity, &ck_inspector_shapes::Get_BoxHalfExtents));

        Builder.AddRow(
            FText::FromString(TEXT("Convex Radius:")),
            [ConvexR](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), ConvexR)); },
            CkStyle::Value_Numeric());
    }

    // ---- Capsule ----
    if (Entity.Has<ck::FFragment_ShapeCapsule_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Capsule")));

        const auto& Dims       = Entity.Get<ck::FFragment_ShapeCapsule_Current>().Get_Dimensions();
        const auto  HalfHeight = Dims.Get_HalfHeight();
        const auto  Radius     = Dims.Get_Radius();

        Builder.AddRow(
            FText::FromString(TEXT("Half Height:")),
            [HalfHeight](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), HalfHeight)); },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Radius:")),
            [Radius](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), Radius)); },
            CkStyle::Value_Numeric());
    }

    // ---- Cylinder ----
    if (Entity.Has<ck::FFragment_ShapeCylinder_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Cylinder")));

        const auto& Dims       = Entity.Get<ck::FFragment_ShapeCylinder_Current>().Get_Dimensions();
        const auto  HalfHeight = Dims.Get_HalfHeight();
        const auto  Radius     = Dims.Get_Radius();
        const auto  ConvexR    = Dims.Get_ConvexRadius();

        Builder.AddRow(
            FText::FromString(TEXT("Half Height:")),
            [HalfHeight](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), HalfHeight)); },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Radius:")),
            [Radius](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), Radius)); },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Convex Radius:")),
            [ConvexR](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), ConvexR)); },
            CkStyle::Value_Numeric());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Shapes::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
