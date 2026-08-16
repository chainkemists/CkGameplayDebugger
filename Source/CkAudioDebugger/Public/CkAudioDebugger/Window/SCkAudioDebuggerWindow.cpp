#include "CkAudioDebugger/Window/SCkAudioDebuggerWindow.h"

#include "CkAudioDebugger/Window/SCkAudioDebugger_FalloffCurve.h"
#include "CkAudioDebugger/Window/SCkAudioDebugger_Radar.h"

#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_AlertRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_UnderlineTabs.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

const FName SCkAudioDebuggerWindow::WindowId = FName(TEXT("AudioDebugger"));

// --------------------------------------------------------------------------------------------------------------------

namespace ck_audio_debugger_window
{
    constexpr auto k_StatePillWidth  = 78.0f;
    constexpr auto k_VolumeTextWidth = 104.0f;
    constexpr auto k_FadeTextWidth   = 96.0f;
    constexpr auto k_MeterHeight     = 9.0f;
    constexpr auto k_LaneHeight      = 62.0f;
    constexpr auto k_LanePageHeight  = 220.0f;
    constexpr auto k_RadarSize       = 260.0f;
    constexpr auto k_CurveHeight     = 110.0f;
    constexpr auto k_EventLogCapacity = 300;

    /** Never smaller than this, so a track sitting on top of the listener still gets rings worth reading rather than
     *  a radar scaled down to a few centimetres. */
    constexpr auto k_RadarMinRangeCm = 500.0f;

    /** Headroom past the selected track so it never sits exactly on the rim, where "at this distance" and "at least
     *  this distance" would look identical. */
    constexpr auto k_RadarRangePadding = 1.25f;

    /** Samples kept per track. At the refresh gate's cadence this is a few seconds of history — long enough that a
     *  crossfade reads as two crossing curves, short enough that a finished one scrolls off instead of lingering. */
    constexpr auto k_HistorySamples = 96;

    // Monospaced by design: the mixer is a column of numbers that has to line up, and routing the sizes through
    // ScaledFont is what puts the whole table under the shared TextScale axis.
    auto Get_RowFont() -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody());
    }

    auto Get_MonoFont() -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall());
    }

    auto Get_MicroFont() -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro());
    }

    auto Get_CardValueFont() -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeH3());
    }

    auto Get_CardLabelFont() -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro());
    }

    auto
        Get_PageId(
            ECkAudioDebugger_Page InPage)
        -> FName
    {
        switch (InPage)
        {
            case ECkAudioDebugger_Page::Directors: return FName{TEXT("Directors")};
            case ECkAudioDebugger_Page::Crossfade: return FName{TEXT("Crossfade")};
            case ECkAudioDebugger_Page::Spatial:   return FName{TEXT("Spatial")};
            case ECkAudioDebugger_Page::Events:    return FName{TEXT("Events")};
            case ECkAudioDebugger_Page::Overlay:   return FName{TEXT("Overlay")};
            case ECkAudioDebugger_Page::Tracks:
            default:                               return FName{TEXT("Tracks")};
        }
    }

    auto
        Build_OverrideText(
            ECk_AudioTrack_OverrideBehavior InBehavior)
        -> FText
    {
        switch (InBehavior)
        {
            case ECk_AudioTrack_OverrideBehavior::Crossfade: return FText::FromString(TEXT("Crossfade"));
            case ECk_AudioTrack_OverrideBehavior::Queue:     return FText::FromString(TEXT("Queue"));
            case ECk_AudioTrack_OverrideBehavior::Interrupt:
            default:                                        return FText::FromString(TEXT("Interrupt"));
        }
    }

    auto
        Build_LoopText(
            ECk_LoopBehavior InBehavior)
        -> FText
    {
        return InBehavior == ECk_LoopBehavior::Loop
            ? FText::FromString(TEXT("Loop"))
            : FText::FromString(TEXT("Play once"));
    }

    auto
        Build_StateText(
            ECk_AudioTrack_State InState)
        -> FText
    {
        switch (InState)
        {
            case ECk_AudioTrack_State::Playing:   return FText::FromString(TEXT("Playing"));
            case ECk_AudioTrack_State::FadingIn:  return FText::FromString(TEXT("Fading in"));
            case ECk_AudioTrack_State::FadingOut: return FText::FromString(TEXT("Fading out"));
            case ECk_AudioTrack_State::Paused:    return FText::FromString(TEXT("Paused"));
            case ECk_AudioTrack_State::Stopped:
            default:                              return FText::FromString(TEXT("Stopped"));
        }
    }

    /** Words, not just a signed number. "-134°" takes a beat to place; "behind-left" does not, and the two together
     *  are unambiguous about which way the sign runs. */
    auto
        Build_BearingText(
            float InBearingDegrees)
        -> FString
    {
        const auto Magnitude = FMath::Abs(InBearingDegrees);
        const auto Side = InBearingDegrees >= 0.0f ? TEXT("right") : TEXT("left");

        if (Magnitude < 22.5f)
        { return FString{TEXT("front")}; }

        if (Magnitude > 157.5f)
        { return FString{TEXT("behind")}; }

        return Magnitude < 67.5f
            ? ck::Format_UE(TEXT("front-{}"), Side)
            : (Magnitude < 112.5f
                ? FString{Side}
                : ck::Format_UE(TEXT("behind-{}"), Side));
    }

    /** The asset's own name, not the package path. A mixer row has one line for this and the leaf is the part that
     *  identifies the sound; the full path stays in the tooltip. */
    auto
        Build_SoundLeaf(
            const FString& InPath)
        -> FString
    {
        if (InPath.IsEmpty())
        { return FString{TEXT("(no sound)")}; }

        auto Leaf = FString{};

        return InPath.Split(TEXT("."), nullptr, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd)
            ? Leaf
            : InPath;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    using namespace ck_audio_debugger_window;

    _StatAudible      = MakeShared<FText>(FText::FromString(TEXT("0")));
    _StatFading       = MakeShared<FText>(FText::FromString(TEXT("0")));
    _StatVirtualized  = MakeShared<FText>(FText::FromString(TEXT("0")));
    _StatConcurrency  = MakeShared<FText>(FText::FromString(TEXT("0")));

    _CrossfadeSeriesA = MakeShared<TArray<float>>();
    _CrossfadeSeriesB = MakeShared<TArray<float>>();

    _SpatialView = MakeShared<FCkAudioDebugger_SpatialView>();

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkAudioDebugger"))
        .ShowRefreshControls(true)
        .StatusText_Lambda([this]()
        {
            const auto& Snapshot = _Collector.Get_Snapshot();

            // "No world" and "a world with no audio" are different statements — collapsing them would show a
            // PIE-less editor as a silent game.
            if (NOT Snapshot.HasWorld)
            { return FText::FromString(TEXT("waiting for a PIE session…")); }

            if (Snapshot.Directors.IsEmpty())
            { return FText::FromString(TEXT("no audio directors in this world")); }

            return FText::FromString(ck::Format_UE(TEXT("{} director(s) · {} track(s)"),
                Snapshot.Directors.Num(), Snapshot.Get_TrackCount()));
        })
        .CommandGroups({
            FCkDebug_CommandGroup::Primary(TEXT("AudioView"), FText::FromString(TEXT("Audio view controls")),
            SNew(SCkDebug_IconToggle)
            .IconId(TEXT("Audio"))
            .Label(FText::FromString(TEXT("Active tracks only")))
            .ToolTip(FText::FromString(
                TEXT("Hide stopped tracks. A director legitimately holds configured-but-stopped tracks, and ")
                TEXT("listing them all buries the ones actually making noise.")))
            .IsOn_Lambda([this]() { return _ShowActiveOnly; })
            .OnStateChanged_Lambda([this](const bool InActiveOnly)
            {
                if (_ShowActiveOnly == InActiveOnly) { return; }
                _ShowActiveOnly = InActiveOnly;

                // Dropping the signature forces the next gated tick through the structure pass — the visible SET
                // just changed, and the value pass writes cells positionally.
                _LastSignature.Reset();
            }))
        })
        .Content()
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                DoCreate_Tabs()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                DoCreate_StatCards()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                DoCreate_FilterRow()
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                // Slot order MUST match ECkAudioDebugger_Page's declaration order — the switcher is driven by the
                // enum's integer value rather than by a lookup, so a reordered enum silently shows the wrong page.
                SAssignNew(_PageSwitcher, SWidgetSwitcher)
                .WidgetIndex_Lambda([this]() { return static_cast<int32>(_ActivePage); })

                + SWidgetSwitcher::Slot()
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    .Padding(CkStyle::SpaceL, CkStyle::SpaceM)
                    [
                        SAssignNew(_DirectorPageBox, SVerticalBox)
                    ]
                ]

                + SWidgetSwitcher::Slot()
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SNew(SScrollBox)
                        + SScrollBox::Slot()
                        .Padding(CkStyle::SpaceL, CkStyle::SpaceS)
                        [
                            SAssignNew(_DirectorBox, SVerticalBox)
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        DoCreate_CrossfadeLane(false)
                    ]
                ]

                + SWidgetSwitcher::Slot()
                [
                    DoCreate_CrossfadeLane(true)
                ]

                + SWidgetSwitcher::Slot()
                [
                    DoCreate_SpatialPage()
                ]

                + SWidgetSwitcher::Slot()
                [
                    DoCreate_EventsPage()
                ]

                + SWidgetSwitcher::Slot()
                [
                    DoCreate_OverlayPage()
                ]
            ]
        ]
    ];

    Register_WithGate();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_Tabs()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    auto Tabs = TArray<FCkDebug_UnderlineTabDesc>{};

    {
        auto Tab = FCkDebug_UnderlineTabDesc{};
        Tab.Id = Get_PageId(ECkAudioDebugger_Page::Directors);
        Tab.Label = FText::FromString(TEXT("Directors"));
        Tab.CountText = TAttribute<FText>::CreateLambda([this]()
        {
            return FText::AsNumber(_Collector.Get_Snapshot().Directors.Num());
        });
        Tabs.Add(MoveTemp(Tab));
    }

    {
        auto Tab = FCkDebug_UnderlineTabDesc{};
        Tab.Id = Get_PageId(ECkAudioDebugger_Page::Tracks);
        Tab.Label = FText::FromString(TEXT("Tracks"));
        Tab.CountText = TAttribute<FText>::CreateLambda([this]()
        {
            return FText::AsNumber(_Collector.Get_Snapshot().Get_TrackCount());
        });

        // The amber dot is the page's own alarm: a virtualized track is playing and inaudible, and the reader has to
        // be able to see that from a tab they are not currently on.
        Tab.ShowWarnDot = TAttribute<bool>::CreateLambda([this]()
        {
            return _Collector.Get_Snapshot().Get_VirtualizedCount() > 0;
        });
        Tabs.Add(MoveTemp(Tab));
    }

    {
        auto Tab = FCkDebug_UnderlineTabDesc{};
        Tab.Id = Get_PageId(ECkAudioDebugger_Page::Crossfade);
        Tab.Label = FText::FromString(TEXT("Crossfade"));
        Tab.CountText = TAttribute<FText>::CreateLambda([this]()
        {
            const auto Fading = _Collector.Get_Snapshot().Get_FadingCount();
            return Fading > 0 ? FText::AsNumber(Fading) : FText::GetEmpty();
        });
        Tabs.Add(MoveTemp(Tab));
    }

    {
        auto Tab = FCkDebug_UnderlineTabDesc{};
        Tab.Id = Get_PageId(ECkAudioDebugger_Page::Spatial);
        Tab.Label = FText::FromString(TEXT("Spatial"));

        // Out-of-range is this page's alarm, exactly as virtualized is the Tracks page's: playing, positioned, and
        // past the last audible metre.
        Tab.ShowWarnDot = TAttribute<bool>::CreateLambda([this]()
        {
            for (const auto& Director : _Collector.Get_Snapshot().Directors)
            {
                for (const auto& Track : Director.Tracks)
                {
                    if (Track.Get_IsOutOfRange())
                    { return true; }
                }
            }

            return false;
        });
        Tabs.Add(MoveTemp(Tab));
    }

    {
        auto Tab = FCkDebug_UnderlineTabDesc{};
        Tab.Id = Get_PageId(ECkAudioDebugger_Page::Events);
        Tab.Label = FText::FromString(TEXT("Events"));
        Tabs.Add(MoveTemp(Tab));
    }

    {
        auto Tab = FCkDebug_UnderlineTabDesc{};
        Tab.Id = Get_PageId(ECkAudioDebugger_Page::Overlay);
        Tab.Label = FText::FromString(TEXT("Overlay"));
        Tabs.Add(MoveTemp(Tab));
    }

    return SNew(SCkDebug_UnderlineTabs)
        .Tabs(Tabs)
        .ActiveTabId_Lambda([this]() { return Get_PageId(_ActivePage); })
        .OnTabSelected_Lambda([this](FName InPageId)
        {
            for (const auto Page : {
                ECkAudioDebugger_Page::Directors,
                ECkAudioDebugger_Page::Tracks,
                ECkAudioDebugger_Page::Crossfade,
                ECkAudioDebugger_Page::Spatial,
                ECkAudioDebugger_Page::Events,
                ECkAudioDebugger_Page::Overlay})
            {
                if (Get_PageId(Page) == InPageId)
                {
                    _ActivePage = Page;
                    return;
                }
            }
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_StatCards()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    const auto MakeCard = [](const FText& InLabel, TSharedPtr<FText> InCell, FLinearColor InValueColor)
        -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_Card)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Font_Static(&Get_CardLabelFont)
                .ColorAndOpacity(CkStyle::TextDim())
                .Text(InLabel)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Font_Static(&Get_CardValueFont)
                .ColorAndOpacity(FSlateColor{InValueColor})
                .Text_Lambda([InCell]() { return *InCell; })
            ]
        ];
    };

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(CkStyle::SpaceL, CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceS)
        [
            MakeCard(FText::FromString(TEXT("Active / max")), _StatConcurrency, CkStyle::Text())
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(CkStyle::SpaceS, CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceS)
        [
            MakeCard(FText::FromString(TEXT("Audible")), _StatAudible, CkStyle::Ok())
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(CkStyle::SpaceS, CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceS)
        [
            MakeCard(FText::FromString(TEXT("Fading")), _StatFading, CkStyle::Warn())
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(CkStyle::SpaceS, CkStyle::SpaceM, CkStyle::SpaceL, CkStyle::SpaceS)
        [
            // Err-toned on purpose: a virtualized track is playing and inaudible, which is a defect far more often
            // than it is intent.
            MakeCard(FText::FromString(TEXT("Virtualized")), _StatVirtualized, CkStyle::Err())
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_FilterRow()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    // Every state toggle drops the signature rather than only flipping its bool: it changes which rows exist, and the
    // value pass writes cells positionally into the rows the structure pass emitted.
    const auto MakeStateToggle = [this](const FText& InLabel, ECk_Tone InTone, bool* InFlag) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_ToggleSurface)
            .IsOn_Lambda([InFlag]() { return *InFlag; })
            .ToolTipText(FText::Format(
                FText::FromString(TEXT("Show {0} tracks")), InLabel))
            .AccessibleText(InLabel)
            .OnStateChanged_Lambda([this, InFlag](const bool InOn)
            {
                if (*InFlag == InOn) { return; }
                *InFlag = InOn;
                _LastSignature.Reset();
            })
            [
                SNew(SCkDebug_StatusPill)
                .Text(InLabel)
                .Tone_Lambda([InFlag, InTone]() { return *InFlag ? InTone : ECk_Tone::Neutral; })
                .ShowDot(false)
            ];
    };

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceL, 0.0f, CkStyle::SpaceM, CkStyle::SpaceM)
        [
            SNew(SCkDebug_SearchBar)
            .HintText(FText::FromString(TEXT("Filter tracks")))
            .OnSearchTextChanged_Lambda([this](const FString& InText)
            {
                _FilterString = InText;
                _HighlightString = InText;
                _LastSignature.Reset();
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceM)
        [
            MakeStateToggle(FText::FromString(TEXT("Playing")), ECk_Tone::Ok, &_ShowPlaying)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceM)
        [
            MakeStateToggle(FText::FromString(TEXT("Fading")), ECk_Tone::Warn, &_ShowFading)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceM, CkStyle::SpaceM)
        [
            MakeStateToggle(FText::FromString(TEXT("Stopped")), ECk_Tone::Accent, &_ShowStopped)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceL, CkStyle::SpaceM)
        [
            SNew(SCkDebug_ToggleSurface)
            .IsOn_Lambda([this]() { return _GroupByDirector; })
            .ToolTipText(FText::FromString(
                TEXT("Group rows under their director. Off flattens every track into one list, which is what you ")
                TEXT("want when comparing volumes across directors.")))
            .AccessibleText(FText::FromString(TEXT("Group by director")))
            .OnStateChanged_Lambda([this](const bool InOn)
            {
                if (_GroupByDirector == InOn) { return; }
                _GroupByDirector = InOn;
                _LastSignature.Reset();
            })
            [
                SNew(STextBlock)
                .Font_Static(&Get_MicroFont)
                .ColorAndOpacity(CkStyle::TextDim())
                .Text(FText::FromString(TEXT("Group by director")))
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_CrossfadeLane(
        bool InIsDedicatedPage)
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    const auto Height = InIsDedicatedPage ? k_LanePageHeight : k_LaneHeight;

    auto Lane =
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Bottom)
            [
                SNew(STextBlock)
                .Font_Static(&Get_RowFont)
                .ColorAndOpacity(CkStyle::Text())
                .Text(FText::FromString(TEXT("Crossfade lane")))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Bottom)
            .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Font_Static(&Get_MicroFont)
                .ColorAndOpacity(CkStyle::TextDim())
                .Text(FText::FromString(TEXT("recent history of _CurrentVolume")))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBox)
            .HeightOverride(Height)
            [
                // Two series in ONE widget: the primary line and the band line. The point of the lane is that a real
                // crossfade is two curves CROSSING, which two stacked sparklines could never show.
                SNew(SCkDebug_Sparkline)
                .Samples(_CrossfadeSeriesA)
                .BandSamples(_CrossfadeSeriesB)
                .Color(CkStyle::Ok())
                .BandColor(CkStyle::Warn())
                .BandFillOpacity(0.0f)
                .DesiredSize(FVector2D{320.0f, Height})
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
        [
            SAssignNew(_CrossfadeLegendText, STextBlock)
            .Font_Static(&Get_MicroFont)
            .ColorAndOpacity(CkStyle::TextDim())
            .Text(FText::FromString(TEXT("(no fades recorded yet)")))
        ];

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
            .Thickness(1.0f)
            .ColorAndOpacity(FSlateColor{CkStyle::Border()})
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceL, CkStyle::SpaceM)
        [
            Lane
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_SpatialPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    const auto MakeStatRow = [](const FText& InLabel, TFunction<FText()> InValue, bool InEmphasise)
        -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font_Static(InEmphasise ? &Get_RowFont : &Get_MicroFont)
                .ColorAndOpacity(InEmphasise ? CkStyle::Text() : CkStyle::TextDim())
                .Text(InLabel)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font_Static(&Get_MonoFont)
                .ColorAndOpacity(InEmphasise ? CkStyle::Text() : CkStyle::TextDim())
                .Text_Lambda(MoveTemp(InValue))
            ];
    };

    const auto View = _SpatialView;

    return SNew(SVerticalBox)

        // ---- Selector: which track the radar is about ----
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceL, CkStyle::SpaceM, CkStyle::SpaceL, CkStyle::SpaceS)
        [
            SNew(SScrollBox)
            .Orientation(Orient_Horizontal)
            + SScrollBox::Slot()
            [
                SAssignNew(_SpatialSelectorBox, SHorizontalBox)
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            .Padding(CkStyle::SpaceL, CkStyle::SpaceS)
            [
                SNew(SVerticalBox)

                // Shown INSTEAD of an empty radar. A blank picture would read as "nothing is near you", which is a
                // claim about the world; "this track is not spatialized" is a claim about the track.
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, CkStyle::SpaceXL)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_MicroFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                    .AutoWrapText(true)
                    .Visibility_Lambda([View]()
                    {
                        return View.IsValid() && View->HasSpatialData
                            ? EVisibility::Collapsed
                            : EVisibility::Visible;
                    })
                    .Text_Lambda([View]()
                    {
                        if (NOT View.IsValid() || NOT View->HasSelection)
                        {
                            return FText::FromString(TEXT(
                                "No track to inspect. The Spatial page needs a track with a live audio component in "
                                "a running PIE session."));
                        }

                        return FText::FromString(ck::Format_UE(TEXT(
                            "'{}' has no spatial data: it is either 2D (no attenuation settings resolved) or its "
                            "pooled audio component has already been released. Neither has a position to plot."),
                            View->TrackName));
                    })
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    .Visibility_Lambda([View]()
                    {
                        return View.IsValid() && View->HasSpatialData
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                    })

                    // ---- Radar ----
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SBox)
                            .WidthOverride(k_RadarSize)
                            .HeightOverride(k_RadarSize)
                            [
                                SNew(SCkAudioDebugger_Radar)
                                .View(_SpatialView)
                            ]
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Center)
                        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                            .Font_Static(&Get_MicroFont)
                            .ColorAndOpacity(CkStyle::TextMute())
                            .Text_Lambda([View]()
                            {
                                if (NOT View.IsValid())
                                { return FText::GetEmpty(); }

                                return FText::FromString(ck::Format_UE(TEXT("inner {}m  ·  falloff {}m"),
                                    FString::SanitizeFloat(View->InnerRadiusCm / 100.0f, 1),
                                    FString::SanitizeFloat(View->MaxFalloffCm / 100.0f, 1)));
                            })
                        ]
                    ]

                    // ---- Curve + the arithmetic ----
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(CkStyle::SpaceXL, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Font_Static(&Get_MicroFont)
                            .ColorAndOpacity(CkStyle::TextDim())
                            .Text_Lambda([View]()
                            {
                                if (NOT View.IsValid())
                                { return FText::GetEmpty(); }

                                return FText::FromString(ck::Format_UE(TEXT("Why the audible volume is {}"),
                                    FString::SanitizeFloat(View->AudibleVolume, 2)));
                            })
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                        [
                            SNew(SBox)
                            .HeightOverride(k_CurveHeight)
                            [
                                SNew(SCkAudioDebugger_FalloffCurve)
                                .View(_SpatialView)
                            ]
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
                        [
                            SNew(SVerticalBox)

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, CkStyle::SpaceXS)
                            [
                                MakeStatRow(FText::FromString(TEXT("Distance")), [View]()
                                {
                                    return View.IsValid()
                                        ? FText::FromString(ck::Format_UE(TEXT("{} m"),
                                            FString::SanitizeFloat(View->DistanceCm / 100.0f, 1)))
                                        : FText::GetEmpty();
                                }, false)
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, CkStyle::SpaceXS)
                            [
                                MakeStatRow(FText::FromString(TEXT("Bearing")), [View]()
                                {
                                    if (NOT View.IsValid())
                                    { return FText::GetEmpty(); }

                                    return FText::FromString(ck::Format_UE(TEXT("{}°  {}"),
                                        FMath::RoundToInt(View->BearingDegrees),
                                        Build_BearingText(View->BearingDegrees)));
                                }, false)
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, CkStyle::SpaceXS)
                            [
                                MakeStatRow(FText::FromString(TEXT("Attenuation gain")), [View]()
                                {
                                    if (NOT View.IsValid())
                                    { return FText::GetEmpty(); }

                                    return FText::FromString(View->IsAttenuated
                                        ? FString::SanitizeFloat(View->AttenuationGain, 2)
                                        : FString{TEXT("n/a (not attenuated)")});
                                }, false)
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, CkStyle::SpaceXS)
                            [
                                MakeStatRow(FText::FromString(TEXT("Track volume")), [View]()
                                {
                                    return View.IsValid()
                                        ? FText::FromString(FString::SanitizeFloat(View->TrackVolume, 2))
                                        : FText::GetEmpty();
                                }, false)
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                            [
                                SNew(SSeparator)
                                .Thickness(1.0f)
                                .ColorAndOpacity(FSlateColor{CkStyle::Border()})
                            ]

                            // The product, stated as its own row. A reader who takes only one number off this page
                            // must take this one — it is the only one that says whether the sound is heard.
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, CkStyle::SpaceS)
                            [
                                MakeStatRow(FText::FromString(TEXT("Audible")), [View]()
                                {
                                    return View.IsValid()
                                        ? FText::FromString(FString::SanitizeFloat(View->AudibleVolume, 2))
                                        : FText::GetEmpty();
                                }, true)
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, CkStyle::SpaceXS)
                            [
                                MakeStatRow(FText::FromString(TEXT("Attenuation asset")), [View]()
                                {
                                    if (NOT View.IsValid())
                                    { return FText::GetEmpty(); }

                                    return FText::FromString(View->AttenuationAssetName.IsEmpty()
                                        ? FString{TEXT("(none)")}
                                        : View->AttenuationAssetName);
                                }, false)
                            ]
                        ]
                    ]
                ]

                // ---- The out-of-range / virtualized call-out ----
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, CkStyle::SpaceL, 0.0f, 0.0f)
                [
                    SNew(SCkDebug_AlertRow)
                    .Tone(ECk_Tone::Err)
                    .Glyph(FText::FromString(TEXT("!")))
                    .LeadText(FText::FromString(TEXT("Inaudible")))
                    .BodyText_Lambda([View]()
                    {
                        if (NOT View.IsValid())
                        { return FText::GetEmpty(); }

                        if (View->IsVirtualized)
                        {
                            return FText::FromString(ck::Format_UE(TEXT(
                                "{} is virtualized — playing at {} and not mixed at all."),
                                View->TrackName, FString::SanitizeFloat(View->TrackVolume, 2)));
                        }

                        return FText::FromString(ck::Format_UE(TEXT(
                            "{} is {} m away — outside its {} m falloff. Playing at {}, audible {}."),
                            View->TrackName,
                            FString::SanitizeFloat(View->DistanceCm / 100.0f, 1),
                            FString::SanitizeFloat(View->MaxFalloffCm / 100.0f, 1),
                            FString::SanitizeFloat(View->TrackVolume, 2),
                            FString::SanitizeFloat(View->AudibleVolume, 2)));
                    })
                    .Visibility_Lambda([View]()
                    {
                        return View.IsValid() && View->HasSpatialData && (View->IsOutOfRange || View->IsVirtualized)
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                    })
                ]
            ]
        ]

        // ---- Which listener every number above was measured from ----
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
            .Thickness(1.0f)
            .ColorAndOpacity(FSlateColor{CkStyle::Border()})
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceL, CkStyle::SpaceM)
        [
            SNew(STextBlock)
            .Font_Static(&Get_MicroFont)
            .ColorAndOpacity(CkStyle::TextMute())
            .Text_Lambda([this]()
            {
                const auto& Snapshot = _Collector.Get_Snapshot();

                return FText::FromString(Snapshot.HasListener
                    ? ck::Format_UE(TEXT("listener: {}"), Snapshot.ListenerSource)
                    : FString{TEXT("no listener — distances cannot be computed")});
            })
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_EventsPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    const auto MakeKindToggle = [this](const FText& InLabel, ECk_Tone InTone, bool* InFlag) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_ToggleSurface)
            .IsOn_Lambda([InFlag]() { return *InFlag; })
            .AccessibleText(InLabel)
            .ToolTipText(FText::Format(FText::FromString(TEXT("Log {0} events")), InLabel))
            .OnStateChanged_Lambda([InFlag](const bool InOn) { *InFlag = InOn; })
            [
                SNew(SCkDebug_StatusPill)
                .Text(InLabel)
                .Tone_Lambda([InFlag, InTone]() { return *InFlag ? InTone : ECk_Tone::Neutral; })
                .ShowDot(false)
            ];
    };

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceL, CkStyle::SpaceM, CkStyle::SpaceL, CkStyle::SpaceS)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                MakeKindToggle(FText::FromString(TEXT("State")), ECk_Tone::Ok, &_EventsShowStateChanges)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                MakeKindToggle(FText::FromString(TEXT("Fades")), ECk_Tone::Warn, &_EventsShowFades)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                MakeKindToggle(FText::FromString(TEXT("Virtualization")), ECk_Tone::Err, &_EventsShowVirtualization)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                MakeKindToggle(FText::FromString(TEXT("Lifecycle")), ECk_Tone::Accent, &_EventsShowLifecycle)
            ]

            // Said on the page, not buried in a header comment. The log is derived by diffing successive refreshes
            // rather than by binding CkAudio's signals — no debugger in this suite binds into a live world — so a
            // transition that begins and ends between two ticks of the gate is genuinely not recorded. A reader
            // hunting a one-frame blip has to know that before concluding it never happened.
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Right)
            [
                SNew(STextBlock)
                .Font_Static(&Get_MicroFont)
                .ColorAndOpacity(CkStyle::TextMute())
                .Text(FText::FromString(TEXT("sampled at the refresh rate — sub-tick transitions are not captured")))
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(CkStyle::SpaceL, 0.0f, CkStyle::SpaceL, CkStyle::SpaceM)
        [
            SAssignNew(_EventLog, SCkDebug_EventLog)
            .MaxEntries(k_EventLogCapacity)
            .EmptyText(FText::FromString(TEXT("Nothing has changed since this window opened.")))
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_OverlayPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    return SNew(SVerticalBox)

        // This page WRITES, and it is the only one that does. Saying so on the page is the point: everything else in
        // this window observes, and a reader flipping a switch here is changing the running game, not the view of it.
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceL, CkStyle::SpaceM, CkStyle::SpaceL, CkStyle::SpaceS)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_ToggleSurface)
                .IsOn_Lambda([]() { return false; })
                .AccessibleText(FText::FromString(TEXT("Draw all tracks")))
                .ToolTipText(FText::FromString(
                    TEXT("Enable the in-world debug draw for every track in every director.")))
                .OnStateChanged_Lambda([this](const bool) { DoSet_DebugDrawOnAll(true); })
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_MicroFont)
                    .ColorAndOpacity(CkStyle::TextDim())
                    .Text(FText::FromString(TEXT("Draw all")))
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_ToggleSurface)
                .IsOn_Lambda([]() { return false; })
                .AccessibleText(FText::FromString(TEXT("Draw none")))
                .ToolTipText(FText::FromString(TEXT("Disable the in-world debug draw for every track.")))
                .OnStateChanged_Lambda([this](const bool) { DoSet_DebugDrawOnAll(false); })
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_MicroFont)
                    .ColorAndOpacity(CkStyle::TextDim())
                    .Text(FText::FromString(TEXT("Draw none")))
                ]
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Right)
            [
                SNew(STextBlock)
                .Font_Static(&Get_MicroFont)
                .ColorAndOpacity(CkStyle::Warn())
                .Text(FText::FromString(TEXT("writes to the running world")))
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            .Padding(CkStyle::SpaceL, CkStyle::SpaceS)
            [
                SAssignNew(_OverlayListBox, SVerticalBox)
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    // MUST be the WindowBase super, not SCompoundWidget — the base Tick drives the gated style-revision watch that
    // routes into OnStyleRevisionChanged.
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _Collector.Collect(DoGet_PieWorld());

    if (const auto Signature = DoBuild_Signature();
        Signature != _LastSignature)
    {
        _LastSignature = Signature;
        DoRebuild_Structure();
    }

    DoRecord_VolumeHistory();
    DoUpdate_LiveValues();

    // Always, regardless of which page is showing: the Events log is a HISTORY, and one that only advanced while its
    // own tab was open would silently omit everything that happened while the reader was on the mixer — which is
    // exactly when they were looking away and most need the record.
    DoRecord_Events();

    DoUpdate_SpatialView();

    // The two list-bearing pages rebuild on their own signatures rather than the mixer's: their contents are the
    // unfiltered track set, so the mixer's filters and state toggles must not empty them.
    const auto AllTracks = DoBuild_AllTracksSignature();

    if (auto SpatialSignature = ck::Format_UE(TEXT("{}|{}"), AllTracks, _SelectedSpatialTrack);
        SpatialSignature != _SpatialSignature)
    {
        _SpatialSignature = MoveTemp(SpatialSignature);
        DoRebuild_SpatialSelector();
    }

    if (AllTracks != _OverlaySignature)
    {
        _OverlaySignature = AllTracks;
        DoRebuild_OverlayList();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    // Force the next tick through the structure pass so the rows pick the new palette up; the cells themselves carry
    // no style.
    _LastSignature.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoGet_VisibleTracks(
        const FCkAudioDebugger_DirectorInfo& InDirector) const
    -> TArray<const FCkAudioDebugger_TrackInfo*>
{
    auto Visible = TArray<const FCkAudioDebugger_TrackInfo*>{};

    for (const auto& Track : InDirector.Tracks)
    {
        if (_ShowActiveOnly && Track.State == ECk_AudioTrack_State::Stopped)
        { continue; }

        if (NOT DoPassesStateFilter(Track))
        { continue; }

        if (NOT DoPassesFilter(Track.TrackName) && NOT DoPassesFilter(InDirector.DirectorName))
        { continue; }

        Visible.Add(&Track);
    }

    return Visible;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoBuild_Signature() const
    -> FString
{
    const auto& Snapshot = _Collector.Get_Snapshot();

    auto Signature = FString{};

    for (const auto& Director : Snapshot.Directors)
    {
        const auto Visible = DoGet_VisibleTracks(Director);

        if (Visible.IsEmpty())
        { continue; }

        Signature += ck::Format_UE(TEXT("D:{}|"), Director.DirectorName);

        for (const auto* Track : Visible)
        {
            // The track's NAME and nothing that moves. Folding in volume or playback percent would change the
            // signature on nearly every tick of every fade and rebuild the tree instead of writing cells — the exact
            // flicker the structure/value split exists to avoid.
            Signature += ck::Format_UE(TEXT("T:{}|"), Track->TrackName);
        }
    }

    return Signature;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoBuild_AllTracksSignature() const
    -> FString
{
    auto Signature = FString{};

    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        Signature += ck::Format_UE(TEXT("D:{}|"), Director.DirectorName);

        for (const auto& Track : Director.Tracks)
        {
            // State is folded in, unlike the mixer's signature: the Spatial selector only lists non-stopped tracks,
            // so a track starting or stopping genuinely changes that list even though the SET is unchanged.
            Signature += ck::Format_UE(TEXT("T:{}:{}|"),
                Track.TrackName, static_cast<int32>(Track.State));
        }
    }

    return Signature;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoRebuild_Structure()
    -> void
{
    if (NOT _DirectorBox.IsValid() || NOT _DirectorPageBox.IsValid())
    { return; }

    _DirectorBox->ClearChildren();
    _DirectorPageBox->ClearChildren();
    _TrackSlots.Reset();
    _DirectorSlots.Reset();

    const auto& Snapshot = _Collector.Get_Snapshot();

    for (const auto& Director : Snapshot.Directors)
    {
        const auto Visible = DoGet_VisibleTracks(Director);

        // A director whose every track was filtered out is dropped WITH its header: a heading over nothing tells the
        // reader the filter failed rather than that it worked.
        if (Visible.IsEmpty())
        { continue; }

        auto DirectorSlot = FCkAudioDebugger_DirectorSlot{};

        auto Header = DoMake_DirectorHeader(Director, DirectorSlot);

        if (_GroupByDirector)
        {
            _DirectorBox->AddSlot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceM, 0.0f, CkStyle::SpaceXS)
            [
                Header
            ];
        }

        _DirectorSlots.Add(DirectorSlot);

        for (const auto* Track : Visible)
        {
            auto TrackSlot = FCkAudioDebugger_TrackSlot{};

            _DirectorBox->AddSlot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceXS)
            [
                DoMake_TrackRow(*Track, TrackSlot)
            ];

            _TrackSlots.Add(TrackSlot);
        }
    }

    // The Directors page is the same headers without the rows — the concurrency/policy view, scoped to one line each.
    for (const auto& Director : Snapshot.Directors)
    {
        if (NOT DoPassesFilter(Director.DirectorName))
        { continue; }

        auto Unused = FCkAudioDebugger_DirectorSlot{};

        _DirectorPageBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceXS)
        [
            SNew(SCkDebug_Card)
            [
                DoMake_DirectorHeader(Director, Unused)
            ]
        ];
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoMake_DirectorHeader(
        const FCkAudioDebugger_DirectorInfo& InDirector,
        FCkAudioDebugger_DirectorSlot& OutSlot) const
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    // The policy is stated on the header because it is what decides what happens to the rows underneath it.
    auto Policy = TArray<FString>{};

    Policy.Add(InDirector.DefaultCrossfadeSeconds.IsSet()
        ? ck::Format_UE(TEXT("crossfade {}s"),
            FString::SanitizeFloat(InDirector.DefaultCrossfadeSeconds.GetValue(), 1))
        : FString{TEXT("no default crossfade")});

    Policy.Add(ck::Format_UE(TEXT("same-priority: {}"),
        InDirector.SamePriorityBehavior == ECk_SamePriorityBehavior::Allow
            ? TEXT("allow")
            : TEXT("block")));

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCkDebug_Icon)
            .Brush(FCkDebuggerCommonStyle::Get_IconBrush(FName{TEXT("Audio")}))
            .Meaning(FText::FromString(TEXT("Audio director — owns a concurrency budget and the tracks under it")))
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Font_Static(&Get_RowFont)
            .ColorAndOpacity(CkStyle::Text())
            .Text(FText::FromString(InDirector.DirectorName))
            .HighlightText_Lambda([this]() { return FText::FromString(_HighlightString); })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_EntityRef)
            .Entity(InDirector.DirectorEntity)
            .Tooltip(FText::FromString(TEXT("Open this director in the CK ECS Debugger")))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [
            SAssignNew(OutSlot.ActiveText, STextBlock)
            .Font_Static(&Get_MicroFont)
            .ColorAndOpacity(CkStyle::TextDim())
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        .HAlign(HAlign_Right)
        [
            SNew(STextBlock)
            .Font_Static(&Get_MicroFont)
            .ColorAndOpacity(CkStyle::TextMute())
            .Text(FText::FromString(FString::Join(Policy, TEXT("  ·  "))))
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoMake_TrackRow(
        const FCkAudioDebugger_TrackInfo& InTrack,
        FCkAudioDebugger_TrackSlot& OutSlot) const
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    OutSlot.VolumeFraction = MakeShared<float>(0.0f);
    OutSlot.TargetFraction = MakeShared<TOptional<float>>();
    OutSlot.VolumeColor    = MakeShared<FLinearColor>(CkStyle::TextMute());
    OutSlot.StateTone      = MakeShared<ECk_Tone>(ECk_Tone::Neutral);
    OutSlot.StateLabel     = MakeShared<FText>(FText::GetEmpty());
    OutSlot.IsVirtualized  = MakeShared<bool>(false);

    const auto VolumeFraction = OutSlot.VolumeFraction;
    const auto TargetFraction = OutSlot.TargetFraction;
    const auto VolumeColor    = OutSlot.VolumeColor;
    const auto StateTone      = OutSlot.StateTone;
    const auto StateLabel     = OutSlot.StateLabel;
    const auto IsVirtualized  = OutSlot.IsVirtualized;

    const auto SoundPath = InTrack.SoundPath;

    auto Chips =
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_Chip)
            .Text(FText::FromString(TEXT("Virtualized")))
            .Kind(ECkDebug_ChipKind::Unsatisfied)
            .ShowDot(false)
            .Visibility_Lambda([IsVirtualized]()
            {
                return *IsVirtualized ? EVisibility::Visible : EVisibility::Collapsed;
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_Chip)
            .Text(FText::FromString(ck::Format_UE(TEXT("p{}"), InTrack.Priority)))
            .ShowDot(false)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_Chip)
            .Text(Build_LoopText(InTrack.LoopBehavior))
            .ShowDot(false)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_Chip)
            .Text(Build_OverrideText(InTrack.OverrideBehavior))
            .ShowDot(false)
        ];

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                // Fixed column, pill centred in it: the pill sizes to its own word ("Playing" vs "Fading out") while
                // the column keeps every track NAME on the same x — which is what makes the list scannable.
                SNew(SBox)
                .WidthOverride(k_StatePillWidth)
                .HAlign(HAlign_Center)
                [
                    SNew(SCkDebug_StatusPill)
                    .Text_Lambda([StateLabel]() { return *StateLabel; })
                    .Tone_Lambda([StateTone]() { return *StateTone; })
                    .ShowDot(false)
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Font_Static(&Get_RowFont)
                .ColorAndOpacity(CkStyle::Text())
                .Text(FText::FromString(InTrack.TrackName))
                .HighlightText_Lambda([this]() { return FText::FromString(_HighlightString); })
            ]

            // The one elastic column, and the one that may be cut: the sound is identified by its leaf, and the full
            // path is a hover away. Everything else on the line is fixed-width so the numbers below stay in a column.
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(STextBlock)
                .Font_Static(&Get_MicroFont)
                .ColorAndOpacity(CkStyle::TextMute())
                .Text(FText::FromString(Build_SoundLeaf(SoundPath)))
                .ToolTipText(FText::FromString(SoundPath.IsEmpty()
                    ? FString{TEXT("This track has no sound asset assigned.")}
                    : SoundPath))
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                Chips
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)

            // Volume against a FIXED 0..1 ceiling, never normalised against the loudest track. A track's volume is an
            // absolute gain, so a relative scale would make one quiet track alone in a director read as full blast.
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(SBox)
                .HeightOverride(k_MeterHeight)
                [
                    SNew(SCkDebug_MeterBar)
                    .Fraction_Lambda([VolumeFraction]() { return *VolumeFraction; })
                    .FillColor_Lambda([VolumeColor]() { return *VolumeColor; })
                    .TargetFraction_Lambda([TargetFraction]() { return *TargetFraction; })
                    .TargetColor(CkStyle::Text())
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(k_VolumeTextWidth)
                [
                    SAssignNew(OutSlot.VolumeText, STextBlock)
                    .Font_Static(&Get_MonoFont)
                    .ColorAndOpacity(CkStyle::TextDim())
                    .Justification(ETextJustify::Right)
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(k_FadeTextWidth)
                [
                    SAssignNew(OutSlot.FadeText, STextBlock)
                    .Font_Static(&Get_MicroFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                    .Justification(ETextJustify::Right)
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)
            .Visibility_Lambda([IsVirtualized]()
            {
                return *IsVirtualized ? EVisibility::Visible : EVisibility::Collapsed;
            })

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(FCkDebuggerCommonStyle::Get_IconBrush(
                    ck::debug_axes::Get_ToneIconId(ECk_Tone::Err)))
                .Meaning(FText::FromString(TEXT("This track is playing and cannot be heard")))
                .ColorAndOpacity(FSlateColor{CkStyle::Err()})
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SAssignNew(OutSlot.AlertText, STextBlock)
                .Font_Static(&Get_MicroFont)
                .ColorAndOpacity(CkStyle::Err())
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoRecord_VolumeHistory()
    -> void
{
    using namespace ck_audio_debugger_window;

    const auto& Snapshot = _Collector.Get_Snapshot();

    auto Live = TSet<FString>{};

    for (const auto& Director : Snapshot.Directors)
    {
        for (const auto& Track : Director.Tracks)
        {
            Live.Add(Track.TrackName);

            auto& Ring = _VolumeHistory.FindOrAdd(Track.TrackName);

            if (NOT Ring.IsValid())
            { Ring = MakeShared<TArray<float>>(); }

            Ring->Add(Track.CurrentVolume);

            if (Ring->Num() > k_HistorySamples)
            { Ring->RemoveAt(0, Ring->Num() - k_HistorySamples, EAllowShrinking::No); }
        }
    }

    // A track that stopped existing must lose its ring, or the lane keeps drawing a curve for something that is gone
    // and the map grows for the lifetime of the session.
    for (auto It = _VolumeHistory.CreateIterator(); It; ++It)
    {
        if (NOT Live.Contains(It.Key()))
        { It.RemoveCurrent(); }
    }

    // Pick the two series worth drawing: fading tracks first (a crossfade is the thing this lane exists for), then
    // the loudest. Ties break on name so the pair does not swap between refreshes.
    auto Candidates = TArray<const FCkAudioDebugger_TrackInfo*>{};

    for (const auto& Director : Snapshot.Directors)
    {
        for (const auto& Track : Director.Tracks)
        {
            if (Track.State == ECk_AudioTrack_State::Stopped)
            { continue; }

            Candidates.Add(&Track);
        }
    }

    Candidates.Sort([](const FCkAudioDebugger_TrackInfo& InLhs, const FCkAudioDebugger_TrackInfo& InRhs)
    {
        const auto LhsFading = InLhs.State == ECk_AudioTrack_State::FadingIn
            || InLhs.State == ECk_AudioTrack_State::FadingOut;

        const auto RhsFading = InRhs.State == ECk_AudioTrack_State::FadingIn
            || InRhs.State == ECk_AudioTrack_State::FadingOut;

        if (LhsFading != RhsFading)
        { return LhsFading; }

        if (NOT FMath::IsNearlyEqual(InLhs.CurrentVolume, InRhs.CurrentVolume))
        { return InLhs.CurrentVolume > InRhs.CurrentVolume; }

        return InLhs.TrackName.Compare(InRhs.TrackName, ESearchCase::IgnoreCase) < 0;
    });

    const auto CopyInto = [this](TSharedPtr<TArray<float>> InTarget, int32 InIndex,
                                 const TArray<const FCkAudioDebugger_TrackInfo*>& InCandidates) -> FString
    {
        if (NOT InTarget.IsValid())
        { return FString{}; }

        if (NOT InCandidates.IsValidIndex(InIndex))
        {
            InTarget->Reset();
            return FString{};
        }

        const auto& Name = InCandidates[InIndex]->TrackName;

        if (const auto* Ring = _VolumeHistory.Find(Name);
            Ring != nullptr && Ring->IsValid())
        { *InTarget = **Ring; }

        return Name;
    };

    _CrossfadeSeriesNames.Reset();

    if (const auto NameA = CopyInto(_CrossfadeSeriesA, 0, Candidates);
        NOT NameA.IsEmpty())
    { _CrossfadeSeriesNames.Add(NameA); }

    if (const auto NameB = CopyInto(_CrossfadeSeriesB, 1, Candidates);
        NOT NameB.IsEmpty())
    { _CrossfadeSeriesNames.Add(NameB); }

    if (_CrossfadeLegendText.IsValid())
    {
        _CrossfadeLegendText->SetText(FText::FromString(_CrossfadeSeriesNames.IsEmpty()
            ? FString{TEXT("(nothing playing)")}
            : FString::Join(_CrossfadeSeriesNames, TEXT("   ·   "))));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoUpdate_LiveValues()
    -> void
{
    const auto& Snapshot = _Collector.Get_Snapshot();

    auto ActiveTotal = 0;
    auto MaxTotal = 0;

    for (const auto& Director : Snapshot.Directors)
    {
        ActiveTotal += Director.Get_ActiveTrackCount();
        MaxTotal += Director.MaxConcurrentTracks;
    }

    *_StatConcurrency = FText::FromString(ck::Format_UE(TEXT("{} / {}"), ActiveTotal, MaxTotal));
    *_StatAudible     = FText::AsNumber(Snapshot.Get_AudibleCount());
    *_StatFading      = FText::AsNumber(Snapshot.Get_FadingCount());
    *_StatVirtualized = FText::AsNumber(Snapshot.Get_VirtualizedCount());

    auto DirectorIndex = 0;
    auto TrackIndex = 0;

    for (const auto& Director : Snapshot.Directors)
    {
        const auto Visible = DoGet_VisibleTracks(Director);

        if (Visible.IsEmpty())
        { continue; }

        if (_DirectorSlots.IsValidIndex(DirectorIndex))
        {
            if (const auto& Slot = _DirectorSlots[DirectorIndex];
                Slot.ActiveText.IsValid())
            {
                Slot.ActiveText->SetText(FText::FromString(Director.MaxConcurrentTracks > 0
                    ? ck::Format_UE(TEXT("{} / {} active"),
                        Director.Get_ActiveTrackCount(), Director.MaxConcurrentTracks)
                    : ck::Format_UE(TEXT("{} active"), Director.Get_ActiveTrackCount())));
            }
        }

        ++DirectorIndex;

        for (const auto* Track : Visible)
        {
            if (NOT _TrackSlots.IsValidIndex(TrackIndex))
            {
                ++TrackIndex;
                continue;
            }

            const auto& Slot = _TrackSlots[TrackIndex];
            const auto Tone = DoGet_StateTone(*Track);

            const auto IsFading = Track->State == ECk_AudioTrack_State::FadingIn
                || Track->State == ECk_AudioTrack_State::FadingOut;

            if (Slot.VolumeFraction.IsValid())
            { *Slot.VolumeFraction = FMath::Clamp(Track->CurrentVolume, 0.0f, 1.0f); }

            if (Slot.TargetFraction.IsValid())
            {
                // The marker is drawn only while the track is HEADED somewhere. A rule sitting exactly under the end
                // of a settled bar reads as a second meaning the row does not have.
                *Slot.TargetFraction = IsFading
                    ? TOptional<float>{FMath::Clamp(Track->TargetVolume, 0.0f, 1.0f)}
                    : TOptional<float>{};
            }

            if (Slot.VolumeColor.IsValid())
            { *Slot.VolumeColor = CkStyle::GetToneColor(Tone); }

            if (Slot.StateTone.IsValid())
            { *Slot.StateTone = Tone; }

            if (Slot.StateLabel.IsValid())
            { *Slot.StateLabel = DoBuild_StateText(*Track); }

            if (Slot.IsVirtualized.IsValid())
            { *Slot.IsVirtualized = Track->IsVirtualized; }

            if (Slot.VolumeText.IsValid())
            {
                // Current AND target, always both. The pair is the whole point: 0.42 alone says nothing about
                // whether the track is settled there or on its way somewhere else.
                Slot.VolumeText->SetText(FText::FromString(ck::Format_UE(TEXT("{} {} {}"),
                    FString::SanitizeFloat(Track->CurrentVolume, 2),
                    IsFading ? TEXT("→") : TEXT("="),
                    FString::SanitizeFloat(Track->TargetVolume, 2))));
            }

            if (Slot.FadeText.IsValid())
            { Slot.FadeText->SetText(FText::FromString(DoBuild_FadeText(*Track))); }

            if (Slot.AlertText.IsValid())
            { Slot.AlertText->SetText(FText::FromString(DoBuild_AlertText(*Track))); }

            ++TrackIndex;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    TryGet_SelectedSpatialTrack() const
    -> const FCkAudioDebugger_TrackInfo*
{
    const auto& Snapshot = _Collector.Get_Snapshot();

    const FCkAudioDebugger_TrackInfo* Best = nullptr;

    // Ranked by how much the reader is likely to have come here for it, not by volume alone: a virtualized track and
    // an out-of-range one are both "playing and silent", which is the question this page answers.
    const auto Score = [](const FCkAudioDebugger_TrackInfo& InTrack) -> int32
    {
        if (InTrack.IsVirtualized)      { return 3; }
        if (InTrack.Get_IsOutOfRange()) { return 2; }
        if (InTrack.State != ECk_AudioTrack_State::Stopped) { return 1; }
        return 0;
    };

    for (const auto& Director : Snapshot.Directors)
    {
        for (const auto& Track : Director.Tracks)
        {
            if (NOT _SelectedSpatialTrack.IsEmpty())
            {
                if (Track.TrackName == _SelectedSpatialTrack)
                { return &Track; }

                continue;
            }

            if (NOT Track.HasSpatialData)
            { continue; }

            if (Best == nullptr)
            {
                Best = &Track;
                continue;
            }

            if (const auto TrackScore = Score(Track);
                TrackScore > Score(*Best)
                || (TrackScore == Score(*Best) && Track.CurrentVolume > Best->CurrentVolume))
            { Best = &Track; }
        }
    }

    return Best;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoUpdate_SpatialView()
    -> void
{
    using namespace ck_audio_debugger_window;

    if (NOT _SpatialView.IsValid())
    { return; }

    auto& View = *_SpatialView;

    View = FCkAudioDebugger_SpatialView{};

    const auto& Snapshot = _Collector.Get_Snapshot();

    View.ListenerSource = Snapshot.ListenerSource;

    const auto* Selected = TryGet_SelectedSpatialTrack();

    if (Selected == nullptr)
    { return; }

    View.HasSelection = true;
    View.TrackName = Selected->TrackName;
    View.TrackVolume = Selected->CurrentVolume;
    View.IsVirtualized = Selected->IsVirtualized;

    if (NOT Selected->HasSpatialData)
    { return; }

    View.HasSpatialData = true;
    View.AttenuationAssetName = Selected->AttenuationAssetName;
    View.DistanceCm = Selected->DistanceToListener;
    View.InnerRadiusCm = Selected->InnerRadius;
    View.FalloffCm = Selected->FalloffDistance;
    View.MaxFalloffCm = Selected->MaxFalloffDistance;
    View.BearingDegrees = Selected->BearingDegrees;
    View.AttenuationGain = Selected->AttenuationGain;
    View.AudibleVolume = Selected->Get_AudibleVolume();
    View.IsAttenuated = Selected->IsAttenuated;
    View.IsSpatialized = Selected->IsSpatialized;
    View.IsOutOfRange = Selected->Get_IsOutOfRange();
    View.FalloffCurve = Selected->FalloffCurve;

    // The range must cover the falloff ring AND the selected track, or the picture would crop the very thing it was
    // opened to explain.
    View.RadarRangeCm = FMath::Max3(
        k_RadarMinRangeCm,
        Selected->MaxFalloffDistance * k_RadarRangePadding,
        Selected->DistanceToListener * k_RadarRangePadding);

    for (const auto& Director : Snapshot.Directors)
    {
        for (const auto& Track : Director.Tracks)
        {
            if (NOT Track.HasSpatialData || Track.State == ECk_AudioTrack_State::Stopped)
            { continue; }

            auto Blip = FCkAudioDebugger_SpatialBlip{};

            Blip.TrackName = Track.TrackName;
            Blip.BearingDegrees = Track.BearingDegrees;
            Blip.DistanceCm = Track.DistanceToListener;
            Blip.IsSelected = Track.TrackName == Selected->TrackName;
            Blip.IsVirtualized = Track.IsVirtualized;
            Blip.IsOutOfRange = Track.Get_IsOutOfRange();
            Blip.IsAudible = Track.Get_AudibleVolume() > KINDA_SMALL_NUMBER;

            View.Blips.Add(MoveTemp(Blip));
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoRebuild_SpatialSelector()
    -> void
{
    using namespace ck_audio_debugger_window;

    if (NOT _SpatialSelectorBox.IsValid())
    { return; }

    _SpatialSelectorBox->ClearChildren();

    const auto* Selected = TryGet_SelectedSpatialTrack();
    const auto SelectedName = Selected != nullptr ? Selected->TrackName : FString{};

    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        for (const auto& Track : Director.Tracks)
        {
            if (Track.State == ECk_AudioTrack_State::Stopped)
            { continue; }

            const auto Name = Track.TrackName;

            const auto Tone = Track.IsVirtualized
                ? ECk_Tone::Err
                : (Track.Get_IsOutOfRange() ? ECk_Tone::Warn : ECk_Tone::Ok);

            _SpatialSelectorBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_ToggleSurface)
                .IsOn_Lambda([this, Name]() { return _SelectedSpatialTrack == Name; })
                .AccessibleText(FText::FromString(Name))
                .ToolTipText(FText::FromString(ck::Format_UE(
                    TEXT("Inspect '{}' on the radar"), Name)))
                .OnStateChanged_Lambda([this, Name](const bool InOn)
                {
                    // Re-clicking the current selection clears it, which hands the page back to the
                    // most-diagnostic-track default rather than pinning the reader to a stale choice.
                    _SelectedSpatialTrack = InOn ? Name : FString{};
                    _SpatialSignature.Reset();
                })
                [
                    SNew(SCkDebug_StatusPill)
                    .Text(FText::FromString(Name))
                    .Tone(Name == SelectedName ? Tone : ECk_Tone::Neutral)
                    .ShowDot(false)
                ]
            ];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoRebuild_OverlayList()
    -> void
{
    using namespace ck_audio_debugger_window;

    if (NOT _OverlayListBox.IsValid())
    { return; }

    _OverlayListBox->ClearChildren();

    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        _OverlayListBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, CkStyle::SpaceXS)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(Director.DirectorName))
            .Underline(true)
        ];

        for (const auto& Track : Director.Tracks)
        {
            // Captured by VALUE. The row outlives this walk, and the snapshot it came from is replaced wholesale on
            // the next refresh — a captured reference would dangle by the time anybody clicked.
            const auto TrackEntity = Track.TrackEntity;
            const auto TrackName = Track.TrackName;

            _OverlayListBox->AddSlot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceXS)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [
                    SNew(SCkDebug_ToggleSurface)
                    .IsOn_Lambda([TrackEntity]()
                    {
                        const auto Track = UCk_Utils_AudioTrack_UE::Cast(TrackEntity);

                        return ck::IsValid(Track) && UCk_Utils_AudioTrack_UE::Get_IsDebugDrawEnabled(Track);
                    })
                    .AccessibleText(FText::FromString(TrackName))
                    .ToolTipText(FText::FromString(
                        TEXT("Draw this track's position and attenuation in the world viewport.")))
                    .OnStateChanged_Lambda([TrackEntity](const bool InEnabled)
                    {
                        // Through the feature's own Utils, never by adding the tag directly: the tag is CkAudio's
                        // internal gate and a debugger writing it would be reaching past the API that owns it.
                        auto Track = UCk_Utils_AudioTrack_UE::Cast(TrackEntity);

                        if (ck::Is_NOT_Valid(Track))
                        { return; }

                        if (InEnabled)
                        { UCk_Utils_AudioTrack_UE::Request_EnableDebugDraw(Track); }
                        else
                        { UCk_Utils_AudioTrack_UE::Request_DisableDebugDraw(Track); }
                    })
                    [
                        SNew(STextBlock)
                        .Font_Static(&Get_MicroFont)
                        .ColorAndOpacity(CkStyle::TextDim())
                        .Text(FText::FromString(TEXT("draw")))
                    ]
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_RowFont)
                    .ColorAndOpacity(CkStyle::Text())
                    .Text(FText::FromString(TrackName))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_MicroFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                    .Text(FText::FromString(Track.HasSpatialData
                        ? ck::Format_UE(TEXT("{} m"),
                            FString::SanitizeFloat(Track.DistanceToListener / 100.0f, 1))
                        : FString{TEXT("2D")}))
                ]
            ];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoSet_DebugDrawOnAll(
        bool InEnabled)
    -> void
{
    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        for (const auto& TrackInfo : Director.Tracks)
        {
            auto Track = UCk_Utils_AudioTrack_UE::Cast(TrackInfo.TrackEntity);

            if (ck::Is_NOT_Valid(Track))
            { continue; }

            if (InEnabled)
            { UCk_Utils_AudioTrack_UE::Request_EnableDebugDraw(Track); }
            else
            { UCk_Utils_AudioTrack_UE::Request_DisableDebugDraw(Track); }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoRecord_Events()
    -> void
{
    if (NOT _EventLog.IsValid())
    { return; }

    const auto& Snapshot = _Collector.Get_Snapshot();
    const auto Now = FPlatformTime::Seconds();

    const auto Append = [this, Now](const FString& InCategory, const FString& InMessage, ECk_Tone InTone)
    {
        auto Entry = FCkDebug_EventLogEntry{};

        Entry.Category = InCategory;
        Entry.Message = InMessage;
        Entry.Tone = InTone;
        Entry.TimeSeconds = Now;

        _EventLog->Add_Entry(Entry);
    };

    auto Seen = TSet<FString>{};

    for (const auto& Director : Snapshot.Directors)
    {
        for (const auto& Track : Director.Tracks)
        {
            Seen.Add(Track.TrackName);

            const auto IsFading = Track.State == ECk_AudioTrack_State::FadingIn
                || Track.State == ECk_AudioTrack_State::FadingOut;

            const auto* Previous = _TrackWatch.Find(Track.TrackName);

            auto Watch = FCkAudioDebugger_TrackWatch{};

            Watch.State = Track.State;
            Watch.IsVirtualized = Track.IsVirtualized;
            Watch.WasFading = IsFading;
            Watch.DirectorName = Director.DirectorName;

            // The baseline pass records state and reports nothing. Everything present when the window opened has
            // always been there as far as this log can honestly claim.
            if (Previous == nullptr)
            {
                if (_HasWatchBaseline && _EventsShowLifecycle)
                {
                    Append(TEXT("TRACK"), ck::Format_UE(TEXT("{} added to {}"),
                        Track.TrackName, Director.DirectorName), ECk_Tone::Accent);
                }

                _TrackWatch.Add(Track.TrackName, MoveTemp(Watch));
                continue;
            }

            if (Previous->State != Track.State && _EventsShowStateChanges)
            {
                Append(TEXT("STATE"), ck::Format_UE(TEXT("{}  {} → {}"),
                    Track.TrackName,
                    ck_audio_debugger_window::Build_StateText(Previous->State).ToString(),
                    ck_audio_debugger_window::Build_StateText(Track.State).ToString()),
                    Track.State == ECk_AudioTrack_State::Stopped ? ECk_Tone::Neutral : ECk_Tone::Ok);
            }

            if (Previous->WasFading && NOT IsFading && _EventsShowFades)
            {
                Append(TEXT("FADE"), ck::Format_UE(TEXT("{} finished fading at {}"),
                    Track.TrackName, FString::SanitizeFloat(Track.CurrentVolume, 2)), ECk_Tone::Warn);
            }

            if (Previous->IsVirtualized != Track.IsVirtualized && _EventsShowVirtualization)
            {
                Append(TEXT("VIRTUAL"), Track.IsVirtualized
                    ? ck::Format_UE(TEXT("{} became virtualized — now inaudible"), Track.TrackName)
                    : ck::Format_UE(TEXT("{} left virtualization — audible again"), Track.TrackName),
                    Track.IsVirtualized ? ECk_Tone::Err : ECk_Tone::Ok);
            }

            _TrackWatch.Add(Track.TrackName, MoveTemp(Watch));
        }
    }

    for (auto It = _TrackWatch.CreateIterator(); It; ++It)
    {
        if (Seen.Contains(It.Key()))
        { continue; }

        if (_HasWatchBaseline && _EventsShowLifecycle)
        {
            Append(TEXT("TRACK"), ck::Format_UE(TEXT("{} removed from {}"),
                It.Key(), It.Value().DirectorName), ECk_Tone::Neutral);
        }

        It.RemoveCurrent();
    }

    _HasWatchBaseline = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoGet_StateTone(
        const FCkAudioDebugger_TrackInfo& InTrack)
    -> ECk_Tone
{
    // Virtualization outranks the state, and that ordering is the point: a virtualized track reports Playing and is
    // inaudible, so colouring it by its state would paint the bug green.
    if (InTrack.IsVirtualized)
    { return ECk_Tone::Err; }

    switch (InTrack.State)
    {
        case ECk_AudioTrack_State::Playing:    return ECk_Tone::Ok;
        case ECk_AudioTrack_State::FadingIn:   return ECk_Tone::Ok;
        case ECk_AudioTrack_State::FadingOut:  return ECk_Tone::Warn;
        case ECk_AudioTrack_State::Paused:     return ECk_Tone::Warn;
        case ECk_AudioTrack_State::Stopped:
        default:                               return ECk_Tone::Neutral;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoBuild_StateText(
        const FCkAudioDebugger_TrackInfo& InTrack)
    -> FText
{
    return ck_audio_debugger_window::Build_StateText(InTrack.State);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoBuild_FadeText(
        const FCkAudioDebugger_TrackInfo& InTrack)
    -> FString
{
    if (InTrack.FadeSpeed > KINDA_SMALL_NUMBER)
    {
        if (const auto Remaining = FMath::Abs(InTrack.TargetVolume - InTrack.CurrentVolume);
            Remaining > KINDA_SMALL_NUMBER)
        {
            // Rate AND time-to-target. The rate says how fast, the ETA says whether the reader will still be looking
            // at this fade by the time they finish reading the row.
            return ck::Format_UE(TEXT("±{}/s · {}s"),
                FString::SanitizeFloat(InTrack.FadeSpeed, 2),
                FString::SanitizeFloat(Remaining / InTrack.FadeSpeed, 1));
        }
    }

    if (InTrack.State == ECk_AudioTrack_State::Stopped)
    { return FString{}; }

    return ck::Format_UE(TEXT("{}% played"), FMath::RoundToInt(InTrack.PlaybackPercent * 100.0f));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoBuild_AlertText(
        const FCkAudioDebugger_TrackInfo& InTrack)
    -> FString
{
    if (NOT InTrack.IsVirtualized)
    { return FString{}; }

    // Said in words with the number in it, not left to the colour. "Playing at 0.85 and inaudible" is the sentence a
    // reader hunting a silent sound needs to see; a red bar alone makes them work it out.
    auto Text = ck::Format_UE(TEXT("Playing at {} but virtualized — inaudible."),
        FString::SanitizeFloat(InTrack.CurrentVolume, 2));

    if (NOT InTrack.HasAudioComponent)
    { Text += TEXT(" Its audio component has already been released."); }

    return Text;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoPassesStateFilter(
        const FCkAudioDebugger_TrackInfo& InTrack) const
    -> bool
{
    switch (InTrack.State)
    {
        case ECk_AudioTrack_State::Playing:
        case ECk_AudioTrack_State::Paused:
            return _ShowPlaying;

        case ECk_AudioTrack_State::FadingIn:
        case ECk_AudioTrack_State::FadingOut:
            return _ShowFading;

        case ECk_AudioTrack_State::Stopped:
        default:
            return _ShowStopped;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoPassesFilter(
        const FString& InText) const
    -> bool
{
    if (_FilterString.IsEmpty())
    { return true; }

    return InText.Contains(_FilterString, ESearchCase::IgnoreCase);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoGet_PieWorld() const
    -> UWorld*
{
    if (ck::Is_NOT_Valid(GEngine))
    { return nullptr; }

    for (const auto& Context : GEngine->GetWorldContexts())
    {
        auto* World = Context.World();

        if (ck::Is_NOT_Valid(World))
        { continue; }

        if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
        { return World; }
    }

    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------
