#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CkInput/CkInputButtonMap_Fragment_Data.h"
#include "CkInput/CkInputLayer_Fragment_Data.h"
#include "CkInput/CkInputSource_Fragment_Data.h"

#include "CkIntent/CkIntentGrammar_Data.h"
#include "CkIntent/CkIntentMatcher_Fragment_Data.h"
#include "CkIntent/CkIntentSampler_Fragment_Data.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Snapshot rows for the CK Intent Debugger.
//
// Every field here is a value the sampler, the matcher or the router already computed and STORED. Nothing in this
// module re-derives an octant, a SOCD reading, a match or a deferral verdict — a divergence between what these rows
// say and what the game did is exactly the signal the debugger exists to show, and a debugger that recomputed would
// share the blind spot instead of exposing it.
//
// FCk_Handle members die with the PIE registry. Every owner of a snapshot clears them on session invalidation.
// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_IntentRow
{
    FName Name = NAME_None;
    FGameplayTag Tag;

    ECk_Intent_Phase Phase = ECk_Intent_Phase::Idle;

    // The frame the CURRENT phase was stamped on, whichever phase that is — a completion frame, a failure frame,
    // or the press frame a wait opened at. This is the span start every phase bar on the timeline is drawn from.
    int32 PhaseFrame = INDEX_NONE;

    int32 CompletionFrame = INDEX_NONE;

    bool IsClaimed = false;
    FString ClaimedBy;

    int32 Priority = 0;
    int32 WindowFrames = 0;
    int32 HoldFrames = 0;
    ECk_Intent_Lenience Lenience = ECk_Intent_Lenience::Strict;

    FString Notation;
    FString TerminalLabel;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_EpisodeRow
{
    FString ButtonLabel;
    int32 PressFrame = INDEX_NONE;
    int32 ChordWindowFrames = 0;
    int32 HoldSiblingFrames = 0;
    bool ChordCauseArmed = false;
    bool HoldCauseArmed = false;
    TArray<FName> CandidateNames;

    auto Get_CauseLabel() const -> FString;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_HoldRow
{
    FString ButtonLabel;
    int32 HeldFrames = 0;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_ResolutionRow
{
    FString TerminalLabel;
    ECk_Input_ButtonTier Tier = ECk_Input_ButtonTier::Mapped;
    FKey ResolvedKey;
    TArray<FName> IntentNames;

    // Zero on both means this terminal is acted on the frame it is pressed — the overwhelmingly common case, and
    // the property the whole bake exists to protect.
    int32 HoldSiblingFrames = 0;
    int32 ChordMemberFrames = 0;

    auto Get_IsDeferred() const -> bool { return HoldSiblingFrames > 0 || ChordMemberFrames > 0; }
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_CaptureRow
{
    ECk_InputLayer_CaptureMatch MatchMode = ECk_InputLayer_CaptureMatch::Key;
    FKey Key;
    ECk_InputLayer_CaptureBehavior Behavior = ECk_InputLayer_CaptureBehavior::Consume;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_LayerRow
{
    FCk_Handle_InputLayer Layer;
    FCk_Handle_IntentMatcher Matcher;

    int32 Priority = 0;
    bool IsGlobalActionLayer = false;
    FString DebugName;

    TArray<FCkIntentDebugger_CaptureRow> Captures;

    bool HasMatcher = false;
    bool HasActiveSet = false;
    int32 ActiveIntentCount = 0;
    int32 ChordWindowFrames = 0;
    ECk_InputLayer_CaptureBehavior MatcherCaptureBehavior = ECk_InputLayer_CaptureBehavior::Consume;
    int32 LatchDecayFrames = 0;
    TArray<FKey> RegisteredCaptureKeys;

    TArray<FCkIntentDebugger_IntentRow> Intents;
    TArray<FCkIntentDebugger_EpisodeRow> Episodes;
    TArray<FCkIntentDebugger_HoldRow> Holds;
    TArray<FCkIntentDebugger_ResolutionRow> Resolutions;
    TArray<FCk_Intent_ScanDiagnostic> ScanDiagnostics;

    auto Get_StackLabel() const -> FString;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_ButtonState
{
    FString Label;
    ECk_Input_ButtonTier Tier = ECk_Input_ButtonTier::Mapped;
    FKey Key;
    bool WentDown = false;
    bool WentUp = false;
};

// --------------------------------------------------------------------------------------------------------------------
// One record row, flattened for display. The octant and both cleaned axes are carried verbatim from the row — they
// are derived ONCE, inside the sampler, precisely so two readers of the same row cannot disagree.
// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_FrameRow
{
    int32 FrameIndex = INDEX_NONE;

    float AxisX = 0.0f;
    float AxisY = 0.0f;

    ECk_Intent_Octant Octant = ECk_Intent_Octant::Neutral;
    ECk_Intent_CleanedAxis CleanedHorizontal = ECk_Intent_CleanedAxis::Neutral;
    ECk_Intent_CleanedAxis CleanedVertical = ECk_Intent_CleanedAxis::Neutral;

    TArray<FCkIntentDebugger_ButtonState> Held;

    int32 NumRoutedEvents = 0;
    int32 NumConsumed = 0;
    int32 NumPassedThrough = 0;
    int32 NumDropped = 0;

    // Which layers ended a walk on this row. Keyed by the layer's priority, which is unique per source.
    TArray<int32> ConsumingLayerPriorities;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_SourceSnapshot
{
    FCk_Handle_InputSource Source;
    FCk_Handle_IntentSampler Sampler;

    int32 LocalPlayerIndex = INDEX_NONE;
    FString Label;

    bool HasSampler = false;
    bool HasButtonMap = false;
    int32 FrameCount = 0;
    int32 RingCapacity = 0;

    // The declared band below which a row reads Neutral regardless of angle. Drawn on the rosette so a reader can
    // see the stick sitting inside it.
    float OctantNeutralRadius = 0.0f;
    float OctantHysteresisMarginDegrees = 0.0f;

    // Oldest first, so the timeline axis and the key/state scrubber walk it in the direction a reader expects.
    TArray<FCkIntentDebugger_FrameRow> Frames;

    TArray<FCkIntentDebugger_LayerRow> Layers;

    // Every key the button map mints — the set the record carries rows for, which is the set the device
    // visualizer can render with exact history. (Unminted keys are witnessed by the ViewModel's ungated
    // edge-capture tick, not through this snapshot — an edge sampled at refresh cadence is an edge missed.)
    TArray<FKey> MintedKeys;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_Snapshot
{
    bool HasWorld = false;
    bool ScanDiagnosticsEnabled = false;

    TArray<FCkIntentDebugger_SourceSnapshot> Sources;

    auto TryGet_Source(int32 InIndex) const -> const FCkIntentDebugger_SourceSnapshot*
    {
        return Sources.IsValidIndex(InIndex) ? &Sources[InIndex] : nullptr;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// One phase transition the debugger WITNESSED, newest last. The matcher keeps no phase history — it keeps one
// current phase and the frame it was stamped on — so a scrubbable timeline can only be assembled from what the
// debugger read while it was open. Rows before the window opened are absent by construction, not lost.
// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_PhaseEvent
{
    FName IntentName = NAME_None;
    ECk_Intent_Phase Phase = ECk_Intent_Phase::Idle;

    // The frame the runtime stamped the phase on. Never a wall-clock time — the record is indexed in frames and a
    // second clock would disagree with it on the first hitch.
    int32 StartFrame = INDEX_NONE;

    // INDEX_NONE while the span is still the intent's current phase; a reader draws those to the latest frame.
    int32 EndFrame = INDEX_NONE;

    int32 LayerPriority = 0;

    auto Get_IsOpen() const -> bool { return EndFrame == INDEX_NONE; }
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::intent_debugger
{
    CKINTENTDEBUGGER_API auto Get_Label(const FCk_Input_ButtonId& InButton) -> FString;
    CKINTENTDEBUGGER_API auto Get_Label(ECk_Intent_Octant InOctant) -> FString;
    CKINTENTDEBUGGER_API auto Get_Label(ECk_Intent_CleanedAxis InAxis) -> FString;
    CKINTENTDEBUGGER_API auto Get_Label(ECk_Intent_Phase InPhase) -> FString;
    CKINTENTDEBUGGER_API auto Get_Label(ECk_Intent_ScanStepOutcome InOutcome) -> FString;
    CKINTENTDEBUGGER_API auto Get_Label(ECk_InputLayer_CaptureBehavior InBehavior) -> FString;

    CKINTENTDEBUGGER_API auto Get_PhaseColor(ECk_Intent_Phase InPhase) -> FLinearColor;
    CKINTENTDEBUGGER_API auto Get_StepOutcomeColor(ECk_Intent_ScanStepOutcome InOutcome) -> FLinearColor;

    // The unit-circle centre of an octant, in the axis pair's own space (+X east, +Y north). Read straight off the
    // enum's own angle ordering — it is arithmetic on the stored value, not a re-derivation of the direction.
    CKINTENTDEBUGGER_API auto Get_OctantUnitVector(ECk_Intent_Octant InOctant) -> FVector2D;
}

// --------------------------------------------------------------------------------------------------------------------
