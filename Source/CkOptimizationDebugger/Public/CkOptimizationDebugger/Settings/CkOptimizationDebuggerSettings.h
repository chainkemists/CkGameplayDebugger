#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Suppression.h"

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
    // SNAPSHOTS
    //
    // Deliberately NOT in the "Thresholds" category: neither is a budget anything is judged against, and the
    // dashboard's threshold editor lists exactly that category. A snapshot setting appearing among the budgets would
    // read as one.
    // ----------------------------------------------------------------------------------------------------------------

    UPROPERTY(config, EditAnywhere, Category = "Snapshots",
        meta = (DisplayName = "Max Stored Snapshots", ClampMin = "1", ClampMax = "64",
            ToolTip = "Oldest snapshots are dropped past this count. Each stored snapshot holds a compressed image and a per-pixel mesh-ID map."))
    int32 MaxStoredSnapshots = 8;

    UPROPERTY(config, EditAnywhere, Category = "Snapshots",
        meta = (DisplayName = "Snapshot Capture Width", ClampMin = "256", ClampMax = "4096",
            ToolTip = "Snapshot image width in pixels; height follows the camera's aspect ratio."))
    int32 SnapshotCaptureWidth = 1280;

    UPROPERTY(config, EditAnywhere, Category = "Snapshots",
        meta = (DisplayName = "Dump Snapshot Debug Images",
            ToolTip = "Writes each captured snapshot to <Project>/Saved/CkOptimizationDebugger/ as two PNGs - the colour image and a false-coloured mesh-ID map. The pair is how mesh identification is proven correct: the ID map has to be a silhouette-exact copy of the colour image."))
    bool DumpSnapshotDebugImages = false;

    // ----------------------------------------------------------------------------------------------------------------
    UPROPERTY(config, EditAnywhere, Category = "Thresholds",
              meta = (DisplayName = "Min Sound Duration For Streaming (s)", ClampMin = "1",
                      ToolTip = "A sound wave longer than this that is not set to stream is decoded whole into memory when it loads."))
    int32 MinSoundDurationForStreaming = 10;

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
     * Stable keys of findings the reader muted, BEFORE suppressions existed. Read once at load and migrated into
     * `PersonalSuppressions`, then cleared — kept only so an existing user's triage decisions survive the change
     * rather than silently reappearing as a hundred findings they had already dealt with.
     */
    UPROPERTY(config)
    TArray<FString> MutedFindingKeys;

    /**
     * This user's own suppressions, one serialized record per line. Driven by the findings list, never typed here —
     * same reasoning as `ExcludedLevelNames` above, and stored sorted for the same reason.
     *
     * PERSONAL rather than project: "I have looked at this and I am not acting on it today" is one person's
     * judgement. The team-wide equivalent lives in `UCkOptimizationDebuggerSuppressions`, which is committed.
     */
    UPROPERTY(config)
    TArray<FString> PersonalSuppressions;

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

    /** This user's own suppressions, plus anything migrated out of the old muted-key list.
     *
     *  Per-USER for the same reason the thresholds are: a personal "not today" must never silently hide findings
     *  from everybody else. The team-wide tier is a different object and a different file — see
     *  `UCkOptimizationDebuggerSuppressions`.
     *
     *  `OutDroppedCount` carries the lines that would not parse. A suppression whose scope cannot be read would
     *  hide findings nobody chose to hide, so it is dropped and the caller says so. */
    static auto Load_PersonalSuppressions(int32& OutDroppedCount) -> TArray<FCkOptimizationDebugger_Suppression>
    {
        OutDroppedCount = 0;

        auto* Settings = GetMutableDefault<UCkOptimizationDebuggerSettings>();

        if (Settings == nullptr)
        { return TArray<FCkOptimizationDebugger_Suppression>{}; }

        auto Loaded = ck_optimization_debugger_suppression::Parse_All(
            Settings->PersonalSuppressions,
            ECkOptimizationDebugger_SuppressionTier::Personal,
            OutDroppedCount);

        // One-time migration off the pre-suppression muted-key list. Done here rather than in the window so the
        // conversion happens once per user whichever entry point loads first, and so the old key list can be
        // cleared in the same breath — two stores answering "don't show me this" is one too many.
        if (NOT Settings->MutedFindingKeys.IsEmpty())
        {
            for (const auto& StableKey : Settings->MutedFindingKeys)
            {
                auto Migrated = FCkOptimizationDebugger_Suppression{};
                Migrated.Scope = ECkOptimizationDebugger_SuppressionScope::Finding;
                Migrated.Tier = ECkOptimizationDebugger_SuppressionTier::Personal;
                Migrated.Pattern = StableKey;
                Migrated.Reason = TEXT("Muted before suppressions existed");

                Loaded.Add(MoveTemp(Migrated));
            }

            Settings->MutedFindingKeys.Reset();
            Save_PersonalSuppressions(Loaded);
        }

        return Loaded;
    }

    /** Writes this user's suppressions back out, SORTED — the same anti-spurious-diff rule the exclusion set
     *  follows, and it matters more here because the project tier's file is committed. */
    static auto Save_PersonalSuppressions(const TArray<FCkOptimizationDebugger_Suppression>& InSuppressions) -> void
    {
        auto* Settings = GetMutableDefault<UCkOptimizationDebuggerSettings>();

        if (Settings == nullptr)
        { return; }

        Settings->PersonalSuppressions = ck_optimization_debugger_suppression::Serialize_All(InSuppressions);
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

/**
 * The TEAM's suppressions — the exceptions everyone inherits.
 *
 * `defaultconfig` + `config=CkOptimizationDebugger` puts these in `Config/DefaultCkOptimizationDebugger.ini`, which
 * is committed with the project. That is the whole point: "this 4096 texture is the hero prop and it is meant to be
 * 4096" is a RULING, and a ruling that lives on one QA machine is one every teammate re-litigates.
 *
 * It is deliberately NOT a `UDeveloperSettings` and carries no `EditAnywhere`: the records are made from the
 * findings list, where the reader can see what they are excusing, and a preferences page offering a free-text
 * `Scope=...;Pattern=...` field would be a page that silently does nothing useful.
 *
 * This does NOT reopen the thresholds decision. A threshold is one person's calibration of what they want flagged,
 * so it stays per-user; a suppression is the team's ruling that a flagged thing is intentional. The two were never
 * the same kind of statement.
 */
UCLASS(defaultconfig, config = CkOptimizationDebugger)
class CKOPTIMIZATIONDEBUGGER_API UCkOptimizationDebuggerSuppressions : public UObject
{
    GENERATED_BODY()

public:
    /** One serialized record per line. Text rather than a struct array so the committed file is readable in a diff
     *  and mergeable by hand when two people add an exception in the same week. */
    UPROPERTY(config)
    TArray<FString> Suppressions;

public:
    /** The team's suppressions. `OutDroppedCount` carries lines that would not parse — dropped rather than
     *  half-applied, and reported by the caller. */
    static auto Load(int32& OutDroppedCount) -> TArray<FCkOptimizationDebugger_Suppression>
    {
        OutDroppedCount = 0;

        const auto* Store = GetDefault<UCkOptimizationDebuggerSuppressions>();

        if (Store == nullptr)
        { return TArray<FCkOptimizationDebugger_Suppression>{}; }

        return ck_optimization_debugger_suppression::Parse_All(
            Store->Suppressions,
            ECkOptimizationDebugger_SuppressionTier::Project,
            OutDroppedCount);
    }

    /** Writes the team's suppressions to the committed config file, SORTED.
     *
     *  `TryUpdateDefaultConfigFile` and its RESULT is checked, exactly as the texture-streaming fix does: a
     *  `Default*.ini` that is read-only under source control would otherwise absorb the write and leave the editor
     *  claiming an exception the file never received. Returns false so the caller can say so. */
    static auto TrySave(const TArray<FCkOptimizationDebugger_Suppression>& InSuppressions) -> bool
    {
        auto* Store = GetMutableDefault<UCkOptimizationDebuggerSuppressions>();

        if (Store == nullptr)
        { return false; }

        const auto Previous = Store->Suppressions;

        Store->Suppressions = ck_optimization_debugger_suppression::Serialize_All(InSuppressions);

        if (Store->TryUpdateDefaultConfigFile())
        { return true; }

        // Rolled back so the running editor and the file on disk keep agreeing.
        Store->Suppressions = Previous;

        return false;
    }

    static auto Get_ConfigFilePath() -> FString
    {
        const auto* Store = GetDefault<UCkOptimizationDebuggerSuppressions>();

        return Store != nullptr ? Store->GetDefaultConfigFilename() : FString{};
    }
};

// ====================================================================================================================
