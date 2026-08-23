#include "CkDebuggerCommon/Picker/CkDebug_ViewportComponentPicker.h"
#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"

#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugViewportComponentPicker_ViewportPixel,
    "Ck.DebuggerCommon.ViewportComponentPicker.ViewportPixel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugViewportComponentPicker_ViewportPixel::RunTest(const FString& Parameters)
{
    const auto Size = FIntPoint{1600, 900};
    const auto TopLeft = ck::DebugViewportView::TryGet_ViewportPixel(FVector2D::ZeroVector, Size);
    TestTrue(TEXT("Top-left is inside the viewport"), TopLeft.IsSet());
    if (TopLeft.IsSet())
    {
        TestEqual(TEXT("Top-left maps to zero pixels"), TopLeft.GetValue(), FVector2D::ZeroVector);
    }

    const auto Interior = ck::DebugViewportView::TryGet_ViewportPixel(FVector2D{0.25, 0.5}, Size);
    TestTrue(TEXT("Interior normalized position is accepted"), Interior.IsSet());
    if (Interior.IsSet())
    {
        TestEqual(
            TEXT("Interior position respects non-square dimensions"),
            Interior.GetValue(),
            FVector2D{400.0, 450.0});
    }

    TestFalse(
        TEXT("Right edge is outside the half-open viewport"),
        ck::DebugViewportView::TryGet_ViewportPixel(FVector2D{1.0, 0.5}, Size).IsSet());
    TestFalse(
        TEXT("Bottom edge is outside the half-open viewport"),
        ck::DebugViewportView::TryGet_ViewportPixel(FVector2D{0.5, 1.0}, Size).IsSet());
    TestFalse(
        TEXT("Non-positive viewport dimensions are rejected"),
        ck::DebugViewportView::TryGet_ViewportPixel(FVector2D::ZeroVector, FIntPoint::ZeroValue).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugViewportComponentPicker_RayBounds,
    "Ck.DebuggerCommon.ViewportComponentPicker.RayBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugViewportComponentPicker_RayBounds::RunTest(const FString& Parameters)
{
    const auto Bounds = FBox{FVector{10.0, -5.0, -5.0}, FVector{20.0, 5.0, 5.0}};

    const auto FrontHit = FCkDebug_ViewportComponentPicker::TryIntersect_RayBounds(
        Bounds,
        FVector::ZeroVector,
        FVector::ForwardVector);
    TestTrue(TEXT("A forward ray reaches the front face"), FrontHit.IsSet());
    if (FrontHit.IsSet())
    {
        TestEqual(TEXT("The nearest front-face distance is returned"), FrontHit.GetValue(), 10.0f);
    }

    const auto InsideHit = FCkDebug_ViewportComponentPicker::TryIntersect_RayBounds(
        Bounds,
        FVector{15.0, 0.0, 0.0},
        FVector::UpVector);
    TestTrue(TEXT("A ray starting inside the bounds intersects immediately"), InsideHit.IsSet());
    if (InsideHit.IsSet())
    {
        TestEqual(TEXT("Inside rays return zero distance"), InsideHit.GetValue(), 0.0f);
    }

    const auto ParallelMiss = FCkDebug_ViewportComponentPicker::TryIntersect_RayBounds(
        Bounds,
        FVector{0.0, 20.0, 0.0},
        FVector::ForwardVector);
    TestFalse(TEXT("A parallel ray outside the slab misses"), ParallelMiss.IsSet());

    const auto BehindMiss = FCkDebug_ViewportComponentPicker::TryIntersect_RayBounds(
        Bounds,
        FVector{30.0, 0.0, 0.0},
        FVector::ForwardVector);
    TestFalse(TEXT("Bounds wholly behind the ray origin miss"), BehindMiss.IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
