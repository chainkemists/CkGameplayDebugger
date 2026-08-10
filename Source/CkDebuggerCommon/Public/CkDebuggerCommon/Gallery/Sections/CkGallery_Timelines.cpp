// --------------------------------------------------------------------------------------------------------------------
// Time-series / history primitives promoted in the 2026-08-09 common-widget consolidation:
//   - SCkDebug_FrameStrip     scrubbable frame history (relative heat AND budget heat)
//   - SCkDebug_ScrubTimeline  segment track with cursor, subdivisions, and every mark kind
//   - SCkDebug_EventLog       capped severity-toned event list
//
// Each section is interactive: scrub the strip and the echo pill follows, click a timeline dot,
// append log lines until the cap evicts. Helpers live in a NAMED namespace — gallery .cpp files
// are unity-built together and anonymous-namespace helpers collide.
// --------------------------------------------------------------------------------------------------------------------

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Gallery/CkDebuggerGallery_Registry.h"
#include "CkGallery_SectionUtils.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_FrameStrip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ScrubTimeline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

using ck::gallery::Caption;

// ====================================================================================================================

namespace ck_gallery_timelines
{
    constexpr auto SampleCount = 240;
    constexpr auto BudgetMs = 16.67;

    // A believable frame trace: a slow breathing baseline, a periodic hitch, and a marked
    // "pumped" frame every so often so every visual feature has something to draw.
    auto Make_Samples() -> TArray<FCkDebug_FrameSample>
    {
        auto Samples = TArray<FCkDebug_FrameSample>{};
        Samples.Reserve(SampleCount);

        for (auto Index = 0; Index < SampleCount; ++Index)
        {
            const auto Phase = static_cast<double>(Index) * 0.11;
            const auto Baseline = 11.0 + FMath::Sin(Phase) * 3.0;
            const auto Hitch = (Index % 37 == 0) ? 22.0 : 0.0;
            const auto Spike = (Index % 91 == 0) ? 40.0 : 0.0;

            auto Sample = FCkDebug_FrameSample{};
            Sample.ValueMs = Baseline + Hitch + Spike;
            Sample.HasMarker = Index % 23 == 0;
            Sample.IsHighlighted = Index % 53 == 0;

            Samples.Add(Sample);
        }

        return Samples;
    }

    auto Make_Segments() -> TArray<FCkDebug_ScrubSegment>
    {
        const auto Names = TArray<FString>{
            TEXT("Idle"), TEXT("Approach"), TEXT("Interact"), TEXT("Recover"), TEXT("Idle")};

        auto Segments = TArray<FCkDebug_ScrubSegment>{};
        auto Cursor = 0.0;

        for (auto Index = 0; Index < Names.Num(); ++Index)
        {
            const auto Duration = 1.4 + static_cast<double>(Index) * 0.7;

            auto Segment = FCkDebug_ScrubSegment{};
            Segment.StartSeconds = Cursor;
            Segment.EndSeconds = Cursor + Duration;
            Segment.Color = ck::debug_axes::Get_CategoricalColor(Index);
            Segment.Label = Names[Index];
            Segment.Tooltip = ck::Format_UE(TEXT("{} — {}s"), Names[Index], FString::Printf(TEXT("%.2f"), Duration));
            Segment.SubdivisionCount = 4 + Index * 3;

            Segments.Add(Segment);
            Cursor += Duration;
        }

        return Segments;
    }

    auto Make_Marks() -> TArray<FCkDebug_ScrubMark>
    {
        auto Marks = TArray<FCkDebug_ScrubMark>{};

        const auto Add = [&Marks](double InTime, ECkDebug_ScrubMarkKind InKind,
                                  const FLinearColor& InColor, const FString& InTooltip, int32 InSelectionId)
        {
            auto Mark = FCkDebug_ScrubMark{};
            Mark.TimeSeconds = InTime;
            Mark.Kind = InKind;
            Mark.Color = InColor;
            Mark.Tooltip = InTooltip;
            Mark.SelectionId = InSelectionId;
            Marks.Add(Mark);
        };

        Add(1.4,  ECkDebug_ScrubMarkKind::Cut,  CkStyle::Warn(),   TEXT("Breakpoint pause"),  INDEX_NONE);
        Add(3.5,  ECkDebug_ScrubMarkKind::Flag, CkStyle::Accent(), TEXT("Bookmark: handoff"), INDEX_NONE);
        Add(5.0,  ECkDebug_ScrubMarkKind::Dot,  CkStyle::Ok(),     TEXT("Plan found"),        0);
        Add(7.2,  ECkDebug_ScrubMarkKind::Dot,  CkStyle::Err(),    TEXT("Plan failed"),       1);
        Add(9.6,  ECkDebug_ScrubMarkKind::Dot,  CkStyle::Info(),   TEXT("Replan"),            2);
        Add(11.0, ECkDebug_ScrubMarkKind::Tick, CkStyle::TextDim(),TEXT("World-state change"),INDEX_NONE);

        return Marks;
    }

    auto Get_TotalDuration(const TArray<FCkDebug_ScrubSegment>& InSegments) -> double
    {
        return InSegments.IsEmpty() ? 1.0 : InSegments.Last().EndSeconds;
    }

    auto Make_SeedEvents() -> TArray<FCkDebug_EventLogEntry>
    {
        const auto Rows = TArray<TTuple<FString, FString, ECk_Tone>>{
            {TEXT("PATH"),   TEXT("Repathed around a closed door"),          ECk_Tone::Info},
            {TEXT("SLEEP"),  TEXT("Agent entered the sleep pool"),           ECk_Tone::Neutral},
            {TEXT("PIERCE"), TEXT("Corner-pierce shortcut accepted"),        ECk_Tone::Ok},
            {TEXT("PROXY"),  TEXT("Proxy body promoted to full simulation"), ECk_Tone::Accent},
            {TEXT("PATH"),   TEXT("Partial path — goal outside the navmesh"),ECk_Tone::Warn},
            {TEXT("PATH"),   TEXT("Path request failed after 3 retries"),    ECk_Tone::Err},
        };

        auto Entries = TArray<FCkDebug_EventLogEntry>{};

        for (auto Index = 0; Index < Rows.Num(); ++Index)
        {
            auto Entry = FCkDebug_EventLogEntry{};
            Entry.Category = Rows[Index].Get<0>();
            Entry.Message = Rows[Index].Get<1>();
            Entry.Tone = Rows[Index].Get<2>();
            Entry.TimeSeconds = 12.5 + static_cast<double>(Index) * 1.37;
            Entry.SelectionId = Index;

            Entries.Add(MoveTemp(Entry));
        }

        return Entries;
    }
}

// ====================================================================================================================
// FRAME STRIP
// ====================================================================================================================

class SCkGallery_FrameStripDemo : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGallery_FrameStripDemo) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void
    {
        auto Samples = ck_gallery_timelines::Make_Samples();

        ChildSlot
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                Caption(TEXT("Relative heat (no budget) — the tallest frame defines both height and color. Drag to scrub, double-click for live, wheel to zoom, right-drag to pan, right-click to copy."))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
            [
                SAssignNew(_RelativeStrip, SCkDebug_FrameStrip)
                .SelectedIndexFromEnd_Lambda([this]() { return _SelectedFromEnd; })
                .MarkerMeaning(FString{TEXT("pumped")})
                .CopyText_Lambda([this]() { return Compose_CopyText(); })
                .OnScrubbed_Lambda([this](int32 InIndexFromEnd) { Do_Select(InIndexFromEnd); })
                .OnReturnToLive_Lambda([this]() { Do_Select(0); })
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                Caption(TEXT("Budget heat (16.67 ms) — Ok under budget, Warn exactly at it, Err at twice. Same selection, so the two strips stay in lockstep."))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
            [
                SAssignNew(_BudgetStrip, SCkDebug_FrameStrip)
                .BudgetMs(ck_gallery_timelines::BudgetMs)
                .HeightScale(ECkDebug_FrameStripHeightScale::Budget)
                .SelectedIndexFromEnd_Lambda([this]() { return _SelectedFromEnd; })
                .MarkerMeaning(FString{TEXT("pumped")})
                .CopyText_Lambda([this]() { return Compose_CopyText(); })
                .OnScrubbed_Lambda([this](int32 InIndexFromEnd) { Do_Select(InIndexFromEnd); })
                .OnReturnToLive_Lambda([this]() { Do_Select(0); })
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Left)
            [
                SNew(SCkDebug_StatusPill)
                .Text_Lambda([this]() { return FText::FromString(Compose_CopyText()); })
                .Tone_Lambda([this]() { return _SelectedFromEnd == 0 ? ECk_Tone::Ok : ECk_Tone::Warn; })
            ]
        ];

        _Samples = Samples;

        if (_RelativeStrip.IsValid()) { _RelativeStrip->Set_Samples(_Samples); }
        if (_BudgetStrip.IsValid())   { _BudgetStrip->Set_Samples(_Samples); }
    }

private:
    auto Do_Select(int32 InIndexFromEnd) -> void
    {
        _SelectedFromEnd = FMath::Clamp(InIndexFromEnd, 0, FMath::Max(0, _Samples.Num() - 1));
    }

    auto Compose_CopyText() const -> FString
    {
        const auto ArrayIndex = _Samples.Num() - 1 - _SelectedFromEnd;

        if (NOT _Samples.IsValidIndex(ArrayIndex))
        { return FString{TEXT("no selection")}; }

        return ck::Format_UE(TEXT("frame -{} — {} ms"),
            _SelectedFromEnd, FString::Printf(TEXT("%.2f"), _Samples[ArrayIndex].ValueMs));
    }

private:
    TArray<FCkDebug_FrameSample> _Samples;
    int32 _SelectedFromEnd = 0;

    TSharedPtr<SCkDebug_FrameStrip> _RelativeStrip;
    TSharedPtr<SCkDebug_FrameStrip> _BudgetStrip;
};

class FCkGallery_FrameStrip : public ICkDebuggerGallery_Section
{
public:
    virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Frame Strip")); }
    virtual auto Get_Description() const -> FText override
    {
        return FText::FromString(TEXT("Scrubbable frame history — the merged Scheduler frame-history bar and Insights frame-bar chart. Heat comes from ck::debug_axes::Get_HeatColor, so the palette drives it."));
    }
    virtual auto Get_SortPriority() const -> int32 override { return 400; }

    virtual auto Build_Widget() -> TSharedRef<SWidget> override
    {
        return SNew(SCkGallery_FrameStripDemo);
    }
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_FrameStrip)

// ====================================================================================================================
// SCRUB TIMELINE
// ====================================================================================================================

class SCkGallery_ScrubTimelineDemo : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGallery_ScrubTimelineDemo) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void
    {
        _Segments = ck_gallery_timelines::Make_Segments();
        _Marks = ck_gallery_timelines::Make_Marks();
        _LiveTime = ck_gallery_timelines::Get_TotalDuration(_Segments);

        ChildSlot
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                Caption(TEXT("Segments carry subdivision cells and contrast-picked labels; marks cover all four kinds (cut, flag, selectable dot, tick). Drag to scrub, click a dot to select it, right-drag or Ctrl+drag to pan, wheel to zoom, F to re-centre."))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
            [
                SNew(SBox)
                .HeightOverride(56.0f)
                [
                    SAssignNew(_Timeline, SCkDebug_ScrubTimeline)
                    .DesiredHeight(56.0f)
                    .InitialViewDuration(_LiveTime)
                    .Mode_Lambda([this]() { return _Mode; })
                    .ScrubTime_Lambda([this]() { return _ScrubTime; })
                    .LiveTime_Lambda([this]() { return _LiveTime; })
                    .SelectedMarkId_Lambda([this]() { return _SelectedMarkId; })
                    .CopyText_Lambda([this]() { return Compose_CopyText(); })
                    .OnScrubbed_Lambda([this](double InTime)
                    {
                        _Mode = ECkDebug_ScrubMode::Scrub;
                        _ScrubTime = InTime;
                    })
                    .OnMarkSelected_Lambda([this](int32 InSelectionId) { _SelectedMarkId = InSelectionId; })
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Go Live")))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        _Mode = ECkDebug_ScrubMode::Live;
                        _SelectedMarkId = INDEX_NONE;

                        if (_Timeline.IsValid()) { _Timeline->Focus_OnCursor(); }

                        return FReply::Handled();
                    })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_StatusPill)
                    .Text_Lambda([this]() { return FText::FromString(Compose_CopyText()); })
                    .Tone_Lambda([this]()
                    {
                        return _Mode == ECkDebug_ScrubMode::Live ? ECk_Tone::Ok : ECk_Tone::Warn;
                    })
                ]
            ]
        ];

        if (_Timeline.IsValid())
        {
            _Timeline->Set_Content(_Segments, _Marks);
            _Timeline->Set_View(0.0, _LiveTime);
        }
    }

private:
    auto Compose_CopyText() const -> FString
    {
        const auto ModeText = _Mode == ECkDebug_ScrubMode::Live ? TEXT("LIVE") : TEXT("SCRUB");
        const auto TimeText = _Mode == ECkDebug_ScrubMode::Live ? _LiveTime : _ScrubTime;

        return ck::Format_UE(TEXT("{} @ {}s  ·  mark {}"),
            ModeText, FString::Printf(TEXT("%.2f"), TimeText), _SelectedMarkId);
    }

private:
    TArray<FCkDebug_ScrubSegment> _Segments;
    TArray<FCkDebug_ScrubMark> _Marks;

    ECkDebug_ScrubMode _Mode = ECkDebug_ScrubMode::Live;
    double _ScrubTime = 0.0;
    double _LiveTime = 1.0;
    int32 _SelectedMarkId = INDEX_NONE;

    TSharedPtr<SCkDebug_ScrubTimeline> _Timeline;
};

class FCkGallery_ScrubTimeline : public ICkDebuggerGallery_Section
{
public:
    virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Scrub Timeline")); }
    virtual auto Get_Description() const -> FText override
    {
        return FText::FromString(TEXT("Segment track with a scrub cursor — the merged SM timeline and GOAP scrub track. The widget owns the view window; the owner owns mode and time."));
    }
    virtual auto Get_SortPriority() const -> int32 override { return 410; }

    virtual auto Build_Widget() -> TSharedRef<SWidget> override
    {
        return SNew(SCkGallery_ScrubTimelineDemo);
    }
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_ScrubTimeline)

// ====================================================================================================================
// EVENT LOG
// ====================================================================================================================

class SCkGallery_EventLogDemo : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGallery_EventLogDemo) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void
    {
        ChildSlot
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                Caption(TEXT("Capped at 8 entries here so eviction is visible. Rows are STextBlock + chip only, so selection works; right-click copies the selected line(s), multi-select included."))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
            [
                SNew(SBox)
                .HeightOverride(150.0f)
                [
                    SAssignNew(_Log, SCkDebug_EventLog)
                    .MaxEntries(8)
                    .SelectedId_Lambda([this]() { return _SelectedId; })
                    .OnEntrySelected_Lambda([this](int32 InSelectionId) { _SelectedId = InSelectionId; })
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Append")))
                    .OnClicked_Lambda([this]() -> FReply { Do_Append(); return FReply::Handled(); })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Clear")))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (_Log.IsValid()) { _Log->Clear_Entries(); }
                        _SelectedId = INDEX_NONE;
                        return FReply::Handled();
                    })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return FText::FromString(ck::Format_UE(TEXT("{} rows · selected {}"),
                            _Log.IsValid() ? _Log->Get_EntryCount() : 0, _SelectedId));
                    })
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ]
        ];

        if (_Log.IsValid())
        { _Log->Set_Entries(ck_gallery_timelines::Make_SeedEvents()); }

        _NextId = 6;
    }

private:
    auto Do_Append() -> void
    {
        if (NOT _Log.IsValid())
        { return; }

        const auto Tones = TArray<ECk_Tone>{
            ECk_Tone::Neutral, ECk_Tone::Info, ECk_Tone::Ok, ECk_Tone::Warn, ECk_Tone::Err, ECk_Tone::Accent};

        auto Entry = FCkDebug_EventLogEntry{};
        Entry.Category = FString{TEXT("TEST")};
        Entry.Message = ck::Format_UE(TEXT("Synthesized event #{}"), _NextId);
        Entry.Tone = Tones[_NextId % Tones.Num()];
        Entry.TimeSeconds = 20.0 + static_cast<double>(_NextId) * 0.83;
        Entry.SelectionId = _NextId;

        _Log->Add_Entry(MoveTemp(Entry));
        ++_NextId;
    }

private:
    TSharedPtr<SCkDebug_EventLog> _Log;
    int32 _SelectedId = INDEX_NONE;
    int32 _NextId = 0;
};

class FCkGallery_EventLog : public ICkDebuggerGallery_Section
{
public:
    virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Event Log")); }
    virtual auto Get_Description() const -> FText override
    {
        return FText::FromString(TEXT("Capped, auto-scrolling event list with severity tones and multi-select copy. Promoted from the Crowd debugger's event-log panel."));
    }
    virtual auto Get_SortPriority() const -> int32 override { return 420; }

    virtual auto Build_Widget() -> TSharedRef<SWidget> override
    {
        return SNew(SCkGallery_EventLogDemo);
    }
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_EventLog)

// --------------------------------------------------------------------------------------------------------------------
