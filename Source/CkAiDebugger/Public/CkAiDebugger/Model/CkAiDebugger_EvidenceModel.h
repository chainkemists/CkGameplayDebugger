#pragma once

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"

#include "CoreMinimal.h"

/** A value-only identity for one overlay fact, including repeated same-field rows. */
struct FCkAiDebugger_EvidenceKey
{
    FString ProviderTag;
    uint32 SourceEntityId = 0;
    FString FieldTag;
    int32 Occurrence = 0;

    auto ToString() const -> FString;
    bool operator==(const FCkAiDebugger_EvidenceKey& InOther) const;
};

/** Presentation-ready current fact. It deliberately retains no ECS handle or UObject reference. */
struct FCkAiDebugger_EvidenceFact
{
    FCkAiDebugger_EvidenceKey Key;
    FString StableKey;
    ECk_Tone Tone = ECk_Tone::Neutral;
    FString Category;
    FString SourceLabel;
    FString Headline;
    FString DisplayValue;
    FString Detail;
    FString CopyText;

    /** Presentation-only deterministic order inherited from the overlay source section. */
    int32 SourceOrder = 0;

    /** Complete value used only by the value-only delta tracker. */
    FString ValueState;
};

/** One concise cross-system change emitted after the initial snapshot is seeded. */
struct FCkAiDebugger_EvidenceEvent
{
    FString StableKey;
    ECk_Tone Tone = ECk_Tone::Neutral;
    FString Category;
    FString Message;
    double TimeSeconds = 0.0;
};

/**
 * Value-only runtime-tree node.
 *
 * A GOAP section yields one node. A State Machine section yields one node PER NESTED LEVEL of the
 * chain the provider emits, so a sub-state-machine inside a state is its own row rather than a
 * fragment of the parent's detail string.
 *
 * The fields are split by how the row renders them: Name/Headline/Status are the scannable line,
 * Chain is the only thing allowed into the muted detail line, and Detail keeps the full-fidelity
 * row dump for copy text and for callers that want everything.
 */
struct FCkAiDebugger_TopologyNode
{
    FString StableKey;
    uint32 SourceEntityId = 0;
    uint32 ParentSourceEntityId = 0;

    /** Indent level: the source's subtree depth plus its nesting level inside that source. */
    int32 Depth = 0;
    int32 SourceOrder = 0;

    /** Nesting level inside this source's own chain. 0 for every GOAP node. */
    int32 ChainLevel = 0;

    /** Raw, un-shortened instance name. Presentation applies the window's name depth. */
    FString Name;

    /** The one live fact: the GOAP action being executed, or the current state at this level. */
    FString Headline;

    /** Right-hand label: GOAP plan status (with cost), or state-machine run status. */
    FString Status;

    /** Full ordered plan (GOAP) or transition trail (State Machine). Never truncated here. */
    TArray<FString> Chain;

    /** Names the chain for the reader: "Plan" or "Trail". Empty when Chain is empty. */
    FString ChainLabel;

    /** Full-fidelity row dump, retained for copy text and the empty-section sentinel. */
    FString Detail;

    ECk_Tone Tone = ECk_Tone::Neutral;
};

/** Complete per-kind tree payload. A source section is never collapsed out of either array. */
struct FCkAiDebugger_Topology
{
    TArray<FCkAiDebugger_TopologyNode> Goaps;
    TArray<FCkAiDebugger_TopologyNode> StateMachines;
};

/** Pure overlay-model normalization; feature fragments are intentionally not visible here. */
namespace ck::ai_debugger::evidence
{
    CKAIDEBUGGER_API auto Normalize(const FCk_DebugOverlay_EntityModel& InModel) -> TArray<FCkAiDebugger_EvidenceFact>;
    CKAIDEBUGGER_API auto NormalizeTopology(const FCk_DebugOverlay_EntityModel& InModel) -> FCkAiDebugger_Topology;
}

/** Tracks normalized snapshots and emits add/change/resolve events without retaining runtime objects. */
class CKAIDEBUGGER_API FCkAiDebugger_EvidenceDeltaTracker
{
public:
    auto Observe(const TArray<FCkAiDebugger_EvidenceFact>& InCurrent, double InTimeSeconds) -> TArray<FCkAiDebugger_EvidenceEvent>;
    auto Reset() -> void;

private:
    TMap<FString, FCkAiDebugger_EvidenceFact> _Previous;
    bool _HasSeeded = false;
};
