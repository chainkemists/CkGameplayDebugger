#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Material.h"

#if WITH_EDITOR

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"

// --------------------------------------------------------------------------------------------------------------------

// File-local helpers in the module's own named namespace — this module compiles unity, and an anonymous namespace
// would collide with the identically-shaped helpers in the other check files.
namespace ck_optimization_debugger_checks_material
{
    /** Whether this blend mode makes the material a translucency-pass cost rather than an opaque one.
     *
     *  Written as an if-chain rather than a switch on purpose: `EBlendMode` in 5.7 carries two HIDDEN aliases
     *  (`BLEND_TranslucentGreyTransmittance == BLEND_Translucent`,
     *  `BLEND_ColoredTransmittanceOnly == BLEND_Modulate`), so a switch listing every name would not compile. */
    auto
        Is_TranslucentBlendMode(
            EBlendMode InBlendMode)
        -> bool
    {
        return InBlendMode == BLEND_Translucent
            || InBlendMode == BLEND_Additive
            || InBlendMode == BLEND_Modulate
            || InBlendMode == BLEND_AlphaComposite
            || InBlendMode == BLEND_AlphaHoldout
            || InBlendMode == BLEND_TranslucentColoredTransmittance;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_BlendModeLabel(
            EBlendMode InBlendMode)
        -> FString
    {
        switch (InBlendMode)
        {
            case BLEND_Opaque:                          return TEXT("Opaque");
            case BLEND_Masked:                          return TEXT("Masked");
            case BLEND_Translucent:                     return TEXT("Translucent");
            case BLEND_Additive:                        return TEXT("Additive");
            case BLEND_Modulate:                        return TEXT("Modulate");
            case BLEND_AlphaComposite:                  return TEXT("AlphaComposite");
            case BLEND_AlphaHoldout:                    return TEXT("AlphaHoldout");
            case BLEND_TranslucentColoredTransmittance: return TEXT("TranslucentColoredTransmittance");
            default:                                    return TEXT("Unknown");
        }
    }

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

        // ---- Per-mesh: how the level ASSIGNS materials ----
        for (const auto& Asset : InContext.StaticMeshes)
        {
            auto* Mesh = Cast<UStaticMesh>(Asset.Asset.Get());

            if (Mesh == nullptr)
            { continue; }

            const auto Target = Build_AssetTarget(Asset.Path, Asset.DisplayName);
            const auto Usage = Build_UsageSentence(Asset);

            const auto& StaticMaterials = Mesh->GetStaticMaterials();
            const auto SlotCount = StaticMaterials.Num();

            // ---- Material.SlotCount ----
            if (SlotCount > InThresholds.MaxMaterialSlots)
            {
                auto Finding = Build_Finding(FName{TEXT("Material.SlotCount")},
                    Get_GraduatedSeverity(SlotCount, InThresholds.MaxMaterialSlots,
                        ECkOptimizationDebugger_Severity::Major),
                    ECkOptimizationDebugger_Category::Material,
                    Target,
                    TEXT("Mesh has more material slots than the budget"),
                    ck::Format_UE(TEXT("{} material slots against a budget of {}. Each slot is a separate draw call per placement, and instancing cannot merge across slots.{}"),
                        SlotCount, InThresholds.MaxMaterialSlots, Usage),
                    TEXT("Atlas the textures and merge the slots, or split the mesh so each piece carries one material."));

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Material.EmptySlot / Material.DuplicateSlots ----
            auto EmptySlotNames = TArray<FString>{};
            auto SlotCountByMaterial = TMap<FSoftObjectPath, int32>{};
            auto DuplicatedNames = TArray<FString>{};

            for (const auto& StaticMaterial : StaticMaterials)
            {
                auto* Material = StaticMaterial.MaterialInterface.Get();

                if (Material == nullptr)
                {
                    EmptySlotNames.Add(StaticMaterial.MaterialSlotName.IsNone()
                        ? FString{TEXT("<unnamed slot>")}
                        : StaticMaterial.MaterialSlotName.ToString());
                    continue;
                }

                const auto MaterialPath = FSoftObjectPath{Material};
                auto& Seen = SlotCountByMaterial.FindOrAdd(MaterialPath);
                ++Seen;

                // Recorded on the SECOND sighting only, so a material in four slots is named once, not three times.
                if (Seen == 2)
                { DuplicatedNames.Add(Material->GetName()); }
            }

            if (NOT EmptySlotNames.IsEmpty())
            {
                auto Finding = Build_Finding(FName{TEXT("Material.EmptySlot")},
                    ECkOptimizationDebugger_Severity::Major,
                    ECkOptimizationDebugger_Category::Material,
                    Target,
                    TEXT("Mesh has empty material slots"),
                    ck::Format_UE(TEXT("{} slot(s) have no material assigned ({}), so those sections render with the engine default. A slot nothing fills is either a missing assignment or a section that should not exist.{}"),
                        EmptySlotNames.Num(), FString::Join(EmptySlotNames, TEXT(", ")), Usage),
                    TEXT("Assign a material to each listed slot, or remove the section from the source mesh."));

                OutFindings.Add(MoveTemp(Finding));
            }

            if (NOT DuplicatedNames.IsEmpty())
            {
                auto Finding = Build_Finding(FName{TEXT("Material.DuplicateSlots")},
                    ECkOptimizationDebugger_Severity::Minor,
                    ECkOptimizationDebugger_Category::Material,
                    Target,
                    TEXT("The same material fills several slots"),
                    ck::Format_UE(TEXT("{} material(s) appear in more than one slot ({}). Sections sharing a material are still separate draw calls — merging them in the source mesh removes one call per placement.{}"),
                        DuplicatedNames.Num(), FString::Join(DuplicatedNames, TEXT(", ")), Usage),
                    TEXT("Merge the sections that share a material in the DCC tool and re-import."));

                OutFindings.Add(MoveTemp(Finding));
            }
        }

        // ---- Per-material: what the material itself costs ----
        for (const auto& Asset : InContext.Materials)
        {
            auto* Material = Cast<UMaterialInterface>(Asset.Asset.Get());

            if (Material == nullptr)
            { continue; }

            const auto Target = Build_AssetTarget(Asset.Path, Asset.DisplayName);
            const auto Usage = Build_UsageSentence(Asset);

            const auto BlendMode = Material->GetBlendMode();
            const auto IsTranslucent = Is_TranslucentBlendMode(BlendMode);
            const auto IsTwoSided = Material->IsTwoSided();

            // ---- Material.TranslucentTwoSided ----
            // One check, two conditions, because the two costs compound: a two-sided translucent surface is shaded
            // twice per overlapping pixel in the pass that already cannot be depth-culled.
            if (IsTranslucent || IsTwoSided)
            {
                const auto Both = IsTranslucent && IsTwoSided;

                auto Finding = Build_Finding(FName{TEXT("Material.TranslucentTwoSided")},
                    Both ? ECkOptimizationDebugger_Severity::Major : ECkOptimizationDebugger_Severity::Minor,
                    ECkOptimizationDebugger_Category::Material,
                    Target,
                    Both
                        ? FString{TEXT("Material is translucent AND two-sided")}
                        : (IsTranslucent
                            ? FString{TEXT("Material is translucent")}
                            : FString{TEXT("Material is two-sided")}),
                    Both
                        ? ck::Format_UE(TEXT("Blend mode {} with two-sided rendering. Translucency cannot be depth-culled and two-sided doubles the shaded pixels, so overlapping surfaces multiply rather than add.{}"),
                            Get_BlendModeLabel(BlendMode), Usage)
                        : (IsTranslucent
                            ? ck::Format_UE(TEXT("Blend mode {}. Translucent surfaces are shaded in draw order with no early-Z rejection, so overdraw costs the full shader every time.{}"),
                                Get_BlendModeLabel(BlendMode), Usage)
                            : ck::Format_UE(TEXT("Two-sided rendering doubles the pixels this material shades wherever both faces are visible, and disables back-face culling everywhere else.{}"),
                                Usage)),
                    Both
                        ? TEXT("Drop two-sided if the geometry is closed, and prefer Masked over Translucent where the edge allows it.")
                        : (IsTranslucent
                            ? TEXT("Use Masked instead where a hard alpha edge is acceptable — it keeps early-Z.")
                            : TEXT("Turn two-sided off unless the mesh is genuinely single-sided geometry (foliage cards, cloth).")));

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Material.SamplerBudget ----
            // Sampler count is the material-complexity proxy this tool can read offline. Instruction counts are NOT
            // available without compiling shaders in 5.7 (`GetMaterialResource` is deprecated to a null return and
            // the material-stats extractor's out-param type lives in a Private header), so the shader-instruction
            // half of the feature spec's "sampler and instruction budgets" is deliberately not implemented — see
            // the module CLAUDE.md.
            auto UsedTextures = TArray<UTexture*>{};
            Material->GetUsedTextures(UsedTextures);

            auto UniqueTextures = TSet<FSoftObjectPath>{};

            for (const auto* Texture : UsedTextures)
            {
                if (Texture == nullptr)
                { continue; }

                UniqueTextures.Add(FSoftObjectPath{Texture});
            }

            const auto SamplerCount = UniqueTextures.Num();

            if (SamplerCount > InThresholds.MaxTextureSamplers)
            {
                auto Finding = Build_Finding(FName{TEXT("Material.SamplerBudget")},
                    Get_GraduatedSeverity(SamplerCount, InThresholds.MaxTextureSamplers,
                        ECkOptimizationDebugger_Severity::Major),
                    ECkOptimizationDebugger_Category::Material,
                    Target,
                    TEXT("Material samples more textures than the budget"),
                    ck::Format_UE(TEXT("{} distinct textures against a sampler budget of {}. The platform sampler limit is a hard ceiling — past it the material fails to compile rather than running slowly.{}"),
                        SamplerCount, InThresholds.MaxTextureSamplers, Usage),
                    TEXT("Pack channels together (ORM/RMA), share a sampler across nodes, or split the material."));

                OutFindings.Add(MoveTemp(Finding));
            }
        }
    }
}

#endif

// --------------------------------------------------------------------------------------------------------------------
