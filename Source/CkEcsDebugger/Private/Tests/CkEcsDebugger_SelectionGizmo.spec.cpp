#include "CkEcsDebugger/Viewport/SCkDebuggerSelectionGizmo.h"

#include "Misc/AutomationTest.h"

#include <limits>

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerSelectionGizmo_ConstantScreenSize,
    "Ck.EcsDebugger.SelectionGizmo.ConstantScreenSize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerSelectionGizmo_SizeValidation,
    "Ck.EcsDebugger.SelectionGizmo.SizeValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_ecs_selection_gizmo_spec
{
    auto Make_Projection(const float InScale) -> ck::DebugViewportView::FProjection
    {
        auto Result = ck::DebugViewportView::FProjection{};
        Result.ViewProjection = FMatrix::Identity;
        Result.ViewProjection.M[0][0] = InScale;
        Result.ViewProjection.M[1][1] = InScale;
        Result.ViewRect = FIntRect{0, 0, 1000, 800};
        Result.ViewSize = FIntPoint{1000, 800};
        return Result;
    }

    auto Get_LongestAxis(const ck::EcsSelectionGizmo::FTriad& InTriad) -> float
    {
        auto Longest = 0.0f;
        for (const auto& Axis : InTriad.Axes)
        { Longest = FMath::Max(Longest, static_cast<float>((Axis.End - Axis.Origin).Size())); }
        return Longest;
    }
}

// --------------------------------------------------------------------------------------------------------------------

bool FCkEcsDebuggerSelectionGizmo_ConstantScreenSize::RunTest(const FString&)
{
    constexpr auto RequestedPixels = 72.0f;
    const auto Transform = FTransform{FRotator{18.0f, 31.0f, 7.0f}, FVector::ZeroVector};
    const auto Near = ck::EcsSelectionGizmo::Project_Triad(
        ck_ecs_selection_gizmo_spec::Make_Projection(1.0f), Transform, RequestedPixels);
    const auto Far = ck::EcsSelectionGizmo::Project_Triad(
        ck_ecs_selection_gizmo_spec::Make_Projection(0.1f), Transform, RequestedPixels);

    TestTrue(TEXT("near projection produces a triad"), Near.IsSet());
    TestTrue(TEXT("far projection produces a triad"), Far.IsSet());
    if (NOT Near.IsSet() || NOT Far.IsSet())
    { return false; }

    TestEqual(TEXT("all three axes retained"), Near->Axes.Num(), 3);
    TestTrue(TEXT("near apparent size matches tuner"),
        FMath::IsNearlyEqual(ck_ecs_selection_gizmo_spec::Get_LongestAxis(Near.GetValue()), RequestedPixels, 0.05f));
    TestTrue(TEXT("far apparent size matches tuner"),
        FMath::IsNearlyEqual(ck_ecs_selection_gizmo_spec::Get_LongestAxis(Far.GetValue()), RequestedPixels, 0.05f));

    const auto Secondary = ck::EcsSelectionGizmo::Project_Triad(
        ck_ecs_selection_gizmo_spec::Make_Projection(1.0f), Transform, RequestedPixels, true);
    TestTrue(TEXT("secondary projection produces a triad"), Secondary.IsSet());
    if (Secondary.IsSet())
    {
        TestTrue(TEXT("parent context is intentionally smaller"),
            ck_ecs_selection_gizmo_spec::Get_LongestAxis(Secondary.GetValue()) < RequestedPixels);
        TestTrue(TEXT("parent context is visually muted"), Secondary->Opacity < 1.0f);
    }
    return true;
}

bool FCkEcsDebuggerSelectionGizmo_SizeValidation::RunTest(const FString&)
{
    TestEqual(TEXT("low values clamp"), ck::EcsSelectionGizmo::Normalize_SizePixels(-10.0f), 24.0f);
    TestEqual(TEXT("high values clamp"), ck::EcsSelectionGizmo::Normalize_SizePixels(500.0f), 192.0f);
    TestEqual(TEXT("NaN falls back to default"),
        ck::EcsSelectionGizmo::Normalize_SizePixels(std::numeric_limits<float>::quiet_NaN()), 72.0f);

    auto Triad = ck::EcsSelectionGizmo::FTriad{};
    Triad.Origin = FVector2D{500.0f, 400.0f};
    Triad.Axes.Add({Triad.Origin, FVector2D{572.0f, 400.0f}, FLinearColor::Red, 0});
    const auto Local = ck::EcsSelectionGizmo::Convert_ToSlateLocal(
        Triad, FIntPoint{1000, 800}, FVector2D{500.0f, 400.0f});
    TestTrue(TEXT("DPI conversion accepts valid sizes"), Local.IsSet());
    if (Local.IsSet())
    {
        TestTrue(TEXT("origin converts from render pixels to Slate local"),
            Local->Origin.Equals(FVector2D{250.0f, 200.0f}, 0.01f));
        TestTrue(TEXT("gizmo length converts by the same scale"),
            FMath::IsNearlyEqual(static_cast<float>((Local->Axes[0].End - Local->Axes[0].Origin).Size()), 36.0f, 0.01f));
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
