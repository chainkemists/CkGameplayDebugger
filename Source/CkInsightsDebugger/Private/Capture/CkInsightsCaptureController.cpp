#include "CkInsightsDebugger/Capture/CkInsightsCaptureController.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include <Containers/Ticker.h>
#include <CoreGlobals.h>
#include <Engine/Engine.h>
#include <Engine/GameViewportClient.h>
#include <HAL/IConsoleManager.h>
#include <Misc/CoreDelegates.h>
#include <Misc/ScopeLock.h>
#include <ProfilingDebugging/MiscTrace.h>
#include <ProfilingDebugging/TraceScreenshot.h>
#include <Stats/StatsSystemTypes.h>
#include <Trace/Trace.h>
#include <UnrealClient.h>

#if WITH_EDITOR
    #include <Editor.h>
    #include <EditorViewportClient.h>
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_insights_capture
{
    auto Get_StatViewportClient() -> FCommonViewportClient*
    {
        if (NOT ck::IsValid(GEngine))
        { return nullptr; }

        if (GEngine->GameViewport && NOT GEngine->GameViewport->IsSimulateInEditorViewport())
        { return GEngine->GameViewport; }

#if WITH_EDITOR
        return GLastKeyLevelEditingViewportClient
            ? GLastKeyLevelEditingViewportClient
            : GCurrentLevelEditingViewportClient;
#else
        return nullptr;
#endif
    }
}

// --------------------------------------------------------------------------------------------------------------------

FCkInsightsCaptureController::FCkInsightsCaptureController()
    : FCkInsightsCaptureController(FTraceAuxiliary::EConnectionType::File, FString{})
{}

FCkInsightsCaptureController::FCkInsightsCaptureController(
        FTraceAuxiliary::EConnectionType InConnectionType,
        FString InConnectionDestination)
    : _ConnectionType(InConnectionType)
    , _ConnectionDestination(MoveTemp(InConnectionDestination))
{
    _TraceStartedHandle = FTraceAuxiliary::OnTraceStarted.AddRaw(
        this,
        &FCkInsightsCaptureController::DoOnTraceStarted);
    _TraceConnectionHandle = FTraceAuxiliary::OnConnection.AddRaw(
        this,
        &FCkInsightsCaptureController::DoOnTraceConnection);
    _TraceStoppedHandle = FTraceAuxiliary::OnTraceStopped.AddRaw(
        this,
        &FCkInsightsCaptureController::DoOnTraceStopped);
    _TimedCaptureTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FCkInsightsCaptureController::DoTick_TimedCapture));
}

FCkInsightsCaptureController::~FCkInsightsCaptureController()
{
    auto Error = FString{};
    if (NOT TryStop_TimedCapture(Error))
    {
        auto CanStopOwnedTimedTrace = false;
        {
            auto Lock = FScopeLock{&_OwnershipMutex};
            CanStopOwnedTimedTrace = _TimedCaptureRequested && _OwnsActiveTrace;
        }
        if (CanStopOwnedTimedTrace)
        { FTraceAuxiliary::Stop(); }
    }
    FTSTicker::GetCoreTicker().RemoveTicker(_TimedCaptureTickerHandle);
    FTraceAuxiliary::OnTraceStarted.Remove(_TraceStartedHandle);
    FTraceAuxiliary::OnConnection.Remove(_TraceConnectionHandle);
    FTraceAuxiliary::OnTraceStopped.Remove(_TraceStoppedHandle);
}

auto FCkInsightsCaptureController::Get_Snapshot() const -> FCkInsightsCaptureSnapshot
{
    auto Snapshot = FCkInsightsCaptureSnapshot{};
    Snapshot.bIsTracing = UE::Trace::IsTracing();
    Snapshot.Destination = FTraceAuxiliary::GetTraceDestinationString();

    auto Lock = FScopeLock{&_OwnershipMutex};
    Snapshot.bIsOwnedByTool = Snapshot.bIsTracing && _OwnsActiveTrace;
    Snapshot.TargetDurationSeconds = _TimedCaptureDurationSeconds;
    if (NOT _TimedCaptureRequested)
    { return Snapshot; }

    if (_TimedCaptureStartedSeconds <= 0.0)
    {
        Snapshot.State = _TimedCaptureStopping
            ? ECkInsightsCaptureState::Stopping
            : ECkInsightsCaptureState::Starting;
        return Snapshot;
    }

    Snapshot.ElapsedSeconds = FMath::Max(0.0, FPlatformTime::Seconds() - _TimedCaptureStartedSeconds);
    Snapshot.Progress = DoGet_TimedCaptureProgress(Snapshot.ElapsedSeconds, Snapshot.TargetDurationSeconds);
    Snapshot.RemainingSeconds = FMath::Max(0.0, Snapshot.TargetDurationSeconds - Snapshot.ElapsedSeconds);
    Snapshot.State = _TimedCaptureStopping
        ? ECkInsightsCaptureState::Stopping
        : ECkInsightsCaptureState::Recording;
    return Snapshot;
}

auto FCkInsightsCaptureController::Can_ToggleTracing() const -> bool
{
#if UE_TRACE_ENABLED
    const auto Snapshot = Get_Snapshot();
    if (NOT Snapshot.bIsTracing)
    { return true; }

    if (Snapshot.State == ECkInsightsCaptureState::Stopping)
    { return false; }

    auto Lock = FScopeLock{&_OwnershipMutex};
    return Snapshot.bIsOwnedByTool && _CanStopOwnedTrace;
#else
    return false;
#endif
}

auto
    FCkInsightsCaptureController::
    TrySet_Tracing(
        bool InShouldTrace,
        FString& OutError,
        FString* OutStoppedTracePath,
        FGuid* OutStoppedTraceGuid)
    -> bool
{
    OutError.Reset();
    if (OutStoppedTracePath)
    { OutStoppedTracePath->Reset(); }
    if (OutStoppedTraceGuid)
    { OutStoppedTraceGuid->Invalidate(); }
    const auto Snapshot = Get_Snapshot();

    if (InShouldTrace == Snapshot.bIsTracing)
    { return true; }

    if (InShouldTrace)
    {
#if UE_TRACE_ENABLED
        {
            auto Lock = FScopeLock{&_OwnershipMutex};
            _OwnsActiveTrace = false;
            _CanStopOwnedTrace = false;
            _StartInProgress = true;
            _ConnectionReadyDuringStart = false;
            _TraceStartedDuringStart = false;
            _TraceStoppedDuringStart = false;
            _OwnedTraceType = FTraceAuxiliary::EConnectionType::None;
            _OwnedTraceDestination.Reset();
            _OwnedTraceGuid.Invalidate();
        }

        const auto Channels = Get_TraceChannels();
        const auto Started = FTraceAuxiliary::Start(
            _ConnectionType,
            *_ConnectionDestination,
            *Channels);

        auto OwnsStartedTrace = false;
        {
            auto Lock = FScopeLock{&_OwnershipMutex};
            _StartInProgress = false;
            OwnsStartedTrace = Started
                && _TraceStartedDuringStart
                && NOT _TraceStoppedDuringStart
                && UE::Trace::IsTracing();
            _OwnsActiveTrace = OwnsStartedTrace;
            _CanStopOwnedTrace = OwnsStartedTrace && _ConnectionReadyDuringStart;
            if (_TimedCaptureRequested
                && _OwnsActiveTrace
                && _CanStopOwnedTrace
                && _TimedCaptureStartedSeconds <= 0.0)
            { _TimedCaptureStartedSeconds = FPlatformTime::Seconds(); }
        }

        if (NOT Started)
        {
            OutError = ck::Format_UE(
                TEXT("Unreal Trace could not start the requested capture at [{}]."),
                _ConnectionDestination);
            return false;
        }

        if (NOT OwnsStartedTrace)
        {
            OutError = TEXT("The trace ended before the Insights Analyzer could establish ownership.");
            return false;
        }

        return true;
#else
        OutError = TEXT("Tracing is not enabled in this build.");
        return false;
#endif
    }

    auto CanStopCurrentTrace = false;
    auto OwnedTraceType = FTraceAuxiliary::EConnectionType::None;
    auto OwnedTraceDestination = FString{};
    auto OwnedTraceGuid = FGuid{};
    {
        auto Lock = FScopeLock{&_OwnershipMutex};
        CanStopCurrentTrace = _OwnsActiveTrace && _CanStopOwnedTrace;
        OwnedTraceType = _OwnedTraceType;
        OwnedTraceDestination = _OwnedTraceDestination;
        OwnedTraceGuid = _OwnedTraceGuid;
    }

    if (NOT Snapshot.bIsOwnedByTool)
    {
        OutError = TEXT("The active trace was started outside the Insights Analyzer and was left running.");
        return false;
    }

    if (NOT CanStopCurrentTrace)
    {
        OutError = TEXT("The trace connection is still starting. Try stopping it again once the button is enabled.");
        return false;
    }

    if (NOT FTraceAuxiliary::Stop())
    {
        OutError = TEXT("Unreal Trace reported that there was no active connection to stop.");
        return false;
    }

    if (OwnedTraceType == FTraceAuxiliary::EConnectionType::File
        && NOT OwnedTraceDestination.IsEmpty()
        && OwnedTraceGuid.IsValid())
    { DoQueue_CompletedCapture(OwnedTraceDestination, OwnedTraceGuid); }

    if (OutStoppedTracePath && OwnedTraceType == FTraceAuxiliary::EConnectionType::File)
    { *OutStoppedTracePath = MoveTemp(OwnedTraceDestination); }
    if (OutStoppedTraceGuid)
    { *OutStoppedTraceGuid = OwnedTraceGuid; }

    auto Lock = FScopeLock{&_OwnershipMutex};
    _OwnsActiveTrace = false;
    _CanStopOwnedTrace = false;
    _OwnedTraceType = FTraceAuxiliary::EConnectionType::None;
    _OwnedTraceDestination.Reset();
    _OwnedTraceGuid.Invalidate();
    return true;
}

auto FCkInsightsCaptureController::TryStart_TimedCapture(double InDurationSeconds, FString& OutError) -> bool
{
    return TryStart_TimedCapture(InDurationSeconds, DefaultTimedCaptureScreenshotCount, OutError);
}

auto
    FCkInsightsCaptureController::
    TryStart_TimedCapture(double InDurationSeconds, int32 InScreenshotCount, FString& OutError)
    -> bool
{
    OutError.Reset();
    const auto DurationIsValid = InDurationSeconds > 0.0;
    CK_ENSURE_IF_NOT(DurationIsValid, TEXT("Timed Insights capture duration must be positive, got [{}]"), InDurationSeconds)
    {
        OutError = TEXT("Timed capture duration must be greater than zero.");
        return false;
    }

    const auto ScreenshotCountIsValid = InScreenshotCount >= MinTimedCaptureScreenshotCount
        && InScreenshotCount <= MaxTimedCaptureScreenshotCount;
    CK_ENSURE_IF_NOT(
        ScreenshotCountIsValid,
        TEXT("Timed Insights capture screenshot count must be from {} through {}, got [{}]"),
        MinTimedCaptureScreenshotCount,
        MaxTimedCaptureScreenshotCount,
        InScreenshotCount)
    {
        OutError = ck::Format_UE(
            TEXT("Timed capture screenshot count must be from {} through {}."),
            MinTimedCaptureScreenshotCount,
            MaxTimedCaptureScreenshotCount);
        return false;
    }

    const auto Snapshot = Get_Snapshot();
    if (Snapshot.bIsTracing)
    {
        OutError = Snapshot.bIsOwnedByTool
            ? TEXT("An Insights capture is already running.")
            : TEXT("The active trace was started outside the Insights Analyzer and was left running.");
        return false;
    }

    {
        auto Lock = FScopeLock{&_OwnershipMutex};
        _TimedCaptureRequested = true;
        _TimedCaptureStopping = false;
        _TimedCaptureDurationSeconds = InDurationSeconds;
        _TimedCaptureStartedSeconds = 0.0;
        _TimedCaptureScreenshotFlushDeadlineSeconds = 0.0;
        _TimedCaptureScreenshotCount = InScreenshotCount;
        _TimedCaptureScreenshotMask = 0;
    }

    if (TrySet_Tracing(true, OutError))
    { return true; }

    auto Lock = FScopeLock{&_OwnershipMutex};
    _TimedCaptureRequested = false;
    _TimedCaptureStopping = false;
    _TimedCaptureDurationSeconds = 0.0;
    _TimedCaptureStartedSeconds = 0.0;
    _TimedCaptureScreenshotFlushDeadlineSeconds = 0.0;
    _TimedCaptureScreenshotCount = DefaultTimedCaptureScreenshotCount;
    _TimedCaptureScreenshotMask = 0;
    return false;
}

auto FCkInsightsCaptureController::TryStop_TimedCapture(FString& OutError) -> bool
{
    return DoTryStop_TimedCapture(false, OutError);
}

auto FCkInsightsCaptureController::DoTryStop_TimedCapture(bool InAllowScreenshotDrain, FString& OutError) -> bool
{
    OutError.Reset();
    auto ShouldStop = false;
    {
        auto Lock = FScopeLock{&_OwnershipMutex};
        const auto IsScreenshotDrainActive = _TimedCaptureStopping;
        if (IsScreenshotDrainActive && NOT InAllowScreenshotDrain)
        {
            OutError = TEXT("Timed capture is finishing scheduled screenshots before it stops.");
            return false;
        }

        ShouldStop = _TimedCaptureRequested && _OwnsActiveTrace && _CanStopOwnedTrace;
        if (ShouldStop)
        { _TimedCaptureStopping = true; }
    }

    if (NOT ShouldStop)
    {
        OutError = TEXT("There is no ready module-owned timed capture to stop.");
        return false;
    }

    auto StoppedTracePath = FString{};
    auto StoppedTraceGuid = FGuid{};
    if (TrySet_Tracing(false, OutError, &StoppedTracePath, &StoppedTraceGuid))
    { return true; }

    auto Lock = FScopeLock{&_OwnershipMutex};
    _TimedCaptureStopping = false;
    return false;
}

auto FCkInsightsCaptureController::Get_CompletedCapture(FString& OutTracePath, FGuid& OutTraceGuid) const -> bool
{
    auto Lock = FScopeLock{&_OwnershipMutex};
    OutTracePath = _CompletedTracePath;
    OutTraceGuid = _CompletedTraceGuid;
    return NOT OutTracePath.IsEmpty() && OutTraceGuid.IsValid();
}

auto FCkInsightsCaptureController::Acknowledge_CompletedCapture(FGuid InTraceGuid) -> bool
{
    auto Lock = FScopeLock{&_OwnershipMutex};
    if (NOT InTraceGuid.IsValid() || InTraceGuid != _CompletedTraceGuid)
    { return false; }

    _CompletedTracePath.Reset();
    _CompletedTraceGuid.Invalidate();
    return true;
}

auto
    FCkInsightsCaptureController::
    IsTraceWriterFinalized(FGuid InStoppedTraceGuid) const
    -> bool
{
    if (NOT UE::Trace::IsTracing())
    { return true; }

    if (NOT InStoppedTraceGuid.IsValid())
    { return false; }

    auto ActiveSessionGuid = FGuid{};
    auto ActiveTraceGuid = FGuid{};
    return FTraceAuxiliary::IsConnected(ActiveSessionGuid, ActiveTraceGuid)
        && ActiveTraceGuid != InStoppedTraceGuid;
}

auto FCkInsightsCaptureController::Get_NamedEventsEnabled() const -> bool
{
    return GCycleStatsShouldEmitNamedEvents > 0;
}

auto FCkInsightsCaptureController::Get_StatMaxPerGroup() const -> TOptional<int32>
{
    const auto* MaxPerGroup = IConsoleManager::Get().FindConsoleVariable(TEXT("stats.MaxPerGroup"));
    if (NOT MaxPerGroup)
    { return {}; }

    return MaxPerGroup->GetInt();
}

auto
    FCkInsightsCaptureController::
    TrySet_StatMaxPerGroup(int32 InValue, FString& OutError)
    -> bool
{
    OutError.Reset();
    const auto ValueIsValid = InValue >= 1;
    CK_ENSURE_IF_NOT(ValueIsValid, TEXT("Stats MaxPerGroup must be at least one, got [{}]"), InValue)
    {
        OutError = TEXT("Max stats per group must be at least 1.");
        return false;
    }

    auto* MaxPerGroup = IConsoleManager::Get().FindConsoleVariable(TEXT("stats.MaxPerGroup"));
    const auto CVarIsValid = MaxPerGroup != nullptr;
    CK_ENSURE_IF_NOT(CVarIsValid, TEXT("Unreal did not register stats.MaxPerGroup"))
    {
        OutError = TEXT("stats.MaxPerGroup is not available in this build.");
        return false;
    }

    MaxPerGroup->Set(InValue, ECVF_SetByConsole);
    if (MaxPerGroup->GetInt() != InValue)
    {
        OutError = TEXT("Unreal did not apply stats.MaxPerGroup.");
        return false;
    }

    return true;
}

auto
    FCkInsightsCaptureController::
    TrySet_NamedEventsEnabled(bool InEnabled, FString& OutError)
    -> bool
{
    OutError.Reset();
    const auto Command = FString::Printf(TEXT("stats.NamedEvents %s"), InEnabled ? TEXT("on") : TEXT("off"));
    const auto CommandWasHandled = IConsoleManager::Get().ProcessUserConsoleInput(*Command, *GLog, nullptr);
    const auto StateWasApplied = Get_NamedEventsEnabled() == InEnabled;

    if (NOT CommandWasHandled || NOT StateWasApplied)
    {
        OutError = TEXT("Unreal did not apply the named-events setting.");
        return false;
    }

    return true;
}

auto
    FCkInsightsCaptureController::
    Get_StatProfileEnabled(ECkInsightsStatProfile InProfile) const
    -> bool
{
    const auto Groups = Get_StatGroups(InProfile);
    if (Groups.IsEmpty())
    { return false; }

    for (const auto Group : Groups)
    {
        if (NOT DoIs_StatGroupEnabled(Group))
        { return false; }
    }
    return true;
}

auto
    FCkInsightsCaptureController::
    TrySet_StatProfileEnabled(
        ECkInsightsStatProfile InProfile,
        bool InEnabled,
        FString& OutError)
    -> bool
{
    OutError.Reset();
    const auto Groups = Get_StatGroups(InProfile);
    const auto ProfileIsValid = NOT Groups.IsEmpty();
    CK_ENSURE_IF_NOT(ProfileIsValid, TEXT("Unknown Insights stat profile [{}]"), static_cast<uint8>(InProfile))
    {
        OutError = TEXT("The requested stat profile is not defined.");
        return false;
    }

    const auto EngineIsValid = ck::IsValid(GEngine);
    CK_ENSURE_IF_NOT(EngineIsValid, TEXT("Cannot change Insights stat profiles before GEngine is available"))
    {
        OutError = TEXT("Unreal's console-command system is not available yet.");
        return false;
    }

    auto OriginalStates = TArray<bool>{};
    OriginalStates.Reserve(Groups.Num());
    for (const auto Group : Groups)
    { OriginalStates.Add(DoIs_StatGroupEnabled(Group)); }

    for (auto GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
    {
        if (DoTrySet_StatGroupDisplayEnabled(Groups[GroupIndex], InEnabled))
        { continue; }

        for (auto RestoreIndex = 0; RestoreIndex <= GroupIndex; ++RestoreIndex)
        { DoTrySet_StatGroupDisplayEnabled(Groups[RestoreIndex], OriginalStates[RestoreIndex]); }

        OutError = ck::Format_UE(
            TEXT("Unreal did not apply [{}]."),
            DoGet_StatCommandName(Groups[GroupIndex]));
        return false;
    }

    for (const auto Group : Groups)
    { DoSet_StatGroupCollectionEnabled(Group, InEnabled); }

    return true;
}

auto FCkInsightsCaptureController::Get_TraceChannels() -> FString
{
    return TEXT("cpu,gpu,frame,log,bookmark,screenshot,region,counters,stats,object,rhicommands,rendercommands");
}

auto
    FCkInsightsCaptureController::
    Get_StatGroups(ECkInsightsStatProfile InProfile)
    -> TArray<FName>
{
    switch (InProfile)
    {
        case ECkInsightsStatProfile::CkProcessors:
            return {TEXT("STATGROUP_CkProcessors"), TEXT("STATGROUP_CkProcessors_Details")};
        case ECkInsightsStatProfile::CkScheduler:
            return {TEXT("STATGROUP_CkScheduler")};
        case ECkInsightsStatProfile::Script:
            return {TEXT("STATGROUP_CkScript")};
        case ECkInsightsStatProfile::UObjects:
            return {TEXT("STATGROUP_UObjects")};
        case ECkInsightsStatProfile::Rhi:
            return {TEXT("STATGROUP_RHI")};
        case ECkInsightsStatProfile::Rendering:
            return {
                TEXT("STATGROUP_SceneRendering"),
                TEXT("STATGROUP_RenderThreadProcessing"),
                TEXT("STATGROUP_RenderThreadCommands")};
        default:
            return {};
    }
}

auto
FCkInsightsCaptureController::
    DoOnTraceStarted(FTraceAuxiliary::EConnectionType InType, const FString& InDestination)
    -> void
{
    auto Lock = FScopeLock{&_OwnershipMutex};
    _TraceStartedDuringStart = _StartInProgress;
    _OwnsActiveTrace = _StartInProgress;
    _CanStopOwnedTrace = _StartInProgress && _ConnectionReadyDuringStart;
    _OwnedTraceType = _StartInProgress ? InType : FTraceAuxiliary::EConnectionType::None;
    _OwnedTraceDestination = _StartInProgress ? InDestination : FString{};
}

auto FCkInsightsCaptureController::DoOnTraceConnection() -> void
{
    auto SessionGuid = FGuid{};
    auto TraceGuid = FGuid{};
    const auto ConnectionHasIdentity = FTraceAuxiliary::IsConnected(SessionGuid, TraceGuid);

    auto Lock = FScopeLock{&_OwnershipMutex};
    if (_StartInProgress)
    { _ConnectionReadyDuringStart = true; }
    if (_OwnsActiveTrace)
    { _CanStopOwnedTrace = true; }
    if (ConnectionHasIdentity && (_StartInProgress || _OwnsActiveTrace))
    { _OwnedTraceGuid = TraceGuid; }
    if (_TimedCaptureRequested && _OwnsActiveTrace && _CanStopOwnedTrace && _TimedCaptureStartedSeconds <= 0.0)
    { _TimedCaptureStartedSeconds = FPlatformTime::Seconds(); }
}

auto FCkInsightsCaptureController::DoTick_TimedCapture(float) -> bool
{
    DoAdvance_TimedCapture(FPlatformTime::Seconds());
    return true;
}

auto FCkInsightsCaptureController::DoAdvance_TimedCapture(double InNowSeconds) -> void
{
    auto ScreenshotMilestones = TArray<int32>{};
    auto ScreenshotMilestoneBits = TArray<uint16>{};
    auto DeadlineReached = false;
    auto ScreenshotQueuedThisTick = false;
    auto HasQueuedScreenshot = false;
    {
        auto Lock = FScopeLock{&_OwnershipMutex};
        const auto IsTimedCaptureActive = _TimedCaptureRequested
            && _OwnsActiveTrace
            && _CanStopOwnedTrace
            && _TimedCaptureStartedSeconds > 0.0;
        if (NOT IsTimedCaptureActive)
        { return; }

        const auto ElapsedSeconds = FMath::Max(0.0, InNowSeconds - _TimedCaptureStartedSeconds);
        DeadlineReached = ElapsedSeconds >= _TimedCaptureDurationSeconds;
        if (DeadlineReached)
        { _TimedCaptureStopping = true; }
        if (_TimedCaptureStopping && NOT DeadlineReached)
        { return; }

        const auto DueScreenshotMilestones = DoGet_ScreenshotMilestones(
            ElapsedSeconds,
            _TimedCaptureDurationSeconds,
            _TimedCaptureScreenshotCount);
        for (auto ScreenshotIndex = 0; ScreenshotIndex < DueScreenshotMilestones.Num(); ++ScreenshotIndex)
        {
            const auto Milestone = DueScreenshotMilestones[ScreenshotIndex];
            const auto MilestoneBit = static_cast<uint16>(1u << ScreenshotIndex);
            if ((_TimedCaptureScreenshotMask & MilestoneBit) != 0)
            { continue; }

            ScreenshotMilestones.Add(Milestone);
            ScreenshotMilestoneBits.Add(MilestoneBit);
        }
        HasQueuedScreenshot = _TimedCaptureScreenshotMask != 0;
    }

    auto ScreenshotTracingEnabled = false;
#if UE_SCREENSHOT_TRACE_ENABLED
    const auto Snapshot = Get_Snapshot();
    ScreenshotTracingEnabled = SHOULD_TRACE_SCREENSHOT();
    if (Snapshot.bIsOwnedByTool
        && (Snapshot.State == ECkInsightsCaptureState::Recording
            || Snapshot.State == ECkInsightsCaptureState::Stopping)
        && ScreenshotTracingEnabled
        && NOT FScreenshotRequest::IsScreenshotRequested()
        && ScreenshotMilestones.Num() > 0)
    {
        const auto Milestone = ScreenshotMilestones[0];
        FTraceScreenshot::RequestScreenshot(
            ck::Format_UE(TEXT("CkInsights_TimedCapture_{}Percent"), Milestone),
            true);
        ScreenshotQueuedThisTick = true;

        {
            auto Lock = FScopeLock{&_OwnershipMutex};
            const auto MilestoneBit = ScreenshotMilestoneBits[0];
            _TimedCaptureScreenshotMask |= MilestoneBit;
            HasQueuedScreenshot = true;
            if (DeadlineReached)
            {
                constexpr double ScreenshotFlushTimeoutSeconds = 2.0;
                _TimedCaptureScreenshotFlushDeadlineSeconds = InNowSeconds + ScreenshotFlushTimeoutSeconds;
            }
        }
    }
#endif

    if (NOT DeadlineReached)
    { return; }

    if (ScreenshotQueuedThisTick)
    { return; }

#if UE_SCREENSHOT_TRACE_ENABLED
    const auto HasPendingScreenshot = ScreenshotMilestones.Num() > 0;
    if (ScreenshotTracingEnabled && (HasQueuedScreenshot || HasPendingScreenshot))
    {
        auto ScreenshotFlushDeadlineSeconds = 0.0;
        {
            auto Lock = FScopeLock{&_OwnershipMutex};
            if (_TimedCaptureScreenshotFlushDeadlineSeconds <= 0.0)
            {
                constexpr double ScreenshotFlushTimeoutSeconds = 2.0;
                _TimedCaptureScreenshotFlushDeadlineSeconds = InNowSeconds + ScreenshotFlushTimeoutSeconds;
            }
            ScreenshotFlushDeadlineSeconds = _TimedCaptureScreenshotFlushDeadlineSeconds;
        }
        if (InNowSeconds < ScreenshotFlushDeadlineSeconds)
        {
            // The same bounded window lets a pre-existing global screenshot request clear before the first timed
            // screenshot is queued. Once queued, FTraceScreenshot clears FScreenshotRequest before its fire-and-forget
            // PNG compression task emits the ScreenshotHeader and ScreenshotChunk trace events, so keep honoring the
            // window after the viewport request clears rather than discarding those asynchronous events on trace stop.
            return;
        }
    }
#endif

    auto Error = FString{};
    DoTryStop_TimedCapture(true, Error);
}

auto FCkInsightsCaptureController::DoQueue_CompletedCapture(FString InTracePath, FGuid InTraceGuid) -> void
{
    auto Lock = FScopeLock{&_OwnershipMutex};
    _CompletedTracePath = MoveTemp(InTracePath);
    _CompletedTraceGuid = InTraceGuid;
}

auto FCkInsightsCaptureController::DoGet_TimedCaptureProgress(double InElapsedSeconds, double InDurationSeconds) -> double
{
    if (InDurationSeconds <= 0.0)
    { return 0.0; }

    return FMath::Clamp(InElapsedSeconds / InDurationSeconds, 0.0, 1.0);
}

auto
    FCkInsightsCaptureController::
    DoGet_ScreenshotMilestones(
        double InElapsedSeconds,
        double InDurationSeconds,
        int32 InScreenshotCount)
    -> TArray<int32>
{
    auto Milestones = TArray<int32>{};
    if (InScreenshotCount <= 0)
    { return Milestones; }

    const auto Progress = DoGet_TimedCaptureProgress(InElapsedSeconds, InDurationSeconds);
    for (auto ScreenshotIndex = 0; ScreenshotIndex < InScreenshotCount; ++ScreenshotIndex)
    {
        const auto Milestone = InScreenshotCount == 1
            ? 50
            : FMath::RoundToInt(10.0 + 80.0 * ScreenshotIndex / (InScreenshotCount - 1));
        if (Progress >= Milestone / 100.0)
        { Milestones.Add(Milestone); }
    }
    return Milestones;
}

auto
    FCkInsightsCaptureController::
    DoGet_StatCommandName(FName InGroup)
    -> FString
{
    auto CommandGroup = InGroup.ToString();
    CommandGroup.RemoveFromStart(TEXT("STATGROUP_"));
    return ck::Format_UE(TEXT("stat {}"), CommandGroup);
}

auto
    FCkInsightsCaptureController::
    DoIs_StatGroupEnabled(FName InGroup)
    -> bool
{
    auto CommandGroup = InGroup.ToString();
    CommandGroup.RemoveFromStart(TEXT("STATGROUP_"));

    auto CurrentViewportEnabled = false;
    auto OtherViewportEnabled = false;
    auto* StatViewportClient = ck_insights_capture::Get_StatViewportClient();
    if (NOT StatViewportClient)
    { return false; }

    auto StatViewportGuard = TGuardValue<FCommonViewportClient*>{
        GStatProcessingViewportClient,
        StatViewportClient};
    FCoreDelegates::StatCheckEnabled.Broadcast(
        *CommandGroup,
        CurrentViewportEnabled,
        OtherViewportEnabled);
    return CurrentViewportEnabled;
}

auto
    FCkInsightsCaptureController::
    DoTrySet_StatGroupDisplayEnabled(FName InGroup, bool InEnabled)
    -> bool
{
    if (DoIs_StatGroupEnabled(InGroup) != InEnabled)
    {
        auto* StatViewportClient = ck_insights_capture::Get_StatViewportClient();
        if (NOT StatViewportClient)
        { return false; }

        auto StatViewportGuard = TGuardValue<FCommonViewportClient*>{
            GStatProcessingViewportClient,
            StatViewportClient};
        const auto CommandWasHandled = GEngine->Exec(
            nullptr,
            *DoGet_StatCommandName(InGroup),
            *GLog);
        if (NOT CommandWasHandled)
        { return false; }
    }

    return DoIs_StatGroupEnabled(InGroup) == InEnabled;
}

auto
    FCkInsightsCaptureController::
    DoSet_StatGroupCollectionEnabled(FName InGroup, bool InEnabled)
    -> void
{
    auto CommandGroup = InGroup.ToString();
    CommandGroup.RemoveFromStart(TEXT("STATGROUP_"));

    IStatGroupEnableManager::Get().StatGroupEnableManagerCommand(ck::Format_UE(
        TEXT("{} {}"),
        InEnabled ? TEXT("enable") : TEXT("disable"),
        CommandGroup));
}

auto
    FCkInsightsCaptureController::
    DoOnTraceStopped(FTraceAuxiliary::EConnectionType, const FString&)
    -> void
{
    auto Lock = FScopeLock{&_OwnershipMutex};
    if (_StartInProgress)
    { _TraceStoppedDuringStart = true; }
    _OwnsActiveTrace = false;
    _CanStopOwnedTrace = false;
    _TimedCaptureRequested = false;
    _TimedCaptureStopping = false;
    _TimedCaptureDurationSeconds = 0.0;
    _TimedCaptureStartedSeconds = 0.0;
    _TimedCaptureScreenshotFlushDeadlineSeconds = 0.0;
    _TimedCaptureScreenshotCount = DefaultTimedCaptureScreenshotCount;
    _TimedCaptureScreenshotMask = 0;
    _OwnedTraceType = FTraceAuxiliary::EConnectionType::None;
    _OwnedTraceDestination.Reset();
    _OwnedTraceGuid.Invalidate();
}

// --------------------------------------------------------------------------------------------------------------------
