#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_Navigation.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/RendererSettings.h"
#include "GameFramework/Actor.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and a same-named anonymous helper in another .cpp would
// collide in the merged translation unit.
namespace ck_optimization_debugger_navigation_impl
{
    auto
        Make_Failure(
            const FString& InMessage)
        -> FCkOptimizationDebugger_NavigationResult
    {
        auto Result = FCkOptimizationDebugger_NavigationResult{};
        Result.Succeeded = false;
        Result.Message = InMessage;

        return Result;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_Success(
            const FString& InMessage)
        -> FCkOptimizationDebugger_NavigationResult
    {
        auto Result = FCkOptimizationDebugger_NavigationResult{};
        Result.Succeeded = true;
        Result.Message = InMessage;

        return Result;
    }

#if WITH_EDITOR
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Navigate_ToAsset(
            const FCkOptimizationDebugger_Target& InTarget)
        -> FCkOptimizationDebugger_NavigationResult
    {
        if (GEditor == nullptr)
        { return Make_Failure(TEXT("No editor to navigate in.")); }

        auto* Asset = InTarget.Path.ResolveObject();

        if (Asset == nullptr)
        { Asset = InTarget.Path.TryLoad(); }

        if (ck::Is_NOT_Valid(Asset))
        {
            return Make_Failure(ck::Format_UE(TEXT("{} could not be loaded — it may have been deleted since the scan."),
                InTarget.Path.ToString()));
        }

        GEditor->SyncBrowserToObjects(TArray<UObject*>{Asset});

        return Make_Success(ck::Format_UE(TEXT("Content Browser synced to {}."), InTarget.DisplayName));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Navigate_ToActor(
            const FCkOptimizationDebugger_Target& InTarget)
        -> FCkOptimizationDebugger_NavigationResult
    {
        if (GEditor == nullptr)
        { return Make_Failure(TEXT("No editor to navigate in.")); }

        // Resolved, never loaded: an actor path names something inside a world, and loading it would mean loading a
        // map behind the reader's back.
        auto* Actor = Cast<AActor>(InTarget.Path.ResolveObject());

        if (ck::Is_NOT_Valid(Actor))
        {
            return Make_Failure(InTarget.LevelName.IsEmpty()
                ? ck::Format_UE(TEXT("{} is not in a loaded level."), InTarget.DisplayName)
                : ck::Format_UE(TEXT("{} is not in a loaded level — open {} and re-scan."),
                    InTarget.DisplayName, InTarget.LevelName));
        }

        // Deselect first: "go to this finding" means the viewport shows THAT actor, not that actor plus whatever the
        // reader had selected before.
        GEditor->SelectNone(false, true);
        GEditor->SelectActor(Actor, true, false);
        GEditor->NoteSelectionChange();
        GEditor->MoveViewportCamerasToActor(*Actor, false);

        return Make_Success(ck::Format_UE(TEXT("Focused {} in the level viewport."), InTarget.DisplayName));
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The settings object a section name stands for. Every settings finding this module produces is about
     *  Rendering; the lookup exists as a lookup so the next section is one line rather than a re-think. */
    auto
        TryGet_SettingsObject(
            const FString& InSectionName)
        -> const UObject*
    {
        if (InSectionName == TEXT("Rendering"))
        { return GetDefault<URendererSettings>(); }

        return nullptr;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Navigate_ToProjectSettings(
            const FCkOptimizationDebugger_Target& InTarget)
        -> FCkOptimizationDebugger_NavigationResult
    {
        auto* SettingsModule = FModuleManager::LoadModulePtr<ISettingsModule>(FName{TEXT("Settings")});

        if (SettingsModule == nullptr)
        { return Make_Failure(TEXT("The settings viewer is not available in this session.")); }

        const auto* Settings = Cast<UDeveloperSettings>(TryGet_SettingsObject(InTarget.SettingsSectionName));

        if (Settings == nullptr)
        {
            return Make_Failure(ck::Format_UE(TEXT("No Project Settings page is registered for the {} section."),
                InTarget.SettingsSectionName));
        }

        // Asked of the settings object itself rather than spelled out here: these three names are exactly what the
        // settings auto-registration used to publish the page, so a hard-coded triple could only ever drift from it.
        SettingsModule->ShowViewer(
            Settings->GetContainerName(), Settings->GetCategoryName(), Settings->GetSectionName());

        return Make_Success(ck::Format_UE(TEXT("Opened Project Settings → {}."), InTarget.SettingsSectionName));
    }
#endif
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_navigation
{
    auto
        Get_NavigationDescription(
            const FCkOptimizationDebugger_Target& InTarget)
        -> FString
    {
        switch (InTarget.Kind)
        {
            case ECkOptimizationDebugger_TargetKind::Asset:
            {
                return ck::Format_UE(TEXT("Show {} in the Content Browser"), InTarget.DisplayName);
            }
            case ECkOptimizationDebugger_TargetKind::Actor:
            {
                return InTarget.LevelName.IsEmpty()
                    ? ck::Format_UE(TEXT("Select and frame {} in the level viewport"), InTarget.DisplayName)
                    : ck::Format_UE(TEXT("Select and frame {} in {}"), InTarget.DisplayName, InTarget.LevelName);
            }
            case ECkOptimizationDebugger_TargetKind::ProjectSettings:
            {
                return ck::Format_UE(TEXT("Open Project Settings → {}"), InTarget.SettingsSectionName);
            }
            default:
            {
                return FString{TEXT("Go to this finding's target")};
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Can_Navigate(
            const FCkOptimizationDebugger_Target& InTarget)
        -> bool
    {
        switch (InTarget.Kind)
        {
            case ECkOptimizationDebugger_TargetKind::Asset:
            case ECkOptimizationDebugger_TargetKind::Actor:
            {
                return NOT InTarget.Path.IsNull();
            }
            case ECkOptimizationDebugger_TargetKind::ProjectSettings:
            {
                return NOT InTarget.SettingsSectionName.IsEmpty();
            }
            default:
            {
                return false;
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Navigate_ToFinding(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_NavigationResult
    {
        using namespace ck_optimization_debugger_navigation_impl;

        if (NOT Can_Navigate(InFinding.Target))
        {
            return Make_Failure(ck::Format_UE(TEXT("{} names nothing to navigate to."), InFinding.Title));
        }

        return Navigate_ToTarget(InFinding.Target);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Navigate_ToTarget(
            const FCkOptimizationDebugger_Target& InTarget)
        -> FCkOptimizationDebugger_NavigationResult
    {
        using namespace ck_optimization_debugger_navigation_impl;

        if (NOT Can_Navigate(InTarget))
        {
            return Make_Failure(ck::Format_UE(TEXT("{} names nothing to navigate to."), InTarget.DisplayName));
        }

#if WITH_EDITOR
        switch (InTarget.Kind)
        {
            case ECkOptimizationDebugger_TargetKind::Asset:           return Navigate_ToAsset(InTarget);
            case ECkOptimizationDebugger_TargetKind::Actor:           return Navigate_ToActor(InTarget);
            case ECkOptimizationDebugger_TargetKind::ProjectSettings: return Navigate_ToProjectSettings(InTarget);
            default: break;
        }

        return Make_Failure(ck::Format_UE(TEXT("{} names a target kind navigation does not handle."),
            InTarget.DisplayName));
#else
        // The module ships in packaged Development/DebugGame targets, where there is no Content Browser, no level
        // viewport and no settings viewer to route to.
        return Make_Failure(TEXT("Navigating to a target needs an editor session."));
#endif
    }
}

// --------------------------------------------------------------------------------------------------------------------
