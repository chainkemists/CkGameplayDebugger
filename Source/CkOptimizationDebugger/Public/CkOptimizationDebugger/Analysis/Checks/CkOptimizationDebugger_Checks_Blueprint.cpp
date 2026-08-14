#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Blueprint.h"

#if WITH_EDITOR

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Misc/AssetRegistryInterface.h"
#include "UObject/Package.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_checks_blueprint
{
    auto
        Run_Checks(
            const FCkOptimizationDebugger_ScanContext& InContext,
            const FCkOptimizationDebugger_Thresholds& InThresholds,
            TArray<FCkOptimizationDebugger_FindingRow>& OutFindings)
        -> void
    {
        using namespace ck_optimization_debugger_scan;
        using namespace ck_optimization_debugger_thresholds;

        // Null during shutdown, and null is not an error here — it means the question cannot be asked right now,
        // which is a reason to skip the dependency check rather than to report every Blueprint as clean.
        auto* AssetRegistry = IAssetRegistry::Get();

        for (const auto& Asset : InContext.BlueprintClasses)
        {
            auto* GeneratedClass = Cast<UBlueprintGeneratedClass>(Asset.Asset.Get());

            if (GeneratedClass == nullptr)
            { continue; }

            const auto Target = Build_AssetTarget(Asset.Path, Asset.DisplayName);
            const auto Usage = Build_UsageSentence(Asset);

            // ---- Blueprint.TickEnabled ----
            // Read off the CDO, not off a placement: the class is what a fix would change, and one instance whose
            // tick was turned off by hand does not make the class cheap.
            if (const auto* Cdo = Cast<AActor>(GeneratedClass->GetDefaultObject()))
            {
                const auto& Tick = Cdo->PrimaryActorTick;

                if (Tick.bCanEverTick && Tick.bStartWithTickEnabled)
                {
                    auto Finding = Build_Finding(FName{TEXT("Blueprint.TickEnabled")},
                        Get_GraduatedSeverity(Asset.UsageCount, InThresholds.MinRepeatedActorsForInstancing,
                            ECkOptimizationDebugger_Severity::Minor),
                        ECkOptimizationDebugger_Category::Blueprint,
                        Target,
                        TEXT("Blueprint ticks every frame"),
                        ck::Format_UE(TEXT("This class ticks from the moment it spawns, and the level places {} of them. A Blueprint tick is the most expensive per-frame unit in the engine — even an empty one pays the dispatch.{}"),
                            Asset.UsageCount, Usage),
                        TEXT("Turn Start With Tick Enabled off and drive the behaviour from a timer, an event, or a component that genuinely needs the frame."));

                    OutFindings.Add(MoveTemp(Finding));
                }
            }

            // ---- Blueprint.DependencyChain ----
            if (AssetRegistry == nullptr)
            { continue; }

            const auto* Package = GeneratedClass->GetOutermost();

            if (Package == nullptr)
            { continue; }

            auto Dependencies = TArray<FName>{};

            const auto Queried = AssetRegistry->GetDependencies(
                Package->GetFName(),
                Dependencies,
                UE::AssetRegistry::EDependencyCategory::Package,
                UE::AssetRegistry::EDependencyQuery::Hard);

            if (NOT Queried)
            { continue; }

            if (Dependencies.Num() <= InThresholds.MaxBlueprintDependencies)
            { continue; }

            auto Finding = Build_Finding(FName{TEXT("Blueprint.DependencyChain")},
                Get_GraduatedSeverity(Dependencies.Num(), InThresholds.MaxBlueprintDependencies,
                    ECkOptimizationDebugger_Severity::Major),
                ECkOptimizationDebugger_Category::Blueprint,
                Target,
                TEXT("Blueprint pulls in a large dependency chain"),
                ck::Format_UE(TEXT("{} hard package dependencies against a budget of {}. Every hard reference is loaded WITH this Blueprint, so the dependency count is the load cost — and the count is transitive in practice, because each of those pulls its own.{}"),
                    Dependencies.Num(), InThresholds.MaxBlueprintDependencies, Usage),
                TEXT("Turn the references that are not needed at load time into soft references, and move shared data out into assets both sides reference."));

            OutFindings.Add(MoveTemp(Finding));
        }
    }
}

#endif

// --------------------------------------------------------------------------------------------------------------------
