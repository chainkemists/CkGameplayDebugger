#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_ProjectSettings.h"

#if WITH_EDITOR

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Engine/EngineTypes.h"
#include "Engine/RendererSettings.h"

// --------------------------------------------------------------------------------------------------------------------

// File-local helpers in the module's own named namespace — this module compiles unity, and an anonymous namespace
// would collide with the identically-shaped helpers in the other check files.
namespace ck_optimization_debugger_checks_project_settings
{
    // Every finding here points the reader at Project Settings → Rendering. The section name is also what stands in
    // for a target path in the stable key, so it must not vary between two runs.
    const auto k_RenderingSection = FString{TEXT("Rendering")};

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Run_Checks(
            const FCkOptimizationDebugger_ScanContext& InContext,
            const FCkOptimizationDebugger_Thresholds& InThresholds,
            TArray<FCkOptimizationDebugger_FindingRow>& OutFindings)
        -> void
    {
        using namespace ck_optimization_debugger_scan;
        using namespace ck_optimization_debugger_thresholds;

        const auto* Settings = GetDefault<URendererSettings>();

        if (Settings == nullptr)
        { return; }

        // ---- ProjectSettings.RayTracing ----
        if (Settings->bEnableRayTracing != 0)
        {
            OutFindings.Add(Build_Finding(FName{TEXT("ProjectSettings.RayTracing")},
                ECkOptimizationDebugger_Severity::Minor,
                ECkOptimizationDebugger_Category::ProjectSettings,
                Build_ProjectSettingsTarget(k_RenderingSection, TEXT("Support Hardware Ray Tracing")),
                TEXT("Hardware ray tracing is enabled project-wide"),
                TEXT("Ray tracing support forces every mesh to keep a ray-tracing acceleration structure resident, whether or not any feature samples it. That is GPU memory and a per-frame BVH update cost paid on all hardware, including the machines that cannot use it."),
                TEXT("Turn it off unless a shipped feature needs hardware ray tracing. This one requires an editor restart and a shader rebuild, so decide it once.")));
        }

        // ---- ProjectSettings.PathTracing ----
        if (Settings->bEnablePathTracing != 0)
        {
            OutFindings.Add(Build_Finding(FName{TEXT("ProjectSettings.PathTracing")},
                ECkOptimizationDebugger_Severity::Minor,
                ECkOptimizationDebugger_Category::ProjectSettings,
                Build_ProjectSettingsTarget(k_RenderingSection, TEXT("Path Tracing")),
                TEXT("Path tracing is enabled project-wide"),
                TEXT("Path tracing compiles an additional set of shader permutations for every material in the project. It is an offline-rendering feature; if nothing is rendering stills with it, the cost is only build time and package size."),
                TEXT("Turn it off unless the project renders cinematics with the path tracer.")));
        }

        // ---- ProjectSettings.ForwardShading ----
        if (Settings->bForwardShading != 0)
        {
            OutFindings.Add(Build_Finding(FName{TEXT("ProjectSettings.ForwardShading")},
                ECkOptimizationDebugger_Severity::Minor,
                ECkOptimizationDebugger_Category::ProjectSettings,
                Build_ProjectSettingsTarget(k_RenderingSection, TEXT("Forward Shading")),
                TEXT("The project renders with forward shading"),
                TEXT("Forward shading is the right answer for VR and for scenes with few lights, and the wrong one almost everywhere else: per-object light lists replace the deferred pass's constant cost, so many-light scenes scale badly. It also disables a set of deferred-only features that content may already assume."),
                TEXT("Confirm this is deliberate. If the project is not VR and the lighting is dynamic, deferred is the cheaper default.")));
        }

        // ---- ProjectSettings.ExpensiveLightingFeatures ----
        // Aggregated rather than one finding per feature: Lumen and virtual shadow maps are the UE5 defaults, so
        // three separate rows telling a reader their defaults are on would be noise. One row that NAMES what is
        // enabled is the useful shape.
        auto EnabledFeatures = TArray<FString>{};

        if (Settings->DynamicGlobalIllumination == EDynamicGlobalIlluminationMethod::Lumen)
        { EnabledFeatures.Add(TEXT("Lumen global illumination")); }

        if (Settings->Reflections == EReflectionMethod::Lumen)
        { EnabledFeatures.Add(TEXT("Lumen reflections")); }

        if (Settings->ShadowMapMethod == EShadowMapMethod::VirtualShadowMaps)
        { EnabledFeatures.Add(TEXT("virtual shadow maps")); }

        if (NOT EnabledFeatures.IsEmpty())
        {
            OutFindings.Add(Build_Finding(FName{TEXT("ProjectSettings.ExpensiveLightingFeatures")},
                ECkOptimizationDebugger_Severity::Minor,
                ECkOptimizationDebugger_Category::ProjectSettings,
                Build_ProjectSettingsTarget(k_RenderingSection, TEXT("Global Illumination / Reflections / Shadows")),
                TEXT("Expensive lighting features are enabled"),
                ck::Format_UE(TEXT("Enabled: {}. These are the engine's own defaults and are not defects — they are listed so their cost is visible when a frame budget is being argued about. Each has a fixed per-frame GPU cost that does not go down with simpler content."),
                    FString::Join(EnabledFeatures, TEXT(", "))),
                TEXT("Leave them on unless a measured GPU capture says otherwise — and if it does, change one at a time and re-measure.")));
        }

        // ---- ProjectSettings.TextureStreamingDisabled ----
        // Only worth reporting once the level actually uses enough textures for streaming to be the thing keeping
        // memory down. On a nearly-empty level it is a setting, not a problem.
        const auto TextureCount = InContext.Textures.Num();

        if (Settings->bTextureStreaming == 0 && TextureCount >= InThresholds.MinTexturesForStreamingWarning)
        {
            auto Finding = Build_Finding(FName{TEXT("ProjectSettings.TextureStreamingDisabled")},
                Get_GraduatedSeverity(TextureCount, InThresholds.MinTexturesForStreamingWarning,
                    ECkOptimizationDebugger_Severity::Major),
                ECkOptimizationDebugger_Category::ProjectSettings,
                Build_ProjectSettingsTarget(k_RenderingSection, TEXT("Texture Streaming")),
                TEXT("Texture streaming is off in a texture-heavy level"),
                ck::Format_UE(TEXT("Texture streaming is disabled while the scanned levels reference {} distinct textures. With streaming off, every mip of every one of them is resident for as long as anything referencing it is loaded — the texture pool budget stops applying at all."),
                    TextureCount),
                TEXT("Re-enable Texture Streaming and size the pool with r.Streaming.PoolSize."));

            Finding.HasAutoFix = true;
            Finding.FixDescription = TEXT("Enable Texture Streaming in the project's rendering settings.");

            OutFindings.Add(MoveTemp(Finding));
        }
    }
}

#endif

// --------------------------------------------------------------------------------------------------------------------
