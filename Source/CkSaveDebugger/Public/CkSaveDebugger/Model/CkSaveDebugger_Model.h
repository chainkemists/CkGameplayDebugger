#pragma once

#include "CkEditorTools/Style/CkIcons_Generated.h"
#include "CkEditorTools/Style/CkIconStyle.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkSnapshot/Inspection/CkSnapshot_Inspection_Data.h"
#include "CkSnapshot/Inspection/CkSnapshot_Inspection_Diff.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Math/Color.h"
#include "Math/Transform.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

// --------------------------------------------------------------------------------------------------------------------

/** A row in the ownership tree. Only Entity rows correspond to a saved row; the two synthetic kinds exist so rows
 *  whose owner is not itself a row still render as a tree instead of vanishing. NonPersistedOwnerRoot groups the
 *  rows owned by one non-persisted entity (usually the saving world's transient root — a NORMAL top-level shape,
 *  mirrored by the OwnerNotPersisted diagnostic whose severity is per-provenance); CycleRoot collects the members
 *  of an owner cycle (always a defect, mirrored by OwnerCycle). */
enum class ECkSaveDebugger_NodeKind : uint8
{
    Entity,
    NonPersistedOwnerRoot,
    CycleRoot,
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_save_debugger_model
{
    // One bit per ECk_Snapshot_V3_Provenance value; all four set == no provenance filtering.
    constexpr auto k_AllProvenanceMask = static_cast<uint8>(0x0F);
}

// --------------------------------------------------------------------------------------------------------------------

/** Identity a tree row is reused BY across refreshes — SListView tracks selection by pointer identity, so the model
 *  keeps one TSharedPtr alive per key and updates its fields in place. */
struct CKSAVEDEBUGGER_API FCkSaveDebugger_NodeKey
{
    ECkSaveDebugger_NodeKind Kind = ECkSaveDebugger_NodeKind::Entity;
    uint32 SavedId = ck::snapshot::k_NoSavedEntity;

    auto operator==(const FCkSaveDebugger_NodeKey& InOther) const -> bool
    { return Kind == InOther.Kind && SavedId == InOther.SavedId; }
};

CKSAVEDEBUGGER_API auto GetTypeHash(const FCkSaveDebugger_NodeKey& InKey) -> uint32;

// --------------------------------------------------------------------------------------------------------------------

/** Plain aggregate on purpose: the widget mutates IsVisible / IsSearchMatch in place every filter pass, and the
 *  scheduler-tree precedent this row model copies does the same. */
struct CKSAVEDEBUGGER_API FCkSaveDebugger_TreeNode
{
    ECkSaveDebugger_NodeKind Kind = ECkSaveDebugger_NodeKind::Entity;
    uint32  SavedId = ck::snapshot::k_NoSavedEntity;
    int32   EntityIndex = INDEX_NONE;

    FString DisplayText;
    FString ProvenanceText;
    FString SearchText;

    // The registered suite icon id this row renders (see Get_ProvenanceIcon / Get_NodeKindIcon). Kept on the row
    // so the glyph survives row reuse without the widget having to re-derive it from the document.
    ECk_Icon Icon;

    bool HasProblems = false;
    bool IsVisible = true;
    bool IsSearchMatch = true;

    TArray<TSharedPtr<FCkSaveDebugger_TreeNode>> Children;
};

// --------------------------------------------------------------------------------------------------------------------

/** Pure projection of the ownership graph — no Slate, no allocation of row widgets. The model materializes stable
 *  TSharedPtr rows from it; the specs assert the shape directly. */
struct CKSAVEDEBUGGER_API FCkSaveDebugger_LayoutNode
{
    ECkSaveDebugger_NodeKind Kind = ECkSaveDebugger_NodeKind::Entity;
    uint32 SavedId = ck::snapshot::k_NoSavedEntity;
    int32  EntityIndex = INDEX_NONE;
    TArray<int32> ChildLayoutIndices;
};

struct CKSAVEDEBUGGER_API FCkSaveDebugger_TreeLayout
{
    TArray<FCkSaveDebugger_LayoutNode> Nodes;
    TArray<int32> RootLayoutIndices;
};

// --------------------------------------------------------------------------------------------------------------------

/** What selecting a diagnostic should select. Either field may be unset. */
struct CKSAVEDEBUGGER_API FCkSaveDebugger_RowTarget
{
    uint32 EntitySavedId = ck::snapshot::k_NoSavedEntity;
    int32  PayloadIndex = INDEX_NONE;

    auto Get_HasEntity() const -> bool { return EntitySavedId != ck::snapshot::k_NoSavedEntity; }
    auto Get_HasPayload() const -> bool { return PayloadIndex != INDEX_NONE; }
};

// --------------------------------------------------------------------------------------------------------------------

struct CKSAVEDEBUGGER_API FCkSaveDebugger_PayloadRow
{
    int32   PayloadIndex = INDEX_NONE;
    uint32  OwnerSavedId = ck::snapshot::k_NoSavedEntity;
    FString TypePath;
    FString ShortTypeName;
    int32   ByteCount = 0;
    bool    TypeAvailable = false;
    bool    HasProblems = false;
};

// --------------------------------------------------------------------------------------------------------------------

struct CKSAVEDEBUGGER_API FCkSaveDebugger_DiagnosticRow
{
    int32 DiagnosticIndex = INDEX_NONE;
    ECk_SnapshotInspection_Severity Severity = ECk_SnapshotInspection_Severity::Info;
    FString CodeText;
    FString Message;
    FCkSaveDebugger_RowTarget Target;
};

// --------------------------------------------------------------------------------------------------------------------

/** One entity-identity group of a save comparison, flattened for a list row. Rows are reused BY IdentityPath — saved
 *  ids are not stable across captures, so the identity path is the only key a group survives a re-diff under. */
struct CKSAVEDEBUGGER_API FCkSaveDebugger_DiffGroupRow
{
    // Index into the diff's own EntityGroups array — the address the group detail panel reads its payload rows from.
    int32   GroupIndex = INDEX_NONE;
    FString IdentityPath;
    FString DisplayName;
    FString KindText;

    ECk_SnapshotInspection_DiffGroupKind Kind = ECk_SnapshotInspection_DiffGroupKind::Unchanged;
    ECk_Tone Tone = ECk_Tone::Neutral;

    int32 CountBaseline = 0;
    int32 CountCurrent = 0;
    int64 PayloadBytesBaseline = 0;
    int64 PayloadBytesCurrent = 0;

    // The row this group selects in the ownership tree, or k_NoSavedEntity when the group has no current-side member
    // (a Removed group names nothing the open save contains).
    uint32 FirstCurrentSavedId = ck::snapshot::k_NoSavedEntity;

    auto Get_HasCurrentMember() const -> bool { return FirstCurrentSavedId != ck::snapshot::k_NoSavedEntity; }
};

// --------------------------------------------------------------------------------------------------------------------

/** How the visualizer represents a placed row beyond its diamond. MeshGhost: a bridged actor row — its Construct
 *  ensures without an owning actor and editor worlds cannot supply one, so its visual is read passively from the
 *  actor class's component templates. ConstructionPreview: a plain script row — the editor ECS world runs its real
 *  Construct (the spawner-preview mechanism), so the visual is whatever construction composes. DiamondOnly:
 *  everything else (EngineOwned rows already exist in the level; DefinitionBuilt recipe replay is not built yet). */
enum class ECkSaveDebugger_VisualKind : uint8
{
    DiamondOnly,
    MeshGhost,
    ConstructionPreview,
};

// --------------------------------------------------------------------------------------------------------------------

/** One placeable save row for the editor-viewport visualizer: where the loader would put this entity, plus the
 *  facts a diamond and its hover label need. Plain data — no Slate, no world, no handle — so the projection is
 *  spec-testable and the drawing side can hold an immutable snapshot of it. */
struct CKSAVEDEBUGGER_API FCkSaveDebugger_VisualizationRow
{
    uint32 SavedId = ck::snapshot::k_NoSavedEntity;
    int32  EntityIndex = INDEX_NONE;

    FTransform WorldTransform = FTransform::Identity;
    FString DisplayText;
    ECk_Snapshot_V3_Provenance Provenance = ECk_Snapshot_V3_Provenance::EngineOwned;
    bool HasProblems = false;

    // The bridged actor class of a RuntimeSpawned actor row, empty otherwise — what the mesh-ghost renderer
    // resolves component templates from. Carried on the row so the visual layer never reads the document.
    FString ActorClassPath;
    FString ScriptClassPath;
    ECkSaveDebugger_VisualKind VisualKind = ECkSaveDebugger_VisualKind::DiamondOnly;

    // Index of this row's lifetime owner within the SAME array — the other endpoint of the owner-chain line.
    // INDEX_NONE when the owner is absent from the table or itself unplaced.
    int32 OwnerRowIndex = INDEX_NONE;
};

// --------------------------------------------------------------------------------------------------------------------

/** Verdict of comparing the save's captured world against the currently open editor level — what the Visualize
 *  command branches its load-the-level prompt on. */
enum class ECkSaveDebugger_LevelMatch : uint8
{
    Match,
    Mismatch,
    // The header carries no world path (recovered/legacy header) — there is nothing to compare, so there is
    // nothing to prompt about either.
    SaveHasNoWorld,
    NoCurrentWorld,
};

// --------------------------------------------------------------------------------------------------------------------

/** Everything the CK Save Debugger window shows, with zero Slate in it: an inspection document, the decode results
 *  the user has asked for so far, the filter/selection state, and the ownership tree built from them.
 *
 *  The window is a pure consumer — every predicate, every projection and both exports are testable without a
 *  widget. The model NEVER holds a world, registry, subsystem or FCk_Handle: a save is inspected offline and every
 *  entity reference in here is a raw saved id. */
class CKSAVEDEBUGGER_API FCkSaveDebugger_Model
{
public:
    CK_GENERATED_BODY(FCkSaveDebugger_Model);

public:
    /** Replaces the document. Decode results, selection and the row set all reset — they belong to the old file. */
    auto
    Set_Document(
        const FCk_SnapshotInspection_Document& InDocument) -> void;

    auto
    Reset() -> void;

public:
    /** Rebuilds the ownership rows from the document, reusing every row TSharedPtr whose key survived.
     *  Returns true when the row SET changed — the only condition under which the tree may be told to refresh. */
    auto
    Rebuild_Tree() -> bool;

    /** Recomputes IsVisible / IsSearchMatch across every row from the current filter, highlight, provenance mask
     *  and ProblemsOnly state. A parent stays visible when any descendant is. */
    auto
    Apply_Filters() -> void;

public:
    auto
    Get_PayloadRows_ForEntity(
        uint32 InEntitySavedId) const -> TArray<FCkSaveDebugger_PayloadRow>;

    auto
    Build_DiagnosticRows() const -> TArray<FCkSaveDebugger_DiagnosticRow>;

public:
    /** Stores a comparison of the open document (CURRENT) against a picked file (BASELINE). Only the baseline's
     *  source DESCRIPTION is kept: a diff is self-contained, so a second full document on the model would be dead
     *  weight nothing projects from. */
    auto
    Set_Diff(
        const FCk_SnapshotInspection_Diff& InDiff,
        const FString& InBaselineSourceDescription) -> void;

    auto
    Clear_Diff() -> void;

    /** The diff's entity groups in the order the analysis already sorted them (biggest population change first),
     *  minus Unchanged groups unless ShowUnchanged is set. Empty on an invalid diff — there is nothing to list. */
    auto
    Build_DiffGroupRows() const -> TArray<FCkSaveDebugger_DiffGroupRow>;

public:
    auto
    Store_PayloadDecode(
        int32 InPayloadIndex,
        const FCk_SnapshotInspection_DecodeResult& InResult) -> void;

    auto
    TryGet_PayloadDecode(
        int32 InPayloadIndex) const -> const FCk_SnapshotInspection_DecodeResult*;

    auto
    Store_SpawnParamsDecode(
        int32 InEntityIndex,
        const FCk_SnapshotInspection_DecodeResult& InResult) -> void;

    auto
    TryGet_SpawnParamsDecode(
        int32 InEntityIndex) const -> const FCk_SnapshotInspection_DecodeResult*;

public:
    auto
    TryGet_SelectedEntity() const -> const FCk_SnapshotInspection_EntitySummary*;

    auto
    TryGet_SelectedPayload() const -> const FCk_SnapshotInspection_PayloadSummary*;

private:
    FCk_SnapshotInspection_Document _Document;

    // Keyed by TABLE index in both cases — the stable address of a row inside its document.
    TMap<int32, FCk_SnapshotInspection_DecodeResult> _PayloadDecodes;
    TMap<int32, FCk_SnapshotInspection_DecodeResult> _SpawnParamsDecodes;

    FString _FilterString;
    FString _HighlightString;
    uint8   _ProvenanceMask = ck_save_debugger_model::k_AllProvenanceMask;
    bool    _ProblemsOnly = false;

    // SaveKey -> "ActorName (Class)", derived by the WINDOW from the open editor level (never by the model — it
    // holds no world). Applied to row display/search text on Rebuild_Tree, so the setter's caller re-runs it.
    TMap<FGuid, FString> _SaveKeyAnnotations;

    uint32 _SelectedEntitySavedId = ck::snapshot::k_NoSavedEntity;
    int32  _SelectedPayloadIndex = INDEX_NONE;

    // A comparison against another file, dropped whenever the document is replaced: a diff whose CURRENT side was
    // swapped out underneath it describes nothing.
    FCk_SnapshotInspection_Diff _Diff;
    FString _DiffBaselineSourceDescription;

    // A compare was RUN — distinct from the diff being VALID, because an invalid diff still has a reason to show.
    bool _HasDiff = false;
    bool _DiffShowUnchanged = false;

    TArray<TSharedPtr<FCkSaveDebugger_TreeNode>> _TreeRoots;
    TMap<FCkSaveDebugger_NodeKey, TSharedPtr<FCkSaveDebugger_TreeNode>> _NodesByKey;

    // Hash of the whole ownership shape. Key churn alone cannot see a pure reparent, and the tree may only be told
    // to refresh when the shape actually moved.
    uint32 _LayoutSignature = 0;

public:
    CK_PROPERTY_GET(_Document);
    CK_PROPERTY_GET(_PayloadDecodes);
    CK_PROPERTY_GET(_SpawnParamsDecodes);
    CK_PROPERTY(_FilterString);
    CK_PROPERTY(_HighlightString);
    CK_PROPERTY(_ProvenanceMask);
    CK_PROPERTY(_ProblemsOnly);
    CK_PROPERTY(_SaveKeyAnnotations);
    CK_PROPERTY(_SelectedEntitySavedId);
    CK_PROPERTY(_SelectedPayloadIndex);
    CK_PROPERTY_GET(_TreeRoots);
    CK_PROPERTY_GET(_Diff);
    CK_PROPERTY_GET(_DiffBaselineSourceDescription);
    CK_PROPERTY_GET(_HasDiff);
    CK_PROPERTY(_DiffShowUnchanged);
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_save_debugger_model
{
    // A blob is untrusted input and a preview is a glance, not a dump — the raw bytes have their own export action.
    constexpr auto k_MaxHexPreviewBytes = 256;
    constexpr auto k_HexBytesPerLine    = 16;

    // ----------------------------------------------------------------------------------------------------------------

    CKSAVEDEBUGGER_API auto
    Get_ProvenanceBit(
        ECk_Snapshot_V3_Provenance InProvenance) -> uint8;

    CKSAVEDEBUGGER_API auto
    Get_AllProvenanceMask() -> uint8;

    CKSAVEDEBUGGER_API auto
    Passes_ProvenanceMask(
        ECk_Snapshot_V3_Provenance InProvenance,
        uint8 InMask) -> bool;

    // ----------------------------------------------------------------------------------------------------------------

    /** Case-insensitive substring match. An empty filter passes everything — that is what "no filter" means. */
    CKSAVEDEBUGGER_API auto
    Passes_TextFilter(
        const FString& InSearchText,
        const FString& InFilter) -> bool;

    CKSAVEDEBUGGER_API auto
    Build_SavedIdText(
        uint32 InSavedId) -> FString;

    CKSAVEDEBUGGER_API auto
    Build_EntityDisplayText(
        const FCk_SnapshotInspection_EntitySummary& InSummary) -> FString;

    /** As above, appending the resolved editor-actor name when the row's SaveKey appears in the annotation map.
     *  A save records nothing but the key for an EngineOwned row (the loader only rendezvouses, never re-creates),
     *  so the name is WORLD data the window derives from the open level — the model just merges strings, which is
     *  what keeps this projection pure and spec-testable. */
    CKSAVEDEBUGGER_API auto
    Build_EntityDisplayText(
        const FCk_SnapshotInspection_EntitySummary& InSummary,
        const TMap<FGuid, FString>& InSaveKeyAnnotations) -> FString;

    /** Everything the Filter / Highlight boxes match a row against, joined into one haystack. */
    CKSAVEDEBUGGER_API auto
    Build_EntitySearchText(
        const FCk_SnapshotInspection_EntitySummary& InSummary) -> FString;

    /** As above; an annotated row is also findable by its editor-actor name. */
    CKSAVEDEBUGGER_API auto
    Build_EntitySearchText(
        const FCk_SnapshotInspection_EntitySummary& InSummary,
        const TMap<FGuid, FString>& InSaveKeyAnnotations) -> FString;

    /** The saved id of the row carrying this SaveKey, or k_NoSavedEntity. Editor-selection sync resolves a clicked
     *  level actor to its row through this (the key is FGuid::NewDeterministicGuid of the actor's level-placed
     *  identity — the same one derivation every stamping site uses). */
    CKSAVEDEBUGGER_API auto
    TryGet_SavedIdForSaveKey(
        const FCk_SnapshotInspection_Document& InDocument,
        const FGuid& InSaveKey) -> uint32;

    CKSAVEDEBUGGER_API auto
    Build_ShortTypeName(
        const FString& InTypePath) -> FString;

    // ----------------------------------------------------------------------------------------------------------------

    /** Children hang under their LifetimeOwner; ownerless rows are roots. Rows naming an owner the entity table
     *  does not contain group under one synthetic Non-Persisted Owner root per absent owner id (the group node's
     *  SavedId IS that owner id), and every member of an owner cycle lands under a synthetic Cycle root. The walk
     *  is coloured, never recursive on the owner chain — a cyclic save must not be able to hang or overflow the
     *  inspector. */
    CKSAVEDEBUGGER_API auto
    Build_OwnershipLayout(
        const FCk_SnapshotInspection_Document& InDocument) -> FCkSaveDebugger_TreeLayout;

    /** Which rows a diagnostic points at. A payload-scoped diagnostic also resolves its owning entity so selecting
     *  it lands the user on both panels at once. */
    CKSAVEDEBUGGER_API auto
    Get_DiagnosticTarget(
        const FCk_SnapshotInspection_Document& InDocument,
        const FCk_SnapshotInspection_Diagnostic& InDiagnostic) -> FCkSaveDebugger_RowTarget;

    // ----------------------------------------------------------------------------------------------------------------

    /** Fixed-width hex dump of at most InMaxBytes bytes, k_HexBytesPerLine per line, with an explicit trailing line
     *  stating how many bytes were cut. Pure formatting — no allocation policy, no state. */
    CKSAVEDEBUGGER_API auto
    Format_HexPreview(
        const TArray<uint8>& InBytes,
        int32 InMaxBytes) -> FString;

    // ----------------------------------------------------------------------------------------------------------------

    CKSAVEDEBUGGER_API auto
    Get_CompatibilityTone(
        const FCk_SnapshotInspection_Document& InDocument) -> ECk_Tone;

    /** TypeUnavailable and UnsupportedBlob are NEUTRAL: this editor not having a type is an observation about the
     *  environment, never a defect in the file, and colouring it red teaches the reader the wrong thing. */
    CKSAVEDEBUGGER_API auto
    Get_DecodeStatusTone(
        ECk_SnapshotInspection_DecodeStatus InStatus) -> ECk_Tone;

    CKSAVEDEBUGGER_API auto
    Get_SeverityTone(
        ECk_SnapshotInspection_Severity InSeverity) -> ECk_Tone;

    /** The typed icon (`FCkIconStyle::Get_Brush`) that stands for a capture provenance. The four
     *  provenances must map to four DISTINCT glyphs — a shared glyph turns the tree's provenance column into
     *  decoration. Slate-free on purpose: the id is a name, resolving it to a brush is the window's job. */
    CKSAVEDEBUGGER_API auto
    Get_ProvenanceIcon(
        ECk_Snapshot_V3_Provenance InProvenance) -> ECk_Icon;

    /** The icon id a synthetic ownership root renders. Entity rows carry a provenance glyph instead. */
    CKSAVEDEBUGGER_API auto
    Get_NodeKindIcon(
        ECkSaveDebugger_NodeKind InKind) -> ECk_Icon;

    /** Tone of an ownership row's own kind. A Non-Persisted Owner group is the loader-sanctioned encoding of "owned
     *  by the transient/world root" and renders NEUTRAL; only an owner cycle is a defect in the file. */
    CKSAVEDEBUGGER_API auto
    Get_NodeKindTone(
        ECkSaveDebugger_NodeKind InKind) -> ECk_Tone;

    /** Payload rows whose declared type this editor cannot see. */
    CKSAVEDEBUGGER_API auto
    Get_OpaquePayloadCount(
        const FCk_SnapshotInspection_Document& InDocument) -> int32;

    // ----------------------------------------------------------------------------------------------------------------

    /** The world transform the loader would restore for this row, or unset when the save carries none. Mirrors the
     *  loader's own placement rules: identity IS the format's "no Transform fragment" sentinel
     *  (CkSnapshot_Header.h, DoApply_SavedTransforms), and a bridged actor's spawn seed stands in when the
     *  general column is empty — a row this returns unset for is one the loader would not place either. */
    CKSAVEDEBUGGER_API auto
    TryGet_VisualizationTransform(
        const FCk_Snapshot_V3_EntityEntry& InEntry) -> TOptional<FTransform>;

    /** Every placeable row of the document, in table order, with owner links resolved to indices into the returned
     *  array. Rows without placement are simply absent — the visualizer draws a spatial SLICE of the save, and the
     *  window states the placed/total split rather than pretending coverage. Row display text carries the
     *  editor-actor annotation when one resolves. */
    CKSAVEDEBUGGER_API auto
    Build_VisualizationRows(
        const FCk_SnapshotInspection_Document& InDocument,
        const TMap<FGuid, FString>& InSaveKeyAnnotations = {}) -> TArray<FCkSaveDebugger_VisualizationRow>;

    /** The viewport tint that stands for a capture provenance — four DISTINCT colors (CkStyle tokens), same
     *  reasoning as Get_ProvenanceIcon: a shared tint turns the census-at-a-glance into decoration. Problem rows
     *  override to CkStyle::Err() at the draw site, exactly like tree rows do. */
    CKSAVEDEBUGGER_API auto
    Get_ProvenanceVisualizationColor(
        ECk_Snapshot_V3_Provenance InProvenance) -> FLinearColor;

    /** Which in-world representation a row earns beyond its diamond — see ECkSaveDebugger_VisualKind. */
    CKSAVEDEBUGGER_API auto
    Get_VisualKind(
        const FCk_Snapshot_V3_EntityEntry& InEntry) -> ECkSaveDebugger_VisualKind;

    /** Compares the header's captured world package against the current editor level's package name. Both sides are
     *  PIE-prefix-stripped: a save captured during PIE names the UEDPIE_N_ package while the editor level does not,
     *  and that difference is not a mismatch. */
    CKSAVEDEBUGGER_API auto
    Get_LevelMatch(
        const FCk_SnapshotInspection_Document& InDocument,
        const FString& InCurrentWorldPackageName) -> ECkSaveDebugger_LevelMatch;

    // ----------------------------------------------------------------------------------------------------------------

    /** Tone of a diff group. GROWTH is the suspect direction — a leak reads as a population that only ever went up —
     *  so Added and a grown CountChanged are Warn while Removed and a shrunk CountChanged are merely Info. A
     *  PayloadsChanged group moved no population at all and gets the Accent tone instead of a severity. */
    CKSAVEDEBUGGER_API auto
    Get_DiffGroupTone(
        ECk_SnapshotInspection_DiffGroupKind InKind,
        int32 InCountDelta) -> ECk_Tone;

    /** Same growth-is-suspect reading, applied to a bare census delta. */
    CKSAVEDEBUGGER_API auto
    Get_DiffDeltaTone(
        int64 InDelta) -> ECk_Tone;

    /** "+12" / "-3", and EMPTY for zero — a row that did not move says so by carrying no delta at all. */
    CKSAVEDEBUGGER_API auto
    Build_DiffDeltaText(
        int64 InDelta) -> FString;

    CKSAVEDEBUGGER_API auto
    Build_DiffCountText(
        int32 InBaseline,
        int32 InCurrent) -> FString;

    CKSAVEDEBUGGER_API auto
    Build_DiffBytesText(
        int64 InBaseline,
        int64 InCurrent) -> FString;

    /** One per-type line of a group's payload table: counts, bytes, and the hash-based content verdict — a value
     *  change that keeps the byte count identical is only visible through that flag. */
    CKSAVEDEBUGGER_API auto
    Build_DiffPayloadTypeText(
        const FCk_SnapshotInspection_DiffPayloadTypeRow& InRow) -> FString;

    /** The clipboard diff report. Deterministic by the same rules as the JSON export: every array is walked in the
     *  order the diff already sorted it, and nothing time-, pointer- or environment-derived enters it. */
    CKSAVEDEBUGGER_API auto
    Build_DiffReportText(
        const FCkSaveDebugger_Model& InModel) -> FString;

    // ----------------------------------------------------------------------------------------------------------------

    /** Deterministic pretty JSON: every array is emitted in an explicitly sorted order, every hash is present, and
     *  no raw blob bytes are ever included (a bounded preview only — raw export is its own per-blob action).
     *  Two calls against the same model produce byte-identical output. */
    CKSAVEDEBUGGER_API auto
    Build_JsonExport(
        const FCkSaveDebugger_Model& InModel) -> FString;

    /** The clipboard census — the same facts as the JSON, in the shape a bug report wants. */
    CKSAVEDEBUGGER_API auto
    Build_TextReport(
        const FCkSaveDebugger_Model& InModel) -> FString;
}

// --------------------------------------------------------------------------------------------------------------------
