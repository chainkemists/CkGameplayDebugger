#pragma once

#include "Engine/DeveloperSettings.h"

#include "Containers/Set.h"
#include "UObject/NameTypes.h"

#include "CkOptimizationDebuggerSettings.generated.h"

// ====================================================================================================================

/**
 * Per-user thresholds the level-analysis checks compare against.
 *
 * `config=GameUserSettings` + `GetContainerName() == "Editor"` puts these under Editor Preferences → Ck and keeps them
 * per-user: an analysis threshold is a QA person's calibration of what they want flagged, not shared project policy,
 * so tightening one must never dirty a committed config file for the whole team.
 */
UCLASS(config=GameUserSettings, meta=(DisplayName="Ck Optimization Debugger"))
class CKOPTIMIZATIONDEBUGGER_API UCkOptimizationDebuggerSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UCkOptimizationDebuggerSettings()
    {
        CategoryName = TEXT("Ck");
        SectionName = TEXT("Optimization Debugger");
    }

    virtual auto
    GetContainerName() const -> FName override
    {
        return TEXT("Editor");
    }

    // ----------------------------------------------------------------------------------------------------------------
    // THRESHOLDS
    // ----------------------------------------------------------------------------------------------------------------

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Max Triangle Count (LOD0)", ClampMin = "0",
            ToolTip = "A static mesh whose LOD0 exceeds this triangle count is flagged as too dense for its role. Raise it for hero assets, lower it for background dressing."))
    int32 MaxTriangleCountLOD0 = 100000;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Min Triangles For Nanite", ClampMin = "0",
            ToolTip = "A non-Nanite static mesh at or above this triangle count is flagged as a Nanite candidate."))
    int32 MinTrianglesForNanite = 5000;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Low-Poly Nanite Floor", ClampMin = "0",
            ToolTip = "A Nanite-enabled static mesh BELOW this triangle count is flagged: Nanite's per-cluster overhead is not repaid by a mesh this simple."))
    int32 MaxTrianglesForNaniteWarning = 2000;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Max Collision Primitives", ClampMin = "1",
            ToolTip = "A static mesh whose SIMPLE collision is built from more primitives than this is flagged — every primitive is a broadphase entry and a narrowphase test per overlap."))
    int32 MaxCollisionPrimitives = 8;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Max Texture Size", ClampMin = "1",
            ToolTip = "A texture whose largest dimension exceeds this is flagged as over-authored for its use. Compare against the on-screen size the asset actually gets."))
    int32 MaxTextureSize = 2048;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Max Material Slots", ClampMin = "1",
            ToolTip = "A mesh with more material slots than this is flagged — every slot is a separate draw call per instance."))
    int32 MaxMaterialSlots = 8;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Max Texture Samplers", ClampMin = "1",
            ToolTip = "A material sampling more textures than this is flagged. The platform sampler limit is the hard ceiling; this is the budget you want to stay under."))
    int32 MaxTextureSamplers = 16;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Max Movable Lights", ClampMin = "0",
            ToolTip = "More movable (fully dynamic) lights than this in one level is flagged — dynamic shadow cost scales with overlap, not just count."))
    int32 MaxMovableLights = 4;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Max Lightmap Resolution", ClampMin = "1",
            ToolTip = "A component whose overridden lightmap resolution exceeds this is flagged as spending bake time and memory disproportionate to its screen area."))
    int32 MaxLightmapResolution = 512;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Min Repeated Actors For Instancing", ClampMin = "2",
            ToolTip = "This many actors sharing one mesh in a level is flagged as an instancing candidate (ISM/HISM)."))
    int32 MinRepeatedActorsForInstancing = 10;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Max Blueprint Dependencies", ClampMin = "1",
            ToolTip = "A Blueprint with more hard references than this is flagged: every hard reference is loaded with it, so the dependency count is the load cost."))
    int32 MaxBlueprintDependencies = 50;

    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
        meta = (DisplayName = "Min Textures For Streaming Warning", ClampMin = "1",
            ToolTip = "Texture streaming being off is only worth flagging once the level actually uses textures. At or above this many distinct textures the project-settings check reports it."))
    int32 MinTexturesForStreamingWarning = 50;

    // ----------------------------------------------------------------------------------------------------------------
    // SCAN SCOPE
    // ----------------------------------------------------------------------------------------------------------------

    /**
     * Levels the next scan skips, by package short name.
     *
     * Driven by the Dashboard's per-level toggles rather than typed here, which is why it carries no `EditAnywhere` —
     * a name typed into Editor Preferences would not match any level the open world lists, and a preferences page
     * that silently does nothing is worse than one that does not offer the field. It IS `config`, because a scope a
     * user narrowed once should still be narrow after they restart the editor.
     */
    UPROPERTY(config)
    TArray<FName> ExcludedLevelNames;

    /**
     * Stable keys of findings the reader muted. Driven by the findings list's context menu, never typed here — same
     * reasoning as `ExcludedLevelNames` above, and stored sorted for the same reason.
     *
     * It grows without bound by design: a key is small, and pruning it against "findings the current scan produced"
     * would silently un-mute everything the reader had triaged on a level they have not opened this session.
     */
    UPROPERTY(config)
    TArray<FString> MutedFindingKeys;

    /**
     * Check ids whose group the findings list draws folded. Driven by the group headers, never typed here — same
     * reasoning as the two lists above, and stored sorted for the same reason.
     *
     * Persisted because it is how the reader arranged a list they will come back to, and a project with twenty-eight
     * checks in it is one where "fold the four families I am not working on" is worth not doing twice a day. It says
     * nothing about which findings matter: a folded group is still counted everywhere this window counts.
     */
    UPROPERTY(config)
    TArray<FName> CollapsedCheckIds;

    // ----------------------------------------------------------------------------------------------------------------
    // ACCESSORS
    // ----------------------------------------------------------------------------------------------------------------

    static auto Get() -> const UCkOptimizationDebuggerSettings*
    {
        return GetDefault<UCkOptimizationDebuggerSettings>();
    }

    /** The persisted exclusion list as the set the scan and the model both work in. */
    static auto Get_ExcludedLevelNameSet() -> TSet<FName>
    {
        const auto* Settings = Get();

        if (Settings == nullptr)
        { return TSet<FName>{}; }

        return TSet<FName>{Settings->ExcludedLevelNames};
    }

    /** Writes the exclusion set back out, SORTED. A `TSet`'s iteration order follows its hash layout, so persisting
     *  it unsorted would rewrite the same ini line in a different order on different machines and turn a
     *  user-preference file into a source of spurious diffs. */
    static auto Save_ExcludedLevelNames(const TSet<FName>& InLevelNames) -> void
    {
        auto* Settings = GetMutableDefault<UCkOptimizationDebuggerSettings>();

        if (Settings == nullptr)
        { return; }

        Settings->ExcludedLevelNames = InLevelNames.Array();
        Settings->ExcludedLevelNames.Sort([](const FName& InLhs, const FName& InRhs)
        {
            return InLhs.Compare(InRhs) < 0;
        });

        Settings->SaveConfig();
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The findings the reader has triaged away, by stable key.
     *
     *  Same shape and same reasoning as `ExcludedLevelNames`: no `EditAnywhere`, because a stable key hand-typed into
     *  a preferences page would match nothing, and `config` because a triage decision that evaporated on restart is
     *  one nobody would make twice. Per-USER rather than project config for the same reason the thresholds are: "I
     *  have looked at this and I am not acting on it" is one person's judgement, not team policy, and one QA pass
     *  must never silently hide findings from everybody else. */
    static auto Load_MutedStableKeys() -> TSet<FString>
    {
        const auto* Settings = Get();

        if (Settings == nullptr)
        { return TSet<FString>{}; }

        return TSet<FString>{Settings->MutedFindingKeys};
    }

    /** Writes the muted set back out, SORTED — the same anti-spurious-diff rule the exclusion set follows. */
    static auto Save_MutedStableKeys(const TSet<FString>& InStableKeys) -> void
    {
        auto* Settings = GetMutableDefault<UCkOptimizationDebuggerSettings>();

        if (Settings == nullptr)
        { return; }

        Settings->MutedFindingKeys = InStableKeys.Array();
        Settings->MutedFindingKeys.Sort([](const FString& InLhs, const FString& InRhs)
        {
            return InLhs.Compare(InRhs, ESearchCase::CaseSensitive) < 0;
        });

        Settings->SaveConfig();
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The check groups the reader last left folded. */
    static auto Load_CollapsedCheckIds() -> TSet<FName>
    {
        const auto* Settings = Get();

        if (Settings == nullptr)
        { return TSet<FName>{}; }

        return TSet<FName>{Settings->CollapsedCheckIds};
    }

    /** Writes the collapsed set back out, SORTED — the same anti-spurious-diff rule the other two sets follow. */
    static auto Save_CollapsedCheckIds(const TSet<FName>& InCheckIds) -> void
    {
        auto* Settings = GetMutableDefault<UCkOptimizationDebuggerSettings>();

        if (Settings == nullptr)
        { return; }

        Settings->CollapsedCheckIds = InCheckIds.Array();
        Settings->CollapsedCheckIds.Sort([](const FName& InLhs, const FName& InRhs)
        {
            return InLhs.Compare(InRhs) < 0;
        });

        Settings->SaveConfig();
    }
};

// ====================================================================================================================
