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

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Shapes)

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
        const auto  Radius = Dims._Radius;

        Builder.AddRow(
            FText::FromString(TEXT("Radius:")),
            [Radius](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), Radius)); },
            CkDebugStyle::Value_Numeric());
    }

    // ---- Box ----
    if (Entity.Has<ck::FFragment_ShapeBox_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Box")));

        const auto& Dims       = Entity.Get<ck::FFragment_ShapeBox_Current>().Get_Dimensions();
        const auto  HalfExtent = Dims._HalfExtents;
        const auto  ConvexR    = Dims._ConvexRadius;

        Builder.AddRow(
            FText::FromString(TEXT("Half Extents:")),
            [HalfExtent](const FCk_Handle&)
            { return FText::FromString(HalfExtent.ToString()); },
            CkDebugStyle::Value_Math());

        Builder.AddRow(
            FText::FromString(TEXT("Convex Radius:")),
            [ConvexR](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), ConvexR)); },
            CkDebugStyle::Value_Numeric());
    }

    // ---- Capsule ----
    if (Entity.Has<ck::FFragment_ShapeCapsule_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Capsule")));

        const auto& Dims       = Entity.Get<ck::FFragment_ShapeCapsule_Current>().Get_Dimensions();
        const auto  HalfHeight = Dims._HalfHeight;
        const auto  Radius     = Dims._Radius;

        Builder.AddRow(
            FText::FromString(TEXT("Half Height:")),
            [HalfHeight](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), HalfHeight)); },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Radius:")),
            [Radius](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), Radius)); },
            CkDebugStyle::Value_Numeric());
    }

    // ---- Cylinder ----
    if (Entity.Has<ck::FFragment_ShapeCylinder_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Cylinder")));

        const auto& Dims       = Entity.Get<ck::FFragment_ShapeCylinder_Current>().Get_Dimensions();
        const auto  HalfHeight = Dims._HalfHeight;
        const auto  Radius     = Dims._Radius;
        const auto  ConvexR    = Dims._ConvexRadius;

        Builder.AddRow(
            FText::FromString(TEXT("Half Height:")),
            [HalfHeight](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), HalfHeight)); },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Radius:")),
            [Radius](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), Radius)); },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Convex Radius:")),
            [ConvexR](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f"), ConvexR)); },
            CkDebugStyle::Value_Numeric());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Shapes::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
