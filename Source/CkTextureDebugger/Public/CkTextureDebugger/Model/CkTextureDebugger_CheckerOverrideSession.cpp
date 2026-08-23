#include "CkTextureDebugger/Model/CkTextureDebugger_CheckerOverrideSession.h"

#include "CkCore/Macros/CkMacros.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_texture_debugger_checker_override_session
{
    using FStrongTopology = TArray<TStrongObjectPtr<UMaterialInterface>>;
    using FWeakTopology = TArray<TWeakObjectPtr<UMaterialInterface>>;

    auto
        MakeReport(
            ECkTextureDebugger_CheckerSessionResult InResult,
            FString InDetail) -> FCkTextureDebugger_CheckerSessionReport
    {
        auto Report = FCkTextureDebugger_CheckerSessionReport{};
        Report.Result = InResult;
        Report.Detail = MoveTemp(InDetail);
        return Report;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        CaptureStrongTopology(
            const UMeshComponent* InComponent) -> FStrongTopology
    {
        auto Result = FStrongTopology{};
        if (InComponent == nullptr)
        { return Result; }

        Result.Reserve(InComponent->OverrideMaterials.Num());
        for (const auto& Material : InComponent->OverrideMaterials)
        {
            Result.Emplace(TStrongObjectPtr<UMaterialInterface>{Material.Get()});
        }

        return Result;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        CaptureWeakTopology(
            const UMeshComponent* InComponent) -> FWeakTopology
    {
        auto Result = FWeakTopology{};
        if (InComponent == nullptr)
        { return Result; }

        Result.Reserve(InComponent->OverrideMaterials.Num());
        for (const auto& Material : InComponent->OverrideMaterials)
        {
            Result.Emplace(Material.Get());
        }

        return Result;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        IsSameWeakTopology(
            const UMeshComponent* InComponent,
            const FWeakTopology& InExpected) -> bool
    {
        if (InComponent == nullptr || InComponent->OverrideMaterials.Num() != InExpected.Num())
        { return false; }

        for (auto Index = 0; Index < InExpected.Num(); ++Index)
        {
            if (InComponent->OverrideMaterials[Index].Get() != InExpected[Index].Get())
            { return false; }
        }

        return true;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        IsSameStrongTopology(
            const UMeshComponent* InComponent,
            const FStrongTopology& InExpected) -> bool
    {
        if (InComponent == nullptr || InComponent->OverrideMaterials.Num() != InExpected.Num())
        { return false; }

        for (auto Index = 0; Index < InExpected.Num(); ++Index)
        {
            if (InComponent->OverrideMaterials[Index].Get() != InExpected[Index].Get())
            { return false; }
        }

        return true;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        RestoreTopology(
            UMeshComponent* InComponent,
            const FStrongTopology& InTopology) -> bool
    {
        if (InComponent == nullptr || NOT InComponent->GetComponentMaterialSlotsOverlayMaterial().IsEmpty())
        { return false; }

        // This is the sole public API that removes tool-created override entries. It is safe only because admission
        // and restore both reject per-slot overlays: EmptyOverrideMaterials clears that unrelated array too.
        InComponent->EmptyOverrideMaterials();

        for (auto Index = 0; Index < InTopology.Num(); ++Index)
        {
            InComponent->SetMaterial(Index, InTopology[Index].Get());
        }

        return IsSameStrongTopology(InComponent, InTopology);
    }

    // ----------------------------------------------------------------------------------------------------------------

}

// --------------------------------------------------------------------------------------------------------------------

FCkTextureDebugger_CheckerOverrideSession::~FCkTextureDebugger_CheckerOverrideSession()
{
    // The integration owner must call TryRestore while a normal world is alive. At shutdown a weak world may already
    // be unsafe, so destruction only releases managed roots rather than attempting a late UObject mutation.
    Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    SwitchCheckerTexture(
        UTexture* InCheckerTexture,
        FName InTextureParameter) -> FCkTextureDebugger_CheckerSessionReport
{
    using namespace ck_texture_debugger_checker_override_session;

    if (IsSuspendedForWorldSave())
    {
        return MakeReport(ECkTextureDebugger_CheckerSessionResult::SessionSuspendedForWorldSave,
            TEXT("The checker session is suspended while the world is being saved."));
    }

    if (NOT HasActiveSession())
    {
        return MakeReport(ECkTextureDebugger_CheckerSessionResult::NoActiveSession,
            TEXT("There is no active checker session to update."));
    }

    if (NOT IsValid(InCheckerTexture))
    {
        return MakeReport(ECkTextureDebugger_CheckerSessionResult::InvalidCheckerTexture,
            TEXT("The selected checker texture is unavailable."));
    }

    auto* Checker = Cast<UMaterialInstanceDynamic>(_CheckerMaterial.Get());
    if (NOT IsValid(Checker))
    {
        return MakeReport(ECkTextureDebugger_CheckerSessionResult::CheckerMaterialNotDynamic,
            TEXT("The active checker material is not a dynamic material instance."));
    }

    auto* PreviousTexture = static_cast<UTexture*>(nullptr);
    const auto ParameterInfo = FMaterialParameterInfo{InTextureParameter};
    const auto ParameterExists = NOT InTextureParameter.IsNone() &&
        Checker->GetTextureParameterValue(ParameterInfo, PreviousTexture);
    if (NOT ParameterExists)
    {
        return MakeReport(ECkTextureDebugger_CheckerSessionResult::InvalidCheckerParameter,
            TEXT("The active checker material does not expose the selected texture parameter."));
    }

    Checker->SetTextureParameterValue(InTextureParameter, InCheckerTexture);
    auto* AppliedTexture = static_cast<UTexture*>(nullptr);
    const auto TextureUpdated = Checker->GetTextureParameterValue(ParameterInfo, AppliedTexture) &&
        AppliedTexture == InCheckerTexture;
    if (NOT TextureUpdated)
    {
        Checker->SetTextureParameterValue(InTextureParameter, PreviousTexture);
        auto* RestoredTexture = static_cast<UTexture*>(nullptr);
        const auto PreviousRestored = Checker->GetTextureParameterValue(ParameterInfo, RestoredTexture) &&
            RestoredTexture == PreviousTexture;
        return MakeReport(ECkTextureDebugger_CheckerSessionResult::VerificationFailed,
            PreviousRestored
                ? TEXT("Checker texture verification failed; the previous texture was restored.")
                : TEXT("Checker texture verification and rollback both failed."));
    }

    auto Report = MakeReport(ECkTextureDebugger_CheckerSessionResult::Success, TEXT("Checker texture updated."));
    Report.AppliedComponentCount = _Ledgers.Num();
    return Report;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    Apply(
        UWorld* InWorld,
        UMaterialInterface* InCheckerMaterial,
        const TArray<FCkTextureDebugger_CheckerTarget>& InTargets) -> FCkTextureDebugger_CheckerSessionReport
{
    using namespace ck_texture_debugger_checker_override_session;

    if (HasActiveSession() || IsSuspendedForWorldSave())
    { return MakeReport(ECkTextureDebugger_CheckerSessionResult::SessionAlreadyActive, TEXT("Restore or finish the active checker session first.")); }

    if (InWorld == nullptr)
    { return MakeReport(ECkTextureDebugger_CheckerSessionResult::InvalidWorld, TEXT("There is no active world.")); }

    if (InCheckerMaterial == nullptr)
    { return MakeReport(ECkTextureDebugger_CheckerSessionResult::InvalidCheckerMaterial, TEXT("The selected checker material is unavailable.")); }

    if (InTargets.IsEmpty())
    { return MakeReport(ECkTextureDebugger_CheckerSessionResult::EmptyTargets, TEXT("No material slots were selected.")); }

    auto Ledgers = TArray<FComponentLedger>{};
    Ledgers.Reserve(InTargets.Num());

    // All admission checks happen before any component mutates.
    for (auto TargetIndex = 0; TargetIndex < InTargets.Num(); ++TargetIndex)
    {
        const auto& Target = InTargets[TargetIndex];
        auto* Component = Target.Component.Get();
        auto Failure = FString{};
        if (NOT CanMutateComponent(Component, InWorld, Failure))
        { return MakeReport(ECkTextureDebugger_CheckerSessionResult::InvalidTarget, MoveTemp(Failure)); }

        if (NOT Component->GetComponentMaterialSlotsOverlayMaterial().IsEmpty())
        {
            return MakeReport(ECkTextureDebugger_CheckerSessionResult::ComponentHasSlotOverlay,
                TEXT("Checker replacement is unavailable while the component has per-slot overlay materials."));
        }

        for (auto PriorIndex = 0; PriorIndex < TargetIndex; ++PriorIndex)
        {
            if (InTargets[PriorIndex].Component.Get() == Component)
            { return MakeReport(ECkTextureDebugger_CheckerSessionResult::InvalidTarget, TEXT("A component was targeted more than once.")); }
        }

        if (Target.SlotIndices.IsEmpty())
        { return MakeReport(ECkTextureDebugger_CheckerSessionResult::InvalidTarget, TEXT("A target did not name any material slots.")); }

        auto AppliedSlots = Target.SlotIndices;
        AppliedSlots.Sort();
        for (auto Index = 0; Index < AppliedSlots.Num(); ++Index)
        {
            const auto SlotIsUnique = Index == 0 || AppliedSlots[Index - 1] != AppliedSlots[Index];
            const auto SlotIsValid = AppliedSlots[Index] >= 0 && AppliedSlots[Index] < Component->GetNumMaterials();
            if (NOT SlotIsUnique || NOT SlotIsValid)
            {
                return MakeReport(ECkTextureDebugger_CheckerSessionResult::InvalidTarget,
                    TEXT("A checker target contains a duplicate or invalid material slot."));
            }
        }

        auto Ledger = FComponentLedger{};
        Ledger.Component = Component;
        Ledger.AppliedSlots = MoveTemp(AppliedSlots);
        Ledger.OriginalTopology = CaptureStrongTopology(Component);
        Ledgers.Add(MoveTemp(Ledger));
    }

    // Mutation cannot yield, so an outside gameplay write cannot interleave this transaction on the game thread.
    for (auto& Ledger : Ledgers)
    {
        auto* Component = Ledger.Component.Get();
        for (const auto SlotIndex : Ledger.AppliedSlots)
        {
            Component->SetMaterial(SlotIndex, InCheckerMaterial);
        }

        Ledger.ExpectedCheckerTopology = CaptureWeakTopology(Component);

        auto Verified = true;
        for (const auto SlotIndex : Ledger.AppliedSlots)
        {
            if (Component->GetMaterial(SlotIndex) != InCheckerMaterial ||
                NOT Ledger.ExpectedCheckerTopology.IsValidIndex(SlotIndex) ||
                Ledger.ExpectedCheckerTopology[SlotIndex].Get() != InCheckerMaterial)
            {
                Verified = false;
                break;
            }
        }

        if (NOT Verified)
        {
            for (auto RollbackIndex = Ledgers.Num() - 1; RollbackIndex >= 0; --RollbackIndex)
            {
                auto* RollbackComponent = Ledgers[RollbackIndex].Component.Get();
                if (RollbackComponent != nullptr)
                { RestoreTopology(RollbackComponent, Ledgers[RollbackIndex].OriginalTopology); }
            }

            return MakeReport(ECkTextureDebugger_CheckerSessionResult::VerificationFailed,
                TEXT("Checker verification failed; every applied component was rolled back."));
        }
    }

    _World = InWorld;
    _CheckerMaterial = TStrongObjectPtr<UMaterialInterface>{InCheckerMaterial};
    _Ledgers = MoveTemp(Ledgers);

    auto Report = MakeReport(ECkTextureDebugger_CheckerSessionResult::Success, TEXT("Checker applied."));
    Report.AppliedComponentCount = _Ledgers.Num();
    return Report;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    CanMutateComponent(
        UMeshComponent* InComponent,
        UWorld* InExpectedWorld,
        FString& OutFailure) const -> bool
{
    OutFailure.Reset();

    if (InComponent == nullptr)
    {
        OutFailure = TEXT("A selected component no longer exists.");
        return false;
    }

    if (InExpectedWorld == nullptr || InComponent->GetWorld() != InExpectedWorld)
    {
        OutFailure = TEXT("A selected component belongs to a different world.");
        return false;
    }

    if (NOT InComponent->IsRegistered())
    {
        OutFailure = TEXT("A selected component is not registered.");
        return false;
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    RestoreLedger(
        FComponentLedger& InOutLedger,
        FCkTextureDebugger_CheckerSessionReport& InOutReport) -> bool
{
    using namespace ck_texture_debugger_checker_override_session;

    auto* Component = InOutLedger.Component.Get();
    if (Component == nullptr)
    {
        ++InOutReport.DroppedDestroyedComponentCount;
        return true;
    }

    auto Failure = FString{};
    if (NOT CanMutateComponent(Component, _World.Get(), Failure))
    {
        InOutReport.Result = ECkTextureDebugger_CheckerSessionResult::WorldChanged;
        InOutReport.Detail = MoveTemp(Failure);
        return false;
    }

    if (NOT Component->GetComponentMaterialSlotsOverlayMaterial().IsEmpty())
    {
        InOutReport.Result = ECkTextureDebugger_CheckerSessionResult::RestoreBlockedBySlotOverlay;
        InOutReport.Detail = TEXT("A per-slot overlay appeared after checker application; restore is blocked to preserve it.");
        return false;
    }

    auto DesiredTopology = CaptureStrongTopology(Component);
    auto HighestExternalIndex = int32{INDEX_NONE};

    const auto MaxSlots = FMath::Max(InOutLedger.ExpectedCheckerTopology.Num(), DesiredTopology.Num());
    if (DesiredTopology.Num() < MaxSlots)
    { DesiredTopology.SetNum(MaxSlots); }

    for (auto SlotIndex = 0; SlotIndex < MaxSlots; ++SlotIndex)
    {
        const auto* Expected = InOutLedger.ExpectedCheckerTopology.IsValidIndex(SlotIndex)
            ? InOutLedger.ExpectedCheckerTopology[SlotIndex].Get()
            : nullptr;
        const auto* Current = Component->OverrideMaterials.IsValidIndex(SlotIndex)
            ? Component->OverrideMaterials[SlotIndex].Get()
            : nullptr;

        if (Current != Expected)
        {
            ++InOutReport.PreservedExternalSlotCount;
            HighestExternalIndex = FMath::Max(HighestExternalIndex, SlotIndex);
            continue;
        }

        if (InOutLedger.AppliedSlots.Contains(SlotIndex))
        {
            DesiredTopology[SlotIndex] = InOutLedger.OriginalTopology.IsValidIndex(SlotIndex)
                ? TStrongObjectPtr<UMaterialInterface>{InOutLedger.OriginalTopology[SlotIndex].Get()}
                : TStrongObjectPtr<UMaterialInterface>{};
        }
    }

    // Exact original topology is preserved when no external writer touched the component. In a conflict case retain
    // every externally added index but discard tool-only trailing checker slots.
    const auto DesiredCount = FMath::Max(InOutLedger.OriginalTopology.Num(), HighestExternalIndex + 1);
    DesiredTopology.SetNum(DesiredCount);

    if (NOT RestoreTopology(Component, DesiredTopology))
    {
        InOutReport.Result = ECkTextureDebugger_CheckerSessionResult::VerificationFailed;
        InOutReport.Detail = TEXT("Material restore verification failed.");
        return false;
    }

    ++InOutReport.RestoredComponentCount;
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    TryRestore() -> FCkTextureDebugger_CheckerSessionReport
{
    using namespace ck_texture_debugger_checker_override_session;

    if (NOT HasActiveSession())
    { return MakeReport(ECkTextureDebugger_CheckerSessionResult::NoActiveSession, TEXT("There is no checker session to restore.")); }

    auto Report = MakeReport(ECkTextureDebugger_CheckerSessionResult::Success, TEXT("Checker restored."));
    auto RemainingLedgers = TArray<FComponentLedger>{};

    for (auto& Ledger : _Ledgers)
    {
        if (RestoreLedger(Ledger, Report))
        { continue; }

        RemainingLedgers.Add(MoveTemp(Ledger));
    }

    _Ledgers = MoveTemp(RemainingLedgers);
    if (_Ledgers.IsEmpty())
    {
        _CheckerMaterial.Reset();
        _World.Reset();
    }

    return Report;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    DiscardDestroyedComponents() -> int32
{
    auto DroppedCount = 0;
    _Ledgers.RemoveAll([&DroppedCount](const FComponentLedger& InLedger)
    {
        if (InLedger.Component.IsValid())
        { return false; }

        ++DroppedCount;
        return true;
    });

    if (_Ledgers.IsEmpty() && NOT IsSuspendedForWorldSave())
    {
        _CheckerMaterial.Reset();
        _World.Reset();
    }

    return DroppedCount;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    ReleaseForWorldCleanup(
        UWorld* InWorld) -> void
{
    const auto* SuspendedWorld = _SaveResumeState.IsSet() ? _SaveResumeState->World.Get() : nullptr;
    if (_World.Get() != InWorld && SuspendedWorld != InWorld)
    { return; }

    Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    PrepareForWorldSave(
        UWorld* InWorld) -> FCkTextureDebugger_CheckerSessionReport
{
    using namespace ck_texture_debugger_checker_override_session;

    if (NOT HasActiveSession() || _World.Get() != InWorld)
    { return MakeReport(ECkTextureDebugger_CheckerSessionResult::NoActiveSession, TEXT("There is no matching checker session to suspend for save.")); }

    auto Resume = FSaveResumeState{};
    Resume.World = _World;
    Resume.CheckerMaterial = TStrongObjectPtr<UMaterialInterface>{_CheckerMaterial.Get()};

    auto Report = MakeReport(
        ECkTextureDebugger_CheckerSessionResult::Success,
        TEXT("Checker removed before world serialization."));

    // Preflight the complete suspension before mutating any component. SetMaterial is a synchronous void API on the
    // game thread; after this pass there is no fallible step and therefore no partial-removal return path.
    for (const auto& Ledger : _Ledgers)
    {
        auto* Component = Ledger.Component.Get();
        if (Component == nullptr)
        { continue; }

        if (Component->GetWorld() != InWorld)
        {
            Report.Result = ECkTextureDebugger_CheckerSessionResult::SavePreparationFailed;
            Report.Detail = TEXT("A checker component changed worlds before save; serialization safety could not be proven.");
            return Report;
        }

        for (const auto SlotIndex : Ledger.AppliedSlots)
        {
            if (SlotIndex < 0 || NOT Ledger.ExpectedCheckerTopology.IsValidIndex(SlotIndex))
            {
                Report.Result = ECkTextureDebugger_CheckerSessionResult::SavePreparationFailed;
                Report.Detail = TEXT("A checker ledger was malformed before save; no component was mutated.");
                return Report;
            }
        }

        auto Target = FCkTextureDebugger_CheckerTarget{};
        Target.Component = Component;
        Target.SlotIndices = Ledger.AppliedSlots;
        Resume.Targets.Add(MoveTemp(Target));
    }

    // Saving has a stricter invariant than ordinary exact-topology restore: no tool checker pointer may reach the
    // serializer. Restore only slots that still contain our expected checker through SetMaterial. This deliberately
    // avoids EmptyOverrideMaterials, so a per-slot overlay that appeared mid-session is preserved and cannot block
    // removal of the checker before Save All.
    for (auto& Ledger : _Ledgers)
    {
        auto* Component = Ledger.Component.Get();
        if (Component == nullptr)
        {
            ++Report.DroppedDestroyedComponentCount;
            continue;
        }

        for (const auto SlotIndex : Ledger.AppliedSlots)
        {
            const auto* ExpectedChecker = Ledger.ExpectedCheckerTopology.IsValidIndex(SlotIndex)
                ? Ledger.ExpectedCheckerTopology[SlotIndex].Get()
                : nullptr;
            auto* CurrentOverride = Component->OverrideMaterials.IsValidIndex(SlotIndex)
                ? Component->OverrideMaterials[SlotIndex].Get()
                : nullptr;

            if (CurrentOverride != ExpectedChecker)
            {
                ++Report.PreservedExternalSlotCount;
                continue;
            }

            auto* Original = Ledger.OriginalTopology.IsValidIndex(SlotIndex)
                ? Ledger.OriginalTopology[SlotIndex].Get()
                : nullptr;
            Component->SetMaterial(SlotIndex, Original);
        }

        ++Report.RestoredComponentCount;
    }

    _Ledgers.Reset();
    _CheckerMaterial.Reset();
    _World.Reset();
    _SaveResumeState = MoveTemp(Resume);
    return Report;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    CompleteWorldSave(
        bool InSaveSucceeded) -> FCkTextureDebugger_CheckerSessionReport
{
    using namespace ck_texture_debugger_checker_override_session;

    if (NOT _SaveResumeState.IsSet())
    { return MakeReport(ECkTextureDebugger_CheckerSessionResult::NoActiveSession, TEXT("There is no suspended checker session.")); }

    auto Resume = MoveTemp(_SaveResumeState.GetValue());
    _SaveResumeState.Reset();

    if (NOT InSaveSucceeded)
    { return MakeReport(ECkTextureDebugger_CheckerSessionResult::Success, TEXT("Save failed; checker was intentionally left restored.")); }

    return Apply(Resume.World.Get(), Resume.CheckerMaterial.Get(), Resume.Targets);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    HasActiveSession() const -> bool
{
    return NOT _Ledgers.IsEmpty();
}

auto
    FCkTextureDebugger_CheckerOverrideSession::
    IsSuspendedForWorldSave() const -> bool
{
    return _SaveResumeState.IsSet();
}

auto
    FCkTextureDebugger_CheckerOverrideSession::
    GetWorld() const -> UWorld*
{
    return _SaveResumeState.IsSet() ? _SaveResumeState->World.Get() : _World.Get();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebugger_CheckerOverrideSession::
    Reset() -> void
{
    _Ledgers.Reset();
    _SaveResumeState.Reset();
    _CheckerMaterial.Reset();
    _World.Reset();
}
