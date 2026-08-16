#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkAudioDebugger/Data/CkAudioDebugger_DataCollector.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkDebug_EventLog;
class SCkDebug_MeterBar;
class SHorizontalBox;
class STextBlock;
class SVerticalBox;
class SWidgetSwitcher;
class UWorld;

// --------------------------------------------------------------------------------------------------------------------

enum class ECkAudioDebugger_Page : uint8
{
    Directors,
    Tracks,
    Crossfade,
    Spatial,
    Events,
    Overlay
};

// --------------------------------------------------------------------------------------------------------------------

/** What the Events page derives from consecutive snapshots.
 *
 *  Kept as its own slim record rather than storing whole `FCkAudioDebugger_TrackInfo`s: the diff only needs the
 *  fields whose CHANGE is an event, and holding the full struct would keep a stale `FCk_Handle` alive across a PIE
 *  boundary for no reason. */
struct FCkAudioDebugger_TrackWatch
{
    ECk_AudioTrack_State State = ECk_AudioTrack_State::Stopped;

    bool IsVirtualized = false;
    bool WasFading = false;

    FString DirectorName;
};

// --------------------------------------------------------------------------------------------------------------------

/** One track's retained row cells.
 *
 *  Everything that changes per refresh is a shared cell the bound lambdas read, so a value pass writes cells and
 *  never touches the widget tree. Volume moves every frame of a crossfade; rebuilding the subtree at that rate tears
 *  down live widgets mid-layout, which reads on screen as violent flicker and text drawn over itself.
 *
 *  Shared cells rather than indices into the slot array, because the array reallocates while rows are added. */
struct FCkAudioDebugger_TrackSlot
{
    TSharedPtr<float>            VolumeFraction;
    TSharedPtr<TOptional<float>> TargetFraction;
    TSharedPtr<FLinearColor>     VolumeColor;
    TSharedPtr<ECk_Tone>         StateTone;
    TSharedPtr<FText>            StateLabel;

    /** Its own cell rather than inferred from the tone. Virtualization drives the tone today, and reading the flag
     *  back OUT of the colour would make the badge a restatement of a rendering decision instead of of the state. */
    TSharedPtr<bool> IsVirtualized;

    TSharedPtr<STextBlock> VolumeText;
    TSharedPtr<STextBlock> FadeText;
    TSharedPtr<STextBlock> AlertText;
};

// --------------------------------------------------------------------------------------------------------------------

/** One director's retained header cells. */
struct FCkAudioDebugger_DirectorSlot
{
    TSharedPtr<STextBlock> ActiveText;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Read-only inspector for CkAudio: every AudioDirector and the tracks it owns, rendered as live volume meters so a
 * crossfade reads as two bars crossing rather than as two numbers the reader has to difference in their head.
 *
 * The question this page exists to answer is the one the ECS state alone does not: **why is that track at that
 * volume** — steady, fading toward a target, or playing at a healthy volume and inaudible because it is virtualized.
 *
 * It is a LIVE-HANDLE debugger, unlike CkOptimizationDebugger: it holds handles into a running world, so it inherits
 * the session-lifecycle teardown the base provides and must never be given that tool's no-live-handle invariant.
 */
class SCkAudioDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkAudioDebuggerWindow) {}
    SLATE_END_ARGS()

    auto
    Construct(
        const FArguments& InArgs) -> void;

    auto
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("Audio")); }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    auto
    DoCreate_Tabs() -> TSharedRef<SWidget>;

    auto
    DoCreate_StatCards() -> TSharedRef<SWidget>;

    auto
    DoCreate_FilterRow() -> TSharedRef<SWidget>;

    auto
    DoCreate_CrossfadeLane(
        bool InIsDedicatedPage) -> TSharedRef<SWidget>;

    auto
    DoCreate_SpatialPage() -> TSharedRef<SWidget>;

    auto
    DoCreate_EventsPage() -> TSharedRef<SWidget>;

    auto
    DoCreate_OverlayPage() -> TSharedRef<SWidget>;

    /** Recompute the radar/curve view from the current snapshot and the current selection. */
    auto
    DoUpdate_SpatialView() -> void;

    /** Fold this refresh's track states against the previous one and append whatever changed to the log. */
    auto
    DoRecord_Events() -> void;

    auto
    DoRebuild_SpatialSelector() -> void;

    auto
    DoRebuild_OverlayList() -> void;

    auto
    DoSet_DebugDrawOnAll(
        bool InEnabled) -> void;

    /** The track the Spatial page is inspecting. Falls back to the most diagnostic one — virtualized, then
     *  out-of-range, then loudest — because a page that opened on an arbitrary track would bury the reason the reader
     *  opened it. */
    auto
    TryGet_SelectedSpatialTrack() const -> const FCkAudioDebugger_TrackInfo*;

    /** Rebuilt ONLY when the director/track SET changes — see `FCkAudioDebugger_TrackSlot` for why. */
    auto
    DoRebuild_Structure() -> void;

    auto
    DoUpdate_LiveValues() -> void;

    /** Push this refresh's volumes into the per-track rings the crossfade lane draws. Keyed by track name and pruned
     *  against the live set, so a track that stops does not leave its curve frozen on the lane forever. */
    auto
    DoRecord_VolumeHistory() -> void;

    /** Identity of the current director/track set. Deliberately excludes volume and playback percent: those change
     *  every frame and would defeat the gate entirely, rebuilding the tree on every tick of every fade. */
    auto
    DoBuild_Signature() const -> FString;

    /** The UNFILTERED track set. The Spatial selector and the Overlay list show every track regardless of the mixer's
     *  filters, so they must not rebuild off a signature the filters can empty. */
    auto
    DoBuild_AllTracksSignature() const -> FString;

    auto
    DoMake_DirectorHeader(
        const FCkAudioDebugger_DirectorInfo& InDirector,
        FCkAudioDebugger_DirectorSlot& OutSlot) const -> TSharedRef<SWidget>;

    auto
    DoMake_TrackRow(
        const FCkAudioDebugger_TrackInfo& InTrack,
        FCkAudioDebugger_TrackSlot& OutSlot) const -> TSharedRef<SWidget>;

    /** The visible tracks of one director, in render order — the single walk the structure pass, the value pass and
     *  the signature all consult, so the three can never disagree about which rows exist. */
    auto
    DoGet_VisibleTracks(
        const FCkAudioDebugger_DirectorInfo& InDirector) const -> TArray<const FCkAudioDebugger_TrackInfo*>;

    auto
    DoPassesFilter(
        const FString& InText) const -> bool;

    auto
    DoPassesStateFilter(
        const FCkAudioDebugger_TrackInfo& InTrack) const -> bool;

    auto
    DoGet_PieWorld() const -> UWorld*;

    static auto
    DoGet_StateTone(
        const FCkAudioDebugger_TrackInfo& InTrack) -> ECk_Tone;

    static auto
    DoBuild_StateText(
        const FCkAudioDebugger_TrackInfo& InTrack) -> FText;

    static auto
    DoBuild_FadeText(
        const FCkAudioDebugger_TrackInfo& InTrack) -> FString;

    static auto
    DoBuild_AlertText(
        const FCkAudioDebugger_TrackInfo& InTrack) -> FString;

    FCkAudioDebugger_DataCollector _Collector;

    TSharedPtr<SWidgetSwitcher> _PageSwitcher;
    TSharedPtr<SVerticalBox>    _DirectorBox;
    TSharedPtr<SVerticalBox>    _DirectorPageBox;
    TSharedPtr<STextBlock>      _StatusText;
    TSharedPtr<STextBlock>      _CrossfadeLegendText;

    TSharedPtr<SHorizontalBox>  _SpatialSelectorBox;
    TSharedPtr<SVerticalBox>    _OverlayListBox;
    TSharedPtr<SCkDebug_EventLog> _EventLog;

    /** Written in place on the refresh gate; the radar and the curve paint straight from it. */
    TSharedPtr<FCkAudioDebugger_SpatialView> _SpatialView;

    TSharedPtr<FText> _StatAudible;
    TSharedPtr<FText> _StatFading;
    TSharedPtr<FText> _StatVirtualized;
    TSharedPtr<FText> _StatConcurrency;

    // Flat and parallel to the rows the structure pass emitted, in the same walk order. The signature guarantees
    // that correspondence: any change to the set rebuilds before the value pass runs.
    TArray<FCkAudioDebugger_TrackSlot>    _TrackSlots;
    TArray<FCkAudioDebugger_DirectorSlot> _DirectorSlots;

    /** Ring per track name, oldest first. Shared so the sparkline reads the same array the refresh mutates in place —
     *  the widget is volatile and follows without invalidation plumbing. */
    TMap<FString, TSharedPtr<TArray<float>>> _VolumeHistory;

    /** The two series the lane actually draws, allocated ONCE and copied into. `SCkDebug_Sparkline` binds its samples
     *  at construction, so handing it a ring straight out of the map would freeze the lane on whichever two tracks
     *  happened to exist when it was built. */
    TSharedPtr<TArray<float>> _CrossfadeSeriesA;
    TSharedPtr<TArray<float>> _CrossfadeSeriesB;

    TArray<FString> _CrossfadeSeriesNames;

    /** Previous refresh's per-track state, keyed by track name — the Events page's entire memory. */
    TMap<FString, FCkAudioDebugger_TrackWatch> _TrackWatch;

    /** Guards the very first diff. Without it every track present at open would be reported as having just started,
     *  which would put a fabricated burst of events at the top of the log. */
    bool _HasWatchBaseline = false;

    FString _LastSignature;
    FString _SpatialSignature;
    FString _OverlaySignature;
    FString _FilterString;
    FString _HighlightString;

    /** Empty means "follow the most diagnostic track"; set by the Spatial page's selector. */
    FString _SelectedSpatialTrack;

    ECkAudioDebugger_Page _ActivePage = ECkAudioDebugger_Page::Tracks;

    bool _EventsShowStateChanges = true;
    bool _EventsShowFades = true;
    bool _EventsShowVirtualization = true;
    bool _EventsShowLifecycle = true;

    /** Hide tracks nothing is playing. A director legitimately holds a dozen configured-but-stopped tracks, and a
     *  mixer that listed them all buries the two that are actually making noise. */
    bool _ShowActiveOnly = false;

    bool _ShowPlaying = true;
    bool _ShowFading  = true;
    bool _ShowStopped = true;

    bool _GroupByDirector = true;
};

// --------------------------------------------------------------------------------------------------------------------
