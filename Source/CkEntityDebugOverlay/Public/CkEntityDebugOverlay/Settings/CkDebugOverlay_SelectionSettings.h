#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "CkDebugOverlay_SelectionSettings.generated.h"

// ====================================================================================================================

UENUM()
enum class ECk_DebugOverlay_SelectionHierarchy : uint8
{
    MeaningfulRoots,
    LiteralRoots,
    AllEntities,
};

UENUM()
enum class ECk_DebugOverlay_SelectionRootAnchor : uint8
{
    Member,
    Root,
};

UENUM()
enum class ECk_DebugOverlay_SelectionScope : uint8
{
    ViewWithNearbyFallback,
    InView,
    Nearby,
};

UENUM()
enum class ECk_DebugOverlay_SelectionTargeting : uint8
{
    Weighted,
    Cone,
};

UENUM()
enum class ECk_DebugOverlay_SelectionOrder : uint8
{
    Score,
    Screen,
};

UENUM()
enum class ECk_DebugOverlay_SelectionStability : uint8
{
    Frozen,
    Live,
};

UENUM()
enum class ECk_DebugOverlay_SelectionFamily : uint8
{
    Hold,
    Toggle,
};

UENUM()
enum class ECk_DebugOverlay_SelectionLabels : uint8
{
    Full,
    Numbers,
    Shortlist,
};

UENUM()
enum class ECk_DebugOverlay_SelectionPreset : uint8
{
    FocusFamily,
    SpatialSweep,
    AimCone,
};

// ====================================================================================================================

USTRUCT()
struct CKENTITYDEBUGOVERLAY_API FCk_DebugOverlay_SelectionConfig
{
    GENERATED_BODY()

    UPROPERTY(Config)
    ECk_DebugOverlay_SelectionHierarchy Hierarchy = ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots;

    UPROPERTY(Config)
    ECk_DebugOverlay_SelectionRootAnchor RootAnchor = ECk_DebugOverlay_SelectionRootAnchor::Member;

    UPROPERTY(Config)
    ECk_DebugOverlay_SelectionScope Scope = ECk_DebugOverlay_SelectionScope::ViewWithNearbyFallback;

    UPROPERTY(Config)
    ECk_DebugOverlay_SelectionTargeting Targeting = ECk_DebugOverlay_SelectionTargeting::Weighted;

    UPROPERTY(Config)
    float ViewBias = 0.70f;

    UPROPERTY(Config)
    float SearchRadius = 10000.0f;

    UPROPERTY(Config)
    float ConeHalfAngle = 15.0f;

    UPROPERTY(Config)
    ECk_DebugOverlay_SelectionOrder Order = ECk_DebugOverlay_SelectionOrder::Score;

    UPROPERTY(Config)
    ECk_DebugOverlay_SelectionStability Stability = ECk_DebugOverlay_SelectionStability::Frozen;

    UPROPERTY(Config)
    ECk_DebugOverlay_SelectionFamily Family = ECk_DebugOverlay_SelectionFamily::Hold;

    UPROPERTY(Config)
    ECk_DebugOverlay_SelectionLabels Labels = ECk_DebugOverlay_SelectionLabels::Full;

    UPROPERTY(Config)
    bool ShowAimCone = true;

    UPROPERTY(Config)
    bool IncludeOccluded = false;

    UPROPERTY(Config)
    float DiamondScale = 1.0f;
};

// ====================================================================================================================

/** Runtime, per-user selection preferences. All mutation validates a complete candidate before
 * publishing it, so malformed config or JSON cannot leave consumers with a partial selection policy. */
UCLASS(Config=GameUserSettings, meta=(DisplayName="Ck On-Screen Debugger (Selection)"))
class CKENTITYDEBUGOVERLAY_API UCk_DebugOverlay_SelectionSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual auto GetContainerName() const -> FName override { return TEXT("Editor"); }
    virtual auto GetCategoryName() const -> FName override { return TEXT("Ck"); }

    static auto Get() -> const UCk_DebugOverlay_SelectionSettings*;
    static auto Get_Mutable() -> UCk_DebugOverlay_SelectionSettings*;

    virtual auto PostInitProperties() -> void override;
    virtual auto PostReloadConfig(FProperty* InPropertyThatWasLoaded) -> void override;

    auto Get_Config() const -> const FCk_DebugOverlay_SelectionConfig& { return Config; }
    auto Get_Revision() const -> uint32 { return _Revision; }
    auto Get_LastError() const -> const FString& { return _LastError; }

    static auto TryValidate_Config(const FCk_DebugOverlay_SelectionConfig& InCandidate, FString& OutError) -> bool;
    auto TrySet_Config(const FCk_DebugOverlay_SelectionConfig& InCandidate, bool InSave = true) -> bool;
    auto ApplyPreset(ECk_DebugOverlay_SelectionPreset InPreset, bool InSave = true) -> bool;
    auto Reset_Config(bool InSave = true) -> void;

    auto TrySave_NamedPreset(FName InName, bool InSave = true) -> bool;
    auto TryApply_NamedPreset(FName InName, bool InSave = true) -> bool;
    auto Export_NamedPresetJson(FName InName, FString& OutJson) const -> bool;
    auto TryImport_NamedPresetJson(const FString& InJson, FName& OutImportedName, bool InSave = true) -> bool;

    // Config deserialization bypasses TrySet_Config. This validates that raw load boundary and
    // replaces a malformed whole policy with the known-good defaults without saving over disk.
    auto Validate_LoadedConfig() -> void;

private:
    auto Commit(bool InSave) -> void;
    auto Fail(FString InError) -> bool;

    UPROPERTY(Config)
    FCk_DebugOverlay_SelectionConfig Config;

    UPROPERTY(Config)
    TMap<FName, FCk_DebugOverlay_SelectionConfig> NamedPresets;

    uint32 _Revision = 0;
    FString _LastError;
};

// ====================================================================================================================
