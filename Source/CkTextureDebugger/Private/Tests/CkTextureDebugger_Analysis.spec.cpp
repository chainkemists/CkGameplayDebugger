#include "CkTextureDebugger/Analysis/CkTextureDebugger_MaterialAnalysis.h"
#include "CkTextureDebugger/Analysis/CkTextureDebugger_SurfaceAnalysis.h"
#include "CkTextureDebugger/Analysis/CkTextureDebugger_UvDensity.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Analysis_MaterialUnavailable,
    "Ck.TextureDebugger.Analysis.MaterialUnavailable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Analysis_MaterialUnavailable::RunTest(const FString& Parameters)
{
    const auto Result = ck::texture_debugger::material_analysis::Analyze(nullptr);

    TestEqual(TEXT("Null material produces one explicit unavailable row"), Result.Rows.Num(), 1);
    TestEqual(TEXT("Null material is never labeled as a resolved parameter"),
        static_cast<int32>(Result.Rows[0].Provenance),
        static_cast<int32>(ECkTextureDebugger_MaterialTextureProvenance::Unavailable));
    TestFalse(TEXT("Unavailable row explains the absence"), Result.Rows[0].UnavailableReason.IsEmpty());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Analysis_UvDensityFailClosed,
    "Ck.TextureDebugger.Analysis.UvDensityFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Analysis_UvDensityFailClosed::RunTest(const FString& Parameters)
{
    const auto MissingEvidence = ck::texture_debugger::uv_density::EvaluateTriangleEvidence({});
    TestEqual(TEXT("Missing world area refuses a density number"),
        static_cast<int32>(MissingEvidence.Availability),
        static_cast<int32>(ECkTextureDebugger_UvDensityAvailability::MissingTriangleWorldArea));
    TestEqual(TEXT("Unavailable density remains zero"), MissingEvidence.TexelsPerCm, 0.0);

    auto ProvenEvidence = FCkTextureDebugger_UvTriangleEvidence{};
    ProvenEvidence.WorldTriangleAreaCm2 = 10000.0;
    ProvenEvidence.UvTriangleArea = 1.0;
    ProvenEvidence.TextureWidth = 512;
    ProvenEvidence.TextureHeight = 512;
    ProvenEvidence.TextureCoordinateScale = FVector2d{1.0, 1.0};
    ProvenEvidence.HasAuthoritativeWorldArea = true;
    ProvenEvidence.HasAuthoritativeUvArea = true;
    ProvenEvidence.HasProvenTextureBinding = true;
    ProvenEvidence.HasProvenTextureTransform = true;

    const auto Available = ck::texture_debugger::uv_density::EvaluateTriangleEvidence(ProvenEvidence);
    TestEqual(TEXT("Complete direct evidence produces a measurement"),
        static_cast<int32>(Available.Availability),
        static_cast<int32>(ECkTextureDebugger_UvDensityAvailability::Available));
    TestTrue(TEXT("512 pixels across one meter is 5.12 texels/cm"),
        FMath::IsNearlyEqual(Available.TexelsPerCm, 5.12, 0.001));

    ProvenEvidence.HasProvenTextureTransform = false;
    const auto UnknownTransform = ck::texture_debugger::uv_density::EvaluateTriangleEvidence(ProvenEvidence);
    TestEqual(TEXT("An unproven transform returns to unavailable"),
        static_cast<int32>(UnknownTransform.Availability),
        static_cast<int32>(ECkTextureDebugger_UvDensityAvailability::UnprovenTextureTransform));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Analysis_SurfaceUnavailable,
    "Ck.TextureDebugger.Analysis.SurfaceUnavailable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Analysis_SurfaceUnavailable::RunTest(const FString& Parameters)
{
    const auto Surface = ck::texture_debugger::surface_analysis::Describe(nullptr, nullptr);

    TestFalse(TEXT("Null material is not invented"), Surface.HasMaterial);
    TestFalse(TEXT("Nanite remains unavailable without a supported static component"), Surface.HasNaniteData.IsSet());
    TestFalse(TEXT("Lightmap resolution remains unavailable without a component"), Surface.LightMapResolution.IsSet());

    return true;
}

#endif
