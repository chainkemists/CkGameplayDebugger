#include "CkInspector_Shapes.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkShapes/Sphere/CkShapeSphere_Fragment.h"
#include "CkShapes/Box/CkShapeBox_Fragment.h"
#include "CkShapes/Capsule/CkShapeCapsule_Fragment.h"
#include "CkShapes/Cylinder/CkShapeCylinder_Fragment.h"

#include "CkShapes/Sphere/CkShapeSphere_Utils.h"
#include "CkShapes/Box/CkShapeBox_Utils.h"
#include "CkShapes/Capsule/CkShapeCapsule_Utils.h"
#include "CkShapes/Cylinder/CkShapeCylinder_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Shapes)

// =====================================================================================================================

namespace ck_inspector_shapes
{
    // A shape's dimensions are a VALUE: every Request_UpdateDimensions replaces the whole struct, so
    // editing one field has to re-read the others and write a full copy back. These helpers keep that
    // read-modify-write in one place per shape, with the typed handle captured BY VALUE and
    // ck::IsValid-checked on every fire (the row outlives the entity).

    // No shape dimension is meaningfully negative — a negative radius/extent is a malformed body, not
    // an experiment worth firing a request for.
    constexpr auto MinDimension = 0.0f;

    static auto Get_BoxHalfExtents(const FCk_Handle_ShapeBox& InBox) -> FVector
    {
        if (ck::Is_NOT_Valid(InBox))
        { return FVector::ZeroVector; }

        return UCk_Utils_ShapeBox_UE::Get_Dimensions(InBox).Get_HalfExtents();
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
    namespace shapes = ck_inspector_shapes;

    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto MutableEntity = Entity;

    // ---- Sphere ----
    if (Entity.Has<ck::FFragment_ShapeSphere_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Sphere")));

        const auto Sphere = UCk_Utils_ShapeSphere_UE::Cast(MutableEntity);

        Builder.AddNumericRow(
            FText::FromString(TEXT("Radius:")),
            TAttribute<float>::CreateLambda([Sphere]()
            {
                if (ck::Is_NOT_Valid(Sphere)) { return 0.0f; }
                return UCk_Utils_ShapeSphere_UE::Get_Dimensions(Sphere).Get_Radius();
            }),
            [Sphere](float InValue)
            {
                auto Mutable = Sphere;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                UCk_Utils_ShapeSphere_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeSphere_UpdateDimensions{FCk_ShapeSphere_Dimensions{InValue}}, {});
            },
            shapes::MinDimension);
    }

    // ---- Box ----
    if (Entity.Has<ck::FFragment_ShapeBox_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Box")));

        const auto Box = UCk_Utils_ShapeBox_UE::Cast(MutableEntity);

        // Convex radius is a separate field on the SAME dimensions struct, so each editor re-reads the
        // other before writing — otherwise committing one silently resets the other to its default.
        Builder.AddVectorRow(
            FText::FromString(TEXT("Half Extents:")),
            TAttribute<FVector>::CreateLambda([Box]() { return shapes::Get_BoxHalfExtents(Box); }),
            [Box](const FVector& InValue)
            {
                auto Mutable = Box;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                auto NewDims = FCk_ShapeBox_Dimensions{InValue};
                NewDims.Set_ConvexRadius(UCk_Utils_ShapeBox_UE::Get_Dimensions(Mutable).Get_ConvexRadius());

                UCk_Utils_ShapeBox_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeBox_UpdateDimensions{NewDims}, {});
            });

        Builder.AddNumericRow(
            FText::FromString(TEXT("Convex Radius:")),
            TAttribute<float>::CreateLambda([Box]()
            {
                if (ck::Is_NOT_Valid(Box)) { return 0.0f; }
                return UCk_Utils_ShapeBox_UE::Get_Dimensions(Box).Get_ConvexRadius();
            }),
            [Box](float InValue)
            {
                auto Mutable = Box;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                auto NewDims = FCk_ShapeBox_Dimensions{UCk_Utils_ShapeBox_UE::Get_Dimensions(Mutable).Get_HalfExtents()};
                NewDims.Set_ConvexRadius(InValue);

                UCk_Utils_ShapeBox_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeBox_UpdateDimensions{NewDims}, {});
            },
            shapes::MinDimension);
    }

    // ---- Capsule ----
    if (Entity.Has<ck::FFragment_ShapeCapsule_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Capsule")));

        const auto Capsule = UCk_Utils_ShapeCapsule_UE::Cast(MutableEntity);

        Builder.AddNumericRow(
            FText::FromString(TEXT("Half Height:")),
            TAttribute<float>::CreateLambda([Capsule]()
            {
                if (ck::Is_NOT_Valid(Capsule)) { return 0.0f; }
                return UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Capsule).Get_HalfHeight();
            }),
            [Capsule](float InValue)
            {
                auto Mutable = Capsule;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                const auto Radius = UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Mutable).Get_Radius();

                UCk_Utils_ShapeCapsule_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeCapsule_UpdateDimensions{FCk_ShapeCapsule_Dimensions{InValue, Radius}}, {});
            },
            shapes::MinDimension);

        Builder.AddNumericRow(
            FText::FromString(TEXT("Radius:")),
            TAttribute<float>::CreateLambda([Capsule]()
            {
                if (ck::Is_NOT_Valid(Capsule)) { return 0.0f; }
                return UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Capsule).Get_Radius();
            }),
            [Capsule](float InValue)
            {
                auto Mutable = Capsule;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                const auto HalfHeight = UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Mutable).Get_HalfHeight();

                UCk_Utils_ShapeCapsule_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeCapsule_UpdateDimensions{FCk_ShapeCapsule_Dimensions{HalfHeight, InValue}}, {});
            },
            shapes::MinDimension);
    }

    // ---- Cylinder ----
    if (Entity.Has<ck::FFragment_ShapeCylinder_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Cylinder")));

        const auto Cylinder = UCk_Utils_ShapeCylinder_UE::Cast(MutableEntity);

        // Cylinder dimensions carry a third field (convex radius) that the two-arg constructor does not
        // take, so every write restores it explicitly.
        const auto MakeCylinderDims = [](float InHalfHeight, float InRadius, float InConvexRadius)
        {
            auto NewDims = FCk_ShapeCylinder_Dimensions{InHalfHeight, InRadius};
            NewDims.Set_ConvexRadius(InConvexRadius);
            return NewDims;
        };

        Builder.AddNumericRow(
            FText::FromString(TEXT("Half Height:")),
            TAttribute<float>::CreateLambda([Cylinder]()
            {
                if (ck::Is_NOT_Valid(Cylinder)) { return 0.0f; }
                return UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_HalfHeight();
            }),
            [Cylinder, MakeCylinderDims](float InValue)
            {
                auto Mutable = Cylinder;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                const auto Dims = UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Mutable);

                UCk_Utils_ShapeCylinder_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeCylinder_UpdateDimensions{
                        MakeCylinderDims(InValue, Dims.Get_Radius(), Dims.Get_ConvexRadius())}, {});
            },
            shapes::MinDimension);

        Builder.AddNumericRow(
            FText::FromString(TEXT("Radius:")),
            TAttribute<float>::CreateLambda([Cylinder]()
            {
                if (ck::Is_NOT_Valid(Cylinder)) { return 0.0f; }
                return UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_Radius();
            }),
            [Cylinder, MakeCylinderDims](float InValue)
            {
                auto Mutable = Cylinder;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                const auto Dims = UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Mutable);

                UCk_Utils_ShapeCylinder_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeCylinder_UpdateDimensions{
                        MakeCylinderDims(Dims.Get_HalfHeight(), InValue, Dims.Get_ConvexRadius())}, {});
            },
            shapes::MinDimension);

        Builder.AddNumericRow(
            FText::FromString(TEXT("Convex Radius:")),
            TAttribute<float>::CreateLambda([Cylinder]()
            {
                if (ck::Is_NOT_Valid(Cylinder)) { return 0.0f; }
                return UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_ConvexRadius();
            }),
            [Cylinder, MakeCylinderDims](float InValue)
            {
                auto Mutable = Cylinder;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                const auto Dims = UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Mutable);

                UCk_Utils_ShapeCylinder_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeCylinder_UpdateDimensions{
                        MakeCylinderDims(Dims.Get_HalfHeight(), Dims.Get_Radius(), InValue)}, {});
            },
            shapes::MinDimension);
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Shapes::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
