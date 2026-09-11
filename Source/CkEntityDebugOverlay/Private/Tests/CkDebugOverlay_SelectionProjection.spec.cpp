#include "Misc/AutomationTest.h"

#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"

#include "SceneView.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugOverlay_SelectionProjection_Test,
    "Ck.DebugOverlay.Selection.Projection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SelectionProjection_Test::RunTest(const FString&)
{
    auto Projection = ck::DebugViewportView::FProjection{};
    Projection.ViewProjection = FMatrix::Identity;
    Projection.ViewRect = FIntRect{10, 20, 110, 220};
    Projection.ViewSize = FIntPoint{200, 300};

    auto DirectPixel = FVector2D{};
    TestTrue(TEXT("FSceneView projects the clip-space center"),
        FSceneView::ProjectWorldToScreen(FVector::ZeroVector, Projection.ViewRect, Projection.ViewProjection, DirectPixel));
    auto Pixel = FVector2D{};
    auto Inside = false;
    TestTrue(TEXT("projection delegates the center to FSceneView"), Projection.Project(FVector::ZeroVector, Pixel, Inside));
    TestTrue(TEXT("projection preserves the FSceneView pixel X"), FMath::IsNearlyEqual(Pixel.X, DirectPixel.X));
    TestTrue(TEXT("projection preserves the FSceneView pixel Y"), FMath::IsNearlyEqual(Pixel.Y, DirectPixel.Y));
    TestTrue(TEXT("clip-space center is inside"), Inside);
    TestTrue(TEXT("constrained view center retains its full-viewport X offset"), FMath::IsNearlyEqual(Pixel.X, 60.0f));
    TestTrue(TEXT("constrained view center retains its full-viewport Y offset"), FMath::IsNearlyEqual(Pixel.Y, 120.0f));

    TestTrue(TEXT("minimum viewport edge projects"), Projection.Project(FVector{-1.0f, 1.0f, 0.0f}, Pixel, Inside));
    TestTrue(TEXT("minimum X edge is inside"), Inside);
    TestTrue(TEXT("minimum X maps to the viewport minimum"), FMath::IsNearlyEqual(Pixel.X, 10.0f));
    TestTrue(TEXT("minimum Y maps to the viewport minimum"), FMath::IsNearlyEqual(Pixel.Y, 20.0f));

    TestTrue(TEXT("maximum viewport edge projects"), Projection.Project(FVector{1.0f, -1.0f, 0.0f}, Pixel, Inside));
    TestFalse(TEXT("exclusive maximum viewport edge is outside"), Inside);
    TestTrue(TEXT("maximum X maps to the viewport maximum"), FMath::IsNearlyEqual(Pixel.X, 110.0f));
    TestTrue(TEXT("maximum Y maps to the viewport maximum"), FMath::IsNearlyEqual(Pixel.Y, 220.0f));

    auto BehindProjection = Projection;
    BehindProjection.ViewProjection.M[3][3] = -1.0f;
    Pixel = FVector2D{1.0f, 1.0f};
    Inside = true;
    TestFalse(TEXT("behind-camera homogeneous W is rejected"), BehindProjection.Project(FVector::ZeroVector, Pixel, Inside));
    TestFalse(TEXT("behind-camera result is never inside"), Inside);
    TestEqual(TEXT("behind-camera result resets pixel"), Pixel, FVector2D::ZeroVector);

    auto ZeroRectProjection = Projection;
    ZeroRectProjection.ViewRect = FIntRect{10, 20, 10, 220};
    Pixel = FVector2D{1.0f, 1.0f};
    Inside = true;
    TestFalse(TEXT("zero-width viewport is rejected"), ZeroRectProjection.Project(FVector::ZeroVector, Pixel, Inside));
    TestFalse(TEXT("zero-width viewport is never inside"), Inside);
    TestEqual(TEXT("zero-width viewport resets pixel"), Pixel, FVector2D::ZeroVector);

    Pixel = FVector2D{1.0f, 1.0f};
    Inside = true;
    TestFalse(TEXT("NaN world position is rejected"), Projection.Project(FVector{NAN, 0.0f, 0.0f}, Pixel, Inside));
    TestFalse(TEXT("NaN world position is never inside"), Inside);
    TestEqual(TEXT("NaN world position resets pixel"), Pixel, FVector2D::ZeroVector);
    return true;
}
