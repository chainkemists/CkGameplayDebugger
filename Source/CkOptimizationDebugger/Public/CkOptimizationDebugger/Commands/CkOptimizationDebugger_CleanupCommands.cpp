#include "CkOptimizationDebugger/Commands/CkOptimizationDebugger_CleanupCommands.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "Editor.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/SoftObjectPath.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and a same-named anonymous helper in another .cpp would
// collide in the merged translation unit.
namespace ck_optimization_debugger_cleanup_commands_impl
{
    auto
        Make_Failure(
            const FString& InMessage)
        -> FCkOptimizationDebugger_CleanupActionResult
    {
        auto Result = FCkOptimizationDebugger_CleanupActionResult{};
        Result.DidRun = false;
        Result.Message = InMessage;

        return Result;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_Success(
            const FString& InMessage)
        -> FCkOptimizationDebugger_CleanupActionResult
    {
        auto Result = FCkOptimizationDebugger_CleanupActionResult{};
        Result.DidRun = true;
        Result.Message = InMessage;

        return Result;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_cleanup_commands
{
    auto
        Get_AllActions()
        -> const TArray<FCkOptimizationDebugger_CleanupActionInfo>&
    {
        // Built once, on first use. A file-scope table would construct its `FName`s during static initialization,
        // before anything guarantees the name table is up.
        static const auto Actions = []() -> TArray<FCkOptimizationDebugger_CleanupActionInfo>
        {
            auto Table = TArray<FCkOptimizationDebugger_CleanupActionInfo>{};

            {
                auto Action = FCkOptimizationDebugger_CleanupActionInfo{};
                Action.Action = ECkOptimizationDebugger_CleanupAction::DeleteAssets;
                Action.Id = FName{TEXT("Cleanup.DeleteAssets")};
                Action.DisplayVerb = FString{TEXT("Delete Selected")};
                Action.Description = FString{TEXT("Hand the selected assets to the editor's own delete dialog, which ")
                    TEXT("lists what still references them and offers Force Delete. Nothing is removed until that ")
                    TEXT("dialog is confirmed.")};
                Action.IsDestructive = true;
                Action.OperatesOnSelection = true;
                Action.Categories = {
                    ECkOptimizationDebugger_CleanupCategory::Unreferenced,
                    ECkOptimizationDebugger_CleanupCategory::Duplicates};

                Table.Add(MoveTemp(Action));
            }

            {
                auto Action = FCkOptimizationDebugger_CleanupActionInfo{};
                Action.Action = ECkOptimizationDebugger_CleanupAction::FixUpRedirectors;
                Action.Id = FName{TEXT("Cleanup.FixUpRedirectors")};
                Action.DisplayVerb = FString{TEXT("Fix Up Redirectors")};
                Action.Description = FString{TEXT("Rewrite every package that still points at the selected ")
                    TEXT("redirectors so it names the real asset, then remove the redirector. Referencing packages ")
                    TEXT("are checked out and re-saved.")};
                Action.IsDestructive = false;
                Action.OperatesOnSelection = true;
                Action.Categories = {ECkOptimizationDebugger_CleanupCategory::Redirectors};

                Table.Add(MoveTemp(Action));
            }

            {
                auto Action = FCkOptimizationDebugger_CleanupActionInfo{};
                Action.Action = ECkOptimizationDebugger_CleanupAction::SaveDirtyPackages;
                Action.Id = FName{TEXT("Cleanup.SaveDirtyPackages")};
                Action.DisplayVerb = FString{TEXT("Save Dirty Packages")};
                Action.Description = FString{TEXT("Open the editor's own save checklist over every dirty package. ")
                    TEXT("The list it shows is the editor's, not this page's selection — which is where the choice of ")
                    TEXT("what to write is actually made.")};
                Action.IsDestructive = false;

                // The one action that ignores the selection. `FEditorFileUtils::SaveDirtyPackages` prompts with the
                // editor's own checklist over EVERY dirty package; there is no supported narrowing of it, and a
                // button that implied one would save more than it said.
                Action.OperatesOnSelection = false;
                Action.Categories = {ECkOptimizationDebugger_CleanupCategory::DirtyPackages};

                Table.Add(MoveTemp(Action));
            }

            return Table;
        }();

        return Actions;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_Action(
            ECkOptimizationDebugger_CleanupAction InAction)
        -> const FCkOptimizationDebugger_CleanupActionInfo*
    {
        for (const auto& Action : Get_AllActions())
        {
            if (Action.Action == InAction)
            { return &Action; }
        }

        return nullptr;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_ActionById(
            FName InId)
        -> const FCkOptimizationDebugger_CleanupActionInfo*
    {
        for (const auto& Action : Get_AllActions())
        {
            if (Action.Id == InId)
            { return &Action; }
        }

        return nullptr;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_ActionForCategory(
            ECkOptimizationDebugger_CleanupCategory InCategory)
        -> const FCkOptimizationDebugger_CleanupActionInfo*
    {
        for (const auto& Action : Get_AllActions())
        {
            if (Accepts_Category(Action, InCategory))
            { return &Action; }
        }

        return nullptr;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Accepts_Category(
            const FCkOptimizationDebugger_CleanupActionInfo& InAction,
            ECkOptimizationDebugger_CleanupCategory InCategory)
        -> bool
    {
        return InAction.Categories.Contains(InCategory);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ApplicableRows(
            const FCkOptimizationDebugger_CleanupActionInfo& InAction,
            const TArray<FCkOptimizationDebugger_CleanupRow>& InRows)
        -> TArray<FCkOptimizationDebugger_CleanupRow>
    {
        auto Rows = TArray<FCkOptimizationDebugger_CleanupRow>{};

        for (const auto& Row : InRows)
        {
            if (NOT Accepts_Category(InAction, Row.Category))
            { continue; }

            Rows.Add(Row);
        }

        return Rows;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Can_RunAction(
            const FCkOptimizationDebugger_CleanupActionInfo& InAction,
            const TArray<FCkOptimizationDebugger_CleanupRow>& InRows)
        -> bool
    {
        if (NOT InAction.OperatesOnSelection)
        { return true; }

        return NOT Get_ApplicableRows(InAction, InRows).IsEmpty();
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ActionDisabledReason(
            const FCkOptimizationDebugger_CleanupActionInfo& InAction,
            const TArray<FCkOptimizationDebugger_CleanupRow>& InRows)
        -> FString
    {
        if (Can_RunAction(InAction, InRows))
        { return FString{}; }

        if (InRows.IsEmpty())
        { return ck::Format_UE(TEXT("Select a row first — {} acts on what is selected."), InAction.DisplayVerb); }

        auto CategoryLabels = TArray<FString>{};

        for (const auto Category : InAction.Categories)
        { CategoryLabels.Add(ck_optimization_debugger_model::Get_CleanupCategoryLabel(Category).ToLower()); }

        return ck::Format_UE(TEXT("Nothing in the selection is a {} row."),
            FString::Join(CategoryLabels, TEXT(" or ")));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_ActionButtonLabel(
            const FCkOptimizationDebugger_CleanupActionInfo& InAction,
            const TArray<FCkOptimizationDebugger_CleanupRow>& InApplicableRows)
        -> FString
    {
        // The action that ignores the selection never grows a count — a "(0)" on a button that would still do
        // something is a button the reader stops pressing.
        if (NOT InAction.OperatesOnSelection)
        { return InAction.DisplayVerb; }

        if (InApplicableRows.Num() <= 1)
        { return InAction.DisplayVerb; }

        return ck::Format_UE(TEXT("{} ({})"), InAction.DisplayVerb, InApplicableRows.Num());
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CanRunActions()
        -> bool
    {
        return Get_ActionsUnavailableReason().IsEmpty();
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ActionsUnavailableReason()
        -> FString
    {
#if WITH_EDITOR
        if (GEditor == nullptr)
        { return FString{TEXT("Cleanup actions need an editor session.")}; }

        // Deliberately blocked during PIE. Every action here mutates content the running session may have loaded:
        // deleting an asset out from under a live world, or writing packages while the game holds them, is a
        // decision nobody asked for — and the delete dialog's referencer list does not know about a play session's
        // hard references either. The rows stay readable; only the button waits.
        if (GEditor->PlayWorld != nullptr || GEditor->bIsSimulatingInEditor)
        { return FString{TEXT("Not while a play session is running — stop PIE first.")}; }

        return FString{};
#else
        return FString{TEXT("Cleanup actions need an editor session.")};
#endif
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Run_Action(
            const FCkOptimizationDebugger_CleanupActionInfo& InAction,
            const TArray<FCkOptimizationDebugger_CleanupRow>& InRows)
        -> FCkOptimizationDebugger_CleanupActionResult
    {
        using namespace ck_optimization_debugger_cleanup_commands_impl;

#if WITH_EDITOR
        if (NOT Get_CanRunActions())
        { return Make_Failure(Get_ActionsUnavailableReason()); }

        const auto Applicable = Get_ApplicableRows(InAction, InRows);

        if (InAction.OperatesOnSelection && Applicable.IsEmpty())
        { return Make_Failure(Get_ActionDisabledReason(InAction, InRows)); }

        switch (InAction.Action)
        {
            case ECkOptimizationDebugger_CleanupAction::DeleteAssets:
            {
                auto* Registry = IAssetRegistry::Get();

                if (Registry == nullptr)
                { return Make_Failure(TEXT("No asset registry to resolve the selection against.")); }

                auto Assets = TArray<FAssetData>{};
                Assets.Reserve(Applicable.Num());

                for (const auto& Row : Applicable)
                {
                    // Re-resolved against the registry rather than trusted from the row: a cleanup scan can be minutes
                    // old, and the asset it named may already be gone. An asset that no longer exists is dropped here
                    // instead of being handed to the delete dialog as a blank line.
                    const auto Asset = Registry->GetAssetByObjectPath(FSoftObjectPath{Row.AssetPath});

                    if (NOT Asset.IsValid())
                    { continue; }

                    Assets.Add(Asset);
                }

                if (Assets.IsEmpty())
                {
                    return Make_Failure(TEXT("None of the selected assets are still in the registry — re-scan."));
                }

                // `bShowConfirmation = true`, always. That dialog is the engine's own — it lists referencers, offers
                // Force Delete, and is the reader's last look before anything is removed. There is no path through
                // this module that passes false.
                const auto DeletedCount = ObjectTools::DeleteAssets(Assets, /*bShowConfirmation*/ true);

                if (DeletedCount <= 0)
                {
                    // Cancelling the dialog is the ordinary outcome, not an error — and it is the whole point of
                    // routing through it.
                    return Make_Failure(ck::Format_UE(
                        TEXT("Nothing was deleted — the editor's delete dialog was cancelled or refused {} asset(s)."),
                        Assets.Num()));
                }

                return Make_Success(ck::Format_UE(TEXT("Deleted {} asset(s) of the {} offered."),
                    DeletedCount, Assets.Num()));
            }
            case ECkOptimizationDebugger_CleanupAction::FixUpRedirectors:
            {
                auto& AssetTools = IAssetTools::Get();

                if (AssetTools.IsFixupReferencersInProgress())
                { return Make_Failure(TEXT("A redirector fix-up is already running.")); }

                auto Redirectors = TArray<UObjectRedirector*>{};
                Redirectors.Reserve(Applicable.Num());

                for (const auto& Row : Applicable)
                {
                    // A redirector has to be LOADED to be fixed up — this is an action the reader pressed, not a scan,
                    // and the fix-up API takes objects. The scan itself still loads nothing.
                    auto* Object = FSoftObjectPath{Row.AssetPath}.TryLoad();

                    if (auto* Redirector = Cast<UObjectRedirector>(Object))
                    { Redirectors.Add(Redirector); }
                }

                if (Redirectors.IsEmpty())
                {
                    return Make_Failure(TEXT("None of the selected redirectors could be loaded — they may already ")
                        TEXT("have been fixed up. Re-scan."));
                }

                // The defaults are the engine's own: prompt for checkout, then delete the redirectors it fixed up.
                AssetTools.FixupReferencers(Redirectors);

                // `FixupReferencers` returns `void` and is NOT necessarily synchronous: while the asset registry is
                // still loading it puts up a discovering-assets dialog and hands the work to a completion delegate.
                // Reporting "referencing packages were rewritten and re-saved" before anything had been read was the
                // window claiming an outcome it had no way to know — and the re-scan that followed then ran on top
                // of an operation that had not started.
                const auto Deferred = AssetTools.IsFixupReferencersInProgress();

                auto Result = Make_Success(Deferred
                    ? ck::Format_UE(
                        TEXT("{} redirector(s) handed to the engine's fix-up — it is waiting on the asset registry to finish indexing and will rewrite the referencing packages when it does. Re-scan once it reports done."),
                        Redirectors.Num())
                    : ck::Format_UE(
                        TEXT("Fixed up {} redirector(s) — referencing packages were rewritten and re-saved."),
                        Redirectors.Num()));

                Result.ShouldRefresh = NOT Deferred;

                return Result;
            }
            case ECkOptimizationDebugger_CleanupAction::SaveDirtyPackages:
            {
                // Counted rather than trusted. `SaveDirtyPackages` returns `true` when the prompt was DECLINED as
                // well as when it saved — its `bCanBeDeclined` default makes "Don't Save" a successful outcome of
                // the call — so the return value alone would report "Saved through the editor's own save dialog"
                // over a reader who pressed Don't Save. The dirty set before and after is the ground truth.
                auto DirtyBefore = TArray<UPackage*>{};
                FEditorFileUtils::GetDirtyPackages(DirtyBefore);

                // Prompted, always: the checklist the editor puts up is where the reader decides what is written, and
                // a silent save of every dirty package is not something a debug window gets to do on its own.
                FEditorFileUtils::SaveDirtyPackages(
                    /*bPromptUserToSave*/ true,
                    /*bSaveMapPackages*/ true,
                    /*bSaveContentPackages*/ true);

                auto DirtyAfter = TArray<UPackage*>{};
                FEditorFileUtils::GetDirtyPackages(DirtyAfter);

                const auto SavedCount = DirtyBefore.Num() - DirtyAfter.Num();

                if (SavedCount <= 0)
                {
                    // Declining is the reader exercising the veto the prompt exists for, so this is Warn-toned by
                    // the window rather than an error.
                    return Make_Failure(TEXT("Nothing was saved — the save dialog was declined or cancelled."));
                }

                return Make_Success(ck::Format_UE(
                    TEXT("Saved {} package(s) through the editor's own save dialog; {} still unsaved."),
                    SavedCount, DirtyAfter.Num()));
            }
            default:
            {
                return Make_Failure(TEXT("Unknown cleanup action."));
            }
        }
#else
        (void)InAction;
        (void)InRows;

        // The module ships in packaged Development/DebugGame builds, where there is no Content Browser to delete
        // from and no package to save. A disabled button plus this line is the honest presentation of "not here".
        return Make_Failure(TEXT("Cleanup actions need an editor session."));
#endif
    }
}

// --------------------------------------------------------------------------------------------------------------------
