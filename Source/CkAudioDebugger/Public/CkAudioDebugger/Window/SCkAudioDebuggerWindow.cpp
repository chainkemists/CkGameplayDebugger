#include "CkAudioDebugger/Window/SCkAudioDebuggerWindow.h"
#include "CkEditorTools/Style/CkIconStyle.h"

#include "CkAudioDebugger/Window/SCkAudioDebugger_FalloffCurve.h"
#include "CkAudioDebugger/Window/SCkAudioDebugger_Radar.h"

#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
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
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/CkUiCollection.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SNullWidget.h"
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

    auto Get_CardRadius() -> float
    {
        switch (UCkDebuggerStyleSettings::Get_Selection().CornerStyle)
        {
            case ECkDebugAxis_CornerStyle::Rounded: return CkStyle::RadiusL();
            case ECkDebugAxis_CornerStyle::Sharp:   return 0.0f;
            case ECkDebugAxis_CornerStyle::Pill:    return CkStyle::RadiusPill();
        }

        return CkStyle::RadiusL();
    }

    auto Get_AuthoredShellStyleTokens() -> FCkUiView::FTokens
    {
        const auto Color = [](const FLinearColor& InColor)
        {
            return TEXT("#") + InColor.ToFColorSRGB().ToHex();
        };
        return {
            {TEXT("--audio-stat-label-size"), FString::FromInt(Get_CardLabelFont().Size)},
            {TEXT("--audio-stat-value-size"), FString::FromInt(Get_CardValueFont().Size)},
            {TEXT("--audio-stat-radius"), FString::SanitizeFloat(Get_CardRadius())},
            {TEXT("--audio-stat-ring-width"), FString::SanitizeFloat(CkStyle::RingWidth())},
            {TEXT("--audio-stat-outer-extent"), FString::SanitizeFloat(ck::debug_axes::Get_CardOuterExtent())},
            {TEXT("--audio-stat-surface"), Color(ck::debug_axes::Get_SurfaceTint(2))},
            {TEXT("--audio-stat-outline"), Color(CkStyle::Border())},
            {TEXT("--audio-stat-label"), Color(CkStyle::TextDim())},
            {TEXT("--audio-stat-text"), Color(CkStyle::Text())},
            {TEXT("--audio-stat-ok"), Color(CkStyle::Ok())},
            {TEXT("--audio-stat-warn"), Color(CkStyle::Warn())},
            {TEXT("--audio-stat-err"), Color(CkStyle::Err())},
            {TEXT("--audio-crossfade-text"), Color(CkStyle::Text())},
            {TEXT("--audio-crossfade-dim"), Color(CkStyle::TextDim())},
            {TEXT("--audio-crossfade-border"), Color(CkStyle::Border())},
            {TEXT("--audio-crossfade-title-size"), FString::FromInt(Get_RowFont().Size)},
            {TEXT("--audio-crossfade-micro-size"), FString::FromInt(Get_MicroFont().Size)},
            {TEXT("--audio-attenuation-text"), Color(CkStyle::Text())},
            {TEXT("--audio-attenuation-dim"), Color(CkStyle::TextDim())},
            {TEXT("--audio-attenuation-border"), Color(CkStyle::Border())},
            {TEXT("--audio-attenuation-row-size"), FString::FromInt(Get_RowFont().Size)},
            {TEXT("--audio-attenuation-value-size"), FString::FromInt(Get_MonoFont().Size)},
            {TEXT("--audio-attenuation-micro-size"), FString::FromInt(Get_MicroFont().Size)},
            {TEXT("--audio-attenuation-curve-height"), FString::SanitizeFloat(k_CurveHeight)},
            {TEXT("--audio-events-muted"), Color(CkStyle::TextMute())},
            {TEXT("--audio-events-micro-size"), FString::FromInt(Get_MicroFont().Size)},
            {TEXT("--audio-director-muted"), Color(CkStyle::TextMute())},
            {TEXT("--audio-track-value-size"), FString::FromInt(Get_MonoFont().Size)},
            {TEXT("--audio-spatial-alert-surface"), Color(CkStyle::GetToneDimColor(ECk_Tone::Err))},
        };
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

    auto
        Get_AttenuationBindings(
            const TSharedPtr<FCkAudioDebugger_SpatialView>& InView)
        -> FCkUiView::FDataBindings
    {
        auto Data = FCkUiView::FDataBindings{};
        Data.SlateUserIndex = 0;
        const auto BindText = [&Data, InView](const TCHAR* InName,
            TFunction<FText(const FCkAudioDebugger_SpatialView&)> InProject)
        {
            Data.Text.Add(InName, TAttribute<FText>::CreateLambda(
                [InView, Project = MoveTemp(InProject)]()
                {
                    return InView.IsValid() && InView->HasSpatialData
                        ? Project(*InView) : FText::GetEmpty();
                }));
        };
        BindText(TEXT("attenuation-heading"), [](const FCkAudioDebugger_SpatialView& InSpatial)
        {
            return FText::FromString(ck::Format_UE(TEXT("Why the audible volume is {}"),
                FString::SanitizeFloat(InSpatial.AudibleVolume, 2)));
        });
        BindText(TEXT("attenuation-distance"), [](const FCkAudioDebugger_SpatialView& InSpatial)
        {
            return FText::FromString(ck::Format_UE(TEXT("{} m"),
                FString::SanitizeFloat(InSpatial.DistanceCm / 100.0f, 1)));
        });
        BindText(TEXT("attenuation-bearing"), [](const FCkAudioDebugger_SpatialView& InSpatial)
        {
            return FText::FromString(ck::Format_UE(TEXT("{}°  {}"),
                FMath::RoundToInt(InSpatial.BearingDegrees), Build_BearingText(InSpatial.BearingDegrees)));
        });
        BindText(TEXT("attenuation-gain"), [](const FCkAudioDebugger_SpatialView& InSpatial)
        {
            return FText::FromString(InSpatial.IsAttenuated
                ? FString::SanitizeFloat(InSpatial.AttenuationGain, 2)
                : FString{TEXT("n/a (not attenuated)")});
        });
        BindText(TEXT("attenuation-track-volume"), [](const FCkAudioDebugger_SpatialView& InSpatial)
        { return FText::FromString(FString::SanitizeFloat(InSpatial.TrackVolume, 2)); });
        BindText(TEXT("attenuation-audible"), [](const FCkAudioDebugger_SpatialView& InSpatial)
        { return FText::FromString(FString::SanitizeFloat(InSpatial.AudibleVolume, 2)); });
        BindText(TEXT("attenuation-asset"), [](const FCkAudioDebugger_SpatialView& InSpatial)
        {
            return FText::FromString(InSpatial.AttenuationAssetName.IsEmpty()
                ? FString{TEXT("(none)")} : InSpatial.AttenuationAssetName);
        });
        return Data;
    }

    auto
        Build_EntityKey(
            const FCk_Handle& InHandle)
        -> FString
    {
        return ck::Format_UE(TEXT("{}"), InHandle.Get_Entity());
    }

    auto Build_DirectorKey(const FCk_Handle& InHandle, int64 InGeneration) -> FString
    {
        return ck::Format_UE(TEXT("{}:{}"), InGeneration, Build_EntityKey(InHandle));
    }

    auto Build_TrackRecordKey(const FCk_Handle& InDirector, const FCk_Handle& InTrack, int64 InGeneration) -> FString
    {
        return ck::Format_UE(TEXT("{}:track:{}:{}"), InGeneration,
            Build_EntityKey(InDirector), Build_EntityKey(InTrack));
    }

    auto Build_TrackHeaderKey(const FCk_Handle& InDirector, int64 InGeneration) -> FString
    {
        return ck::Format_UE(TEXT("{}:header:{}"), InGeneration, Build_EntityKey(InDirector));
    }

    auto Build_SpatialRecordKey(const FCk_Handle& InTrack, int64 InGeneration) -> FString
    {
        return ck::Format_UE(TEXT("{}:spatial:{}"), InGeneration, Build_EntityKey(InTrack));
    }

    auto Get_TrackRecordSchema() -> TArray<FCkUiFieldSchema>
    {
        return {{TEXT("is-header"), ECkUiFieldKind::Bool}, {TEXT("is-track"), ECkUiFieldKind::Bool},
            {TEXT("name"), ECkUiFieldKind::Text}, {TEXT("entity-id"), ECkUiFieldKind::Text},
            {TEXT("active"), ECkUiFieldKind::Text}, {TEXT("policy"), ECkUiFieldKind::Text},
            {TEXT("state"), ECkUiFieldKind::Text}, {TEXT("state-color"), ECkUiFieldKind::Color},
            {TEXT("state-background"), ECkUiFieldKind::Color}, {TEXT("sound"), ECkUiFieldKind::Text},
            {TEXT("sound-tooltip"), ECkUiFieldKind::Text}, {TEXT("priority"), ECkUiFieldKind::Text},
            {TEXT("loop"), ECkUiFieldKind::Text}, {TEXT("override"), ECkUiFieldKind::Text},
            {TEXT("virtualized"), ECkUiFieldKind::Bool}, {TEXT("fraction"), ECkUiFieldKind::Number},
            {TEXT("target-visible"), ECkUiFieldKind::Bool}, {TEXT("target-fraction"), ECkUiFieldKind::Number},
            {TEXT("volume"), ECkUiFieldKind::Text}, {TEXT("fade"), ECkUiFieldKind::Text},
            {TEXT("alert"), ECkUiFieldKind::Text}};
    }

    auto Build_DirectorPolicy(const FCkAudioDebugger_DirectorInfo& InDirector) -> FText
    {
        const auto Crossfade = InDirector.DefaultCrossfadeSeconds.IsSet()
            ? ck::Format_UE(TEXT("crossfade {}s"), FString::SanitizeFloat(InDirector.DefaultCrossfadeSeconds.GetValue(), 1))
            : FString{TEXT("no default crossfade")};
        return FText::FromString(ck::Format_UE(TEXT("{}  ·  same-priority: {}"), Crossfade,
            InDirector.SamePriorityBehavior == ECk_SamePriorityBehavior::Allow ? TEXT("allow") : TEXT("block")));
    }

    // This passive leaf preserves the native director-name substring highlight. Layout, card decoration and
    // all other text remain authored; the generic authored text primitive does not support Slate HighlightText.
    class FDirectorName final : public ICkUiRetainedWidget, public TSharedFromThis<FDirectorName>
    {
    public:
        struct FConfiguration
        {
            TAttribute<FText> Text;
            TAttribute<FText> Highlight;
            FSlateFontInfo Font;
            FLinearColor Color = FLinearColor::White;
        };

        static auto TryConfiguration(const FCkUiCustomWidgetArguments& InArguments,
            FConfiguration& OutConfiguration, FString& OutFailure) -> bool
        {
            const auto* Text = InArguments.TextBindings.Find(TEXT("text"));
            const auto* Highlight = InArguments.TextBindings.Find(TEXT("highlight"));
            if (Text == nullptr || NOT Text->IsSet() || Highlight == nullptr || NOT Highlight->IsSet())
            {
                OutFailure = TEXT("audio-director-name requires text and highlight bindings.");
                return false;
            }
            OutConfiguration.Text = *Text;
            OutConfiguration.Highlight = *Highlight;
            OutConfiguration.Font = InArguments.BaseFont;
            OutConfiguration.Font.TypefaceFontName = TEXT("Bold");
            if (const auto* Size = InArguments.Style.CustomProperties.Find(TEXT("-ck-audio-director-name-size")))
            { OutConfiguration.Font.Size = FMath::RoundToInt(Size->Number); }
            if (const auto* Color = InArguments.Style.CustomProperties.Find(TEXT("-ck-audio-director-name-color")))
            { OutConfiguration.Color = Color->Color; }
            return true;
        }

        explicit FDirectorName(FConfiguration InConfiguration) : Configuration(MoveTemp(InConfiguration)) {}

        auto Initialize() -> void
        {
            const TWeakPtr<FDirectorName> WeakName = AsShared();
            Widget = SNew(STextBlock)
                .Font_Lambda([WeakName]()
                {
                    const auto Name = WeakName.Pin();
                    return Name.IsValid() ? Name->Configuration.Font : FSlateFontInfo{};
                })
                .ColorAndOpacity_Lambda([WeakName]()
                {
                    const auto Name = WeakName.Pin();
                    return FSlateColor{Name.IsValid() ? Name->Configuration.Color : FLinearColor::Transparent};
                })
                .Text_Lambda([WeakName]()
                {
                    const auto Name = WeakName.Pin();
                    return Name.IsValid() ? Name->Configuration.Text.Get(FText::GetEmpty()) : FText::GetEmpty();
                })
                .HighlightText_Lambda([WeakName]()
                {
                    const auto Name = WeakName.Pin();
                    return Name.IsValid() ? Name->Configuration.Highlight.Get(FText::GetEmpty()) : FText::GetEmpty();
                });
            Widget->SetTag(TEXT("audio-director-name"));
        }

        auto GetWidget() const -> TSharedRef<SWidget> override { return Widget.ToSharedRef(); }

        auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            auto Next = FConfiguration{};
            if (NOT TryConfiguration(InArguments, Next, OutFailure)) { return {}; }
            return MakeUnique<FUpdate>(ConstCastSharedRef<FDirectorName>(AsShared()), MoveTemp(Next));
        }

    private:
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FDirectorName> InOwner, FConfiguration InConfiguration)
                : Owner(MoveTemp(InOwner)), Configuration(MoveTemp(InConfiguration)) {}
            void Commit() noexcept override { Owner->Configuration = MoveTemp(Configuration); }
        private:
            TSharedRef<FDirectorName> Owner;
            FConfiguration Configuration;
        };
        FConfiguration Configuration;
        TSharedPtr<STextBlock> Widget;
    };

    // These are the native read-only ChipStyle atoms, not a second chip renderer. Audio owns only the copied
    // text/alert projection; the shared chip continues to own its live Style Lab treatment.
    class FTrackChip final : public ICkUiRetainedWidget, public TSharedFromThis<FTrackChip>
    {
    public:
        struct FConfiguration
        {
            TAttribute<FText> Text;
            TAttribute<bool> Alert;
        };
        static auto TryConfiguration(const FCkUiCustomWidgetArguments& InArguments,
            FConfiguration& OutConfiguration, FString& OutFailure) -> bool
        {
            const auto* Text = InArguments.TextBindings.Find(TEXT("text"));
            const auto* Alert = InArguments.BoolBindings.Find(TEXT("alert"));
            if (Text == nullptr || NOT Text->IsSet() || Alert == nullptr || NOT Alert->IsSet())
            { OutFailure = TEXT("audio-track-chip requires text and alert bindings."); return false; }
            OutConfiguration = {*Text, *Alert};
            return true;
        }
        explicit FTrackChip(FConfiguration InConfiguration) : Configuration(MoveTemp(InConfiguration)) {}
        auto Initialize() -> void
        {
            const TWeakPtr<FTrackChip> WeakChip = AsShared();
            Widget = SNew(SCkDebug_Chip).ShowDot(false)
                .Text_Lambda([WeakChip]()
                {
                    const auto Chip = WeakChip.Pin();
                    return Chip.IsValid() ? Chip->Configuration.Text.Get(FText::GetEmpty()) : FText::GetEmpty();
                })
                .Kind_Lambda([WeakChip]()
                {
                    const auto Chip = WeakChip.Pin();
                    return Chip.IsValid() && Chip->Configuration.Alert.Get(false)
                        ? ECkDebug_ChipKind::Unsatisfied : ECkDebug_ChipKind::Neutral;
                });
        }
        auto GetWidget() const -> TSharedRef<SWidget> override { return Widget.ToSharedRef(); }
        auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            auto Next = FConfiguration{};
            if (NOT TryConfiguration(InArguments, Next, OutFailure)) { return {}; }
            return MakeUnique<FUpdate>(ConstCastSharedRef<FTrackChip>(AsShared()), MoveTemp(Next));
        }
    private:
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FTrackChip> InOwner, FConfiguration InConfiguration)
                : Owner(MoveTemp(InOwner)), Configuration(MoveTemp(InConfiguration)) {}
            void Commit() noexcept override { Owner->Configuration = MoveTemp(Configuration); }
        private:
            TSharedRef<FTrackChip> Owner;
            FConfiguration Configuration;
        };
        FConfiguration Configuration;
        TSharedPtr<SCkDebug_Chip> Widget;
    };

    // The selector combines Audio's pin/follow semantics with the exact shared toggle and status primitives.
    // It has no handle ownership; retiring the repeat record or view revokes its dispatch attribute.
    class FSpatialSelector final : public ICkUiRetainedWidget, public TSharedFromThis<FSpatialSelector>
    {
    public:
        struct FConfiguration
        {
            TAttribute<FText> Label;
            TAttribute<bool> Checked;
            TAttribute<float> Tone;
            TAttribute<bool> CanDispatch;
            FSimpleDelegate Action;
        };
        static auto TryConfiguration(const FCkUiCustomWidgetArguments& InArguments,
            FConfiguration& OutConfiguration, FString& OutFailure) -> bool
        {
            const auto* Label = InArguments.TextBindings.Find(TEXT("label"));
            const auto* Checked = InArguments.BoolBindings.Find(TEXT("checked"));
            const auto* Tone = InArguments.NumberBindings.Find(TEXT("tone"));
            const auto* Action = InArguments.Actions.Find(TEXT("action"));
            if (Label == nullptr || NOT Label->IsSet() || Checked == nullptr || NOT Checked->IsSet()
                || Tone == nullptr || NOT Tone->IsSet() || Action == nullptr || NOT Action->IsBound())
            { OutFailure = TEXT("audio-spatial-selector requires label, checked, tone and action bindings."); return false; }
            OutConfiguration = {*Label, *Checked, *Tone, InArguments.CanDispatchEvents, *Action};
            return true;
        }
        explicit FSpatialSelector(FConfiguration InConfiguration) : Configuration(MoveTemp(InConfiguration)) {}
        auto Initialize() -> void
        {
            const TWeakPtr<FSpatialSelector> WeakSelector = AsShared();
            const auto Label = TAttribute<FText>::CreateLambda([WeakSelector]()
            {
                const auto Selector = WeakSelector.Pin();
                return Selector.IsValid() ? Selector->Configuration.Label.Get(FText::GetEmpty()) : FText::GetEmpty();
            });
            Widget = SNew(SCkDebug_ToggleSurface)
                .AccessibleText(Label)
                .ToolTipText_Lambda([Label]()
                { return FText::FromString(ck::Format_UE(TEXT("Inspect '{}' on the radar"), Label.Get().ToString())); })
                .IsOn_Lambda([WeakSelector]()
                {
                    const auto Selector = WeakSelector.Pin();
                    return Selector.IsValid() && Selector->Configuration.CanDispatch.Get(false)
                        && Selector->Configuration.Checked.Get(false);
                })
                .IsEnabled_Lambda([WeakSelector]()
                {
                    const auto Selector = WeakSelector.Pin();
                    return Selector.IsValid() && Selector->Configuration.CanDispatch.Get(false);
                })
                .OnStateChanged_Lambda([WeakSelector](bool)
                {
                    const auto Selector = WeakSelector.Pin();
                    if (Selector.IsValid() && Selector->Configuration.CanDispatch.Get(false))
                    { const auto Action = Selector->Configuration.Action; Action.ExecuteIfBound(); }
                })
                [
                    SNew(SCkDebug_StatusPill).ShowDot(false).Text(Label)
                    .Tone_Lambda([WeakSelector]()
                    {
                        const auto Selector = WeakSelector.Pin();
                        if (NOT Selector.IsValid()) { return ECk_Tone::Neutral; }
                        const auto Tone = Selector->Configuration.Tone.Get(0.0f);
                        if (Tone == static_cast<float>(ECk_Tone::Err)) { return ECk_Tone::Err; }
                        if (Tone == static_cast<float>(ECk_Tone::Warn)) { return ECk_Tone::Warn; }
                        if (Tone == static_cast<float>(ECk_Tone::Ok)) { return ECk_Tone::Ok; }
                        return ECk_Tone::Neutral;
                    })
                ];
        }
        auto GetWidget() const -> TSharedRef<SWidget> override { return Widget.ToSharedRef(); }
        auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            auto Next = FConfiguration{};
            if (NOT TryConfiguration(InArguments, Next, OutFailure)) { return {}; }
            return MakeUnique<FUpdate>(ConstCastSharedRef<FSpatialSelector>(AsShared()), MoveTemp(Next));
        }
    private:
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FSpatialSelector> InOwner, FConfiguration InConfiguration)
                : Owner(MoveTemp(InOwner)), Configuration(MoveTemp(InConfiguration)) {}
            void Commit() noexcept override { Owner->Configuration = MoveTemp(Configuration); }
        private:
            TSharedRef<FSpatialSelector> Owner;
            FConfiguration Configuration;
        };
        FConfiguration Configuration;
        TSharedPtr<SCkDebug_ToggleSurface> Widget;
    };

    class FOverlayToggle final : public ICkUiRetainedWidget, public TSharedFromThis<FOverlayToggle>
    {
    public:
        struct FConfiguration
        {
            TAttribute<FText> Label;
            TAttribute<FText> Caption;
            TAttribute<bool> Checked;
            TAttribute<bool> CanDispatch;
            FSimpleDelegate Action;
        };
        static auto TryConfiguration(const FCkUiCustomWidgetArguments& InArguments,
            FConfiguration& OutConfiguration, FString& OutFailure) -> bool
        {
            const auto* Label = InArguments.TextBindings.Find(TEXT("label"));
            const auto* Caption = InArguments.TextBindings.Find(TEXT("caption"));
            const auto* Checked = InArguments.BoolBindings.Find(TEXT("checked"));
            const auto* Action = InArguments.Actions.Find(TEXT("action"));
            if (Label == nullptr || NOT Label->IsSet() || Caption == nullptr || NOT Caption->IsSet()
                || Checked == nullptr || NOT Checked->IsSet() || Action == nullptr || NOT Action->IsBound())
            { OutFailure = TEXT("audio-overlay-toggle requires label, caption, checked and action bindings."); return false; }
            OutConfiguration = {*Label, *Caption, *Checked, InArguments.CanDispatchEvents, *Action};
            return true;
        }
        explicit FOverlayToggle(FConfiguration InConfiguration) : Configuration(MoveTemp(InConfiguration)) {}
        auto Initialize() -> void
        {
            const TWeakPtr<FOverlayToggle> WeakToggle = AsShared();
            Widget = SNew(SCkDebug_ToggleSurface)
                .AccessibleText_Lambda([WeakToggle]()
                {
                    const auto Toggle = WeakToggle.Pin();
                    return Toggle.IsValid() ? Toggle->Configuration.Label.Get(FText::GetEmpty()) : FText::GetEmpty();
                })
                .ToolTipText(FText::FromString(TEXT("Change audio debug draw in the running world.")))
                .IsOn_Lambda([WeakToggle]()
                {
                    const auto Toggle = WeakToggle.Pin();
                    return Toggle.IsValid() && Toggle->Configuration.CanDispatch.Get(false)
                        && Toggle->Configuration.Checked.Get(false);
                })
                .IsEnabled_Lambda([WeakToggle]()
                {
                    const auto Toggle = WeakToggle.Pin();
                    return Toggle.IsValid() && Toggle->Configuration.CanDispatch.Get(false);
                })
                .OnStateChanged_Lambda([WeakToggle](bool)
                {
                    const auto Toggle = WeakToggle.Pin();
                    if (Toggle.IsValid() && Toggle->Configuration.CanDispatch.Get(false))
                    { const auto Action = Toggle->Configuration.Action; Action.ExecuteIfBound(); }
                })
                [
                    SNew(STextBlock).Font_Static(&Get_MicroFont).ColorAndOpacity_Lambda([]() { return CkStyle::TextDim(); })
                    .Text_Lambda([WeakToggle]()
                    {
                        const auto Toggle = WeakToggle.Pin();
                        return Toggle.IsValid() ? Toggle->Configuration.Caption.Get(FText::GetEmpty()) : FText::GetEmpty();
                    })
                ];
        }
        auto GetWidget() const -> TSharedRef<SWidget> override { return Widget.ToSharedRef(); }
        auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            auto Next = FConfiguration{};
            if (NOT TryConfiguration(InArguments, Next, OutFailure)) { return {}; }
            return MakeUnique<FUpdate>(ConstCastSharedRef<FOverlayToggle>(AsShared()), MoveTemp(Next));
        }
    private:
        class FUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FUpdate(TSharedRef<FOverlayToggle> InOwner, FConfiguration InConfiguration)
                : Owner(MoveTemp(InOwner)), Configuration(MoveTemp(InConfiguration)) {}
            void Commit() noexcept override { Owner->Configuration = MoveTemp(Configuration); }
        private:
            TSharedRef<FOverlayToggle> Owner;
            FConfiguration Configuration;
        };
        FConfiguration Configuration;
        TSharedPtr<SCkDebug_ToggleSurface> Widget;
    };

    auto TryCreate_OverlayRegistry(TSharedPtr<const FCkUiWidgetRegistrySnapshot>& OutRegistry) -> bool
    {
        auto Staging = FCkUiWidgetRegistry{};
        auto Toggle = FCkUiCustomWidgetRegistration{};
        Toggle.Schema.Tag = TEXT("audio-overlay-toggle");
        Toggle.Schema.Properties = {{TEXT("label"), ECkUiCustomPropertyKind::TextBinding},
            {TEXT("caption"), ECkUiCustomPropertyKind::TextBinding},
            {TEXT("checked"), ECkUiCustomPropertyKind::BoolBinding}, {TEXT("action"), ECkUiCustomPropertyKind::Action}};
        Toggle.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments,
            FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
        {
            auto Configuration = FOverlayToggle::FConfiguration{};
            if (NOT FOverlayToggle::TryConfiguration(InArguments, Configuration, OutFailure)) { return {}; }
            const auto Result = MakeShared<FOverlayToggle>(MoveTemp(Configuration));
            Result->Initialize();
            return Result;
        };
        if (NOT Staging.Register(MoveTemp(Toggle)).Succeeded) { return false; }
        OutRegistry = Staging.CreateSnapshot();
        return true;
    }

    auto Build_OverlayActionKey(int64 InGeneration, bool InEnabled) -> FString
    { return ck::Format_UE(TEXT("{}:overlay:{}"), InGeneration, InEnabled ? TEXT("all") : TEXT("none")); }

    // Audio's observer-relative plot retains the one window-created Radar and its shared in-place spatial model.
    class FSpatialRadar final : public ICkUiRetainedWidget
    {
    public:
        explicit FSpatialRadar(TSharedRef<SCkAudioDebugger_Radar> InRadar) : Radar(MoveTemp(InRadar)) {}
        auto GetWidget() const -> TSharedRef<SWidget> override { return Radar; }
        auto PrepareReload(const FCkUiCustomWidgetArguments&, FString&) const
            -> TUniquePtr<ICkUiPreparedWidgetUpdate> override { return MakeUnique<FUpdate>(); }
    private:
        class FUpdate final : public ICkUiPreparedWidgetUpdate { void Commit() noexcept override {} };
        TSharedRef<SCkAudioDebugger_Radar> Radar;
    };

    auto TryCreate_SpatialRegistry(const TSharedRef<SCkAudioDebugger_Radar>& InRadar,
        TSharedPtr<const FCkUiWidgetRegistrySnapshot>& OutRegistry) -> bool
    {
        TSharedPtr<const FCkUiWidgetRegistrySnapshot> Common;
        if (NOT FCkDebug_UiRegistry::TryCreate(Common).Succeeded) { return false; }
        auto Staging = FCkUiWidgetRegistry{};
        const auto* Icon = Common->Find(TEXT("debug-icon"));
        if (Icon == nullptr || NOT Staging.Register(*Icon).Succeeded) { return false; }
        auto Selector = FCkUiCustomWidgetRegistration{};
        Selector.Schema.Tag = TEXT("audio-spatial-selector");
        Selector.Schema.Properties = {{TEXT("label"), ECkUiCustomPropertyKind::TextBinding},
            {TEXT("checked"), ECkUiCustomPropertyKind::BoolBinding},
            {TEXT("tone"), ECkUiCustomPropertyKind::NumberBinding}, {TEXT("action"), ECkUiCustomPropertyKind::Action}};
        Selector.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments,
            FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
        {
            auto Configuration = FSpatialSelector::FConfiguration{};
            if (NOT FSpatialSelector::TryConfiguration(InArguments, Configuration, OutFailure)) { return {}; }
            const auto Result = MakeShared<FSpatialSelector>(MoveTemp(Configuration));
            Result->Initialize();
            return Result;
        };
        if (NOT Staging.Register(MoveTemp(Selector)).Succeeded) { return false; }
        auto Radar = FCkUiCustomWidgetRegistration{};
        Radar.Schema.Tag = TEXT("audio-radar");
        Radar.RetainedFactory = [InRadar](const FCkUiCustomWidgetArguments&, FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
        {
            if (InRadar->GetParentWidget().IsValid())
            { OutFailure = TEXT("audio-radar requires the window's detached Radar."); return {}; }
            return MakeShared<FSpatialRadar>(InRadar);
        };
        if (NOT Staging.Register(MoveTemp(Radar)).Succeeded) { return false; }
        OutRegistry = Staging.CreateSnapshot();
        return true;
    }

    auto TryCreate_DirectorsRegistry(TSharedPtr<const FCkUiWidgetRegistrySnapshot>& OutRegistry) -> bool
    {
        TSharedPtr<const FCkUiWidgetRegistrySnapshot> Common;
        if (NOT FCkDebug_UiRegistry::TryCreate(Common).Succeeded) { return false; }
        auto Staging = FCkUiWidgetRegistry{};
        for (const auto* Tag : {TEXT("debug-entity-ref"), TEXT("debug-icon"), TEXT("debug-status"), TEXT("debug-meter")})
        {
            const auto* Registration = Common->Find(Tag);
            if (Registration == nullptr || NOT Staging.Register(*Registration).Succeeded) { return false; }
        }
        auto Name = FCkUiCustomWidgetRegistration{};
        Name.Schema.Tag = TEXT("audio-director-name");
        Name.Schema.Properties = {{TEXT("text"), ECkUiCustomPropertyKind::TextBinding},
            {TEXT("highlight"), ECkUiCustomPropertyKind::TextBinding}};
        Name.Schema.StyleProperties = {
            {TEXT("-ck-audio-director-name-size"), ECkUiCustomStyleKind::Length},
            {TEXT("-ck-audio-director-name-color"), ECkUiCustomStyleKind::Color}};
        Name.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments,
            FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
        {
            auto Configuration = FDirectorName::FConfiguration{};
            if (NOT FDirectorName::TryConfiguration(InArguments, Configuration, OutFailure)) { return {}; }
            const auto Result = MakeShared<FDirectorName>(MoveTemp(Configuration));
            Result->Initialize();
            return Result;
        };
        if (NOT Staging.Register(MoveTemp(Name)).Succeeded) { return false; }
        auto Chip = FCkUiCustomWidgetRegistration{};
        Chip.Schema.Tag = TEXT("audio-track-chip");
        Chip.Schema.Properties = {{TEXT("text"), ECkUiCustomPropertyKind::TextBinding},
            {TEXT("alert"), ECkUiCustomPropertyKind::BoolBinding}};
        Chip.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments,
            FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
        {
            auto Configuration = FTrackChip::FConfiguration{};
            if (NOT FTrackChip::TryConfiguration(InArguments, Configuration, OutFailure)) { return {}; }
            const auto Result = MakeShared<FTrackChip>(MoveTemp(Configuration));
            Result->Initialize();
            return Result;
        };
        if (NOT Staging.Register(MoveTemp(Chip)).Succeeded) { return false; }
        OutRegistry = Staging.CreateSnapshot();
        return true;
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
    _CrossfadeLegendText = MakeShared<FText>(FText::FromString(TEXT("(no fades recorded yet)")));

    _SpatialView = MakeShared<FCkAudioDebugger_SpatialView>();

    const FCkUiLoadResult TabsCreated = FCkUiCollection::TryCreate({
        {TEXT("label"), ECkUiFieldKind::Text}, {TEXT("count"), ECkUiFieldKind::Text},
        {TEXT("warning"), ECkUiFieldKind::Bool}}, _TabRecords);
    if (TabsCreated.Succeeded) { DoUpdate_TabRecords(); }
    FCkUiCollection::TryCreate({{TEXT("name"), ECkUiFieldKind::Text},
        {TEXT("entity-id"), ECkUiFieldKind::Text}, {TEXT("active"), ECkUiFieldKind::Text},
        {TEXT("policy"), ECkUiFieldKind::Text}}, _DirectorRecords);
    DoUpdate_DirectorRecords();
    FCkUiCollection::TryCreate(Get_TrackRecordSchema(), _TrackRecords);
    DoUpdate_TrackRecords();
    FCkUiCollection::TryCreate({{TEXT("name"), ECkUiFieldKind::Text},
        {TEXT("checked"), ECkUiFieldKind::Bool}, {TEXT("tone"), ECkUiFieldKind::Number}}, _SpatialRecords);
    DoUpdate_SpatialRecords();
    FCkUiCollection::TryCreate({{TEXT("name"), ECkUiFieldKind::Text},
        {TEXT("distance"), ECkUiFieldKind::Text}, {TEXT("checked"), ECkUiFieldKind::Bool},
        {TEXT("is-header"), ECkUiFieldKind::Bool}, {TEXT("is-track"), ECkUiFieldKind::Bool}}, _OverlayRecords);
    FCkUiCollection::TryCreate({{TEXT("name"), ECkUiFieldKind::Text},
        {TEXT("checked"), ECkUiFieldKind::Bool}}, _OverlayActionRecords);
    DoUpdate_OverlayRecords();
    _Tabs = DoCreate_Tabs();
    _StatCards = DoCreate_StatCards();
    DoCreate_FilterControls();
    _PageSwitcher = DoCreate_PageSwitcher();

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
            .IconId(ECk_Icon::Audio)
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
            SAssignNew(_AuthoredShellHost, SBox)
        ]
    ];

    BuildAuthoredShell();
    if (NOT _UsingNativeFallback)
    {
        BuildAuthoredCrossfadePage();
        BuildAuthoredAttenuationPanel();
        BuildAuthoredEventsToolbar();
        BuildAuthoredEventsPage();
        BuildAuthoredDirectorsPage();
        BuildAuthoredTracksPage();
        BuildAuthoredSpatialPage();
        BuildAuthoredOverlayPage();
    }
    DoRebuild_OverlayActions();
    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkAudioDebuggerWindow::HandleSessionInvalidated);
    _WorldInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnWorldInvalidated().AddSP(
        this, &SCkAudioDebuggerWindow::HandleWorldInvalidated);
    Register_WithGate();
}

SCkAudioDebuggerWindow::~SCkAudioDebuggerWindow()
{
    if (_SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }
    if (_WorldInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Remove(_WorldInvalidatedHandle); }
    DoInvalidate_RuntimeState();
    if (_CrossfadePageHost.IsValid())
    { _CrossfadePageHost->SetContent(SNullWidget::NullWidget); }
    _AuthoredCrossfadeView.Reset();
    if (_AttenuationPanelHost.IsValid())
    { _AttenuationPanelHost->SetContent(SNullWidget::NullWidget); }
    _AuthoredAttenuationView.Reset();
    if (_EventsToolbarHost.IsValid())
    { _EventsToolbarHost->SetContent(SNullWidget::NullWidget); }
    _AuthoredEventsToolbarView.Reset();
    if (_EventsPageHost.IsValid()) { _EventsPageHost->SetContent(SNullWidget::NullWidget); }
    _AuthoredEventsPageView.Reset();
    if (_DirectorsPageHost.IsValid()) { _DirectorsPageHost->SetContent(SNullWidget::NullWidget); }
    _AuthoredDirectorsView.Reset();
    if (_TracksPageHost.IsValid()) { _TracksPageHost->SetContent(SNullWidget::NullWidget); }
    _AuthoredTracksView.Reset();
    if (_SpatialPageHost.IsValid()) { _SpatialPageHost->SetContent(SNullWidget::NullWidget); }
    _AuthoredSpatialView.Reset();
    if (_OverlayPageHost.IsValid()) { _OverlayPageHost->SetContent(SNullWidget::NullWidget); }
    _AuthoredOverlayView.Reset();
    _AuthoredShellView.Reset();
    if (_Tabs.IsValid()) { _Tabs->ReleaseOwnerInteraction(); }
}

auto SCkAudioDebuggerWindow::BuildNativeContent() -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[_Tabs.ToSharedRef()]
        + SVerticalBox::Slot().AutoHeight()[_StatCards.ToSharedRef()]
        + SVerticalBox::Slot().AutoHeight()[DoCreate_FilterRow()]
        + SVerticalBox::Slot().FillHeight(1.0f)[_PageSwitcher.ToSharedRef()];
}

auto SCkAudioDebuggerWindow::BuildAuthoredShell() -> void
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid())
    {
        _UsingNativeFallback = true;
        _AuthoredShellHost->SetContent(BuildNativeContent());
        return;
    }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    if (NOT _TabsProjectionReady || NOT FCkDebug_UiRegistry::TryCreate(Registry).Succeeded)
    {
        _UsingNativeFallback = true;
        _AuthoredShellHost->SetContent(BuildNativeContent());
        return;
    }

    auto NativeBindings = FCkUiView::FNativeBindings{};
    NativeBindings.Add(TEXT("audio-filter-search"), _FilterSearchBar.ToSharedRef());
    NativeBindings.Add(TEXT("audio-filter-playing"), _FilterPlayingToggle.ToSharedRef());
    NativeBindings.Add(TEXT("audio-filter-fading"), _FilterFadingToggle.ToSharedRef());
    NativeBindings.Add(TEXT("audio-filter-stopped"), _FilterStoppedToggle.ToSharedRef());
    NativeBindings.Add(TEXT("audio-filter-group"), _FilterGroupToggle.ToSharedRef());
    NativeBindings.Add(TEXT("audio-pages"), _PageSwitcher.ToSharedRef());
    auto Data = FCkUiView::FDataBindings{};
    Data.SlateUserIndex = 0;
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow = SharedThis(this);
    Data.Collections.Add(TEXT("audio-tabs"), _TabRecords);
    Data.String.Add(TEXT("audio-page"), TAttribute<FString>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() ? ck_audio_debugger_window::Get_PageId(Window->_ActivePage).ToString() : FString{};
    }));
    Data.Visibility.Add(TEXT("audio-tabs-ready"), TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_TabsProjectionReady;
    }));
    Data.StringChanged.Add(TEXT("audio-select-page"), FCkUiOnStringChanged::CreateLambda([WeakWindow](const FString& InKey)
    {
        const auto Window = WeakWindow.Pin();
        if (Window.IsValid() && Window->_TabsProjectionReady && Window->_TabRecords->FindRecord(InKey).IsValid())
        { Window->DoSelect_Page(FName(*InKey)); }
    }));
    Data.Text.Add(TEXT("audio-stat-concurrency"), TAttribute<FText>::CreateLambda(
        [Cell = _StatConcurrency]() { return *Cell; }));
    Data.Text.Add(TEXT("audio-stat-audible"), TAttribute<FText>::CreateLambda(
        [Cell = _StatAudible]() { return *Cell; }));
    Data.Text.Add(TEXT("audio-stat-fading"), TAttribute<FText>::CreateLambda(
        [Cell = _StatFading]() { return *Cell; }));
    Data.Text.Add(TEXT("audio-stat-virtualized"), TAttribute<FText>::CreateLambda(
        [Cell = _StatVirtualized]() { return *Cell; }));
    const TSharedRef<FCkUiView> View = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, ck_audio_debugger_window::Get_AuthoredShellStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _AuthoredMarkupPath = FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html"));
    _AuthoredStylesheetPath = FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.css"));
    View->SetFiles(_AuthoredMarkupPath, _AuthoredStylesheetPath);
    _AuthoredShellView = View;
    View->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    _UsingNativeFallback = NOT View->GetLastResult().Succeeded;
    if (NOT _UsingNativeFallback) { _Tabs->ReleaseOwnerInteraction(); }
    _AuthoredShellHost->SetContent(_UsingNativeFallback ? BuildNativeContent() : Main);
}

auto SCkAudioDebuggerWindow::PollAuthoredShell(const double InCurrentTime) -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InCurrentTime < _NextAuthoredShellPollSeconds || NOT _AuthoredShellView.IsValid()) { return; }
    _NextAuthoredShellPollSeconds = InCurrentTime + PollIntervalSeconds;
    const FCkUiView::FTokens StyleTokens = ck_audio_debugger_window::Get_AuthoredShellStyleTokens();
    const bool ContentChanged = _AuthoredShellView->PollFiles(StyleTokens);
    if (NOT _UsingNativeFallback || NOT ContentChanged) { return; }

    // A startup fallback owns the same native widgets that a valid authored candidate must stage. Poll once to
    // detect a content change, then detach the fallback and retry that candidate against unparented bindings.
    _AuthoredShellHost->SetContent(SNullWidget::NullWidget);
    // Resetting the paths clears the poll cache, so the retry uses FCkUiView's bounded file reader and stages the
    // restored source plus current style tokens in one transaction after the fallback releases its native ports.
    _AuthoredShellView->SetFiles(_AuthoredMarkupPath, _AuthoredStylesheetPath);
    _AuthoredShellView->PollFiles(StyleTokens);
    const FCkUiLoadResult& Recovery = _AuthoredShellView->GetLastResult();
    _UsingNativeFallback = NOT Recovery.Succeeded;
    if (NOT _UsingNativeFallback) { _Tabs->ReleaseOwnerInteraction(); }
    _AuthoredShellHost->SetContent(
        _UsingNativeFallback ? BuildNativeContent() : _AuthoredShellView->GetRegion(TEXT("main")));
    if (NOT _UsingNativeFallback && NOT _AuthoredCrossfadeView.IsValid())
    { BuildAuthoredCrossfadePage(); }
    if (NOT _UsingNativeFallback && NOT _AuthoredAttenuationView.IsValid())
    { BuildAuthoredAttenuationPanel(); }
    if (NOT _UsingNativeFallback && NOT _AuthoredEventsToolbarView.IsValid())
    { BuildAuthoredEventsToolbar(); }
    if (NOT _UsingNativeFallback && NOT _AuthoredEventsPageView.IsValid())
    { BuildAuthoredEventsPage(); }
    if (NOT _UsingNativeFallback && NOT _AuthoredDirectorsView.IsValid())
    { BuildAuthoredDirectorsPage(); }
    if (NOT _UsingNativeFallback && NOT _AuthoredTracksView.IsValid())
    { BuildAuthoredTracksPage(); }
    if (NOT _UsingNativeFallback && NOT _AuthoredSpatialView.IsValid())
    { BuildAuthoredSpatialPage(); }
    if (NOT _UsingNativeFallback && NOT _AuthoredOverlayView.IsValid())
    { BuildAuthoredOverlayPage(); }
}

auto SCkAudioDebuggerWindow::BuildAuthoredOverlayPage() -> void
{
    if (NOT _OverlayPageHost.IsValid() || NOT _OverlayRecords.IsValid() || NOT _OverlayActionRecords.IsValid())
    { return; }
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    if (NOT Plugin.IsValid() || NOT ck_audio_debugger_window::TryCreate_OverlayRegistry(Registry)) { return; }
    auto Data = FCkUiView::FDataBindings{};
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow = SharedThis(this);
    Data.SlateUserIndex = 0;
    Data.Collections.Add(TEXT("audio-overlay-records"), _OverlayRecords);
    Data.Collections.Add(TEXT("audio-overlay-actions"), _OverlayActionRecords);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_OverlayRecordsReady && Window->_Collector.Get_Snapshot().HasWorld;
    });
    Data.Visibility.Add(TEXT("audio-overlay-ready"), TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_OverlayRecordsReady;
    }));
    Data.Text.Add(TEXT("audio-overlay-draw"), FText::FromString(TEXT("draw")));
    Data.ItemActions.Add(TEXT("audio-overlay-toggle"), FCkUiOnItemAction::CreateLambda([WeakWindow](FString InKey)
    {
        if (const auto Window = WeakWindow.Pin()) { Window->DoToggle_OverlayRecord(InKey); }
    }));
    Data.ItemActions.Add(TEXT("audio-overlay-batch"), FCkUiOnItemAction::CreateLambda([WeakWindow](FString InKey)
    {
        if (const auto Window = WeakWindow.Pin()) { Window->DoDispatch_OverlayBatch(InKey); }
    }));
    const auto View = FCkUiView::Create({}, {}, ck_audio_debugger_window::Get_AuthoredShellStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const auto Main = View->GetRegion(TEXT("main"));
    const auto Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _AuthoredOverlayMarkupPath = FPaths::Combine(Directory, TEXT("AudioDebuggerOverlay.ui.html"));
    _AuthoredOverlayStylesheetPath = FPaths::Combine(Directory, TEXT("AudioDebuggerOverlay.ui.css"));
    View->SetFiles(_AuthoredOverlayMarkupPath, _AuthoredOverlayStylesheetPath);
    _AuthoredOverlayView = View;
    View->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    _UsingNativeOverlayFallback = NOT View->GetLastResult().Succeeded;
    _OverlayPageHost->SetContent(_UsingNativeOverlayFallback ? _NativeOverlayPage.ToSharedRef() : Main);
    if (NOT _UsingNativeOverlayFallback)
    { _OverlayActionsBox->ClearChildren(); _OverlayListBox->ClearChildren(); }
}

auto SCkAudioDebuggerWindow::PollAuthoredOverlayPage(const double InCurrentTime) -> void
{
    if (InCurrentTime < _NextAuthoredOverlayPollSeconds || NOT _AuthoredOverlayView.IsValid()) { return; }
    _NextAuthoredOverlayPollSeconds = InCurrentTime + 0.5;
    _AuthoredOverlayView->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    if (_UsingNativeOverlayFallback && _AuthoredOverlayView->GetLastResult().Succeeded)
    {
        _UsingNativeOverlayFallback = false;
        _OverlayPageHost->SetContent(_AuthoredOverlayView->GetRegion(TEXT("main")));
        _OverlayActionsBox->ClearChildren();
        _OverlayListBox->ClearChildren();
    }
}

auto SCkAudioDebuggerWindow::DoUpdate_OverlayRecords() -> void
{
    using namespace ck_audio_debugger_window;
    _OverlayRecordsReady = false;
    if (NOT _OverlayRecords.IsValid() || NOT _OverlayActionRecords.IsValid()) { return; }
    auto Records = TArray<FCkUiRecordData>{};
    auto Actions = TArray<FCkUiRecordData>{};
    const auto SetText = [](FCkUiRecordData& InRecord, const TCHAR* InField, const FString& InText)
    {
        auto Value = FCkUiFieldValue{};
        Value.Text = FText::FromString(InText);
        InRecord.Fields.Add(InField, MoveTemp(Value));
    };
    const auto SetBool = [](FCkUiRecordData& InRecord, const TCHAR* InField, bool InValue)
    {
        auto Value = FCkUiFieldValue{};
        Value.Kind = ECkUiFieldKind::Bool;
        Value.Bool = InValue;
        InRecord.Fields.Add(InField, MoveTemp(Value));
    };
    if (_Collector.Get_Snapshot().HasWorld)
    {
        for (const auto& Director : _Collector.Get_Snapshot().Directors)
        {
            auto Header = FCkUiRecordData{};
            Header.Key = Build_TrackHeaderKey(Director.DirectorEntity, _DirectorSessionGeneration);
            SetText(Header, TEXT("name"), Director.DirectorName);
            SetText(Header, TEXT("distance"), {});
            SetBool(Header, TEXT("checked"), false);
            SetBool(Header, TEXT("is-header"), true);
            SetBool(Header, TEXT("is-track"), false);
            Records.Add(MoveTemp(Header));
            for (const auto& Track : Director.Tracks)
            {
                auto Record = FCkUiRecordData{};
                Record.Key = Build_TrackRecordKey(Director.DirectorEntity, Track.TrackEntity, _DirectorSessionGeneration);
                SetText(Record, TEXT("name"), Track.TrackName);
                SetText(Record, TEXT("distance"), Track.HasSpatialData
                    ? ck::Format_UE(TEXT("{} m"), FString::SanitizeFloat(Track.DistanceToListener / 100.0f, 1)) : FString{TEXT("2D")});
                const auto Handle = UCk_Utils_AudioTrack_UE::Cast(Track.TrackEntity);
                SetBool(Record, TEXT("checked"), ck::IsValid(Handle) && UCk_Utils_AudioTrack_UE::Get_IsDebugDrawEnabled(Handle));
                SetBool(Record, TEXT("is-header"), false);
                SetBool(Record, TEXT("is-track"), true);
                Records.Add(MoveTemp(Record));
            }
        }
        for (const auto Enabled : {true, false})
        {
            auto Record = FCkUiRecordData{};
            Record.Key = Build_OverlayActionKey(_DirectorSessionGeneration, Enabled);
            SetText(Record, TEXT("name"), Enabled ? TEXT("Draw all") : TEXT("Draw none"));
            SetBool(Record, TEXT("checked"), false);
            Actions.Add(MoveTemp(Record));
        }
    }
    _OverlayRecordsReady = _OverlayRecords->TrySetRecords(MoveTemp(Records)).Succeeded
        && _OverlayActionRecords->TrySetRecords(MoveTemp(Actions)).Succeeded;
    if (NOT _OverlayRecordsReady)
    { _OverlayRecords->TrySetRecords({}); _OverlayActionRecords->TrySetRecords({}); }
}

auto SCkAudioDebuggerWindow::TryGet_OverlayTrack(const FString& InKey) const -> const FCkAudioDebugger_TrackInfo*
{
    if (NOT _OverlayRecordsReady || NOT _OverlayRecords.IsValid() || NOT _Collector.Get_Snapshot().HasWorld
        || NOT _OverlayRecords->FindRecord(InKey).IsValid()) { return nullptr; }
    const auto* Found = static_cast<const FCkAudioDebugger_TrackInfo*>(nullptr);
    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        for (const auto& Track : Director.Tracks)
        {
            if (ck_audio_debugger_window::Build_TrackRecordKey(Director.DirectorEntity, Track.TrackEntity,
                _DirectorSessionGeneration) != InKey) { continue; }
            if (Found != nullptr || ck::Is_NOT_Valid(UCk_Utils_AudioTrack_UE::Cast(Track.TrackEntity))) { return nullptr; }
            Found = &Track;
        }
    }
    return Found;
}

auto SCkAudioDebuggerWindow::DoToggle_OverlayRecord(const FString& InKey) -> void
{
    DoUpdate_OverlayRecords();
    const auto* Current = TryGet_OverlayTrack(InKey);
    if (Current == nullptr) { return; }
    auto Track = UCk_Utils_AudioTrack_UE::Cast(Current->TrackEntity);
    if (UCk_Utils_AudioTrack_UE::Get_IsDebugDrawEnabled(Track))
    { UCk_Utils_AudioTrack_UE::Request_DisableDebugDraw(Track); }
    else
    { UCk_Utils_AudioTrack_UE::Request_EnableDebugDraw(Track); }
    DoUpdate_OverlayRecords();
}

auto SCkAudioDebuggerWindow::DoDispatch_OverlayBatch(const FString& InKey) -> void
{
    DoUpdate_OverlayRecords();
    if (NOT _OverlayRecordsReady || NOT _OverlayActionRecords->FindRecord(InKey).IsValid()) { return; }
    const auto EnableKey = ck_audio_debugger_window::Build_OverlayActionKey(_DirectorSessionGeneration, true);
    const auto DisableKey = ck_audio_debugger_window::Build_OverlayActionKey(_DirectorSessionGeneration, false);
    if (InKey != EnableKey && InKey != DisableKey) { return; }
    DoSet_DebugDrawOnAll(InKey == EnableKey, _DirectorSessionGeneration);
    DoUpdate_OverlayRecords();
}

auto SCkAudioDebuggerWindow::BuildAuthoredSpatialPage() -> void
{
    if (NOT _SpatialPageHost.IsValid() || NOT _SpatialRecords.IsValid() || NOT _Radar.IsValid()) { return; }
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    if (NOT Plugin.IsValid() || NOT ck_audio_debugger_window::TryCreate_SpatialRegistry(_Radar.ToSharedRef(), Registry)) { return; }
    auto Data = FCkUiView::FDataBindings{};
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow = SharedThis(this);
    const auto Model = _SpatialView;
    Data.SlateUserIndex = 0;
    Data.Collections.Add(TEXT("audio-spatial-tracks"), _SpatialRecords);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_SpatialRecordsReady && Window->_Collector.Get_Snapshot().HasWorld;
    });
    Data.Visibility.Add(TEXT("audio-spatial-ready"), TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_SpatialRecordsReady;
    }));
    Data.Visibility.Add(TEXT("audio-spatial-unavailable"), TAttribute<bool>::CreateLambda([Model]()
    { return NOT Model.IsValid() || NOT Model->HasSpatialData; }));
    Data.Visibility.Add(TEXT("audio-spatial-plots"), TAttribute<bool>::CreateLambda([Model]()
    { return Model.IsValid() && Model->HasSpatialData; }));
    Data.Visibility.Add(TEXT("audio-spatial-alert"), TAttribute<bool>::CreateLambda([Model]()
    { return Model.IsValid() && Model->HasSpatialData && (Model->IsVirtualized || Model->IsOutOfRange); }));
    Data.Text.Add(TEXT("audio-spatial-unavailable"), TAttribute<FText>::CreateLambda([Model]()
    {
        if (NOT Model.IsValid() || NOT Model->HasSelection)
        { return FText::FromString(TEXT("No track to inspect. The Spatial page needs a track with a live audio component in a running PIE session.")); }
        return FText::FromString(ck::Format_UE(TEXT("'{}' has no spatial data: it is either 2D (no attenuation settings resolved) or its pooled audio component has already been released. Neither has a position to plot."), Model->TrackName));
    }));
    Data.Text.Add(TEXT("audio-spatial-legend"), TAttribute<FText>::CreateLambda([Model]()
    {
        return Model.IsValid() && Model->HasSpatialData ? FText::FromString(ck::Format_UE(TEXT("inner {}m  ·  falloff {}m"),
            FString::SanitizeFloat(Model->InnerRadiusCm / 100.0f, 1),
            FString::SanitizeFloat(Model->MaxFalloffCm / 100.0f, 1))) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("audio-spatial-alert"), TAttribute<FText>::CreateLambda([Model]()
    {
        if (NOT Model.IsValid() || NOT Model->HasSpatialData) { return FText::GetEmpty(); }
        if (Model->IsVirtualized)
        { return FText::FromString(ck::Format_UE(TEXT("{} is virtualized — playing at {} and not mixed at all."),
            Model->TrackName, FString::SanitizeFloat(Model->TrackVolume, 2))); }
        return FText::FromString(ck::Format_UE(TEXT("{} is {} m away — outside its {} m falloff. Playing at {}, audible {}."),
            Model->TrackName, FString::SanitizeFloat(Model->DistanceCm / 100.0f, 1),
            FString::SanitizeFloat(Model->MaxFalloffCm / 100.0f, 1),
            FString::SanitizeFloat(Model->TrackVolume, 2), FString::SanitizeFloat(Model->AudibleVolume, 2)));
    }));
    Data.Text.Add(TEXT("audio-spatial-listener"), TAttribute<FText>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        if (NOT Window.IsValid()) { return FText::GetEmpty(); }
        const auto& Snapshot = Window->_Collector.Get_Snapshot();
        return FText::FromString(Snapshot.HasListener ? ck::Format_UE(TEXT("listener: {}"), Snapshot.ListenerSource)
            : FString{TEXT("no listener — distances cannot be computed")});
    }));
    Data.Images.Add(TEXT("audio-spatial-alert-icon"), TAttribute<const FSlateBrush*>::CreateLambda([]()
    { return FCkIconStyle::Get_Brush(ck::debug_axes::Get_ToneIcon(ECk_Tone::Err), ECk_Icon_BrushSize::Size_16x16); }));
    Data.Text.Add(TEXT("audio-spatial-alert-meaning"), FText::FromString(TEXT("This track is playing and cannot be heard")));
    Data.Color.Add(TEXT("audio-spatial-alert-color"), TAttribute<FLinearColor>::CreateLambda([]() { return CkStyle::Err(); }));
    Data.ItemActions.Add(TEXT("audio-spatial-select"), FCkUiOnItemAction::CreateLambda([WeakWindow](FString InKey)
    { if (const auto Window = WeakWindow.Pin()) { Window->DoSelect_SpatialRecord(InKey); } }));

    // Both fallback mounts release their exact child before the authored candidate acquires it.
    _NativeRadarHost->SetContent(SNullWidget::NullWidget);
    _NativeSpatialAttenuationHost->SetContent(SNullWidget::NullWidget);
    auto NativeBindings = FCkUiView::FNativeBindings{};
    NativeBindings.Add(TEXT("audio-spatial-attenuation"), _AttenuationPanelHost.ToSharedRef());
    const auto View = FCkUiView::Create(MoveTemp(NativeBindings), {}, ck_audio_debugger_window::Get_AuthoredShellStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const auto Main = View->GetRegion(TEXT("main"));
    const auto Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _AuthoredSpatialMarkupPath = FPaths::Combine(Directory, TEXT("AudioDebuggerSpatial.ui.html"));
    _AuthoredSpatialStylesheetPath = FPaths::Combine(Directory, TEXT("AudioDebuggerSpatial.ui.css"));
    View->SetFiles(_AuthoredSpatialMarkupPath, _AuthoredSpatialStylesheetPath);
    _AuthoredSpatialView = View;
    View->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    _UsingNativeSpatialFallback = NOT View->GetLastResult().Succeeded;
    if (_UsingNativeSpatialFallback)
    {
        _NativeRadarHost->SetContent(_Radar.ToSharedRef());
        _NativeSpatialAttenuationHost->SetContent(_AttenuationPanelHost.ToSharedRef());
    }
    else { _SpatialSelectorBox->ClearChildren(); }
    _SpatialPageHost->SetContent(_UsingNativeSpatialFallback ? _NativeSpatialPage.ToSharedRef() : Main);
}

auto SCkAudioDebuggerWindow::PollAuthoredSpatialPage(const double InCurrentTime) -> void
{
    if (InCurrentTime < _NextAuthoredSpatialPollSeconds || NOT _AuthoredSpatialView.IsValid()) { return; }
    _NextAuthoredSpatialPollSeconds = InCurrentTime + 0.5;
    if (_UsingNativeSpatialFallback)
    {
        _NativeRadarHost->SetContent(SNullWidget::NullWidget);
        _NativeSpatialAttenuationHost->SetContent(SNullWidget::NullWidget);
    }
    _AuthoredSpatialView->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    if (_UsingNativeSpatialFallback && _AuthoredSpatialView->GetLastResult().Succeeded)
    {
        _UsingNativeSpatialFallback = false;
        _SpatialSelectorBox->ClearChildren();
        _SpatialPageHost->SetContent(_AuthoredSpatialView->GetRegion(TEXT("main")));
    }
    else if (_UsingNativeSpatialFallback)
    {
        _NativeRadarHost->SetContent(_Radar.ToSharedRef());
        _NativeSpatialAttenuationHost->SetContent(_AttenuationPanelHost.ToSharedRef());
    }
}

auto SCkAudioDebuggerWindow::DoUpdate_SpatialRecords() -> void
{
    using namespace ck_audio_debugger_window;
    if (NOT _SpatialRecords.IsValid()) { _SpatialRecordsReady = false; return; }
    auto Records = TArray<FCkUiRecordData>{};
    const auto& Snapshot = _Collector.Get_Snapshot();
    const auto* Selected = Snapshot.HasWorld ? TryGet_SelectedSpatialTrack() : nullptr;
    const auto SelectedKey = Selected != nullptr ? Build_SpatialRecordKey(Selected->TrackEntity, _DirectorSessionGeneration) : FString{};
    if (Snapshot.HasWorld)
    {
        for (const auto& Director : Snapshot.Directors)
        {
            for (const auto& Track : Director.Tracks)
            {
                if (Track.State == ECk_AudioTrack_State::Stopped || ck::Is_NOT_Valid(Track.TrackEntity)) { continue; }
                const auto Key = Build_SpatialRecordKey(Track.TrackEntity, _DirectorSessionGeneration);
                const auto Tone = Key != SelectedKey ? ECk_Tone::Neutral
                    : Track.IsVirtualized ? ECk_Tone::Err : Track.Get_IsOutOfRange() ? ECk_Tone::Warn : ECk_Tone::Ok;
                auto Record = FCkUiRecordData{};
                Record.Key = Key;
                auto NameField = FCkUiFieldValue{};
                NameField.Text = FText::FromString(Track.TrackName);
                Record.Fields.Add(TEXT("name"), MoveTemp(NameField));
                auto CheckedField = FCkUiFieldValue{};
                CheckedField.Kind = ECkUiFieldKind::Bool;
                CheckedField.Bool = Key == _SelectedSpatialTrackKey;
                Record.Fields.Add(TEXT("checked"), MoveTemp(CheckedField));
                auto ToneField = FCkUiFieldValue{};
                ToneField.Kind = ECkUiFieldKind::Number;
                ToneField.Number = static_cast<float>(Tone);
                Record.Fields.Add(TEXT("tone"), MoveTemp(ToneField));
                Records.Add(MoveTemp(Record));
            }
        }
    }
    _SpatialRecordsReady = _SpatialRecords->TrySetRecords(MoveTemp(Records)).Succeeded;
    if (NOT _SpatialRecordsReady) { _SpatialRecords->TrySetRecords({}); }
}

auto SCkAudioDebuggerWindow::DoSelect_SpatialRecord(const FString& InKey) -> void
{
    if (NOT _SpatialRecordsReady || NOT _SpatialRecords.IsValid() || NOT _Collector.Get_Snapshot().HasWorld
        || NOT _SpatialRecords->FindRecord(InKey).IsValid()) { return; }
    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        for (const auto& Track : Director.Tracks)
        {
            if (Track.State != ECk_AudioTrack_State::Stopped && ck::IsValid(Track.TrackEntity)
                && ck_audio_debugger_window::Build_SpatialRecordKey(Track.TrackEntity, _DirectorSessionGeneration) == InKey)
            {
                _SelectedSpatialTrackKey = _SelectedSpatialTrackKey == InKey ? FString{} : InKey;
                _SpatialSignature.Reset();
                DoUpdate_SpatialView();
                return;
            }
        }
    }
}

auto SCkAudioDebuggerWindow::BuildAuthoredDirectorsPage() -> void
{
    if (NOT _DirectorsPageHost.IsValid() || NOT _DirectorRecords.IsValid()) { return; }
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    if (NOT Plugin.IsValid() || NOT ck_audio_debugger_window::TryCreate_DirectorsRegistry(Registry)) { return; }

    auto Data = FCkUiView::FDataBindings{};
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow = SharedThis(this);
    Data.SlateUserIndex = 0;
    Data.Collections.Add(TEXT("audio-directors"), _DirectorRecords);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_DirectorRecordsReady
            && Window->_Collector.Get_Snapshot().HasWorld;
    });
    Data.Visibility.Add(TEXT("audio-directors-ready"), TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_DirectorRecordsReady;
    }));
    Data.Text.Add(TEXT("audio-director-highlight"), TAttribute<FText>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() ? FText::FromString(Window->_HighlightString) : FText::GetEmpty();
    }));
    Data.Images.Add(TEXT("audio-director-icon"), TAttribute<const FSlateBrush*>::CreateLambda([]()
    { return FCkIconStyle::Get_Brush(ECk_Icon::Audio, ECk_Icon_BrushSize::Size_16x16); }));
    Data.Text.Add(TEXT("audio-director-meaning"), FText::FromString(
        TEXT("Audio director — owns a concurrency budget and the tracks under it")));
    Data.Color.Add(TEXT("audio-director-icon-color"), TAttribute<FLinearColor>::CreateLambda([]()
    { return CkStyle::TextDim(); }));
    Data.ItemActions.Add(TEXT("audio-director-navigate"), FCkUiOnItemAction::CreateLambda([WeakWindow](FString InKey)
    {
        if (const auto Window = WeakWindow.Pin()) { Window->DoNavigate_Director(InKey); }
    }));
    const auto View = FCkUiView::Create({}, {}, ck_audio_debugger_window::Get_AuthoredShellStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const auto Main = View->GetRegion(TEXT("main"));
    const auto Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _AuthoredDirectorsMarkupPath = FPaths::Combine(Directory, TEXT("AudioDebuggerDirectors.ui.html"));
    _AuthoredDirectorsStylesheetPath = FPaths::Combine(Directory, TEXT("AudioDebuggerDirectors.ui.css"));
    View->SetFiles(_AuthoredDirectorsMarkupPath, _AuthoredDirectorsStylesheetPath);
    _AuthoredDirectorsView = View;
    View->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    _UsingNativeDirectorsFallback = NOT View->GetLastResult().Succeeded;
    _DirectorsPageHost->SetContent(_UsingNativeDirectorsFallback ? _NativeDirectorsPage.ToSharedRef() : Main);
    if (NOT _UsingNativeDirectorsFallback)
    {
        _DirectorPageBox->ClearChildren();
        _DirectorPageSlots.Reset();
    }
}

auto SCkAudioDebuggerWindow::PollAuthoredDirectorsPage(const double InCurrentTime) -> void
{
    if (InCurrentTime < _NextAuthoredDirectorsPollSeconds || NOT _AuthoredDirectorsView.IsValid()) { return; }
    _NextAuthoredDirectorsPollSeconds = InCurrentTime + 0.5;
    _AuthoredDirectorsView->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    if (_UsingNativeDirectorsFallback && _AuthoredDirectorsView->GetLastResult().Succeeded)
    {
        _UsingNativeDirectorsFallback = false;
        _DirectorsPageHost->SetContent(_AuthoredDirectorsView->GetRegion(TEXT("main")));
        _DirectorPageBox->ClearChildren();
        _DirectorPageSlots.Reset();
    }
}

auto SCkAudioDebuggerWindow::DoUpdate_DirectorRecords() -> void
{
    if (NOT _DirectorRecords.IsValid()) { _DirectorRecordsReady = false; return; }
    auto Records = TArray<FCkUiRecordData>{};
    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        if (NOT DoPassesFilter(Director.DirectorName)) { continue; }
        auto Record = FCkUiRecordData{};
        Record.Key = ck_audio_debugger_window::Build_DirectorKey(Director.DirectorEntity, _DirectorSessionGeneration);
        const auto AddText = [&Record](const TCHAR* InName, FText InText)
        {
            auto Value = FCkUiFieldValue{};
            Value.Kind = ECkUiFieldKind::Text;
            Value.Text = MoveTemp(InText);
            Record.Fields.Add(InName, MoveTemp(Value));
        };
        AddText(TEXT("name"), FText::FromString(Director.DirectorName));
        AddText(TEXT("entity-id"), ck::IsValid(Director.DirectorEntity)
            ? FText::FromString(ck_audio_debugger_window::Build_EntityKey(Director.DirectorEntity)) : FText::GetEmpty());
        AddText(TEXT("active"), FText::FromString(Director.MaxConcurrentTracks > 0
            ? ck::Format_UE(TEXT("{} / {} active"), Director.Get_ActiveTrackCount(), Director.MaxConcurrentTracks)
            : ck::Format_UE(TEXT("{} active"), Director.Get_ActiveTrackCount())));
        AddText(TEXT("policy"), ck_audio_debugger_window::Build_DirectorPolicy(Director));
        Records.Add(MoveTemp(Record));
    }
    _DirectorRecordsReady = _DirectorRecords->TrySetRecords(MoveTemp(Records)).Succeeded;
    if (NOT _DirectorRecordsReady)
    {
        // Duplicate identities or malformed publication cannot leave yesterday's interactive rows alive.
        _DirectorRecords->TrySetRecords({});
    }
}

auto SCkAudioDebuggerWindow::DoNavigate_Director(const FString& InKey) -> void
{
    if (NOT _DirectorRecordsReady || NOT _Collector.Get_Snapshot().HasWorld
        || NOT _DirectorRecords.IsValid() || NOT _DirectorRecords->FindRecord(InKey).IsValid()) { return; }
    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        if (ck_audio_debugger_window::Build_DirectorKey(Director.DirectorEntity, _DirectorSessionGeneration) == InKey
            && DoPassesFilter(Director.DirectorName) && ck::IsValid(Director.DirectorEntity))
        {
#if WITH_DEV_AUTOMATION_TESTS
            if (_DirectorNavigationForTests)
            {
                _DirectorNavigationForTests(Director.DirectorEntity);
                return;
            }
#endif
            ck::DebugNav::Goto_Entity(Director.DirectorEntity);
            return;
        }
    }
}

auto SCkAudioDebuggerWindow::BuildAuthoredTracksPage() -> void
{
    if (NOT _TracksPageHost.IsValid() || NOT _TrackRecords.IsValid() || NOT _CompactCrossfadePlot.IsValid()) { return; }
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    if (NOT Plugin.IsValid() || NOT ck_audio_debugger_window::TryCreate_DirectorsRegistry(Registry)) { return; }

    auto Data = FCkUiView::FDataBindings{};
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow = SharedThis(this);
    Data.SlateUserIndex = 0;
    Data.Collections.Add(TEXT("audio-tracks"), _TrackRecords);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_TrackRecordsReady && Window->_Collector.Get_Snapshot().HasWorld;
    });
    Data.Visibility.Add(TEXT("audio-tracks-ready"), TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_TrackRecordsReady;
    }));
    Data.Visibility.Add(TEXT("audio-chip-neutral"), false);
    Data.Visibility.Add(TEXT("audio-chip-alert"), true);
    Data.Text.Add(TEXT("audio-track-virtualized"), FText::FromString(TEXT("Virtualized")));
    Data.Text.Add(TEXT("audio-track-highlight"), TAttribute<FText>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() ? FText::FromString(Window->_HighlightString) : FText::GetEmpty();
    }));
    Data.Images.Add(TEXT("audio-director-icon"), TAttribute<const FSlateBrush*>::CreateLambda([]()
    { return FCkIconStyle::Get_Brush(ECk_Icon::Audio, ECk_Icon_BrushSize::Size_16x16); }));
    Data.Text.Add(TEXT("audio-director-meaning"), FText::FromString(
        TEXT("Audio director — owns a concurrency budget and the tracks under it")));
    Data.Color.Add(TEXT("audio-director-icon-color"), TAttribute<FLinearColor>::CreateLambda([]()
    { return CkStyle::TextDim(); }));
    Data.Images.Add(TEXT("audio-alert-icon"), TAttribute<const FSlateBrush*>::CreateLambda([]()
    { return FCkIconStyle::Get_Brush(ck::debug_axes::Get_ToneIcon(ECk_Tone::Err), ECk_Icon_BrushSize::Size_16x16); }));
    Data.Text.Add(TEXT("audio-alert-meaning"), FText::FromString(TEXT("This track is playing and cannot be heard")));
    Data.Color.Add(TEXT("audio-alert-color"), TAttribute<FLinearColor>::CreateLambda([]() { return CkStyle::Err(); }));
    Data.Color.Add(TEXT("audio-target-color"), TAttribute<FLinearColor>::CreateLambda([]() { return CkStyle::Text(); }));
    Data.Text.Add(TEXT("crossfade-legend"), TAttribute<FText>::CreateLambda(
        [Cell = _CrossfadeLegendText]() { return *Cell; }));
    Data.ItemActions.Add(TEXT("audio-track-navigate"), FCkUiOnItemAction::CreateLambda([WeakWindow](FString InKey)
    {
        if (const auto Window = WeakWindow.Pin()) { Window->DoNavigate_TrackRecord(InKey); }
    }));
    // The exact plot has one parent. Detach its fallback slot before the authored native port takes ownership.
    _CompactCrossfadeHost->SetContent(SNullWidget::NullWidget);
    auto NativeBindings = FCkUiView::FNativeBindings{};
    NativeBindings.Add(TEXT("compact-crossfade-plot"), _CompactCrossfadePlot.ToSharedRef());
    const auto View = FCkUiView::Create(MoveTemp(NativeBindings), {}, ck_audio_debugger_window::Get_AuthoredShellStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const auto Main = View->GetRegion(TEXT("main"));
    const auto Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _AuthoredTracksMarkupPath = FPaths::Combine(Directory, TEXT("AudioDebuggerTracks.ui.html"));
    _AuthoredTracksStylesheetPath = FPaths::Combine(Directory, TEXT("AudioDebuggerTracks.ui.css"));
    View->SetFiles(_AuthoredTracksMarkupPath, _AuthoredTracksStylesheetPath);
    _AuthoredTracksView = View;
    View->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    _UsingNativeTracksFallback = NOT View->GetLastResult().Succeeded;
    if (_UsingNativeTracksFallback)
    { _CompactCrossfadeHost->SetContent(DoCreate_NativeCrossfadeLane(false, _CompactCrossfadePlot.ToSharedRef())); }
    _TracksPageHost->SetContent(_UsingNativeTracksFallback ? _NativeTracksPage.ToSharedRef() : Main);
    if (NOT _UsingNativeTracksFallback)
    {
        _DirectorBox->ClearChildren();
        _TrackSlots.Reset();
        _DirectorSlots.Reset();
    }
}

auto SCkAudioDebuggerWindow::PollAuthoredTracksPage(const double InCurrentTime) -> void
{
    if (InCurrentTime < _NextAuthoredTracksPollSeconds || NOT _AuthoredTracksView.IsValid()) { return; }
    _NextAuthoredTracksPollSeconds = InCurrentTime + 0.5;
    if (_UsingNativeTracksFallback) { _CompactCrossfadeHost->SetContent(SNullWidget::NullWidget); }
    _AuthoredTracksView->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    if (_UsingNativeTracksFallback && _AuthoredTracksView->GetLastResult().Succeeded)
    {
        _UsingNativeTracksFallback = false;
        _TracksPageHost->SetContent(_AuthoredTracksView->GetRegion(TEXT("main")));
        _DirectorBox->ClearChildren();
        _TrackSlots.Reset();
        _DirectorSlots.Reset();
    }
    else if (_UsingNativeTracksFallback)
    { _CompactCrossfadeHost->SetContent(DoCreate_NativeCrossfadeLane(false, _CompactCrossfadePlot.ToSharedRef())); }
}

auto SCkAudioDebuggerWindow::DoUpdate_TrackRecords() -> void
{
    using namespace ck_audio_debugger_window;
    if (NOT _TrackRecords.IsValid()) { _TrackRecordsReady = false; return; }
    auto Records = TArray<FCkUiRecordData>{};
    const auto MakeRecord = [this](FString InKey)
    {
        auto Record = FCkUiRecordData{};
        Record.Key = MoveTemp(InKey);
        for (const auto& Field : _TrackRecords->GetSchema())
        { Record.Fields.Add(Field.Name, FCkUiFieldValue{.Kind = Field.Kind}); }
        return Record;
    };
    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        const auto Visible = DoGet_VisibleTracks(Director);
        if (Visible.IsEmpty()) { continue; }
        if (_GroupByDirector)
        {
            auto Header = MakeRecord(Build_TrackHeaderKey(Director.DirectorEntity, _DirectorSessionGeneration));
            Header.Fields[TEXT("is-header")].Bool = true;
            Header.Fields[TEXT("name")].Text = FText::FromString(Director.DirectorName);
            Header.Fields[TEXT("entity-id")].Text = ck::IsValid(Director.DirectorEntity)
                ? FText::FromString(Build_EntityKey(Director.DirectorEntity)) : FText::GetEmpty();
            Header.Fields[TEXT("active")].Text = FText::FromString(Director.MaxConcurrentTracks > 0
                ? ck::Format_UE(TEXT("{} / {} active"), Director.Get_ActiveTrackCount(), Director.MaxConcurrentTracks)
                : ck::Format_UE(TEXT("{} active"), Director.Get_ActiveTrackCount()));
            Header.Fields[TEXT("policy")].Text = Build_DirectorPolicy(Director);
            Records.Add(MoveTemp(Header));
        }
        for (const auto* Track : Visible)
        {
            auto Record = MakeRecord(Build_TrackRecordKey(Director.DirectorEntity, Track->TrackEntity, _DirectorSessionGeneration));
            const auto IsFading = Track->State == ECk_AudioTrack_State::FadingIn || Track->State == ECk_AudioTrack_State::FadingOut;
            const auto Tone = DoGet_StateTone(*Track);
            Record.Fields[TEXT("is-track")].Bool = true;
            Record.Fields[TEXT("name")].Text = FText::FromString(Track->TrackName);
            Record.Fields[TEXT("entity-id")].Text = ck::IsValid(Track->TrackEntity)
                ? FText::FromString(Build_EntityKey(Track->TrackEntity)) : FText::GetEmpty();
            Record.Fields[TEXT("state")].Text = DoBuild_StateText(*Track);
            Record.Fields[TEXT("state-color")].Color = CkStyle::GetToneColor(Tone);
            Record.Fields[TEXT("state-background")].Color = CkStyle::GetToneDimColor(Tone);
            Record.Fields[TEXT("sound")].Text = FText::FromString(Build_SoundLeaf(Track->SoundPath));
            Record.Fields[TEXT("sound-tooltip")].Text = FText::FromString(Track->SoundPath.IsEmpty()
                ? FString{TEXT("This track has no sound asset assigned.")} : Track->SoundPath);
            Record.Fields[TEXT("priority")].Text = FText::FromString(ck::Format_UE(TEXT("p{}"), Track->Priority));
            Record.Fields[TEXT("loop")].Text = Build_LoopText(Track->LoopBehavior);
            Record.Fields[TEXT("override")].Text = Build_OverrideText(Track->OverrideBehavior);
            Record.Fields[TEXT("virtualized")].Bool = Track->IsVirtualized;
            Record.Fields[TEXT("fraction")].Number = FMath::IsFinite(Track->CurrentVolume)
                ? FMath::Clamp(Track->CurrentVolume, 0.0f, 1.0f) : 0.0f;
            Record.Fields[TEXT("target-visible")].Bool = IsFading && FMath::IsFinite(Track->TargetVolume);
            Record.Fields[TEXT("target-fraction")].Number = FMath::IsFinite(Track->TargetVolume)
                ? FMath::Clamp(Track->TargetVolume, 0.0f, 1.0f) : 0.0f;
            Record.Fields[TEXT("volume")].Text = FText::FromString(ck::Format_UE(TEXT("{} {} {}"),
                FString::SanitizeFloat(Track->CurrentVolume, 2), IsFading ? TEXT("→") : TEXT("="),
                FString::SanitizeFloat(Track->TargetVolume, 2)));
            Record.Fields[TEXT("fade")].Text = FText::FromString(DoBuild_FadeText(*Track));
            Record.Fields[TEXT("alert")].Text = FText::FromString(DoBuild_AlertText(*Track));
            Records.Add(MoveTemp(Record));
        }
    }
    _TrackRecordsReady = _TrackRecords->TrySetRecords(MoveTemp(Records)).Succeeded;
    if (NOT _TrackRecordsReady) { _TrackRecords->TrySetRecords({}); }
}

auto SCkAudioDebuggerWindow::DoNavigate_TrackRecord(const FString& InKey) -> void
{
    using namespace ck_audio_debugger_window;
    if (NOT _TrackRecordsReady || NOT _Collector.Get_Snapshot().HasWorld
        || NOT _TrackRecords.IsValid() || NOT _TrackRecords->FindRecord(InKey).IsValid()) { return; }
    const auto Navigate = [this](const FCk_Handle& InEntity)
    {
        if (NOT ck::IsValid(InEntity)) { return; }
#if WITH_DEV_AUTOMATION_TESTS
        if (_TrackNavigationForTests) { _TrackNavigationForTests(InEntity); return; }
#endif
        ck::DebugNav::Goto_Entity(InEntity);
    };
    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        const auto Visible = DoGet_VisibleTracks(Director);
        if (Visible.IsEmpty()) { continue; }
        if (_GroupByDirector && Build_TrackHeaderKey(Director.DirectorEntity, _DirectorSessionGeneration) == InKey)
        { Navigate(Director.DirectorEntity); return; }
        for (const auto* Track : Visible)
        {
            if (Build_TrackRecordKey(Director.DirectorEntity, Track->TrackEntity, _DirectorSessionGeneration) == InKey)
            { Navigate(Track->TrackEntity); return; }
        }
    }
}

auto SCkAudioDebuggerWindow::BuildAuthoredCrossfadePage() -> void
{
    if (NOT _CrossfadePageHost.IsValid() || NOT _CrossfadePagePlot.IsValid()) { return; }
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid()) { return; }

    _CrossfadePageHost->SetContent(SNullWidget::NullWidget);
    auto NativeBindings = FCkUiView::FNativeBindings{};
    NativeBindings.Add(TEXT("crossfade-plot"), _CrossfadePagePlot.ToSharedRef());
    auto Data = FCkUiView::FDataBindings{};
    Data.SlateUserIndex = 0;
    Data.Text.Add(TEXT("crossfade-legend"), TAttribute<FText>::CreateLambda(
        [Cell = _CrossfadeLegendText]() { return *Cell; }));
    const TSharedRef<FCkUiView> View = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, ck_audio_debugger_window::Get_AuthoredShellStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data));
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _AuthoredCrossfadeMarkupPath = FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.html"));
    _AuthoredCrossfadeStylesheetPath = FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.css"));
    View->SetFiles(_AuthoredCrossfadeMarkupPath, _AuthoredCrossfadeStylesheetPath);
    _AuthoredCrossfadeView = View;
    View->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    _UsingNativeCrossfadeFallback = NOT View->GetLastResult().Succeeded;
    _CrossfadePageHost->SetContent(_UsingNativeCrossfadeFallback
        ? DoCreate_NativeCrossfadeLane(true, _CrossfadePagePlot.ToSharedRef())
        : Main);
}

auto SCkAudioDebuggerWindow::PollAuthoredCrossfadePage(const double InCurrentTime) -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InCurrentTime < _NextAuthoredCrossfadePollSeconds || NOT _AuthoredCrossfadeView.IsValid()) { return; }
    _NextAuthoredCrossfadePollSeconds = InCurrentTime + PollIntervalSeconds;
    const FCkUiView::FTokens StyleTokens = ck_audio_debugger_window::Get_AuthoredShellStyleTokens();
    const bool ContentChanged = _AuthoredCrossfadeView->PollFiles(StyleTokens);
    if (NOT _UsingNativeCrossfadeFallback || NOT ContentChanged) { return; }

    _CrossfadePageHost->SetContent(SNullWidget::NullWidget);
    _AuthoredCrossfadeView->SetFiles(_AuthoredCrossfadeMarkupPath, _AuthoredCrossfadeStylesheetPath);
    _AuthoredCrossfadeView->PollFiles(StyleTokens);
    _UsingNativeCrossfadeFallback = NOT _AuthoredCrossfadeView->GetLastResult().Succeeded;
    _CrossfadePageHost->SetContent(_UsingNativeCrossfadeFallback
        ? DoCreate_NativeCrossfadeLane(true, _CrossfadePagePlot.ToSharedRef())
        : _AuthoredCrossfadeView->GetRegion(TEXT("main")));
}

auto
    SCkAudioDebuggerWindow::
    BuildAuthoredAttenuationPanel()
    -> void
{
    if (NOT _AttenuationPanelHost.IsValid() || NOT _AttenuationCurve.IsValid())
    { return; }
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid())
    { return; }

    _AttenuationPanelHost->SetContent(SNullWidget::NullWidget);
    auto NativeBindings = FCkUiView::FNativeBindings{};
    NativeBindings.Add(TEXT("attenuation-curve"), _AttenuationCurve.ToSharedRef());
    const TSharedRef<FCkUiView> View = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, ck_audio_debugger_window::Get_AuthoredShellStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()),
        ck_audio_debugger_window::Get_AttenuationBindings(_SpatialView));
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _AuthoredAttenuationMarkupPath = FPaths::Combine(Directory, TEXT("AudioDebuggerAttenuation.ui.html"));
    _AuthoredAttenuationStylesheetPath = FPaths::Combine(Directory, TEXT("AudioDebuggerAttenuation.ui.css"));
    View->SetFiles(_AuthoredAttenuationMarkupPath, _AuthoredAttenuationStylesheetPath);
    _AuthoredAttenuationView = View;
    View->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    _UsingNativeAttenuationFallback = NOT View->GetLastResult().Succeeded;
    _AttenuationPanelHost->SetContent(_UsingNativeAttenuationFallback
        ? DoCreate_NativeAttenuationPanel() : Main);
}

auto
    SCkAudioDebuggerWindow::
    PollAuthoredAttenuationPanel(
        const double InCurrentTime)
    -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InCurrentTime < _NextAuthoredAttenuationPollSeconds || NOT _AuthoredAttenuationView.IsValid())
    { return; }
    _NextAuthoredAttenuationPollSeconds = InCurrentTime + PollIntervalSeconds;
    const FCkUiView::FTokens StyleTokens = ck_audio_debugger_window::Get_AuthoredShellStyleTokens();
    const bool ContentChanged = _AuthoredAttenuationView->PollFiles(StyleTokens);
    if (NOT _UsingNativeAttenuationFallback || NOT ContentChanged)
    { return; }

    _AttenuationPanelHost->SetContent(SNullWidget::NullWidget);
    _AuthoredAttenuationView->SetFiles(_AuthoredAttenuationMarkupPath, _AuthoredAttenuationStylesheetPath);
    _AuthoredAttenuationView->PollFiles(StyleTokens);
    _UsingNativeAttenuationFallback = NOT _AuthoredAttenuationView->GetLastResult().Succeeded;
    _AttenuationPanelHost->SetContent(_UsingNativeAttenuationFallback
        ? DoCreate_NativeAttenuationPanel() : _AuthoredAttenuationView->GetRegion(TEXT("main")));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_Tabs()
    -> TSharedRef<SCkDebug_UnderlineTabs>
{
    using namespace ck_audio_debugger_window;

    auto Tabs = TArray<FCkDebug_UnderlineTabDesc>{};
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow = SharedThis(this);

    {
        auto Tab = FCkDebug_UnderlineTabDesc{};
        Tab.Id = Get_PageId(ECkAudioDebugger_Page::Directors);
        Tab.Label = FText::FromString(TEXT("Directors"));
        Tab.CountText = TAttribute<FText>::CreateLambda([WeakWindow]()
        {
            const auto Window = WeakWindow.Pin();
            return Window.IsValid() ? FText::AsNumber(Window->_Collector.Get_Snapshot().Directors.Num()) : FText::GetEmpty();
        });
        Tabs.Add(MoveTemp(Tab));
    }

    {
        auto Tab = FCkDebug_UnderlineTabDesc{};
        Tab.Id = Get_PageId(ECkAudioDebugger_Page::Tracks);
        Tab.Label = FText::FromString(TEXT("Tracks"));
        Tab.CountText = TAttribute<FText>::CreateLambda([WeakWindow]()
        {
            const auto Window = WeakWindow.Pin();
            return Window.IsValid() ? FText::AsNumber(Window->_Collector.Get_Snapshot().Get_TrackCount()) : FText::GetEmpty();
        });

        // The amber dot is the page's own alarm: a virtualized track is playing and inaudible, and the reader has to
        // be able to see that from a tab they are not currently on.
        Tab.ShowWarnDot = TAttribute<bool>::CreateLambda([WeakWindow]()
        {
            const auto Window = WeakWindow.Pin();
            return Window.IsValid() && Window->_Collector.Get_Snapshot().Get_VirtualizedCount() > 0;
        });
        Tabs.Add(MoveTemp(Tab));
    }

    {
        auto Tab = FCkDebug_UnderlineTabDesc{};
        Tab.Id = Get_PageId(ECkAudioDebugger_Page::Crossfade);
        Tab.Label = FText::FromString(TEXT("Crossfade"));
        Tab.CountText = TAttribute<FText>::CreateLambda([WeakWindow]()
        {
            const auto Window = WeakWindow.Pin();
            const auto Fading = Window.IsValid() ? Window->_Collector.Get_Snapshot().Get_FadingCount() : 0;
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
        Tab.ShowWarnDot = TAttribute<bool>::CreateLambda([WeakWindow]()
        {
            const auto Window = WeakWindow.Pin();
            if (NOT Window.IsValid()) { return false; }
            for (const auto& Director : Window->_Collector.Get_Snapshot().Directors)
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
        .CanDispatchEvents_Lambda([WeakWindow]()
        {
            const auto Window = WeakWindow.Pin();
            return Window.IsValid() && Window->_UsingNativeFallback;
        })
        .ActiveTabId_Lambda([WeakWindow]()
        {
            const auto Window = WeakWindow.Pin();
            return Window.IsValid() ? Get_PageId(Window->_ActivePage) : NAME_None;
        })
        .OnTabSelected_Lambda([WeakWindow](FName InPageId)
        {
            const auto Window = WeakWindow.Pin();
            if (Window.IsValid() && Window->_UsingNativeFallback) { Window->DoSelect_Page(InPageId); }
        });
}

auto SCkAudioDebuggerWindow::DoSelect_Page(FName InPageId) -> void
{
    for (const auto Page : {ECkAudioDebugger_Page::Directors, ECkAudioDebugger_Page::Tracks,
        ECkAudioDebugger_Page::Crossfade, ECkAudioDebugger_Page::Spatial,
        ECkAudioDebugger_Page::Events, ECkAudioDebugger_Page::Overlay})
    {
        if (ck_audio_debugger_window::Get_PageId(Page) == InPageId) { _ActivePage = Page; return; }
    }
}

auto SCkAudioDebuggerWindow::DoUpdate_TabRecords() -> void
{
    if (NOT _TabRecords.IsValid()) { _TabsProjectionReady = false; return; }
    const auto& Snapshot = _Collector.Get_Snapshot();
    bool OutOfRange = false;
    for (const auto& Director : Snapshot.Directors)
    {
        for (const auto& Track : Director.Tracks) { OutOfRange |= Track.Get_IsOutOfRange(); }
    }
    TArray<FCkUiRecordData> Records;
    for (const auto Page : {ECkAudioDebugger_Page::Directors, ECkAudioDebugger_Page::Tracks,
        ECkAudioDebugger_Page::Crossfade, ECkAudioDebugger_Page::Spatial,
        ECkAudioDebugger_Page::Events, ECkAudioDebugger_Page::Overlay})
    {
        FCkUiRecordData Record;
        Record.Key = ck_audio_debugger_window::Get_PageId(Page).ToString();
        FText Count;
        if (Page == ECkAudioDebugger_Page::Directors) { Count = FText::AsNumber(Snapshot.Directors.Num()); }
        if (Page == ECkAudioDebugger_Page::Tracks) { Count = FText::AsNumber(Snapshot.Get_TrackCount()); }
        if (Page == ECkAudioDebugger_Page::Crossfade && Snapshot.Get_FadingCount() > 0)
        { Count = FText::AsNumber(Snapshot.Get_FadingCount()); }
        Record.Fields.Add(TEXT("label"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Record.Key)});
        Record.Fields.Add(TEXT("count"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = Count});
        Record.Fields.Add(TEXT("warning"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool,
            .Bool = (Page == ECkAudioDebugger_Page::Tracks && Snapshot.Get_VirtualizedCount() > 0)
                || (Page == ECkAudioDebugger_Page::Spatial && OutOfRange)});
        Records.Add(MoveTemp(Record));
    }
    _TabsProjectionReady = _TabRecords->TrySetRecords(MoveTemp(Records)).Succeeded;
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
    DoCreate_PageSwitcher()
    -> TSharedRef<SWidgetSwitcher>
{
    _NativeDirectorsPage = SNew(SScrollBox)
        + SScrollBox::Slot().Padding(CkStyle::SpaceL, CkStyle::SpaceM)
        [SAssignNew(_DirectorPageBox, SVerticalBox)];
    _NativeTracksPage = SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot().Padding(CkStyle::SpaceL, CkStyle::SpaceS)
            [SAssignNew(_DirectorBox, SVerticalBox)]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [DoCreate_CrossfadeLane(false)];
    // Slot order MUST match ECkAudioDebugger_Page's declaration order. The switcher is driven by the enum's integer
    // value, so a reordered enum would otherwise silently show the wrong page.
    return SNew(SWidgetSwitcher)
        .WidgetIndex_Lambda([this]() { return static_cast<int32>(_ActivePage); })

        + SWidgetSwitcher::Slot()
        [
            SAssignNew(_DirectorsPageHost, SBox)[_NativeDirectorsPage.ToSharedRef()]
        ]

        + SWidgetSwitcher::Slot()
        [
            SAssignNew(_TracksPageHost, SBox)[_NativeTracksPage.ToSharedRef()]
        ]

        + SWidgetSwitcher::Slot()[DoCreate_CrossfadeLane(true)]
        + SWidgetSwitcher::Slot()[DoCreate_SpatialPage()]
        + SWidgetSwitcher::Slot()[DoCreate_EventsPage()]
        + SWidgetSwitcher::Slot()[DoCreate_OverlayPage()];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_FilterControls()
    -> void
{
    using namespace ck_audio_debugger_window;

    // Every state toggle drops the signature rather than only flipping its bool: it changes which rows exist, and the
    // value pass writes cells positionally into the rows the structure pass emitted.
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow = SharedThis(this);
    const auto MakeOnStateChanged = [WeakWindow](bool SCkAudioDebuggerWindow::* InFlag)
    {
        return FOnCkDebug_ToggleSurfaceChanged::CreateLambda([WeakWindow, InFlag](const bool InOn)
        {
            if (const TSharedPtr<SCkAudioDebuggerWindow> Window = WeakWindow.Pin())
            {
                if (Window.Get()->*InFlag == InOn) { return; }
                Window.Get()->*InFlag = InOn;
                Window->_LastSignature.Reset();
            }
        });
    };
    const auto MakeIsOn = [WeakWindow](bool SCkAudioDebuggerWindow::* InFlag)
    {
        return TAttribute<bool>::CreateLambda([WeakWindow, InFlag]()
        {
            const TSharedPtr<SCkAudioDebuggerWindow> Window = WeakWindow.Pin();
            return Window.IsValid() && Window.Get()->*InFlag;
        });
    };
    const auto MakeStateToggle = [WeakWindow, MakeIsOn, MakeOnStateChanged](const FText& InLabel,
        ECk_Tone InTone, bool SCkAudioDebuggerWindow::* InFlag) -> TSharedRef<SCkDebug_ToggleSurface>
    {
        return SNew(SCkDebug_ToggleSurface)
            .IsOn(MakeIsOn(InFlag))
            .IsEnabled_Lambda([WeakWindow]() { return WeakWindow.IsValid(); })
            .ToolTipText(FText::Format(
                FText::FromString(TEXT("Show {0} tracks")), InLabel))
            .AccessibleText(InLabel)
            .OnStateChanged(MakeOnStateChanged(InFlag))
            [
                SNew(SCkDebug_StatusPill)
                .Text(InLabel)
                .Tone_Lambda([IsOn = MakeIsOn(InFlag), InTone]() { return IsOn.Get(false) ? InTone : ECk_Tone::Neutral; })
                .ShowDot(false)
            ];
    };

    _FilterSearchBar = SNew(SCkDebug_SearchBar)
        .HintText(FText::FromString(TEXT("Filter tracks")))
        .IsEnabled_Lambda([WeakWindow]() { return WeakWindow.IsValid(); })
        .OnSearchTextChanged_Lambda([WeakWindow](const FString& InText)
        {
            if (const TSharedPtr<SCkAudioDebuggerWindow> Window = WeakWindow.Pin())
            {
                Window->_FilterString = InText;
                Window->_HighlightString = InText;
                Window->_LastSignature.Reset();
            }
        });
    _FilterPlayingToggle = MakeStateToggle(FText::FromString(TEXT("Playing")), ECk_Tone::Ok,
        &SCkAudioDebuggerWindow::_ShowPlaying);
    _FilterFadingToggle = MakeStateToggle(FText::FromString(TEXT("Fading")), ECk_Tone::Warn,
        &SCkAudioDebuggerWindow::_ShowFading);
    _FilterStoppedToggle = MakeStateToggle(FText::FromString(TEXT("Stopped")), ECk_Tone::Accent,
        &SCkAudioDebuggerWindow::_ShowStopped);
    _FilterGroupToggle = SNew(SCkDebug_ToggleSurface)
        .IsOn(MakeIsOn(&SCkAudioDebuggerWindow::_GroupByDirector))
        .IsEnabled_Lambda([WeakWindow]() { return WeakWindow.IsValid(); })
        .ToolTipText(FText::FromString(
            TEXT("Group rows under their director. Off flattens every track into one list, which is what you ")
            TEXT("want when comparing volumes across directors.")))
        .AccessibleText(FText::FromString(TEXT("Group by director")))
        .OnStateChanged(MakeOnStateChanged(&SCkAudioDebuggerWindow::_GroupByDirector))
        [
            SNew(STextBlock)
            .Font_Static(&Get_MicroFont)
            .ColorAndOpacity(CkStyle::TextDim())
            .Text(FText::FromString(TEXT("Group by director")))
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_FilterRow()
    -> TSharedRef<SWidget>
{
    // Fallback-only layout: never retain this parent after the shell releases it to transfer its five native ports.

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceL, 0.0f, CkStyle::SpaceM, CkStyle::SpaceM)
        [
            _FilterSearchBar.ToSharedRef()
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceM)
        [
            _FilterPlayingToggle.ToSharedRef()
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceM)
        [
            _FilterFadingToggle.ToSharedRef()
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceM, CkStyle::SpaceM)
        [
            _FilterStoppedToggle.ToSharedRef()
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceL, CkStyle::SpaceM)
        [
            _FilterGroupToggle.ToSharedRef()
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
    const TSharedRef<SCkDebug_Sparkline> Plot = SNew(SCkDebug_Sparkline)
        .Samples(_CrossfadeSeriesA)
        .BandSamples(_CrossfadeSeriesB)
        .Color(CkStyle::Ok())
        .BandColor(CkStyle::Warn())
        .BandFillOpacity(0.0f)
        .DesiredSize(FVector2D{320.0f, Height});

    if (NOT InIsDedicatedPage)
    {
        _CompactCrossfadePlot = Plot;
        return SAssignNew(_CompactCrossfadeHost, SBox)[DoCreate_NativeCrossfadeLane(false, Plot)];
    }

    _CrossfadePagePlot = Plot;
    return SAssignNew(_CrossfadePageHost, SBox)
    [
        DoCreate_NativeCrossfadeLane(true, Plot)
    ];
}

auto
    SCkAudioDebuggerWindow::
    DoCreate_NativeCrossfadeLane(
        const bool InIsDedicatedPage,
        const TSharedRef<SCkDebug_Sparkline>& InPlot)
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
                InPlot
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Font_Static(&Get_MicroFont)
            .ColorAndOpacity(CkStyle::TextDim())
            .Text_Lambda([Cell = _CrossfadeLegendText]() { return *Cell; })
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
    DoCreate_AttenuationPanel()
    -> TSharedRef<SWidget>
{
    _AttenuationCurve = SNew(SCkAudioDebugger_FalloffCurve).View(_SpatialView);
    return SAssignNew(_AttenuationPanelHost, SBox)
    [
        DoCreate_NativeAttenuationPanel()
    ];
}

auto
    SCkAudioDebuggerWindow::
    DoCreate_NativeAttenuationPanel()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;
    const auto Data = Get_AttenuationBindings(_SpatialView);
    const auto MakeStatRow = [&Data](const TCHAR* InLabel, const TCHAR* InBinding, const bool InEmphasise)
        -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font_Static(InEmphasise ? &Get_RowFont : &Get_MicroFont)
                .ColorAndOpacity(InEmphasise ? CkStyle::Text() : CkStyle::TextDim())
                .Text(FText::FromString(InLabel))
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font_Static(&Get_MonoFont)
                .ColorAndOpacity(InEmphasise ? CkStyle::Text() : CkStyle::TextDim())
                .Text(Data.Text.FindRef(InBinding))
            ];
    };
    constexpr auto OrdinaryRow = false;
    constexpr auto EmphasisedRow = true;
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Font_Static(&Get_MicroFont)
            .ColorAndOpacity(CkStyle::TextDim())
            .Text(Data.Text.FindRef(TEXT("attenuation-heading")))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SNew(SBox).HeightOverride(k_CurveHeight)
            [_AttenuationCurve.ToSharedRef()]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)
            [MakeStatRow(TEXT("Distance"), TEXT("attenuation-distance"), OrdinaryRow)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)
            [MakeStatRow(TEXT("Bearing"), TEXT("attenuation-bearing"), OrdinaryRow)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)
            [MakeStatRow(TEXT("Attenuation gain"), TEXT("attenuation-gain"), OrdinaryRow)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)
            [MakeStatRow(TEXT("Track volume"), TEXT("attenuation-track-volume"), OrdinaryRow)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [SNew(SSeparator).Thickness(1.0f).ColorAndOpacity(FSlateColor{CkStyle::Border()})]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
            [MakeStatRow(TEXT("Audible"), TEXT("attenuation-audible"), EmphasisedRow)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)
            [MakeStatRow(TEXT("Attenuation asset"), TEXT("attenuation-asset"), OrdinaryRow)]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_SpatialPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    const auto View = _SpatialView;
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow = SharedThis(this);
    _Radar = SNew(SCkAudioDebugger_Radar).View(View);
    const auto Attenuation = DoCreate_AttenuationPanel();

    _NativeSpatialPage = SNew(SVerticalBox)

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
                    .Tag(FName{TEXT("audio-spatial-unavailable")})
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
                    .Tag(FName{TEXT("audio-spatial-plots")})
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
                            SAssignNew(_NativeRadarHost, SBox)
                            .WidthOverride(k_RadarSize)
                            .HeightOverride(k_RadarSize)
                            [
                                _Radar.ToSharedRef()
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
                        SAssignNew(_NativeSpatialAttenuationHost, SBox)[Attenuation]
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
            .Text_Lambda([WeakWindow]()
            {
                const auto Window = WeakWindow.Pin();
                if (NOT Window.IsValid()) { return FText::GetEmpty(); }
                const auto& Snapshot = Window->_Collector.Get_Snapshot();

                return FText::FromString(Snapshot.HasListener
                    ? ck::Format_UE(TEXT("listener: {}"), Snapshot.ListenerSource)
                    : FString{TEXT("no listener — distances cannot be computed")});
            })
        ];
    return SAssignNew(_SpatialPageHost, SBox)[_NativeSpatialPage.ToSharedRef()];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    BuildAuthoredEventsToolbar()
    -> void
{
    if (NOT _EventsToolbarHost.IsValid() || NOT _EventsStateToggle.IsValid()
        || NOT _EventsFadesToggle.IsValid() || NOT _EventsVirtualizationToggle.IsValid()
        || NOT _EventsLifecycleToggle.IsValid())
    { return; }
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid())
    { return; }

    _EventsToolbarHost->SetContent(SNullWidget::NullWidget);
    auto NativeBindings = FCkUiView::FNativeBindings{};
    NativeBindings.Add(TEXT("events-state"), _EventsStateToggle.ToSharedRef());
    NativeBindings.Add(TEXT("events-fades"), _EventsFadesToggle.ToSharedRef());
    NativeBindings.Add(TEXT("events-virtualization"), _EventsVirtualizationToggle.ToSharedRef());
    NativeBindings.Add(TEXT("events-lifecycle"), _EventsLifecycleToggle.ToSharedRef());
    auto Data = FCkUiView::FDataBindings{};
    Data.SlateUserIndex = 0;
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow = SharedThis(this);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]() { return WeakWindow.IsValid(); });
    const TSharedRef<FCkUiView> View = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, ck_audio_debugger_window::Get_AuthoredShellStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data));
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _AuthoredEventsToolbarMarkupPath = FPaths::Combine(Directory, TEXT("AudioDebuggerEventsToolbar.ui.html"));
    _AuthoredEventsToolbarStylesheetPath = FPaths::Combine(Directory, TEXT("AudioDebuggerEventsToolbar.ui.css"));
    View->SetFiles(_AuthoredEventsToolbarMarkupPath, _AuthoredEventsToolbarStylesheetPath);
    _AuthoredEventsToolbarView = View;
    View->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    _UsingNativeEventsToolbarFallback = NOT View->GetLastResult().Succeeded;
    _EventsToolbarHost->SetContent(_UsingNativeEventsToolbarFallback ? DoCreate_NativeEventsToolbar() : Main);
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkAudioDebuggerWindow::BuildAuthoredEventsPage() -> void
{
    if (NOT _EventsPageHost.IsValid() || NOT _EventsToolbarHost.IsValid() || NOT _EventLog.IsValid()) { return; }
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid()) { return; }
    _EventsPageHost->SetContent(SNullWidget::NullWidget);
    const auto View = FCkUiView::Create({{TEXT("events-toolbar"), _EventsToolbarHost}, {TEXT("events-log"), _EventLog}}, {},
        ck_audio_debugger_window::Get_AuthoredShellStyleTokens(), CkStyle::RegularFont(CkStyle::FontSizeBody()));
    const auto Main = View->GetRegion(TEXT("main"));
    const auto Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _AuthoredEventsPageMarkupPath = FPaths::Combine(Directory, TEXT("AudioDebuggerEvents.ui.html"));
    _AuthoredEventsPageStylesheetPath = FPaths::Combine(Directory, TEXT("AudioDebuggerEvents.ui.css"));
    View->SetFiles(_AuthoredEventsPageMarkupPath, _AuthoredEventsPageStylesheetPath);
    _AuthoredEventsPageView = View;
    View->PollFiles(ck_audio_debugger_window::Get_AuthoredShellStyleTokens());
    _UsingNativeEventsPageFallback = NOT View->GetLastResult().Succeeded;
    _EventsPageHost->SetContent(_UsingNativeEventsPageFallback ? DoCreate_NativeEventsPage() : Main);
}

auto SCkAudioDebuggerWindow::PollAuthoredEventsPage(double InCurrentTime) -> void
{
    if (InCurrentTime < _NextAuthoredEventsPagePollSeconds || NOT _AuthoredEventsPageView.IsValid()) { return; }
    _NextAuthoredEventsPagePollSeconds = InCurrentTime + 0.5;
    const auto Tokens = ck_audio_debugger_window::Get_AuthoredShellStyleTokens();
    const bool Changed = _AuthoredEventsPageView->PollFiles(Tokens);
    if (NOT _UsingNativeEventsPageFallback || NOT Changed) { return; }
    _EventsPageHost->SetContent(SNullWidget::NullWidget);
    _AuthoredEventsPageView->SetFiles(_AuthoredEventsPageMarkupPath, _AuthoredEventsPageStylesheetPath);
    _AuthoredEventsPageView->PollFiles(Tokens);
    _UsingNativeEventsPageFallback = NOT _AuthoredEventsPageView->GetLastResult().Succeeded;
    _EventsPageHost->SetContent(_UsingNativeEventsPageFallback
        ? DoCreate_NativeEventsPage() : _AuthoredEventsPageView->GetRegion(TEXT("main")));
}

auto
    SCkAudioDebuggerWindow::
    PollAuthoredEventsToolbar(
        const double InCurrentTime)
    -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InCurrentTime < _NextAuthoredEventsToolbarPollSeconds || NOT _AuthoredEventsToolbarView.IsValid())
    { return; }
    _NextAuthoredEventsToolbarPollSeconds = InCurrentTime + PollIntervalSeconds;
    const FCkUiView::FTokens StyleTokens = ck_audio_debugger_window::Get_AuthoredShellStyleTokens();
    const bool ContentChanged = _AuthoredEventsToolbarView->PollFiles(StyleTokens);
    if (NOT _UsingNativeEventsToolbarFallback || NOT ContentChanged)
    { return; }

    // The fallback owns all four ports. Release it before the bounded retry so admission cannot steal children.
    _EventsToolbarHost->SetContent(SNullWidget::NullWidget);
    _AuthoredEventsToolbarView->SetFiles(_AuthoredEventsToolbarMarkupPath, _AuthoredEventsToolbarStylesheetPath);
    _AuthoredEventsToolbarView->PollFiles(StyleTokens);
    _UsingNativeEventsToolbarFallback = NOT _AuthoredEventsToolbarView->GetLastResult().Succeeded;
    _EventsToolbarHost->SetContent(_UsingNativeEventsToolbarFallback
        ? DoCreate_NativeEventsToolbar() : _AuthoredEventsToolbarView->GetRegion(TEXT("main")));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_EventsPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow = SharedThis(this);
    const auto MakeKindToggle = [WeakWindow](const FText& InLabel, ECk_Tone InTone,
        bool SCkAudioDebuggerWindow::* InFlag) -> TSharedRef<SCkDebug_ToggleSurface>
    {
        return SNew(SCkDebug_ToggleSurface)
            .IsOn_Lambda([WeakWindow, InFlag]()
            {
                const TSharedPtr<SCkAudioDebuggerWindow> Window = WeakWindow.Pin();
                return Window.IsValid() && Window.Get()->*InFlag;
            })
            .IsEnabled_Lambda([WeakWindow]() { return WeakWindow.IsValid(); })
            .AccessibleText(InLabel)
            .ToolTipText(FText::Format(FText::FromString(TEXT("Log {0} events")), InLabel))
            .OnStateChanged_Lambda([WeakWindow, InFlag](const bool InOn)
            {
                // These are window preferences, not world actions: no session-generation or HasWorld gate.
                if (const TSharedPtr<SCkAudioDebuggerWindow> Window = WeakWindow.Pin())
                { Window.Get()->*InFlag = InOn; }
            })
            [
                SNew(SCkDebug_StatusPill)
                .Text(InLabel)
                .Tone_Lambda([WeakWindow, InFlag, InTone]()
                {
                    const TSharedPtr<SCkAudioDebuggerWindow> Window = WeakWindow.Pin();
                    return Window.IsValid() && Window.Get()->*InFlag ? InTone : ECk_Tone::Neutral;
                })
                .ShowDot(false)
            ];
    };

    _EventsStateToggle = MakeKindToggle(FText::FromString(TEXT("State")), ECk_Tone::Ok,
        &SCkAudioDebuggerWindow::_EventsShowStateChanges);
    _EventsFadesToggle = MakeKindToggle(FText::FromString(TEXT("Fades")), ECk_Tone::Warn,
        &SCkAudioDebuggerWindow::_EventsShowFades);
    _EventsVirtualizationToggle = MakeKindToggle(FText::FromString(TEXT("Virtualization")), ECk_Tone::Err,
        &SCkAudioDebuggerWindow::_EventsShowVirtualization);
    _EventsLifecycleToggle = MakeKindToggle(FText::FromString(TEXT("Lifecycle")), ECk_Tone::Accent,
        &SCkAudioDebuggerWindow::_EventsShowLifecycle);

    SAssignNew(_EventsToolbarHost, SBox)[DoCreate_NativeEventsToolbar()];
    SAssignNew(_EventLog, SCkDebug_EventLog)
        .MaxEntries(k_EventLogCapacity)
        .UseAuthoredPresentation(true)
        .EmptyText(FText::FromString(TEXT("Nothing has changed since this window opened.")));
    return SAssignNew(_EventsPageHost, SBox)[DoCreate_NativeEventsPage()];
}

auto SCkAudioDebuggerWindow::DoCreate_NativeEventsPage() -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [_EventsToolbarHost.ToSharedRef()]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(CkStyle::SpaceL, 0.0f, CkStyle::SpaceL, CkStyle::SpaceM)
        [_EventLog.ToSharedRef()];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_NativeEventsToolbar()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    return SNew(SBox)
        .Padding(CkStyle::SpaceL, CkStyle::SpaceM, CkStyle::SpaceL, CkStyle::SpaceS)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                _EventsStateToggle.ToSharedRef()
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                _EventsFadesToggle.ToSharedRef()
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                _EventsVirtualizationToggle.ToSharedRef()
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                _EventsLifecycleToggle.ToSharedRef()
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
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoCreate_OverlayPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_audio_debugger_window;

    _NativeOverlayPage = SNew(SVerticalBox)

        // This page WRITES, and it is the only one that does. Saying so on the page is the point: everything else in
        // this window observes, and a reader flipping a switch here is changing the running game, not the view of it.
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceL, CkStyle::SpaceM, CkStyle::SpaceL, CkStyle::SpaceS)
        [
            SAssignNew(_OverlayActionsBox, SHorizontalBox)
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
    return SAssignNew(_OverlayPageHost, SBox)[_NativeOverlayPage.ToSharedRef()];
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
    PollAuthoredShell(InCurrentTime);
    PollAuthoredCrossfadePage(InCurrentTime);
    PollAuthoredAttenuationPanel(InCurrentTime);
    PollAuthoredEventsToolbar(InCurrentTime);
    PollAuthoredEventsPage(InCurrentTime);
    PollAuthoredDirectorsPage(InCurrentTime);
    PollAuthoredTracksPage(InCurrentTime);
    PollAuthoredSpatialPage(InCurrentTime);
    PollAuthoredOverlayPage(InCurrentTime);

    UWorld* World = DoGet_PieWorld();
    if (World == _InvalidatedWorld.Get())
    { World = nullptr; }
    else if (_InvalidatedWorld.IsValid())
    { _InvalidatedWorld = nullptr; }
    if (World != _ObservedWorld.Get())
    {
        DoInvalidate_RuntimeState();
        _ObservedWorld = World;
        DoRebuild_OverlayActions();
    }

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _Collector.Collect(World);
    DoUpdate_OverlayRecords();

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

    if (AllTracks != _OverlaySignature)
    {
        _OverlaySignature = AllTracks;
        DoRebuild_OverlayList();
    }

    if (auto SpatialSignature = ck::Format_UE(TEXT("{}|{}"), AllTracks, _SelectedSpatialTrackKey);
        SpatialSignature != _SpatialSignature)
    {
        _SpatialSignature = MoveTemp(SpatialSignature);
        DoRebuild_SpatialSelector();
    }

}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    // PollAuthoredShell owns source and token publication together because it also coordinates native-fallback
    // detachment. Polling here could consume a restored-file change while the fallback still owns those ports.
    _NextAuthoredShellPollSeconds = 0.0;
    _NextAuthoredCrossfadePollSeconds = 0.0;
    _NextAuthoredAttenuationPollSeconds = 0.0;
    _NextAuthoredEventsToolbarPollSeconds = 0.0;
    _NextAuthoredEventsPagePollSeconds = 0.0;
    if (_EventLog.IsValid()) { _EventLog->Poll_AuthoredPresentation(); }
    _NextAuthoredDirectorsPollSeconds = 0.0;
    _NextAuthoredTracksPollSeconds = 0.0;
    _NextAuthoredSpatialPollSeconds = 0.0;
    _NextAuthoredOverlayPollSeconds = 0.0;
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

        Signature += ck::Format_UE(TEXT("D:{}:{}|"),
            ck_audio_debugger_window::Build_EntityKey(Director.DirectorEntity), Director.DirectorName);

        if (Visible.IsEmpty())
        { continue; }

        for (const auto* Track : Visible)
        {
            // Stable identity and the display name, but nothing that moves. Folding in volume or playback percent
            // signature on nearly every tick of every fade and rebuild the tree instead of writing cells — the exact
            // flicker the structure/value split exists to avoid.
            Signature += ck::Format_UE(TEXT("T:{}:{}|"),
                ck_audio_debugger_window::Build_EntityKey(Track->TrackEntity), Track->TrackName);
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
        Signature += ck::Format_UE(TEXT("D:{}:{}|"),
            ck_audio_debugger_window::Build_EntityKey(Director.DirectorEntity), Director.DirectorName);

        for (const auto& Track : Director.Tracks)
        {
            // State is folded in, unlike the mixer's signature: the Spatial selector only lists non-stopped tracks,
            // so a track starting or stopping genuinely changes that list even though the SET is unchanged.
            Signature += ck::Format_UE(TEXT("T:{}:{}:{}|"),
                ck_audio_debugger_window::Build_EntityKey(Track.TrackEntity),
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
    _DirectorPageSlots.Reset();

    const auto& Snapshot = _Collector.Get_Snapshot();

    // Accepted authored rows reconcile by lifecycle identity; the positional native tree exists only in fallback.
    if (_UsingNativeTracksFallback)
    {
        for (const auto& Director : Snapshot.Directors)
        {
            const auto Visible = DoGet_VisibleTracks(Director);

            // A director whose every track was filtered out is dropped WITH its header: a heading over nothing tells
            // the reader the filter failed rather than that it worked.
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
    }

    DoUpdate_DirectorRecords();
    DoUpdate_TrackRecords();
    if (NOT _UsingNativeDirectorsFallback) { return; }

    // Native startup fallback only. The authored page owns keyed records rather than positional header cells.
    for (const auto& Director : Snapshot.Directors)
    {
        if (NOT DoPassesFilter(Director.DirectorName))
        { continue; }

        auto DirectorPageSlot = FCkAudioDebugger_DirectorSlot{};

        _DirectorPageBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceXS)
        [
            SNew(SCkDebug_Card)
            [
                DoMake_DirectorHeader(Director, DirectorPageSlot)
            ]
        ];

        _DirectorPageSlots.Add(MoveTemp(DirectorPageSlot));
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
            .Brush(FCkIconStyle::Get_Brush(ECk_Icon::Audio, ECk_Icon_BrushSize::Size_16x16))
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
                .Brush(FCkIconStyle::Get_Brush(
                    ck::debug_axes::Get_ToneIcon(ECk_Tone::Err), ECk_Icon_BrushSize::Size_16x16))
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
            const auto TrackKey = ck_audio_debugger_window::Build_EntityKey(Track.TrackEntity);
            Live.Add(TrackKey);

            auto& Ring = _VolumeHistory.FindOrAdd(TrackKey);

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

        const auto NameOrder = InLhs.TrackName.Compare(InRhs.TrackName, ESearchCase::IgnoreCase);
        if (NameOrder != 0)
        { return NameOrder < 0; }

        return InLhs.TrackEntity.Get_Entity().Get_ID() < InRhs.TrackEntity.Get_Entity().Get_ID();
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

        const auto& Track = *InCandidates[InIndex];
        const auto Key = Build_EntityKey(Track.TrackEntity);

        if (const auto* Ring = _VolumeHistory.Find(Key);
            Ring != nullptr && Ring->IsValid())
        { *InTarget = **Ring; }

        return Track.TrackName;
    };

    _CrossfadeSeriesNames.Reset();

    if (const auto NameA = CopyInto(_CrossfadeSeriesA, 0, Candidates);
        NOT NameA.IsEmpty())
    { _CrossfadeSeriesNames.Add(NameA); }

    if (const auto NameB = CopyInto(_CrossfadeSeriesB, 1, Candidates);
        NOT NameB.IsEmpty())
    { _CrossfadeSeriesNames.Add(NameB); }

    *_CrossfadeLegendText = FText::FromString(_CrossfadeSeriesNames.IsEmpty()
        ? FString{TEXT("(nothing playing)")}
        : FString::Join(_CrossfadeSeriesNames, TEXT("   ·   ")));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoUpdate_LiveValues()
    -> void
{
    DoUpdate_TabRecords();
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

    DoUpdate_DirectorRecords();
    auto DirectorPageIndex = 0;
    DoUpdate_TrackRecords();

    for (const auto& Director : Snapshot.Directors)
    {
        if (NOT DoPassesFilter(Director.DirectorName))
        { continue; }

        if (_DirectorPageSlots.IsValidIndex(DirectorPageIndex))
        {
            if (const auto& Slot = _DirectorPageSlots[DirectorPageIndex];
                Slot.ActiveText.IsValid())
            {
                Slot.ActiveText->SetText(FText::FromString(Director.MaxConcurrentTracks > 0
                    ? ck::Format_UE(TEXT("{} / {} active"),
                        Director.Get_ActiveTrackCount(), Director.MaxConcurrentTracks)
                    : ck::Format_UE(TEXT("{} active"), Director.Get_ActiveTrackCount())));
            }
        }

        ++DirectorPageIndex;
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
    if (NOT Snapshot.HasWorld) { return nullptr; }

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
            if (Track.State == ECk_AudioTrack_State::Stopped || ck::Is_NOT_Valid(Track.TrackEntity)) { continue; }
            if (NOT _SelectedSpatialTrackKey.IsEmpty())
            {
                if (ck_audio_debugger_window::Build_SpatialRecordKey(Track.TrackEntity, _DirectorSessionGeneration) == _SelectedSpatialTrackKey)
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

    DoUpdate_SpatialRecords();
    auto& View = *_SpatialView;

    View = FCkAudioDebugger_SpatialView{};
    if (NOT _SpatialRecordsReady) { return; }

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
            Blip.IsSelected = Build_EntityKey(Track.TrackEntity) == Build_EntityKey(Selected->TrackEntity);
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
    if (NOT _UsingNativeSpatialFallback || NOT _SpatialRecordsReady) { return; }

    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow{SharedThis(this)};
    const TWeakPtr<FCkUiCollection> WeakRecords = _SpatialRecords;
    const auto Generation = _DirectorSessionGeneration;

    for (const auto& Director : _Collector.Get_Snapshot().Directors)
    {
        for (const auto& Track : Director.Tracks)
        {
            if (Track.State == ECk_AudioTrack_State::Stopped)
            { continue; }

            const auto Key = Build_SpatialRecordKey(Track.TrackEntity, _DirectorSessionGeneration);
            const auto Label = TAttribute<FText>::CreateLambda([WeakRecords, Key]()
            {
                const auto Records = WeakRecords.Pin();
                const auto Record = Records.IsValid() ? Records->FindRecord(Key) : nullptr;
                const auto* Field = Record.IsValid() ? Record->FindField(TEXT("name")) : nullptr;
                return Field != nullptr ? Field->Text : FText::GetEmpty();
            });

            _SpatialSelectorBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_ToggleSurface)
                .IsOn_Lambda([WeakWindow, Generation, Key]()
                {
                    const auto Window = WeakWindow.Pin();
                    return Window.IsValid()
                        && Window->_DirectorSessionGeneration == Generation && Window->_SpatialRecordsReady
                        && Window->_SpatialRecords->FindRecord(Key).IsValid()
                        && Window->_SelectedSpatialTrackKey == Key;
                })
                .AccessibleText(Label)
                .ToolTipText_Lambda([Label]()
                { return FText::FromString(ck::Format_UE(TEXT("Inspect '{}' on the radar"), Label.Get().ToString())); })
                .OnStateChanged_Lambda([WeakWindow, Generation, Key](const bool)
                {
                    const auto Window = WeakWindow.Pin();
                    if (NOT Window.IsValid() || Window->_DirectorSessionGeneration != Generation)
                    { return; }

                    // Re-clicking the current selection clears it, which hands the page back to the
                    // most-diagnostic-track default rather than pinning the reader to a stale choice.
                    Window->DoSelect_SpatialRecord(Key);
                })
                [
                    SNew(SCkDebug_StatusPill)
                    .Text(Label)
                    .Tone_Lambda([WeakRecords, Key]()
                    {
                        const auto Records = WeakRecords.Pin();
                        const auto Record = Records.IsValid() ? Records->FindRecord(Key) : nullptr;
                        const auto* Field = Record.IsValid() ? Record->FindField(TEXT("tone")) : nullptr;
                        return Field != nullptr ? static_cast<ECk_Tone>(Field->Number) : ECk_Tone::Neutral;
                    })
                    .ShowDot(false)
                ]
            ];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoRebuild_OverlayActions()
    -> void
{
    using namespace ck_audio_debugger_window;

    if (NOT _OverlayActionsBox.IsValid() || NOT _UsingNativeOverlayFallback)
    { return; }

    _OverlayActionsBox->ClearChildren();
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow{SharedThis(this)};
    for (const auto& Record : _OverlayActionRecords->GetRecords())
    {
        const auto Key = Record->GetKey();
        const auto Label = Record->FindField(TEXT("name"))->Text;
        _OverlayActionsBox->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCkDebug_ToggleSurface)
            .IsOn_Lambda([]() { return false; })
            .AccessibleText(Label)
            .ToolTipText(FText::FromString(TEXT("Change audio debug draw for every track in the running world.")))
            .IsEnabled_Lambda([WeakWindow, Key]()
            {
                const auto Window = WeakWindow.Pin();
                return Window.IsValid() && Window->_UsingNativeOverlayFallback && Window->_OverlayRecordsReady
                    && Window->_Collector.Get_Snapshot().HasWorld && Window->_OverlayActionRecords->FindRecord(Key).IsValid();
            })
            .OnStateChanged_Lambda([WeakWindow, Key](const bool)
            {
                const auto Window = WeakWindow.Pin();
                if (Window.IsValid() && Window->_UsingNativeOverlayFallback)
                { Window->DoDispatch_OverlayBatch(Key); }
            })
            [
                SNew(STextBlock)
                .Font_Static(&Get_MicroFont)
                .ColorAndOpacity(CkStyle::TextDim())
                .Text(Label)
            ]
        ];
    }

    _OverlayActionsBox->AddSlot()
    .FillWidth(1.0f)
    .VAlign(VAlign_Center)
    .HAlign(HAlign_Right)
    [
        SNew(STextBlock)
        .Font_Static(&Get_MicroFont)
        .ColorAndOpacity(CkStyle::Warn())
        .Text(FText::FromString(TEXT("writes to the running world")))
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoRebuild_OverlayList()
    -> void
{
    using namespace ck_audio_debugger_window;

    DoUpdate_OverlayRecords();
    if (NOT _OverlayListBox.IsValid() || NOT _UsingNativeOverlayFallback)
    { return; }

    DoRebuild_OverlayActions();
    _OverlayListBox->ClearChildren();
    const TWeakPtr<SCkAudioDebuggerWindow> WeakWindow{SharedThis(this)};
    if (NOT _OverlayRecordsReady) { return; }

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
            const auto Key = Build_TrackRecordKey(Director.DirectorEntity, Track.TrackEntity, _DirectorSessionGeneration);
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
                    .IsOn_Lambda([WeakWindow, Key]()
                    {
                        const auto Window = WeakWindow.Pin();
                        if (NOT Window.IsValid() || NOT Window->_UsingNativeOverlayFallback)
                        { return false; }
                        const auto* Current = Window->TryGet_OverlayTrack(Key);
                        return Current != nullptr && UCk_Utils_AudioTrack_UE::Get_IsDebugDrawEnabled(
                            UCk_Utils_AudioTrack_UE::Cast(Current->TrackEntity));
                    })
                    .IsEnabled_Lambda([WeakWindow, Key]()
                    {
                        const auto Window = WeakWindow.Pin();
                        return Window.IsValid() && Window->_UsingNativeOverlayFallback && Window->TryGet_OverlayTrack(Key) != nullptr;
                    })
                    .AccessibleText(FText::FromString(TrackName))
                    .ToolTipText(FText::FromString(
                        TEXT("Draw this track's position and attenuation in the world viewport.")))
                    .OnStateChanged_Lambda([WeakWindow, Key](const bool)
                    {
                        const auto Window = WeakWindow.Pin();
                        if (Window.IsValid() && Window->_UsingNativeOverlayFallback)
                        { Window->DoToggle_OverlayRecord(Key); }
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
                    .Text_Lambda([WeakWindow, Key]()
                    {
                        const auto Window = WeakWindow.Pin();
                        const auto* Current = Window.IsValid() ? Window->TryGet_OverlayTrack(Key) : nullptr;
                        if (Current == nullptr) { return FText::GetEmpty(); }
                        return FText::FromString(Current->HasSpatialData
                            ? ck::Format_UE(TEXT("{} m"), FString::SanitizeFloat(Current->DistanceToListener / 100.0f, 1))
                            : FString{TEXT("2D")});
                    })
                ]
            ];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoSet_DebugDrawOnAll(
        bool InEnabled,
        int64 InGeneration)
    -> void
{
    if (NOT CanDispatch_RuntimeAction(InGeneration))
    { return; }

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
            const auto TrackKey = ck_audio_debugger_window::Build_EntityKey(Track.TrackEntity);
            Seen.Add(TrackKey);

            const auto IsFading = Track.State == ECk_AudioTrack_State::FadingIn
                || Track.State == ECk_AudioTrack_State::FadingOut;

            const auto* Previous = _TrackWatch.Find(TrackKey);

            auto Watch = FCkAudioDebugger_TrackWatch{};

            Watch.State = Track.State;
            Watch.IsVirtualized = Track.IsVirtualized;
            Watch.WasFading = IsFading;
            Watch.TrackName = Track.TrackName;
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

                _TrackWatch.Add(TrackKey, MoveTemp(Watch));
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

            _TrackWatch.Add(TrackKey, MoveTemp(Watch));
        }
    }

    for (auto It = _TrackWatch.CreateIterator(); It; ++It)
    {
        if (Seen.Contains(It.Key()))
        { continue; }

        if (_HasWatchBaseline && _EventsShowLifecycle)
        {
            Append(TEXT("TRACK"), ck::Format_UE(TEXT("{} removed from {}"),
                It.Value().TrackName, It.Value().DirectorName), ECk_Tone::Neutral);
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
    CanDispatch_RuntimeAction(
        int64 InGeneration) const
    -> bool
{
    return InGeneration == _DirectorSessionGeneration && _OverlayRecordsReady
        && _Collector.Get_Snapshot().HasWorld;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    DoInvalidate_RuntimeState()
    -> void
{
    ++_DirectorSessionGeneration;
    _Collector.Reset();
    DoUpdate_TabRecords();
    DoUpdate_DirectorRecords();
    DoUpdate_TrackRecords();
    DoUpdate_SpatialRecords();

    DoUpdate_OverlayRecords();

    if (_DirectorBox.IsValid()) { _DirectorBox->ClearChildren(); }
    if (_DirectorPageBox.IsValid()) { _DirectorPageBox->ClearChildren(); }
    if (_SpatialSelectorBox.IsValid()) { _SpatialSelectorBox->ClearChildren(); }
    if (_OverlayActionsBox.IsValid()) { _OverlayActionsBox->ClearChildren(); }
    if (_OverlayListBox.IsValid()) { _OverlayListBox->ClearChildren(); }
    if (_EventLog.IsValid()) { _EventLog->Clear_Entries(); }

    _TrackSlots.Reset();
    _DirectorSlots.Reset();
    _DirectorPageSlots.Reset();
    _VolumeHistory.Reset();
    _TrackWatch.Reset();
    _CrossfadeSeriesA->Reset();
    _CrossfadeSeriesB->Reset();
    _CrossfadeSeriesNames.Reset();
    _HasWatchBaseline = false;
    _LastSignature.Reset();
    _SpatialSignature.Reset();
    _OverlaySignature.Reset();
    _SelectedSpatialTrackKey.Reset();

    if (_SpatialView.IsValid())
    { *_SpatialView = FCkAudioDebugger_SpatialView{}; }
    if (_CrossfadeLegendText.IsValid())
    { *_CrossfadeLegendText = FText::FromString(TEXT("(nothing playing)")); }

    *_StatAudible = FText::AsNumber(0);
    *_StatFading = FText::AsNumber(0);
    *_StatVirtualized = FText::AsNumber(0);
    *_StatConcurrency = FText::FromString(TEXT("0 / 0"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    HandleSessionInvalidated()
    -> void
{
    if (_ObservedWorld.IsValid())
    { _InvalidatedWorld = _ObservedWorld; }
    DoInvalidate_RuntimeState();
    _ObservedWorld = nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebuggerWindow::
    HandleWorldInvalidated(
        UWorld* InWorld)
    -> void
{
    if (ck::IsValid(InWorld) && InWorld != _ObservedWorld.Get())
    { return; }

    HandleSessionInvalidated();
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
