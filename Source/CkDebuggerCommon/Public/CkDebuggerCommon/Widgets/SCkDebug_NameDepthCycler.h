#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// The toolbar "Name ◀ value ▶" control — one widget for every debugger that
// tunes display-name verbosity (the depth consumed by
// SCkDebug_NameLabel::Get_ShortName / the SCkDebug_NameLabel widget).
//
// Cycle semantics (shared by the GOAP + SM debuggers, now canonical):
//   ◀ : Full(0) → MaxDepth → … → 2 → 1 → Full(0)
//   ▶ : Full(0) → 1 → 2 → … → MaxDepth → Full(0)
// The value text renders "Full" for depth 0.
//
// State stays with the owner (view model, graph object, window member) — the
// widget is attribute-bound and only reports the new depth via OnDepthChanged.
// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkDebug_NameDepthChanged, int32 /* NewDepth */);

class CKDEBUGGERCOMMON_API SCkDebug_NameDepthCycler : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_NameDepthCycler)
        : _Depth(1)
        , _MaxDepth(1)
    {}
        SLATE_ATTRIBUTE(int32, Depth)
        // Largest meaningful depth (segment count of the longest name) — the
        // cycle wraps to Full(0) past it. Bind it live where the content set
        // changes (e.g. a graph's Get_MaxNameDepth).
        SLATE_ATTRIBUTE(int32, MaxDepth)
        SLATE_EVENT(FOnCkDebug_NameDepthChanged, OnDepthChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto DoCycle(int32 InDirection) -> FReply;

    TAttribute<int32> _Depth;
    TAttribute<int32> _MaxDepth;
    FOnCkDebug_NameDepthChanged _OnDepthChanged;
};

// ====================================================================================================================
