#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UMaterialInterface;
class UMeshComponent;
class UTexture;
class UWorld;

// --------------------------------------------------------------------------------------------------------------------

enum class ECkTextureDebugger_CheckerSessionResult : uint8
{
    Success,
    NoActiveSession,
    SessionAlreadyActive,
    InvalidWorld,
    InvalidCheckerMaterial,
    InvalidCheckerTexture,
    InvalidCheckerParameter,
    CheckerMaterialNotDynamic,
    SessionSuspendedForWorldSave,
    EmptyTargets,
    InvalidTarget,
    ComponentHasSlotOverlay,
    ComponentDestroyed,
    WorldChanged,
    VerificationFailed,
    RestoreBlockedBySlotOverlay,
    SavePreparationFailed
};

/** A material-slot set on one component. The component link is intentionally weak. */
struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_CheckerTarget
{
    TWeakObjectPtr<UMeshComponent> Component;
    TArray<int32> SlotIndices;
};

/** Result of a mutation attempt. Text is presentation-ready but the enum remains the stable contract. */
struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_CheckerSessionReport
{
    ECkTextureDebugger_CheckerSessionResult Result = ECkTextureDebugger_CheckerSessionResult::Success;
    int32 AppliedComponentCount = 0;
    int32 RestoredComponentCount = 0;
    int32 PreservedExternalSlotCount = 0;
    int32 DroppedDestroyedComponentCount = 0;
    FString Detail;

    auto Succeeded() const -> bool
    {
        return Result == ECkTextureDebugger_CheckerSessionResult::Success;
    }
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Owns only a temporary checker session. It never owns a component or world: those are weak because Slate can outlive
 * PIE. Original material references are strong because a component may be their only root while checker is applied.
 *
 * Integration owner responsibilities:
 * - call ReleaseForWorldCleanup before dereferencing a world being torn down;
 * - call PrepareForWorldSave from the editor pre-save delegate and CompleteWorldSave from the matching completion path;
 * - call TryRestore before ordinary window destruction, and release on engine pre-exit if the world is no longer safe.
 */
class CKTEXTUREDEBUGGER_API FCkTextureDebugger_CheckerOverrideSession
{
public:
    FCkTextureDebugger_CheckerOverrideSession() = default;
    ~FCkTextureDebugger_CheckerOverrideSession();
    FCkTextureDebugger_CheckerOverrideSession(const FCkTextureDebugger_CheckerOverrideSession&) = delete;
    auto operator=(const FCkTextureDebugger_CheckerOverrideSession&) -> FCkTextureDebugger_CheckerOverrideSession& = delete;

    auto Apply(
        UWorld* InWorld,
        UMaterialInterface* InCheckerMaterial,
        const TArray<FCkTextureDebugger_CheckerTarget>& InTargets) -> FCkTextureDebugger_CheckerSessionReport;

    /** Updates the active MID without replacing any component material or rebuilding the restore ledger. */
    auto SwitchCheckerTexture(
        UTexture* InCheckerTexture,
        FName InTextureParameter) -> FCkTextureDebugger_CheckerSessionReport;

    /** Restores checker-owned values and preserves externally replaced values. */
    auto TryRestore() -> FCkTextureDebugger_CheckerSessionReport;

    /** Removes stale weak entries without touching their components. Call from tick or destruction notifications. */
    auto DiscardDestroyedComponents() -> int32;

    /** World teardown is terminal: no material write is useful or safe once cleanup begins. */
    auto ReleaseForWorldCleanup(
        UWorld* InWorld) -> void;

    /** Explicit state transition for the module-owned editor pre-save delegate. */
    auto PrepareForWorldSave(
        UWorld* InWorld) -> FCkTextureDebugger_CheckerSessionReport;

    /** Reapplies only when the caller proves that the save completed successfully. */
    auto CompleteWorldSave(
        bool InSaveSucceeded) -> FCkTextureDebugger_CheckerSessionReport;

    auto HasActiveSession() const -> bool;
    auto IsSuspendedForWorldSave() const -> bool;
    auto GetWorld() const -> UWorld*;

private:
    struct FComponentLedger
    {
        TWeakObjectPtr<UMeshComponent> Component;
        TArray<int32> AppliedSlots;
        TArray<TStrongObjectPtr<UMaterialInterface>> OriginalTopology;
        TArray<TWeakObjectPtr<UMaterialInterface>> ExpectedCheckerTopology;
    };

    struct FSaveResumeState
    {
        TWeakObjectPtr<UWorld> World;
        TStrongObjectPtr<UMaterialInterface> CheckerMaterial;
        TArray<FCkTextureDebugger_CheckerTarget> Targets;
    };

    auto Reset() -> void;
    auto CanMutateComponent(
        UMeshComponent* InComponent,
        UWorld* InExpectedWorld,
        FString& OutFailure) const -> bool;
    auto RestoreLedger(
        FComponentLedger& InOutLedger,
        FCkTextureDebugger_CheckerSessionReport& InOutReport) -> bool;

    TWeakObjectPtr<UWorld> _World;
    TStrongObjectPtr<UMaterialInterface> _CheckerMaterial;
    TArray<FComponentLedger> _Ledgers;
    TOptional<FSaveResumeState> _SaveResumeState;
};
