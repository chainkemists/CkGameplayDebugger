#pragma once

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

class STextBlock;

// --------------------------------------------------------------------------------------------------------------------
// Cached world-level Jolt stats, refreshed on the gated Tick and read by the value-row TAttribute lambdas.
// --------------------------------------------------------------------------------------------------------------------

struct FCkJoltDebugger_Stats
{
    bool    HasWorld = false;
    FString WorldLabel;

    bool    AsyncPhysics    = false;
    bool    ParallelPhysics = false;
    int32   ThreadCount     = 0;

    int32   NumBodies    = 0;
    int32   NumDynamic   = 0;
    int32   NumKinematic = 0;
    int32   NumStatic    = 0;
    int32   NumAwake     = 0;
    int32   NumAsleep    = 0;

    int32   NumCharacters = 0;

    int32   NumStaticActors = 0;
    int32   NumStaticBodies = 0;
    int32   NumUniqueShapes = 0;
};

// --------------------------------------------------------------------------------------------------------------------
// CK Jolt Physics Debugger window — placed inside a NomadTab. Read-only world-level stats.
//
// Scrunch-free: the widget tree is built ONCE in Construct; every value is a TAttribute<FText> lambda
// reading _Stats. The gated Tick recomputes _Stats from the live PIE/Game world (ECS view walk +
// the Jolt subsystems) — never rebuilds Slate structure.
// --------------------------------------------------------------------------------------------------------------------

class SCkJoltDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkJoltDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Jolt Physics Debugger")); }

private:
    auto DoRefreshStats() -> void;

    auto MakeSectionHeader(const FString& InText) const -> TSharedRef<SWidget>;
    auto MakeStatRow(const FString& InLabel, TAttribute<FText> InValue) const -> TSharedRef<SWidget>;

    FCkJoltDebugger_Stats _Stats;
};

// --------------------------------------------------------------------------------------------------------------------
