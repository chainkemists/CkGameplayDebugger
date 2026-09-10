#include "CkInsightsDebugger/Analysis/CkInsightsFrameRequest.h"

#include "CkInsightsAnalyzer/Core/CkTimerCategorizer.h"
#include "CkCore/Format/CkFormat.h"

#include <Async/Async.h>
#include <Misc/ScopeLock.h>

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsFrameTimingView::IsValid() const -> bool
{
    return FMath::IsFinite(FrameDurationMs)
        && FrameDurationMs > 0.0
        && NOT Events.IsEmpty();
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsFrameTimingView::Build(
    const FCk_FrameAnalysisResult& InResult,
    const TMap<uint32, FString>& InTimerNames,
    uint64 InSelectedFrameCount,
    bool InIsProvisional)
    -> TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe>
{
    const auto HasFiniteFrameWindow = FMath::IsFinite(InResult.FrameStartTime)
        && FMath::IsFinite(InResult.FrameEndTime)
        && InResult.FrameEndTime > InResult.FrameStartTime;
    if (NOT HasFiniteFrameWindow || InResult.Events.IsEmpty())
    {
        return {};
    }

    auto View = MakeShared<FCkInsightsFrameTimingView, ESPMode::ThreadSafe>();
    View->FrameIndex = InResult.FrameIndex;
    View->FrameDurationMs = (InResult.FrameEndTime - InResult.FrameStartTime) * 1000.0;
    View->SelectedFrameCount = FMath::Max<uint64>(1, InSelectedFrameCount);
    View->IsProvisional = InIsProvisional;
    View->Events.Reserve(InResult.Events.Num());
    View->HeaderText = View->SelectedFrameCount > 1
        ? ck::Format_UE(TEXT("Frame {} | first of {} selected | {:.2f} ms{}"),
            View->FrameIndex,
            View->SelectedFrameCount,
            View->FrameDurationMs,
            View->IsProvisional ? TEXT(" | provisional") : TEXT(""))
        : ck::Format_UE(TEXT("Frame {} | {:.2f} ms{}"),
            View->FrameIndex,
            View->FrameDurationMs,
            View->IsProvisional ? TEXT(" | provisional") : TEXT(""));
    View->TimeScaleLabels.Reserve(5);
    for (auto Tick = 0; Tick <= 4; ++Tick)
    {
        View->TimeScaleLabels.Add(ck::Format_UE(
            TEXT("{:.1f}"),
            View->FrameDurationMs * static_cast<double>(Tick) / 4.0));
    }

    auto MinimumDepth = MAX_uint32;
    for (const auto& Event : InResult.Events)
    {
        if (NOT FMath::IsFinite(Event.StartTime) || NOT FMath::IsFinite(Event.EndTime))
        {
            continue;
        }

        const auto StartTime = FMath::Max(Event.StartTime, InResult.FrameStartTime);
        const auto EndTime = FMath::Min(Event.EndTime, InResult.FrameEndTime);
        if (EndTime <= StartTime)
        {
            continue;
        }

        auto& TimingEvent = View->Events.AddDefaulted_GetRef();
        TimingEvent.TimerIndex = Event.TimerIndex;
        TimingEvent.StartMs = (StartTime - InResult.FrameStartTime) * 1000.0;
        TimingEvent.EndMs = (EndTime - InResult.FrameStartTime) * 1000.0;
        TimingEvent.Depth = Event.Depth;
        MinimumDepth = FMath::Min(MinimumDepth, Event.Depth);

        if (NOT View->Timers.Contains(Event.TimerIndex))
        {
            const auto* RawName = InTimerNames.Find(Event.TimerIndex);
            const auto Name = RawName != nullptr
                ? *RawName
                : FString{TEXT("Unknown")};
            View->Timers.Add(Event.TimerIndex, FCkInsightsFrameTimingTimer{
                Name,
                FCk_TimerCategorizer::SimplifyName(Name),
            });
        }
    }

    if (View->Events.IsEmpty())
    {
        return {};
    }

    constexpr auto MaximumRenderedDepth = uint32{1023};
    for (auto& Event : View->Events)
    {
        const auto RelativeDepth = Event.Depth >= MinimumDepth
            ? Event.Depth - MinimumDepth
            : 0;
        Event.Depth = FMath::Min(RelativeDepth, MaximumRenderedDepth);
        View->MaxDepth = FMath::Max(View->MaxDepth, Event.Depth);
    }

    return View;
}

// --------------------------------------------------------------------------------------------------------------------

FCkInsightsFrameRequestQueue::~FCkInsightsFrameRequestQueue()
{
    Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsFrameRequestQueue::Request(uint64 InFrameIndex, FWork InWork) -> void
{
    if (NOT InWork)
    {
        return;
    }

    RequestProgressive(InFrameIndex,
        [Work = MoveTemp(InWork)](const FCancelToken& InCancelToken, const FPublish&) mutable
        {
            return Work(InCancelToken);
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsFrameRequestQueue::RequestProgressive(uint64 InFrameIndex, FProgressWork InWork) -> void
{
    if (NOT InWork)
    {
        return;
    }

    ++_Revision;

    if (_Active.IsSet())
    {
        _Active->CancelToken->Store(true);
    }

    _Pending = FPendingRequest{
        InFrameIndex,
        _Revision,
        MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false),
        MoveTemp(InWork),
    };

    if (NOT _Active.IsSet())
    {
        DoStartPending();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsFrameRequestQueue::Poll() -> TOptional<FCkInsightsFrameDetails>
{
    if (NOT _Active.IsSet())
    {
        return {};
    }

    const auto CompletedRevision = _Active->Revision;
    const auto CompletedFrameIndex = _Active->FrameIndex;
    const auto IsCurrentGeneration = CompletedRevision == _Revision;

    if (NOT _Active->Future.IsReady())
    {
        if (NOT IsCurrentGeneration)
        {
            return {};
        }

        auto Intermediate = _Active->Mailbox->Consume();
        if (NOT Intermediate.IsSet())
        {
            return {};
        }

        Intermediate->Result.FrameIndex = CompletedFrameIndex;
        Intermediate->IsIntermediate = true;
        return MoveTemp(Intermediate);
    }

    auto Completed = _Active->Future.Consume();
    _Active.Reset();

    DoStartPending();

    if (NOT IsCurrentGeneration)
    {
        return {};
    }

    Completed.Result.FrameIndex = CompletedFrameIndex;
    Completed.IsIntermediate = false;
    return MoveTemp(Completed);
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsFrameRequestQueue::Reset() -> void
{
    ++_Revision;
    _Pending.Reset();

    if (_Active.IsSet())
    {
        _Active->CancelToken->Store(true);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsFrameRequestQueue::IsBusy() const -> bool
{
    return _Active.IsSet() || _Pending.IsSet();
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsFrameRequestQueue::DoStartPending() -> void
{
    if (NOT _Pending.IsSet() || _Active.IsSet())
    {
        return;
    }

    auto Pending = MoveTemp(_Pending.GetValue());
    _Pending.Reset();

    auto Work = MoveTemp(Pending.Work);
    auto CancelToken = Pending.CancelToken;
    auto Mailbox = MakeShared<FProgressMailbox, ESPMode::ThreadSafe>();
    auto Future = Async(EAsyncExecution::ThreadPool, [Work = MoveTemp(Work), CancelToken, Mailbox]() mutable
    {
        const auto Publish = FPublish{[CancelToken, Mailbox](FCkInsightsFrameDetails InDetails)
        {
            if (CancelToken->Load())
            {
                return;
            }

            InDetails.IsIntermediate = true;
            Mailbox->Publish(MoveTemp(InDetails));
        }};
        return Work(CancelToken, Publish);
    });

    _Active = FActiveRequest{
        Pending.FrameIndex,
        Pending.Revision,
        MoveTemp(CancelToken),
        MoveTemp(Mailbox),
        MoveTemp(Future),
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsFrameRequestQueue::FProgressMailbox::Publish(FCkInsightsFrameDetails InDetails) -> void
{
    // Keep a potentially large discarded snapshot alive until after the mutex is released.
    auto Superseded = TOptional<FCkInsightsFrameDetails>{};
    {
        const auto Lock = FScopeLock{&_Mutex};
        if (_Latest.IsSet())
        {
            Superseded.Emplace(MoveTemp(_Latest.GetValue()));
            _Latest.Reset();
        }
        _Latest.Emplace(MoveTemp(InDetails));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsFrameRequestQueue::FProgressMailbox::Consume() -> TOptional<FCkInsightsFrameDetails>
{
    const auto Lock = FScopeLock{&_Mutex};
    if (NOT _Latest.IsSet())
    {
        return {};
    }

    auto Latest = MoveTemp(_Latest.GetValue());
    _Latest.Reset();
    return MoveTemp(Latest);
}

// --------------------------------------------------------------------------------------------------------------------
