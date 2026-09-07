#include "CkInsightsDebugger/Analysis/CkInsightsFrameRequest.h"

#include <Async/Async.h>
#include <Misc/ScopeLock.h>

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
