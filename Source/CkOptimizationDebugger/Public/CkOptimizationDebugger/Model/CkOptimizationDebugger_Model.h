#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "Misc/Optional.h"
#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPath.h"

// --------------------------------------------------------------------------------------------------------------------

/** The window's five pages. The page bar and the body switcher are both driven from this enum IN DECLARATION ORDER —
 *  `Get_PageIndex` is the enum's own value, so a switcher slot must be added per page in exactly this order. */
enum class ECkOptimizationDebugger_Page : uint8
{
    Dashboard,
    Findings,
    Memory,
    Profiling,
    Cleanup,
};

// --------------------------------------------------------------------------------------------------------------------

/** How badly a finding hurts. Declaration order IS severity order (most severe first) — the sort and the mask bits
 *  both read it that way, so inserting a value in the middle re-orders the findings list on purpose. */
enum class ECkOptimizationDebugger_Severity : uint8
{
    Critical,
    Major,
    Minor,
};

// --------------------------------------------------------------------------------------------------------------------

/** What kind of thing a check looked at. Used for the category filter and for grouping the findings list. */
enum class ECkOptimizationDebugger_Category : uint8
{
    Mesh,
    Texture,
    Material,
    Lighting,
    Actor,
    Blueprint,
    ProjectSettings,
};

// --------------------------------------------------------------------------------------------------------------------

/** What a finding points AT, which is also what a later phase's navigation action has to do with it: sync an asset in
 *  the Content Browser, focus an actor in a level, or open a Project Settings page. */
enum class ECkOptimizationDebugger_TargetKind : uint8
{
    Asset,
    Actor,
    ProjectSettings,
};

// --------------------------------------------------------------------------------------------------------------------

/** The thing a finding names. Plain data on purpose: an offline analysis row must survive a map change and a PIE
 *  session without holding a UObject, a handle, or anything else that can die underneath it. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_Target
{
    ECkOptimizationDebugger_TargetKind Kind = ECkOptimizationDebugger_TargetKind::Asset;

    // Asset path for an Asset target, the actor's object path for an Actor target, empty for ProjectSettings.
    FSoftObjectPath Path;

    FString DisplayName;

    // Which level the actor lives in — Actor targets only. A sub-level's findings are attributable to that sub-level.
    FString LevelName;

    // The Project Settings section a ProjectSettings finding is about (e.g. "Rendering") — empty otherwise.
    FString SettingsSectionName;
};

// --------------------------------------------------------------------------------------------------------------------

/** One thing the analysis found. Rows are reused across refreshes BY `StableKey` — SListView tracks selection by
 *  pointer identity, and a re-scan that reproduces the same finding must not move the user's selection. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_FindingRow
{
    // CheckId + '|' + target path. Unique per (check, thing) pair, which is exactly what a finding is.
    FString StableKey;

    FName CheckId;

    ECkOptimizationDebugger_Severity Severity = ECkOptimizationDebugger_Severity::Minor;
    ECkOptimizationDebugger_Category Category = ECkOptimizationDebugger_Category::Mesh;

    FCkOptimizationDebugger_Target Target;

    FString Title;
    FString Explanation;
    FString Recommendation;

    // Whether a later phase can apply this finding's fix through a UE transaction. Findings without one are still
    // worth listing — an explanation the reader acts on by hand is not a lesser finding.
    bool HasAutoFix = false;
    FString FixDescription;
};

// --------------------------------------------------------------------------------------------------------------------

/** What the user has narrowed the findings list to. Two independent text queries (the suite's Filter/Highlight
 *  contract) plus one visibility bit per severity and per category. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_FilterState
{
    FString FilterString;
    FString HighlightString;

    // One bit per enum value; all bits set == no filtering. The defaults are spelled out here rather than left to
    // zero-init, because a zero mask would read as "hide everything" — the one state a fresh filter must never be in.
    // `k_AllSeverityMask` / `k_AllCategoryMask` below are the same values, pinned by a spec.
    uint8 SeverityMask = 0x07;
    uint8 CategoryMask = 0x7F;
};

// --------------------------------------------------------------------------------------------------------------------

/** Every finding one check produced, in the order the sorted findings list already put them. The findings page
 *  renders one non-selectable header per group followed by its rows, so the group's own fields are exactly what a
 *  header needs and nothing more: a check is named once, at its worst severity, in its own category. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_FindingGroup
{
    FName CheckId;

    // The first finding's title. Every finding a check emits shares a title shape, so the first one names the group
    // without inventing a second string for the same idea.
    FString Title;

    ECkOptimizationDebugger_Severity WorstSeverity = ECkOptimizationDebugger_Severity::Minor;
    ECkOptimizationDebugger_Category Category = ECkOptimizationDebugger_Category::Mesh;

    TArray<FCkOptimizationDebugger_FindingRow> Findings;
};

// --------------------------------------------------------------------------------------------------------------------

/** Findings per severity — the dashboard's headline projection and the page bar's Findings count. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SeverityCounts
{
    int32 CriticalCount = 0;
    int32 MajorCount = 0;
    int32 MinorCount = 0;

    auto Get_Total() const -> int32 { return CriticalCount + MajorCount + MinorCount; }
};

// --------------------------------------------------------------------------------------------------------------------

/** The asset families the project's disk-size breakdown buckets `/Game` into. Declaration order IS display order for
 *  the label table; the dashboard itself sorts the rows by size, so this order only decides ties.
 *
 *  Eight buckets on purpose. A per-class breakdown of a real project is a hundred rows nobody reads; the question the
 *  reader is actually asking is "what is eating the disk", and that answer has never been more than a handful of
 *  families. `Animation` covers skeletal content as a whole — skeletons, skeletal meshes and animation assets — since
 *  a project that is heavy in one is heavy in all three, and splitting them would put three rows where one belongs. */
enum class ECkOptimizationDebugger_DiskCategory : uint8
{
    Textures,
    StaticMeshes,
    Materials,
    Blueprints,
    Levels,
    Audio,
    Animation,
    Other,
};

// --------------------------------------------------------------------------------------------------------------------

/** One bucket of the disk-size breakdown. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_DiskCategorySize
{
    ECkOptimizationDebugger_DiskCategory Category = ECkOptimizationDebugger_DiskCategory::Other;

    int64 TotalBytes = 0;

    // Packages, not assets: `DiskSize` is a property of a package, so counting the assets inside one would report a
    // number that does not divide into the bytes next to it.
    int32 PackageCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** What `/Game` costs on disk, bucketed by asset family.
 *
 *  `IsAvailable` is the difference between "the project has no content" and "nobody asked the asset registry" — the
 *  second is what a packaged build and a not-yet-scanned window both are, and reporting 0 bytes for either would be
 *  a lie the reader has no way to catch. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_DiskBreakdown
{
    // Non-empty buckets only, largest first with the enum value as the tie-break. An empty bucket is a row that
    // says nothing.
    TArray<FCkOptimizationDebugger_DiskCategorySize> Categories;

    int64 TotalBytes = 0;
    int32 PackageCount = 0;

    bool IsAvailable = false;

    // The asset registry was still indexing when this was taken, so the numbers are a floor rather than a total.
    bool WasStillIndexing = false;
};

// --------------------------------------------------------------------------------------------------------------------

/** One level the last scan KNEW about — which is not the same as one level it walked. An excluded level and an
 *  unloaded one are both listed, because a dashboard that silently omitted them would make a narrowed scan
 *  indistinguishable from a small project. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_LevelSummary
{
    FString LevelName;

    bool IsPersistentLevel = false;

    // The user's own per-level toggle. False means the scan skipped it entirely — gather AND checks.
    bool WasIncluded = true;

    // A streaming sub-level the world knows about but has not loaded. Nothing can be said about its contents.
    bool IsLoaded = true;

    // Zero for anything that was not walked.
    int32 ActorCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** The headline census one scan produced — the dashboard's whole data source.
 *
 *  Every number here comes out of the SAME gather the checks ran against, never a second walk of the world. Two walks
 *  can disagree about what was in the level, and a dashboard that disagreed with its own findings list is worse than
 *  a dashboard with fewer numbers on it. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ScanSummary
{
    int32 ActorCount = 0;

    // Placements, i.e. static-mesh components — how many times the level uses a mesh.
    int32 StaticMeshUsageCount = 0;

    // Distinct assets. `UniqueStaticMeshCount` and `StaticMeshUsageCount` answer different questions: one asset
    // placed a thousand times is one mesh to fix and a thousand draws to pay for.
    int32 UniqueStaticMeshCount = 0;
    int32 UniqueMaterialCount = 0;
    int32 UniqueTextureCount = 0;

    // Summed over UNIQUE meshes, not over placements: it is the content budget, not the frame cost, and counting a
    // mesh once per placement would report a number no amount of instancing could ever move.
    int64 Lod0TriangleTotal = 0;

    int32 LightCount = 0;
    int32 StaticLightCount = 0;
    int32 StationaryLightCount = 0;
    int32 MovableLightCount = 0;

    // The whole scan, before any filtering — the dashboard headline, never the page-bar count.
    FCkOptimizationDebugger_SeverityCounts FindingCounts;

    // Persistent level first, then every sub-level the world listed, included or not.
    TArray<FCkOptimizationDebugger_LevelSummary> Levels;

    FCkOptimizationDebugger_DiskBreakdown Disk;
};

// --------------------------------------------------------------------------------------------------------------------

/** How the current scan differs from the one before it. Signed, so the sign IS the direction, and `int64` throughout
 *  because a triangle total minus another triangle total does not fit an `int32` on a project that deserves this tool.
 *
 *  `HasPrevious` is load-bearing: without a previous scan every delta is zero, and a row of zeroes reads as "nothing
 *  changed" rather than "nothing to compare against". */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SummaryDelta
{
    bool HasPrevious = false;

    int64 ActorCountDelta = 0;
    int64 StaticMeshUsageCountDelta = 0;
    int64 UniqueStaticMeshCountDelta = 0;
    int64 UniqueMaterialCountDelta = 0;
    int64 UniqueTextureCountDelta = 0;
    int64 Lod0TriangleTotalDelta = 0;
    int64 LightCountDelta = 0;

    int64 CriticalCountDelta = 0;
    int64 MajorCountDelta = 0;
    int64 MinorCountDelta = 0;
    int64 FindingTotalDelta = 0;

    int64 DiskTotalBytesDelta = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** The three tables the memory analyzer keeps. Declaration order IS the order of the sub-table selector, and
 *  `Get_MemoryTotals` returns one entry per value in this order — a table added in the middle moves both. */
enum class ECkOptimizationDebugger_MemoryTable : uint8
{
    Textures,
    RenderTargets,
    StaticMeshes,
};

// --------------------------------------------------------------------------------------------------------------------

/** A sortable column of a memory table. Declaration order IS header-row order. Every column has a total sort key —
 *  see `Compare_MemoryRows` — because a header the reader clicks must produce the same list twice. */
enum class ECkOptimizationDebugger_MemoryColumn : uint8
{
    Name,
    Type,
    Dimensions,
    ResourceSize,
    GpuSize,
    Streaming,
};

// --------------------------------------------------------------------------------------------------------------------

/** Whether per-texture streaming metrics can be reported at all, and if not, why not.
 *
 *  Three states rather than a bool, because "there is no streaming manager in this session" and "there is one and
 *  streaming is switched off" are different sentences to the reader, and a table that printed zeroes for either
 *  would be claiming a measurement nobody took. */
enum class ECkOptimizationDebugger_StreamingAvailability : uint8
{
    Available,
    ManagerUnavailable,
    StreamingDisabled,
};

// --------------------------------------------------------------------------------------------------------------------

/** One resident asset the memory analyzer measured. Plain data, like every other row in this tool: the scan reads a
 *  live `UObject` once and keeps nothing but numbers and strings, so a row cannot outlive what it describes.
 *
 *  `AssetPath` is the identity a row is reused BY across refreshes — one resident object per path, which is what
 *  makes it a key at all. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_MemoryRow
{
    ECkOptimizationDebugger_MemoryTable Table = ECkOptimizationDebugger_MemoryTable::Textures;

    FString AssetPath;
    FString DisplayName;
    FString ClassName;

    // Pixel format for a texture or a render target ("PF_DXT1"); empty for a mesh, which has no single format.
    FString FormatName;

    // Textures and render targets only.
    int32 Width = 0;
    int32 Height = 0;
    int32 MipCount = 0;

    // The texture group the streamer buckets this texture into ("World", "UI"). Empty for anything that has none.
    FString LodGroupName;

    // Static meshes only.
    int32 LodCount = 0;
    int64 Lod0TriangleCount = 0;

    // `GetResourceSizeEx(EResourceSizeMode::Exclusive)` — what this object is costing right now, resident mips and
    // all, rather than what it would cost fully loaded. See the module CLAUDE.md for why Exclusive is the mode.
    int64 ResourceSizeBytes = 0;

    // The video-memory part of that total, when the engine reports one separately. Only textures do; a mesh and a
    // render target both fold their whole cost into the untagged bucket, and `HasSeparableGpuSize` is what stops
    // this column reading as "0 bytes on the GPU" for them.
    int64 GpuSizeBytes = 0;
    bool HasSeparableGpuSize = false;

    // Whether the asset is linked to the streamer at all. False is an ordinary answer — a never-stream texture is
    // not a broken one.
    bool IsStreamable = false;

    // Whether the two mip counts below mean anything. False when streaming is disabled or the manager is gone, and
    // false for an asset that is not streamable in the first place.
    bool HasStreamingMetrics = false;

    int32 ResidentMipCount = 0;
    int32 RequestedMipCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** What one memory table weighs. `SeparableGpuRowCount` is on the record so the header can say whether its GPU
 *  figure covers the whole table or only the rows that report one. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_MemoryTableTotals
{
    ECkOptimizationDebugger_MemoryTable Table = ECkOptimizationDebugger_MemoryTable::Textures;

    int32 RowCount = 0;

    int64 ResourceSizeBytes = 0;
    int64 GpuSizeBytes = 0;

    int32 SeparableGpuRowCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** The whole resident census: one entry per table in enum order, plus the grand totals the page header prints. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_MemoryTotals
{
    // Always one per `ECkOptimizationDebugger_MemoryTable` value, in declaration order, present even when empty —
    // a table that vanished from this array would make an empty table indistinguishable from a missing one.
    TArray<FCkOptimizationDebugger_MemoryTableTotals> Tables;

    int32 RowCount = 0;

    int64 ResourceSizeBytes = 0;
    int64 GpuSizeBytes = 0;

    int32 SeparableGpuRowCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** The four things the cleanup pass presents for review. Declaration order IS the order of the category sub-tabs, and
 *  `Get_CleanupTotals` returns one entry per value in this order — a category added in the middle moves both.
 *
 *  Every one of them is a REVIEW category, never an instruction. Nothing here is deleted by a scan, and the actions
 *  that can delete route through the engine's own confirmation dialog — see the module CLAUDE.md's action-safety
 *  contract. */
enum class ECkOptimizationDebugger_CleanupCategory : uint8
{
    Unreferenced,
    Duplicates,
    Redirectors,
    DirtyPackages,
};

// --------------------------------------------------------------------------------------------------------------------

/** One thing the cleanup pass found. Plain data, like every other row in this tool: no `UObject`, no package handle,
 *  nothing that can die underneath it between the scan and the reader acting on it.
 *
 *  `AssetPath` plus `Category` is the identity a row is reused BY across re-scans. The category is part of the key
 *  because one asset can legitimately appear twice — a redirector nothing references is both a redirector and an
 *  unreferenced asset, and collapsing the two would hide it from whichever tab the reader happened to open. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_CleanupRow
{
    ECkOptimizationDebugger_CleanupCategory Category = ECkOptimizationDebugger_CleanupCategory::Unreferenced;

    FString AssetPath;
    FString DisplayName;
    FString ClassName;

    // The PACKAGE's size on disk, which is the granularity `FAssetPackageData::DiskSize` exists at. Zero for a dirty
    // package that has never been written, and zero is the honest answer there rather than a guess at what it will
    // weigh once it is saved.
    int64 DiskSizeBytes = 0;

    // Why this row is here, in the words the list prints: the sibling paths of a duplicate, the destination of a
    // redirector, "never saved" for a dirty package.
    FString Detail;

    // Duplicates only: `<name>|<class>|<size>`, the whole conservative match. Empty on every other category, which is
    // what makes "is this a grouped row" answerable without consulting the category twice.
    FString DuplicateGroupKey;
};

// --------------------------------------------------------------------------------------------------------------------

/** A set of assets that share a name, a class and a disk size. **Possible** duplicates — the match is deliberately
 *  conservative and is never a claim that the bytes are identical; see the module CLAUDE.md for the rule verbatim. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_CleanupDuplicateGroup
{
    FString GroupKey;

    FString DisplayName;
    FString ClassName;

    // The size ONE member weighs — every member of a group weighs the same by construction, which is what makes the
    // group a group.
    int64 DiskSizeBytes = 0;

    TArray<FCkOptimizationDebugger_CleanupRow> Rows;
};

// --------------------------------------------------------------------------------------------------------------------

/** What one cleanup category holds. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_CleanupCategoryTotals
{
    ECkOptimizationDebugger_CleanupCategory Category = ECkOptimizationDebugger_CleanupCategory::Unreferenced;

    int32 RowCount = 0;
    int64 TotalBytes = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** The whole cleanup census: one entry per category in enum order, plus the totals the page header prints. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_CleanupTotals
{
    // Always one per `ECkOptimizationDebugger_CleanupCategory` value, in declaration order, present even when empty.
    TArray<FCkOptimizationDebugger_CleanupCategoryTotals> Categories;

    int32 RowCount = 0;

    // The UNREFERENCED bytes alone, and deliberately not the sum of every category. A duplicate's bytes are only
    // reclaimable if the reader decides one of the copies is redundant, a redirector weighs almost nothing, and a
    // dirty package is not on disk yet — adding those together would put a number on a decision nobody has made.
    int64 ReclaimableBytes = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** Slate-free state for the optimization toolkit window: which page is up, the findings the last scan produced, and
 *  what the user has narrowed them to. Every rule worth testing is a pure free function in
 *  `ck_optimization_debugger_model` below, so the specs assert behaviour without constructing a widget.
 *
 *  This model holds no `FCk_Handle`, no `UWorld` and no `UObject` — see the module CLAUDE.md's no-live-handle
 *  invariant. Everything it stores is a copy of plain data an offline scan produced. */
class CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_Model
{
public:
    CK_GENERATED_BODY(FCkOptimizationDebugger_Model);

public:
    /** Drops the findings, the scanned flag and the selection. Filter state SURVIVES — a re-scan is the same question
     *  asked again, and clearing the user's narrowing under them would be a different one. */
    auto
    Reset() -> void;

    /** Replaces the findings with the deterministic sort of what was passed in, and marks the model as scanned. */
    auto
    Set_Findings(
        TArray<FCkOptimizationDebugger_FindingRow> InFindings) -> void;

    /** What the last scan covered and when it ran. Passed in rather than read off the clock here so a spec can pin a
     *  status line without the wall clock deciding what it says. Display only: nothing derived from it is exported,
     *  sorted on, or compared. */
    auto
    Set_ScanInfo(
        TArray<FString> InScannedLevelNames,
        FDateTime InScanTime) -> void;

    /** Stores the census the scan produced and ROTATES the one it replaces into `_PreviousSummary`, which is where
     *  every delta on the dashboard comes from. The rotation lives here rather than at the call site so the two can
     *  never get out of step — a caller that overwrote the summary without rotating would silently make every delta
     *  read zero, and zero is exactly what "nothing changed" looks like. */
    auto
    Set_Summary(
        FCkOptimizationDebugger_ScanSummary InSummary) -> void;

    /** The current census against the previous one, or an unset (`HasPrevious == false`) delta when this is the
     *  first scan of the session. */
    auto
    Get_SummaryDelta() const -> FCkOptimizationDebugger_SummaryDelta;

public:
    /** The findings the current filter state admits, in the order `Set_Findings` already sorted them. Copies — call
     *  it from a rebuild, never from a paint-path attribute lambda; `Get_VisibleFindingCount` is the cheap one. */
    auto
    Get_VisibleFindings() const -> TArray<FCkOptimizationDebugger_FindingRow>;

    /** How many findings survive the filter, without materializing them. This is what a tab-bar count binds to. */
    auto
    Get_VisibleFindingCount() const -> int32;

    auto
    Get_CountsBySeverity() const -> FCkOptimizationDebugger_SeverityCounts;

    auto
    Get_VisibleCountsBySeverity() const -> FCkOptimizationDebugger_SeverityCounts;

    /** The findings the filter admits, grouped by the check that produced them. Rebuild path only — it materializes
     *  every visible finding twice over. */
    auto
    Get_VisibleFindingsGroupedByCheck() const -> TArray<FCkOptimizationDebugger_FindingGroup>;

    /** The most severe thing still on screen, or unset when the filter admits nothing. The status strip's tone. */
    auto
    TryGet_WorstVisibleSeverity() const -> TOptional<ECkOptimizationDebugger_Severity>;

public:
    auto
    Get_SeverityVisible(
        ECkOptimizationDebugger_Severity InSeverity) const -> bool;

    auto
    Set_SeverityVisible(
        ECkOptimizationDebugger_Severity InSeverity,
        bool InVisible) -> void;

    auto
    Get_CategoryVisible(
        ECkOptimizationDebugger_Category InCategory) const -> bool;

    auto
    Set_CategoryVisible(
        ECkOptimizationDebugger_Category InCategory,
        bool InVisible) -> void;

    /** Narrows the severity mask to exactly one severity — what a dashboard badge does when the reader clicks it.
     *  A solo is a jump to "show me only these", not a toggle: clicking Critical twice must not end with an empty
     *  list, which is what a plain toggle of the same bit would do. */
    auto
    Set_SeveritySolo(
        ECkOptimizationDebugger_Severity InSeverity) -> void;

public:
    /** Whether the NEXT scan will skip this level. Excluded levels still appear on the dashboard, greyed — a level
     *  that vanished from the list would make a narrowed scan look like a smaller project. */
    auto
    Get_LevelExcluded(
        FName InLevelName) const -> bool;

    auto
    Set_LevelExcluded(
        FName InLevelName,
        bool InExcluded) -> void;

    /** Replaces the whole exclusion set — how the persisted settings are loaded back in at window construction. */
    auto
    Set_ExcludedLevelNames(
        TSet<FName> InLevelNames) -> void;

public:
    /** Replaces the resident-memory census and records whether streaming metrics were reachable when it was taken.
     *  The two arrive together on purpose: a row's `HasStreamingMetrics` is only meaningful next to the reason the
     *  scan gives for the ones that have none, and storing them apart would let the table and its footnote drift. */
    auto
    Set_MemoryRows(
        TArray<FCkOptimizationDebugger_MemoryRow> InRows,
        ECkOptimizationDebugger_StreamingAvailability InStreamingAvailability) -> void;

    /** One table's rows, narrowed by the memory filter and sorted by the column the reader clicked. Copies — call it
     *  from a rebuild, never from a paint-path attribute lambda. */
    auto
    Get_SortedMemoryRows(
        ECkOptimizationDebugger_MemoryTable InTable,
        ECkOptimizationDebugger_MemoryColumn InColumn,
        bool InAscending) const -> TArray<FCkOptimizationDebugger_MemoryRow>;

    /** The WHOLE resident census, per table and in total — never the filtered view. The page header answers "what is
     *  resident", and a filter the reader typed into the search box must not make that number drop and read as a
     *  session that got lighter. The sub-table selector's own counts are the filtered ones. */
    auto
    Get_MemoryTotals() const -> FCkOptimizationDebugger_MemoryTotals;

    /** How many rows of one table survive the memory filter — what the sub-table selector and the page tab bind to. */
    auto
    Get_VisibleMemoryRowCount(
        ECkOptimizationDebugger_MemoryTable InTable) const -> int32;

public:
    /** Replaces the cleanup census. The scan time is handed IN for the same reason `Set_ScanInfo`'s is: nothing in
     *  this class reads the clock, which is what lets a spec pin a status line. */
    auto
    Set_CleanupRows(
        TArray<FCkOptimizationDebugger_CleanupRow> InRows,
        FDateTime InScanTime) -> void;

    /** One category's rows, narrowed by the cleanup filter and in the deterministic order the projection defines.
     *  Copies — call it from a rebuild, never from a paint-path attribute lambda. */
    auto
    Get_SortedCleanupRows(
        ECkOptimizationDebugger_CleanupCategory InCategory) const -> TArray<FCkOptimizationDebugger_CleanupRow>;

    /** The duplicate rows the filter admits, grouped by their conservative match key. */
    auto
    Get_CleanupDuplicateGroups() const -> TArray<FCkOptimizationDebugger_CleanupDuplicateGroup>;

    /** The WHOLE cleanup census, per category and in total — never the filtered view, for the same reason the memory
     *  totals are not filtered: the header answers "what is there", and a query the reader typed must not make it
     *  drop and read as a project that got tidier. */
    auto
    Get_CleanupTotals() const -> FCkOptimizationDebugger_CleanupTotals;

    /** How many rows of one category survive the cleanup filter — what the category sub-tabs bind to. */
    auto
    Get_VisibleCleanupRowCount(
        ECkOptimizationDebugger_CleanupCategory InCategory) const -> int32;

    /** Drops the DIRTY-PACKAGE rows and keeps the other three categories. Returns how many it dropped.
     *
     *  This is the honest half of "the cleanup census survives a PIE boundary". That claim rests on an asset nothing
     *  references being unreferenced whether or not somebody pressed Play — which is true of unreferenced assets,
     *  duplicates and redirectors, all of them registry facts. It is NOT true of dirty packages: those are live
     *  editor state, a play session saves and dirties packages, and a row that survived the boundary would name an
     *  unsaved change that may already be on disk. Keeping them while the status strip said "results cleared" was
     *  the page contradicting itself in two places at once. */
    auto
    Drop_DirtyPackageRows() -> int32;

private:
    ECkOptimizationDebugger_Page _ActivePage = ECkOptimizationDebugger_Page::Dashboard;

    // Sorted on store, never re-sorted on read — a second sort site is a second place for the ordering contract to
    // drift, and the list is what row identity is keyed against.
    TArray<FCkOptimizationDebugger_FindingRow> _Findings;

    FCkOptimizationDebugger_FilterState _Filter;

    // A scan RAN — distinct from it having found nothing, because "no findings" and "no scan yet" are different
    // things to say on the status strip.
    bool _HasScanned = false;

    // What the last scan walked, in the order it walked it (persistent level first). Display only.
    TArray<FString> _ScannedLevelNames;

    // When the last scan ran. Runtime display only — never serialized, never exported, never sorted on. The window
    // hands it in; nothing in this class reads the clock.
    FDateTime _LastScanTime;

    // The census the last scan produced, and the one before it. Both are plain copies — see the no-live-handle
    // invariant; nothing in here resolves against a world.
    FCkOptimizationDebugger_ScanSummary _Summary;
    FCkOptimizationDebugger_ScanSummary _PreviousSummary;

    // Distinct from `_HasScanned`: a scan that could not reach an editor world produces no summary at all, and a
    // summary of zeroes is not the same statement as no summary.
    bool _HasSummary = false;
    bool _HasPreviousSummary = false;

    // Levels the NEXT scan will skip. Survives `Reset` on purpose — it is the user's narrowing of the question, in
    // exactly the same sense the filter state is, and clearing it under them would silently widen the next scan.
    // Persisted per-user through `UCkOptimizationDebuggerSettings::ExcludedLevelNames`.
    TSet<FName> _ExcludedLevelNames;

    // What was resident when the memory analyzer last ran. Independent of the level scan in both directions: a level
    // scan does not refresh it and it does not touch the findings, because "what is in this level" and "what is
    // loaded right now" are different questions with different answers.
    TArray<FCkOptimizationDebugger_MemoryRow> _MemoryRows;

    // A memory scan RAN — distinct from it having found nothing, exactly as `_HasScanned` is for findings.
    bool _HasMemoryScan = false;

    // Recorded at scan time, not read live: a table has to keep saying why its streaming column is empty for as long
    // as the rows it describes are on screen, even if the manager comes back afterwards.
    ECkOptimizationDebugger_StreamingAvailability _StreamingAvailability =
        ECkOptimizationDebugger_StreamingAvailability::ManagerUnavailable;

    // The memory page's own query. Deliberately NOT `_Filter.FilterString`: the two pages are searched
    // independently, and a filter typed on the findings page silently narrowing the memory tables would be a list
    // the reader cannot explain. The MATCHING semantics are shared — see `Passes_TextFilter`.
    FString _MemoryFilterString;

    // What the last cleanup pass found. Asset-scoped, not world-scoped: an unreferenced asset is unreferenced whether
    // or not a play session is running, which is why `Reset` deliberately LEAVES these alone where it drops the
    // findings and the resident census. The dirty-package rows inside are the one live-state exception, and they are
    // refreshed on demand like everything else on that page.
    TArray<FCkOptimizationDebugger_CleanupRow> _CleanupRows;

    // A cleanup pass RAN — distinct from it having found nothing, exactly as `_HasScanned` is for findings.
    bool _HasCleanupScan = false;

    // When it ran. Display only, handed in by the window; nothing in this class reads the clock.
    FDateTime _LastCleanupScanTime;

    // The cleanup page's own query, for the same reason `_MemoryFilterString` is its own field: three pages that
    // shared one search box would each narrow when the reader typed into another.
    FString _CleanupFilterString;

public:
    CK_PROPERTY(_ActivePage);
    CK_PROPERTY_GET(_Findings);
    CK_PROPERTY(_Filter);
    CK_PROPERTY_GET(_HasScanned);
    CK_PROPERTY_GET(_ScannedLevelNames);
    CK_PROPERTY_GET(_LastScanTime);
    CK_PROPERTY_GET(_Summary);
    CK_PROPERTY_GET(_PreviousSummary);
    CK_PROPERTY_GET(_HasSummary);
    CK_PROPERTY_GET(_HasPreviousSummary);
    CK_PROPERTY_GET(_ExcludedLevelNames);
    CK_PROPERTY_GET(_MemoryRows);
    CK_PROPERTY_GET(_HasMemoryScan);
    CK_PROPERTY_GET(_StreamingAvailability);
    CK_PROPERTY(_MemoryFilterString);
    CK_PROPERTY_GET(_CleanupRows);
    CK_PROPERTY_GET(_HasCleanupScan);
    CK_PROPERTY_GET(_LastCleanupScanTime);
    CK_PROPERTY(_CleanupFilterString);
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_model
{
    constexpr auto k_PageCount         = 5;
    constexpr auto k_SeverityCount     = 3;
    constexpr auto k_CategoryCount     = 7;
    constexpr auto k_DiskCategoryCount = 8;
    constexpr auto k_MemoryTableCount  = 3;
    constexpr auto k_MemoryColumnCount = 6;
    constexpr auto k_CleanupCategoryCount = 4;

    // All bits set == no filtering. Kept next to the counts so the two can never disagree.
    constexpr auto k_AllSeverityMask = static_cast<uint8>(0x07);
    constexpr auto k_AllCategoryMask = static_cast<uint8>(0x7F);

    // ----------------------------------------------------------------------------------------------------------------

    /** Every page, in the enum's own order — the page bar and the body switcher both walk this. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllPages() -> TArray<ECkOptimizationDebugger_Page>;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllSeverities() -> TArray<ECkOptimizationDebugger_Severity>;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllCategories() -> TArray<ECkOptimizationDebugger_Category>;

    /** The page-bar tab id. Stable strings, not enum casts — a saved layout or a copied log line naming a page must
     *  keep meaning something if the enum is ever re-ordered. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_PageId(
        ECkOptimizationDebugger_Page InPage) -> FName;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_PageLabel(
        ECkOptimizationDebugger_Page InPage) -> FString;

    /** Which switcher slot the page occupies — the enum value, which is why slots must be added in enum order. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_PageIndex(
        ECkOptimizationDebugger_Page InPage) -> int32;

    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_PageFromId(
        FName InPageId) -> TOptional<ECkOptimizationDebugger_Page>;

    // ----------------------------------------------------------------------------------------------------------------

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_SeverityBit(
        ECkOptimizationDebugger_Severity InSeverity) -> uint8;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CategoryBit(
        ECkOptimizationDebugger_Category InCategory) -> uint8;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_SeverityLabel(
        ECkOptimizationDebugger_Severity InSeverity) -> FString;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CategoryLabel(
        ECkOptimizationDebugger_Category InCategory) -> FString;

    /** Critical → Err, Major → Warn, Minor → Info. Nothing here is ever Ok-toned: a finding is by definition
     *  something the reader may want to act on, and painting one green says the opposite. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_SeverityTone(
        ECkOptimizationDebugger_Severity InSeverity) -> ECk_Tone;

    // ----------------------------------------------------------------------------------------------------------------

    /** CheckId + '|' + target path — the identity a row is reused by across re-scans. ProjectSettings targets carry
     *  no object path, so their section name stands in; an empty section still yields a distinct key per check. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_StableKey(
        FName InCheckId,
        const FCkOptimizationDebugger_Target& InTarget) -> FString;

    /** Everything the Filter / Highlight boxes match a finding against, joined into one haystack. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_FindingSearchText(
        const FCkOptimizationDebugger_FindingRow& InFinding) -> FString;

    /** Case-insensitive substring match. An empty filter passes everything — that is what "no filter" means. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Passes_TextFilter(
        const FString& InSearchText,
        const FString& InFilter) -> bool;

    /** Does this finding survive the whole filter state: the text filter across title / target name / check id, the
     *  severity toggles and the category toggles. The Highlight query is deliberately NOT consulted — highlight
     *  dims, it never hides. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Matches_Filter(
        const FCkOptimizationDebugger_FindingRow& InFinding,
        const FCkOptimizationDebugger_FilterState& InFilter) -> bool;

    /** The OTHER half of the dual-search contract: whether the Highlight query names this finding. A finding that
     *  fails it is still listed — it is drawn muted. An empty highlight matches everything, which is why a window
     *  with no highlight typed draws nothing dimmed. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Matches_Highlight(
        const FCkOptimizationDebugger_FindingRow& InFinding,
        const FCkOptimizationDebugger_FilterState& InFilter) -> bool;

    // ----------------------------------------------------------------------------------------------------------------

    /** Severity first (most severe first), then category, then title, then the stable key. The final tie-break is
     *  what makes this total: `TArray::Sort` is unstable, so two findings equal on every visible key would otherwise
     *  swap places between scans and the list would appear to churn on its own. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_SortedFindings(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings) -> TArray<FCkOptimizationDebugger_FindingRow>;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CountsBySeverity(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings) -> FCkOptimizationDebugger_SeverityCounts;

    /** Groups findings by the check that produced them, keeping the input's order. Because the input is already
     *  severity-sorted, groups come out worst-first without a second sort — and a second sort here would be a second
     *  place for the ordering contract to drift. Findings inside a group keep their relative input order too. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_FindingsGroupedByCheck(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings) -> TArray<FCkOptimizationDebugger_FindingGroup>;

    /** The most severe finding in the list, or unset when the list is empty. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_WorstSeverity(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings) -> TOptional<ECkOptimizationDebugger_Severity>;

    // ----------------------------------------------------------------------------------------------------------------
    // DASHBOARD PROJECTIONS
    // ----------------------------------------------------------------------------------------------------------------

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllDiskCategories() -> TArray<ECkOptimizationDebugger_DiskCategory>;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_DiskCategoryLabel(
        ECkOptimizationDebugger_DiskCategory InCategory) -> FString;

    /** **The exclusion seam.** Whether a scan should walk a level at all, given the user's exclusion set. Both the
     *  gather and the checks are downstream of this one answer — an excluded level is skipped whole, never gathered
     *  and then filtered out of the findings, because a half-excluded level would still pay for its own walk and
     *  could still contribute to a per-level count. */
    CKOPTIMIZATIONDEBUGGER_API auto
    ShouldScanLevel(
        FName InLevelName,
        const TSet<FName>& InExcludedLevelNames) -> bool;

    /** Per-stat signed change, current minus previous. Pure so the whole delta contract is testable without a scan. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_SummaryDelta(
        const FCkOptimizationDebugger_ScanSummary& InPrevious,
        const FCkOptimizationDebugger_ScanSummary& InCurrent) -> FCkOptimizationDebugger_SummaryDelta;

    /** "1.4 GB", "812 KB", "0 B". Binary units, one fractional digit above kilobytes — a disk figure the reader
     *  compares against Explorer, which counts the same way. Negative input keeps its sign rather than clamping,
     *  so a byte DELTA can be printed through the same function. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Format_ByteSize(
        int64 InBytes) -> FString;

    /** "1.2M", "12.3K", "947". Decimal, because a triangle count is not a memory size and rounding it to 1,048,576
     *  would be answering a different question. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Format_AbbreviatedCount(
        int64 InValue) -> FString;

    /** "+12", "-3", or an em dash for no change — the delta caption under a stat tile. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Format_Delta(
        int64 InDelta) -> FString;

    /** How a delta should be PAINTED. Only a stat with a direction gets a colour: findings going down is an
     *  improvement, findings going up is a regression, and an actor count going either way is neither — painting a
     *  bigger level red would be this tool asserting an opinion it has no basis for. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_DeltaTone(
        int64 InDelta,
        bool InFewerIsBetter) -> ECk_Tone;

    /** A bucket's share of the whole, clamped to 0..1. A non-positive total yields 0 — a meter of an empty project
     *  is empty, not full. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_DiskCategoryFraction(
        int64 InCategoryBytes,
        int64 InTotalBytes) -> float;

    // ----------------------------------------------------------------------------------------------------------------
    // MEMORY ANALYZER PROJECTIONS
    // ----------------------------------------------------------------------------------------------------------------

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllMemoryTables() -> TArray<ECkOptimizationDebugger_MemoryTable>;

    /** Stable id strings, not enum casts — the sub-table selector is an id-keyed tab bar, and an id that moved when
     *  the enum was re-ordered would silently select a different table. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MemoryTableId(
        ECkOptimizationDebugger_MemoryTable InTable) -> FName;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MemoryTableLabel(
        ECkOptimizationDebugger_MemoryTable InTable) -> FString;

    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_MemoryTableFromId(
        FName InTableId) -> TOptional<ECkOptimizationDebugger_MemoryTable>;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllMemoryColumns() -> TArray<ECkOptimizationDebugger_MemoryColumn>;

    /** The `SHeaderRow` column id. Same reasoning as the table ids: Slate hands a column NAME back through the sort
     *  callback, so the mapping has to survive an enum edit. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MemoryColumnId(
        ECkOptimizationDebugger_MemoryColumn InColumn) -> FName;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MemoryColumnLabel(
        ECkOptimizationDebugger_MemoryColumn InColumn) -> FString;

    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_MemoryColumnFromId(
        FName InColumnId) -> TOptional<ECkOptimizationDebugger_MemoryColumn>;

    // ----------------------------------------------------------------------------------------------------------------

    /** What the Type column prints: the pixel format for anything that has one, the class name otherwise. A texture's
     *  class is the same for every row of its table and tells the reader nothing; its format is the whole question. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MemoryTypeText(
        const FCkOptimizationDebugger_MemoryRow& InRow) -> FString;

    /** "2048 × 2048 · 12 mips · World" for a texture, "4 LODs · 12.3K tris" for a mesh. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MemoryDimensionText(
        const FCkOptimizationDebugger_MemoryRow& InRow) -> FString;

    /** What the Dimensions column SORTS on — pixel count for a texture, LOD0 triangles for a mesh. Sorting the
     *  printed string would order "1024" after "2048×2048" and before "512", which is alphabetical order pretending
     *  to be a size. Each table is homogeneous, so one key per row kind is total within the table it appears in. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MemoryDimensionSortKey(
        const FCkOptimizationDebugger_MemoryRow& InRow) -> int64;

    /** "8 / 12 mips", "not streamable", or an em dash when nothing could be measured. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MemoryStreamingText(
        const FCkOptimizationDebugger_MemoryRow& InRow) -> FString;

    /** Rows with no metrics sort BELOW every measured row (-1) rather than mixing in among the zero-resident ones —
     *  "nothing was measured" is not "nothing is resident". */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MemoryStreamingSortKey(
        const FCkOptimizationDebugger_MemoryRow& InRow) -> int32;

    /** Whether rows in this table report a video-memory figure SEPARATELY from their total.
     *
     *  A property of the TYPE, never of the number that came back: `UTexture2D::GetResourceSizeEx` routes its mip
     *  bytes through `AddDedicatedVideoMemoryBytes`, while a render target and a static mesh both fold their whole
     *  cost into the untagged bucket. Deriving this from `GpuSizeBytes > 0` would make a fully streamed-out texture
     *  — zero resident mips, therefore zero dedicated bytes — claim its type reports no such figure, and print the
     *  em dash that means "this could not be measured" over a number that was measured and is genuinely zero. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Has_SeparableGpuSize(
        ECkOptimizationDebugger_MemoryTable InTable) -> bool;

    // ----------------------------------------------------------------------------------------------------------------

    /** Path, name and class joined into one haystack — the same shape `Build_FindingSearchText` has, over the fields
     *  a memory row actually carries. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_MemorySearchText(
        const FCkOptimizationDebugger_MemoryRow& InRow) -> FString;

    /** The memory page's filter predicate. Shares `Passes_TextFilter`'s semantics — case-insensitive substring, an
     *  empty query passes everything — over the memory haystack. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Matches_MemoryFilter(
        const FCkOptimizationDebugger_MemoryRow& InRow,
        const FString& InFilter) -> bool;

    /** Orders two rows by one column alone: -1, 0 or +1, ASCENDING. Split out of the sort so the direction and the
     *  tie-break are applied in exactly one place. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Compare_MemoryRows(
        const FCkOptimizationDebugger_MemoryRow& InLhs,
        const FCkOptimizationDebugger_MemoryRow& InRhs,
        ECkOptimizationDebugger_MemoryColumn InColumn) -> int32;

    /** One table's rows, filtered and sorted. Total by construction: the asset path breaks every tie, and it does so
     *  ASCENDING in both directions — the header toggle reverses the COLUMN, never the tie-break, so two rows equal
     *  on the sorted column keep their relative order whichever way the arrow points. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_SortedMemoryRows(
        const TArray<FCkOptimizationDebugger_MemoryRow>& InRows,
        ECkOptimizationDebugger_MemoryTable InTable,
        ECkOptimizationDebugger_MemoryColumn InColumn,
        bool InAscending,
        const FString& InFilter) -> TArray<FCkOptimizationDebugger_MemoryRow>;

    /** Per-table and grand totals over the rows as given — no filtering, because the page header is the census and
     *  not the view. Always yields one table entry per enum value, in declaration order. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MemoryTotals(
        const TArray<FCkOptimizationDebugger_MemoryRow>& InRows) -> FCkOptimizationDebugger_MemoryTotals;

    /** The one-line note the memory page prints above its table when streaming metrics are missing, and an empty
     *  string when they are not — an unconditional footnote is a footnote nobody reads. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_StreamingAvailabilityNote(
        ECkOptimizationDebugger_StreamingAvailability InAvailability) -> FString;

    // ----------------------------------------------------------------------------------------------------------------
    // PROJECT CLEANUP PROJECTIONS
    // ----------------------------------------------------------------------------------------------------------------

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllCleanupCategories() -> TArray<ECkOptimizationDebugger_CleanupCategory>;

    /** Stable id strings, not enum casts — the category selector is an id-keyed tab bar, exactly like the memory
     *  sub-table one, and an id that moved when the enum was re-ordered would silently select a different category. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CleanupCategoryId(
        ECkOptimizationDebugger_CleanupCategory InCategory) -> FName;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CleanupCategoryLabel(
        ECkOptimizationDebugger_CleanupCategory InCategory) -> FString;

    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_CleanupCategoryFromId(
        FName InCategoryId) -> TOptional<ECkOptimizationDebugger_CleanupCategory>;

    /** The one line the page prints under a category's heading, saying what the category IS and — for duplicates —
     *  what it is not. Kept here rather than in the window so the spec that asserts the conservative wording is
     *  asserting the string the reader sees. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CleanupCategoryHint(
        ECkOptimizationDebugger_CleanupCategory InCategory) -> FString;

    /** The conservative duplicate key: `<name>|<class>|<disk size>`, and nothing else. Content is never hashed and
     *  never compared — two assets that match here are **possible** duplicates the reader is asked to look at, not
     *  assets this tool claims are byte-identical. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_DuplicateGroupKey(
        const FString& InDisplayName,
        const FString& InClassName,
        int64 InDiskSizeBytes) -> FString;

    /** Name, path, class and detail joined into one haystack — the same shape the finding and memory haystacks have,
     *  over the fields a cleanup row carries. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_CleanupSearchText(
        const FCkOptimizationDebugger_CleanupRow& InRow) -> FString;

    CKOPTIMIZATIONDEBUGGER_API auto
    Matches_CleanupFilter(
        const FCkOptimizationDebugger_CleanupRow& InRow,
        const FString& InFilter) -> bool;

    /** One category's rows, filtered and in a total order: biggest first, with the asset path breaking every tie.
     *  Total by construction for the same reason every other list in this tool is — `TArray::Sort` is unstable, and
     *  a project full of same-sized rows would otherwise churn between two identical scans. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_SortedCleanupRows(
        const TArray<FCkOptimizationDebugger_CleanupRow>& InRows,
        ECkOptimizationDebugger_CleanupCategory InCategory,
        const FString& InFilter) -> TArray<FCkOptimizationDebugger_CleanupRow>;

    /** The duplicate rows the filter admits, grouped by `DuplicateGroupKey`.
     *
     *  Groups come out heaviest first — a group's weight is its member count times one member's size, i.e. what the
     *  reader could reclaim by keeping one copy — with the group key as the tie-break, and members sorted by path.
     *  A group with fewer than two members is dropped: one asset is not a duplicate of anything. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CleanupDuplicateGroups(
        const TArray<FCkOptimizationDebugger_CleanupRow>& InRows,
        const FString& InFilter) -> TArray<FCkOptimizationDebugger_CleanupDuplicateGroup>;

    /** Per-category and grand totals over the rows as given — no filtering, because the page header is the census and
     *  not the view. Always yields one category entry per enum value, in declaration order. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CleanupTotals(
        const TArray<FCkOptimizationDebugger_CleanupRow>& InRows) -> FCkOptimizationDebugger_CleanupTotals;
}

// --------------------------------------------------------------------------------------------------------------------
