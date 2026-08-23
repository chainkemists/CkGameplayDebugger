#include "CkDebug_WorldSpeed.h"

#include "CkCore/Validation/CkIsValid.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::DebugWorldSpeed::
    Choose_AuthorityIndex(
        const TArray<ENetMode>& InCandidateModes,
        FText& OutReason)
    -> int32
{
    OutReason = FText::GetEmpty();
    int32 AuthorityIndex = INDEX_NONE;

    for (auto Index = 0; Index < InCandidateModes.Num(); ++Index)
    {
        if (InCandidateModes[Index] == NM_Client)
        { continue; }

        if (AuthorityIndex != INDEX_NONE)
        {
            OutReason = FText::FromString(TEXT(
                "More than one authority game world is active. Close the unrelated PIE/Game session before changing speed."));
            return INDEX_NONE;
        }

        AuthorityIndex = Index;
    }

    if (AuthorityIndex == INDEX_NONE)
    {
        OutReason = InCandidateModes.IsEmpty()
            ? FText::FromString(TEXT("No running PIE or Game world is available."))
            : FText::FromString(TEXT(
                "Client-only session. World speed is authority-owned; change it from the listen server or server process."));
    }

    return AuthorityIndex;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::DebugWorldSpeed::
    Resolve_AuthorityWorld()
    -> FCkDebug_WorldSpeedTarget
{
    if (NOT ck::IsValid(GEngine))
    { return {{}, FText::FromString(TEXT("The engine is not available."))}; }

    auto Worlds = TArray<TWeakObjectPtr<UWorld>>{};
    auto Modes = TArray<ENetMode>{};

    for (const auto& Context : GEngine->GetWorldContexts())
    {
        auto* World = Context.World();
        const auto IsGameWorld = ck::IsValid(World)
            && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game);
        if (NOT IsGameWorld)
        { continue; }

        Worlds.Add(World);
        Modes.Add(World->GetNetMode());
    }

    auto Reason = FText::GetEmpty();
    const auto AuthorityIndex = Choose_AuthorityIndex(Modes, Reason);
    if (AuthorityIndex == INDEX_NONE)
    { return {{}, MoveTemp(Reason)}; }

    return {Worlds[AuthorityIndex], FText::GetEmpty()};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::DebugWorldSpeed::
    Get_Multiplier()
    -> TOptional<float>
{
    const auto Target = Resolve_AuthorityWorld();
    auto* World = Target.World.Get();
    if (NOT ck::IsValid(World))
    { return {}; }

    const auto* WorldSettings = World->GetWorldSettings();
    if (NOT ck::IsValid(WorldSettings))
    { return {}; }

    return WorldSettings->TimeDilation;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::DebugWorldSpeed::
    Try_SetMultiplier(
        float InMultiplier,
        FText& OutFailureReason)
    -> bool
{
    OutFailureReason = FText::GetEmpty();

    const auto MultiplierIsValid = FMath::IsFinite(InMultiplier) && InMultiplier > 0.0f;
    if (NOT MultiplierIsValid)
    {
        OutFailureReason = FText::FromString(TEXT("World speed must be a finite value greater than zero."));
        return false;
    }

    const auto Target = Resolve_AuthorityWorld();
    auto* World = Target.World.Get();
    if (NOT ck::IsValid(World))
    {
        OutFailureReason = Target.Reason;
        return false;
    }

    auto* WorldSettings = World->GetWorldSettings();
    if (NOT ck::IsValid(WorldSettings))
    {
        OutFailureReason = FText::FromString(TEXT("The authority world has no valid WorldSettings."));
        return false;
    }

    WorldSettings->SetTimeDilation(InMultiplier);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
