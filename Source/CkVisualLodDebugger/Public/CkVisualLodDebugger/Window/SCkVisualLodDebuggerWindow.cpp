#include "CkVisualLodDebugger/Window/SCkVisualLodDebuggerWindow.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Focus.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_AlertRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_UnderlineTabs.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkVisualLod/CkVisualLod_Utils.h"
#include "CkVisualLod/CkVisualLodArbiter_Utils.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkVisualLodDebuggerWindow"

// --------------------------------------------------------------------------------------------------------------------

const FName SCkVisualLodDebuggerWindow::WindowId = FName(TEXT("VisualLodDebugger"));

// --------------------------------------------------------------------------------------------------------------------

namespace ck_visuallod_debugger_window
{
    constexpr auto k_ActivityRingSize   = 60;
    constexpr auto k_MeterKeyWidth      = 78.0f;
    constexpr auto k_MeterValueWidth    = 72.0f;
    constexpr auto k_MeterHeight        = 8.0f;
    constexpr auto k_SparkKeyWidth      = 78.0f;
    constexpr auto k_SparkValueWidth    = 32.0f;
    constexpr auto k_SparkHeight        = 24.0f;

    // Routing every size through ScaledFont is what puts this window under the shared TextScale axis.
    // Attribute form (not value) so a style flip lands on widgets the signature has no reason to rebuild.
    auto Get_RowFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); }

    auto Get_MetaFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); }

    auto Get_HeadingFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); }

    auto Get_Fraction(
        int32 InValue,
        int32 InMax)
        -> float
    {
        if (InMax <= 0)
        { return 0.0f; }

        return FMath::Clamp(static_cast<float>(InValue) / static_cast<float>(InMax), 0.0f, 1.0f);
    }

    auto Get_PolicyText(
        ECk_VisualLod_PoolExhaustionPolicy InPolicy)
        -> FString
    {
        return InPolicy == ECk_VisualLod_PoolExhaustionPolicy::PromoteInstead
            ? FString(TEXT("PromoteInstead"))
            : FString(TEXT("Unrendered"));
    }

    auto Get_BoolText(
        bool InValue)
        -> FText
    {
        return InValue ? LOCTEXT("True", "true") : LOCTEXT("False", "false");
    }

    auto Get_BoolColor(
        bool InValue)
        -> FLinearColor
    {
        return InValue ? CkStyle::Ok() : CkStyle::TextMute();
    }

    auto Make_PaneHeading(
        const FText&      InLabel,
        TAttribute<FText> InRightText)
        -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font_Static(&Get_HeadingFont)
                .ColorAndOpacity(CkStyle::PaneHeadingColor())
                .Text(FText::FromString(InLabel.ToString().ToUpper()))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font_Static(&Get_MetaFont)
                .ColorAndOpacity(CkStyle::TextDim())
                .Text(MoveTemp(InRightText))
            ];
    }

    auto Make_StatCard(
        const FText&             InLabel,
        TAttribute<FText>        InValue,
        TAttribute<FText>        InMeta,
        TAttribute<FSlateColor>  InValueColor,
        TAttribute<FLinearColor> InStripeColor)
        -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_Card)
            .StripeColor(MoveTemp(InStripeColor))
            .BodyPadding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                [
                    SNew(SCkDebug_StatPair)
                    .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    .Value(MoveTemp(InValue))
                    .Label(InLabel)
                    .ValueColor(MoveTemp(InValueColor))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_MetaFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                    .Text(MoveTemp(InMeta))
                ]
            ];
    }

    auto Make_MeterRow(
        const FText&             InKey,
        TAttribute<float>        InFraction,
        TAttribute<FLinearColor> InFillColor,
        TAttribute<FText>        InValueText,
        const FText&             InToolTip)
        -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(k_MeterKeyWidth)
                .HAlign(HAlign_Left)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_MetaFont)
                    .ColorAndOpacity(CkStyle::TextDim())
                    .Text(InKey)
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceM, 0.0f)
            [
                SNew(SCkDebug_MeterBar)
                .Fraction(MoveTemp(InFraction))
                .FillColor(MoveTemp(InFillColor))
                .DesiredSize(FVector2D(120.0f, k_MeterHeight))
                .ToolTipText(InToolTip)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(k_MeterValueWidth)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_MetaFont)
                    .ColorAndOpacity(CkStyle::Text())
                    .Text(MoveTemp(InValueText))
                ]
            ];
    }

    auto Make_KvRow(
        const FText&             InKey,
        TAttribute<FText>        InValue,
        TAttribute<FLinearColor> InValueColor)
        -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_KeyValueRow)
            .KeyText(InKey)
            .ValueText(MoveTemp(InValue))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(MoveTemp(InValueColor));
    }

    auto Make_TunerRow(
        const FText&                       InKey,
        const FText&                       InToolTip,
        TAttribute<double>                 InValue,
        ECkDebug_NumericKind               InKind,
        const TOptional<double>&           InMinValue,
        const TOptional<double>&           InMaxValue,
        int32                              InFractionalDigits,
        FOnCkDebug_NumericCommitted        InOnCommitted)
        -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .ToolTipText(InToolTip)
            [
                SNew(SCkDebug_KeyValueRow)
                .KeyText(InKey)
                .Tone(ECkDebug_KeyValueTone::Custom)
                .CustomValueColor(CkStyle::Text())
                .ValueWidget()
                [
                    SNew(SCkDebug_NumericEditor)
                    .Value(MoveTemp(InValue))
                    .Kind(InKind)
                    .MinValue(InMinValue)
                    .MaxValue(InMaxValue)
                    .FractionalDigits(InFractionalDigits)
                    .Width(86.0f)
                    .OnValueCommitted(MoveTemp(InOnCommitted))
                ]
            ];
    }

    auto Make_SparkRow(
        const FText&              InKey,
        TSharedPtr<TArray<float>> InSamples,
        const FLinearColor&       InColor,
        TAttribute<FText>         InValueText)
        -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(k_SparkKeyWidth)
                .HAlign(HAlign_Left)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_MetaFont)
                    .ColorAndOpacity(CkStyle::TextDim())
                    .Text(InKey)
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceM, 0.0f)
            [
                SNew(SCkDebug_Sparkline)
                .Samples(MoveTemp(InSamples))
                .Color(InColor)
                .FillOpacity(0.18f)
                .DesiredSize(FVector2D(140.0f, k_SparkHeight))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(k_SparkValueWidth)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_RowFont)
                    .ColorAndOpacity(CkStyle::TextStrong())
                    .Text(MoveTemp(InValueText))
                ]
            ];
    }

    auto Make_Separator() -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .HeightOverride(1.0f)
            .Padding(FMargin{0.0f, CkStyle::SpaceM})
            [
                ck::debug_axes::Make_AxisSeparator()
            ];
    }

    // ================================================================================================
    // Roster
    // ================================================================================================

    using FMemberItemPtr = TSharedPtr<FCkVisualLodDebugger_MemberInfo>;

    const FName k_Col_Entity   = TEXT("Entity");
    const FName k_Col_Rep      = TEXT("Rep");
    const FName k_Col_Distance = TEXT("Distance");
    const FName k_Col_View     = TEXT("View");
    const FName k_Col_Fade     = TEXT("Fade");
    const FName k_Col_Slot     = TEXT("Slot");
    const FName k_Col_Flags    = TEXT("Flags");

    constexpr auto k_DotDiameter      = 8.0f;
    constexpr auto k_FadeMeterWidth   = 44.0f;
    constexpr auto k_FadeMeterHeight  = 5.0f;
    constexpr auto k_HighlightBarWide = 3.0f;
    constexpr auto k_HiddenRowOpacity = 0.45f;
    constexpr auto k_EventLogMaxItems = 120;
    constexpr auto k_DetailMeterWidth = 200.0f;

    // Marker heights and colours: the shape says the representation, matching the roster's Rep dot exactly, so a
    // reader who learned one legend has learned both.
    constexpr auto k_MarkerHeightCm = 190.0f;

    auto Get_RepColor(
        ECk_VisualLod_Representation InRepresentation)
        -> FLinearColor
    {
        switch (InRepresentation)
        {
            case ECk_VisualLod_Representation::PromotedProxy: return CkStyle::Accent();
            case ECk_VisualLod_Representation::FarMember:     return CkStyle::Ok();
            default:                                          return CkStyle::TextMute();
        }
    }

    auto Get_RepLabel(
        ECk_VisualLod_Representation InRepresentation)
        -> FText
    {
        switch (InRepresentation)
        {
            // The framework says "proxy" from the RENDERER's side (the pooled SKMC proxies a batched-crowd
            // instance). From the user's side the near one IS the real skeletal mesh — label it that way.
            case ECk_VisualLod_Representation::PromotedProxy: return LOCTEXT("RepProxy", "SkelMesh");
            case ECk_VisualLod_Representation::FarMember:     return LOCTEXT("RepFar", "Far");
            default:                                          return LOCTEXT("RepNone", "None");
        }
    }

    // Hidden members stay on the roster — a member suspended mid-flip is exactly what a reader hunts for — but they
    // are not participating, so every cell of theirs reads back at reduced opacity.
    auto Apply_RowOpacity(
        const FCkVisualLodDebugger_MemberInfo& InMember,
        FLinearColor                           InColor)
        -> FLinearColor
    {
        if (InMember.Hidden)
        { InColor.A *= k_HiddenRowOpacity; }

        return InColor;
    }

    auto Get_MemberIdText(
        const FCkVisualLodDebugger_MemberInfo& InMember)
        -> FString
    {
        // The entity NUMBER — the leading half of the canonical ID|Version(Raw) the EntityRef pill prints, so the
        // roster and the detail rail's pill name the same thing.
        return FString::Printf(TEXT("#%d"), static_cast<int32>(InMember.Entity.Get_Entity().Get_EntityNumber()));
    }

    // ck::EVisualLod_FadePhase is a plain C++ enum, not a UENUM — there is no StaticEnum to name it with.
    auto Get_FadePhaseText(
        ck::EVisualLod_FadePhase InPhase)
        -> FText
    {
        switch (InPhase)
        {
            case ck::EVisualLod_FadePhase::PromoteFade: return LOCTEXT("PhasePromote", "PromoteFade");
            case ck::EVisualLod_FadePhase::DemoteFade:  return LOCTEXT("PhaseDemote", "DemoteFade");
            default:                                    return LOCTEXT("PhaseNone", "None");
        }
    }

    auto Get_DistanceText(
        const FCkVisualLodDebugger_MemberInfo& InMember)
        -> FString
    {
        // -1 is "never ranked", which reads very differently from "ranked at 0" and must not print as a number.
        return InMember.LastDistance < 0.0f
            ? FString(TEXT("—"))
            : FString::Printf(TEXT("%.0f"), InMember.LastDistance);
    }

    auto Get_FadeText(
        const FCkVisualLodDebugger_MemberInfo& InMember)
        -> FString
    {
        return NOT InMember.Get_IsFading() && InMember.FadeAlpha >= 1.0f
            ? FString(TEXT("—"))
            : FString::Printf(TEXT("%.2f"), InMember.FadeAlpha);
    }

    auto Get_SlotText(
        const FCkVisualLodDebugger_MemberInfo& InMember)
        -> FString
    {
        return InMember.SlotIndex >= 0
            ? FString::Printf(TEXT("%d"), InMember.SlotIndex)
            : FString(TEXT("—"));
    }

    // Which representation is actually drawing this member: the promoted proxy's live SKMC, or the
    // authored far render profile selected by its band. Legacy crowds retain their shared primitive
    // fallback. Unresolved flags render as "—" rather than as false.
    auto Get_EffectiveRenderFlags(
        const FCkVisualLodDebugger_ArbiterInfo& InArbiter,
        const FCkVisualLodDebugger_MemberInfo&  InMember)
        -> FCkVisualLodDebugger_RenderFlags
    {
        if (InMember.Promoted)
        { return InMember.ProxyRender; }

        if (InMember.FarRender.Resolved)
        { return InMember.FarRender; }

        if (InMember.CrowdIndex != INDEX_NONE && InArbiter.Crowds.IsValidIndex(InMember.CrowdIndex))
        { return InArbiter.Crowds[InMember.CrowdIndex].Render; }

        return {};
    }

    auto Make_RowCopyText(
        const FCkVisualLodDebugger_MemberInfo& InMember)
        -> FString
    {
        return FString::Printf(TEXT("%s  %s  rep=%s  dist=%s  inview=%s  alpha=%s  slot=%s  %s"),
            *Get_MemberIdText(InMember),
            *InMember.Name,
            *Get_RepLabel(InMember.Representation).ToString(),
            *Get_DistanceText(InMember),
            InMember.LastInView ? TEXT("true") : TEXT("false"),
            *Get_FadeText(InMember),
            *Get_SlotText(InMember),
            *InMember.Get_FlagsText());
    }

    auto Make_Dot(
        TAttribute<FSlateColor> InColor)
        -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(k_DotDiameter)
            .HeightOverride(k_DotDiameter)
            [
                SNew(SImage)
                .Image(CkStyle::GetRoundedBrush_Pill())
                .ColorAndOpacity(MoveTemp(InColor))
            ];
    }

    // ----------------------------------------------------------------------------------------------
    // One roster row.
    //
    // Every cell is STextBlock / SImage / SBox / SCkDebug_MeterBar — nothing that returns Handled on a left click,
    // so the STableRow underneath keeps its selection (CkDebuggerCommon/CLAUDE.md, "List / tree rows"). That is also
    // why the Entity cell prints the id as text rather than hosting an SCkDebug_EntityRef: the pill is a button, and
    // one inside a row makes the row unselectable. The detail rail carries the clickable pill instead.
    // ----------------------------------------------------------------------------------------------
    class SRosterRow final : public SMultiColumnTableRow<FMemberItemPtr>
    {
    public:
        SLATE_BEGIN_ARGS(SRosterRow) {}
            SLATE_ARGUMENT(FMemberItemPtr, Item)
            SLATE_ATTRIBUTE(float, DemoteDistance)
            /** A highlight query is active — rows that do not match dim, rows that do gain an accent edge. */
            SLATE_ATTRIBUTE(bool, HasHighlightQuery)
            SLATE_ATTRIBUTE(bool, IsHighlightMatch)
        SLATE_END_ARGS()

        auto
        Construct(
            const FArguments&                 InArgs,
            const TSharedRef<STableViewBase>& InOwnerTable) -> void;

        virtual auto
        GenerateWidgetForColumn(
            const FName& InColumnName) -> TSharedRef<SWidget> override;

    private:
        auto Make_FlagChip(TAttribute<bool> InVisible, TAttribute<FText> InText, FLinearColor InColor) const -> TSharedRef<SWidget>;

        FMemberItemPtr    _Item;
        TAttribute<float> _DemoteDistance;
        TAttribute<bool>  _HasHighlightQuery;
        TAttribute<bool>  _IsHighlightMatch;
    };

    auto
        SRosterRow::
        Construct(
            const FArguments&                 InArgs,
            const TSharedRef<STableViewBase>& InOwnerTable)
        -> void
    {
        _Item              = InArgs._Item;
        _DemoteDistance    = InArgs._DemoteDistance;
        _HasHighlightQuery = InArgs._HasHighlightQuery;
        _IsHighlightMatch  = InArgs._IsHighlightMatch;

        FSuperRowType::Construct(
            FSuperRowType::FArguments()
                .Style(&FCkDebuggerStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("CkDebugger.TableView.Row")))
                .ShowSelection(true)
                .Padding(FMargin{0.0f, 1.0f}),
            InOwnerTable);
    }

    auto
        SRosterRow::
        Make_FlagChip(
            TAttribute<bool>  InVisible,
            TAttribute<FText> InText,
            FLinearColor      InColor) const
        -> TSharedRef<SWidget>
    {
        const auto WeakItem = TWeakPtr<FCkVisualLodDebugger_MemberInfo>{_Item};

        return SNew(STextBlock)
            .Font_Static(&Get_MetaFont)
            .Text(MoveTemp(InText))
            .Visibility_Lambda([Visible = MoveTemp(InVisible)]()
            {
                return Visible.Get(false) ? EVisibility::Visible : EVisibility::Collapsed;
            })
            .ColorAndOpacity_Lambda([WeakItem, InColor]() -> FSlateColor
            {
                const auto Item = WeakItem.Pin();
                return FSlateColor(Item.IsValid() ? Apply_RowOpacity(*Item, InColor) : InColor);
            });
    }

    auto
        SRosterRow::
        GenerateWidgetForColumn(
            const FName& InColumnName)
        -> TSharedRef<SWidget>
    {
        const auto WeakItem = TWeakPtr<FCkVisualLodDebugger_MemberInfo>{_Item};

        // Every cell reads through this: a row's TSharedPtr survives refreshes and its CONTENTS are replaced in
        // place, so a cell that captured a value at build time would freeze at the values of one tick.
        const auto TextColor = [WeakItem](FLinearColor InBase)
        {
            return TAttribute<FSlateColor>::CreateLambda([WeakItem, InBase]() -> FSlateColor
            {
                const auto Item = WeakItem.Pin();
                return FSlateColor(Item.IsValid() ? Apply_RowOpacity(*Item, InBase) : InBase);
            });
        };

        if (InColumnName == k_Col_Entity)
        {
            return SNew(SHorizontalBox)
                // Highlight affordance — a bare tinted box, deliberately not a chip or a button: a click-consuming
                // widget here would eat the row's selection click.
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Fill)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SBox)
                    .WidthOverride(k_HighlightBarWide)
                    .Visibility_Lambda([this]()
                    {
                        return _HasHighlightQuery.Get(false) && _IsHighlightMatch.Get(false)
                            ? EVisibility::Visible
                            : EVisibility::Hidden;
                    })
                    [
                        SNew(SImage)
                        .Image(CkStyle::GetRoundedBrush_Small())
                        .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_RowFont)
                    .ColorAndOpacity(TextColor(CkStyle::EntityId()))
                    .Text_Lambda([WeakItem]() -> FText
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() ? FText::FromString(Get_MemberIdText(*Item)) : FText::GetEmpty();
                    })
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_RowFont)
                    .ColorAndOpacity_Lambda([this, WeakItem]() -> FSlateColor
                    {
                        const auto Item = WeakItem.Pin();
                        if (NOT Item.IsValid())
                        { return FSlateColor(CkStyle::Text()); }

                        const auto Dimmed = _HasHighlightQuery.Get(false) && NOT _IsHighlightMatch.Get(false);
                        return FSlateColor(Apply_RowOpacity(*Item, Dimmed ? CkStyle::TextMute() : CkStyle::Text()));
                    })
                    .Text_Lambda([WeakItem]() -> FText
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() ? FText::FromString(Item->Name) : FText::GetEmpty();
                    })
                ];
        }

        if (InColumnName == k_Col_Rep)
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    Make_Dot(TAttribute<FSlateColor>::CreateLambda([WeakItem]() -> FSlateColor
                    {
                        const auto Item = WeakItem.Pin();
                        if (NOT Item.IsValid())
                        { return FSlateColor(CkStyle::TextMute()); }

                        return FSlateColor(Apply_RowOpacity(*Item, Get_RepColor(Item->Representation)));
                    }))
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_MetaFont)
                    .ColorAndOpacity_Lambda([WeakItem]() -> FSlateColor
                    {
                        const auto Item = WeakItem.Pin();
                        if (NOT Item.IsValid())
                        { return FSlateColor(CkStyle::TextMute()); }

                        return FSlateColor(Apply_RowOpacity(*Item, Get_RepColor(Item->Representation)));
                    })
                    .Text_Lambda([WeakItem]() -> FText
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() ? Get_RepLabel(Item->Representation) : FText::GetEmpty();
                    })
                ];
        }

        if (InColumnName == k_Col_Distance)
        {
            return SNew(SBox)
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_RowFont)
                    .ColorAndOpacity_Lambda([this, WeakItem]() -> FSlateColor
                    {
                        const auto Item = WeakItem.Pin();
                        if (NOT Item.IsValid())
                        { return FSlateColor(CkStyle::Text()); }

                        // Past the demote band the number stops being a promote candidate's distance and starts
                        // being background, so it recedes rather than competing with the near members.
                        const auto Demote = _DemoteDistance.Get(0.0f);
                        const auto Beyond = Demote > 0.0f && Item->LastDistance > Demote;
                        return FSlateColor(Apply_RowOpacity(*Item, Beyond ? CkStyle::TextMute() : CkStyle::Text()));
                    })
                    .Text_Lambda([WeakItem]() -> FText
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() ? FText::FromString(Get_DistanceText(*Item)) : FText::GetEmpty();
                    })
                ];
        }

        if (InColumnName == k_Col_View)
        {
            return SNew(SBox)
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    Make_Dot(TAttribute<FSlateColor>::CreateLambda([WeakItem]() -> FSlateColor
                    {
                        const auto Item = WeakItem.Pin();
                        if (NOT Item.IsValid())
                        { return FSlateColor(CkStyle::BorderStrong()); }

                        return FSlateColor(Apply_RowOpacity(*Item,
                            Item->LastInView ? CkStyle::Ok() : CkStyle::BorderStrong()));
                    }))
                ];
        }

        if (InColumnName == k_Col_Fade)
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    // SCkDebug_MeterBar is an SLeafWidget with no children and no button, so it nests safely in a row.
                    SNew(SCkDebug_MeterBar)
                    .DesiredSize(FVector2D(k_FadeMeterWidth, k_FadeMeterHeight))
                    .Fraction_Lambda([WeakItem]()
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() ? Item->FadeAlpha : 0.0f;
                    })
                    .FillColor_Lambda([WeakItem]() -> FLinearColor
                    {
                        const auto Item = WeakItem.Pin();
                        if (NOT Item.IsValid())
                        { return CkStyle::TextMute(); }

                        return Apply_RowOpacity(*Item, Item->Get_IsFading() ? CkStyle::Info() : CkStyle::TextDim());
                    })
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_MetaFont)
                    .ColorAndOpacity(TextColor(CkStyle::TextDim()))
                    .Text_Lambda([WeakItem]() -> FText
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() ? FText::FromString(Get_FadeText(*Item)) : FText::GetEmpty();
                    })
                ];
        }

        if (InColumnName == k_Col_Slot)
        {
            return SNew(SBox)
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_RowFont)
                    .ColorAndOpacity(TextColor(CkStyle::TextDim()))
                    .Text_Lambda([WeakItem]() -> FText
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() ? FText::FromString(Get_SlotText(*Item)) : FText::GetEmpty();
                    })
                ];
        }

        // Flags. Text-only chips whose tone carries the severity — no borders, no buttons, nothing that could take
        // the row's click. Each is present in the tree for the row's life and shows itself when its flag is up.
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                Make_FlagChip(
                    TAttribute<bool>::CreateLambda([WeakItem]()
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() && Item->PromoteLockCount > 0;
                    }),
                    TAttribute<FText>::CreateLambda([WeakItem]() -> FText
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid()
                            ? FText::FromString(FString::Printf(TEXT("LOCK×%d"), Item->PromoteLockCount))
                            : FText::GetEmpty();
                    }),
                    CkStyle::Warn())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                Make_FlagChip(
                    TAttribute<bool>::CreateLambda([WeakItem]()
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() && Item->PromotedUnbudgeted;
                    }),
                    LOCTEXT("FlagUnbudgeted", "UNBUD"),
                    CkStyle::Err())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                Make_FlagChip(
                    TAttribute<bool>::CreateLambda([WeakItem]()
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() && Item->PreemptDemote;
                    }),
                    LOCTEXT("FlagPreempt", "PREEMPT"),
                    CkStyle::Info())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                Make_FlagChip(
                    TAttribute<bool>::CreateLambda([WeakItem]()
                    {
                        const auto Item = WeakItem.Pin();
                        return Item.IsValid() && Item->Hidden;
                    }),
                    LOCTEXT("FlagHidden", "HIDDEN"),
                    CkStyle::TextMute())
            ];
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _RingPromotes = MakeShared<TArray<float>>();
    _RingDemotes  = MakeShared<TArray<float>>();
    _RingPreempts = MakeShared<TArray<float>>();

    _RingPromotes->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize);
    _RingDemotes->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize);
    _RingPreempts->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize);

    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    _ViewportPicker = MakeShared<FCkDebug_ViewportPicker>();
    {
        auto PickerParams = FCkDebug_ViewportPicker::FParams{};
        PickerParams.Get_TargetWorld = [WeakPanel]() -> UWorld*
        {
            const auto Panel = WeakPanel.Pin();
            return Panel.IsValid() ? Panel->DoGet_TargetWorld() : nullptr;
        };
        PickerParams.TargetFilter = [](const FCk_Handle& InEntity)
        {
            return SCkVisualLodDebuggerWindow::Is_VisualLodPickCandidate(InEntity);
        };
        PickerParams.OnEntityPicked = [WeakPanel](const FCk_Handle& InEntity)
        {
            const auto Panel = WeakPanel.Pin();
            if (NOT Panel.IsValid())
            { return; }

            // A pick is a user-originated selection, so it goes through the same path a roster click does and
            // broadcasts — a programmatic selection that stays silent is the "world-pick doesn't show in the other
            // debuggers" defect (CkDebuggerCommon/CLAUDE.md, selection-sync inverse trap).
            Panel->TargetEntity(InEntity);
        };
        _ViewportPicker->Construct(MoveTemp(PickerParams));
    }

    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkVisualLodDebuggerWindow::HandleSessionInvalidated);
    _WorldInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnWorldInvalidated().AddSP(
        this, &SCkVisualLodDebuggerWindow::HandleWorldInvalidated);
    _SelectionSyncHandle = ck::DebugSelectionSync::Get_OnSelection().AddSP(
        this, &SCkVisualLodDebuggerWindow::HandleGlobalSelectionSync);

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
            .WindowId(WindowId)
            .ToolTabId(TEXT("CkVisualLodDebugger"))
            .ShowRefreshControls(true)
            .StatusText_Lambda([WeakPanel]()
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() ? Panel->DoGet_StatusText() : FText::GetEmpty();
            })
            .CommandGroups(DoBuild_CommandGroups())
            .CommonActionsContent()
            [
                SNew(SCkDebug_ViewportPickerControls)
                    .Picker(_ViewportPicker)
                    .PickTooltip(LOCTEXT("PickVisualLodTooltip",
                        "Pick a Visual LOD member or arbiter in the level viewport."))
            ]
            .Content()
            [
                SNew(SCkDebug_PaneHost)
                [
                    DoBuild_Body()
                ]
            ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

SCkVisualLodDebuggerWindow::~SCkVisualLodDebuggerWindow()
{
    if (_SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }

    if (_WorldInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Remove(_WorldInvalidatedHandle); }

    if (_SelectionSyncHandle.IsValid())
    { ck::DebugSelectionSync::Get_OnSelection().Remove(_SelectionSyncHandle); }

    if (_ViewportPicker.IsValid())
    { _ViewportPicker->Deactivate(); }

    // FCk_Handle is deliberately released while the ECS registries still exist.
    DoReset_WorldState();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    // MUST be the WindowBase super, not SCompoundWidget — the base Tick drives the gated style-revision
    // watch that routes into OnStyleRevisionChanged.
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    // UNGATED on purpose: the picker's hover affordance is an immediate-mode draw, and running it behind
    // the refresh gate makes it blink on every frame the gate declines.
    if (_ViewportPicker.IsValid())
    { _ViewportPicker->Tick(InDeltaTime); }

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _TickTimeSeconds = InCurrentTime;

    _Collector.Collect(DoGet_TargetWorld());

    DoUpdate_Live();
    DoPush_ActivitySample();

    // Order matters exactly once: the event log diffs against the state the PREVIOUS gated tick recorded, so it must
    // read _Live before anything else overwrites the previous-state map.
    DoUpdate_EventLog();
    DoApply_RosterPipeline();
    DoUpdate_Markers();

    if (const auto Signature = DoBuild_StructureSignature();
        Signature != _LastStructureSignature)
    {
        _LastStructureSignature = Signature;
        DoRebuild_DomainTabs();
        DoRebuild_CrowdPools();
    }

    if (const auto AlertSignature = DoBuild_AlertSignature();
        AlertSignature != _LastAlertSignature)
    {
        _LastAlertSignature = AlertSignature;
        DoRebuild_Alerts();
    }
}

auto
    SCkVisualLodDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    // The three signature-driven regions rebuild only when their own data changes, so with static data a
    // profile flip would never reach them. Poisoning both cached signatures is this window's "rebuild
    // once" lever; the next gated tick re-emits the tabs, the pool meters and the alert lane.
    _LastStructureSignature.Reset();
    _LastAlertSignature.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    Is_VisualLodPickCandidate(
        const FCk_Handle& InEntity)
    -> bool
{
    if (ck::Is_NOT_Valid(InEntity))
    { return false; }

    return UCk_Utils_VisualLod_UE::Has_Any(InEntity)
        || UCk_Utils_VisualLodArbiter_UE::Has_Any(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    DoGet_TargetWorld() const
    -> UWorld*
{
    if (ck::Is_NOT_Valid(GEngine))
    { return nullptr; }

    for (const auto& Context : GEngine->GetWorldContexts())
    {
        auto* World = Context.World();
        if (ck::Is_NOT_Valid(World))
        { continue; }

        if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
            && World->HasBegunPlay())
        { return World; }
    }

    return nullptr;
}

auto
    SCkVisualLodDebuggerWindow::
    DoGet_StatusText() const
    -> FText
{
    const auto& Snapshot = _Collector.Get_Snapshot();

    if (NOT Snapshot.HasWorld)
    { return LOCTEXT("NoWorld", "(no active PIE session — start Play In Editor to inspect visual LOD arbitration)"); }

    if (Snapshot.NumArbiters == 0)
    {
        // Visual LOD is a client-side rendering decision, so a dedicated-server world genuinely has no
        // arbiters to show. Saying so stops the empty window reading as a broken one.
        return LOCTEXT("NoArbiters",
            "(no Visual LOD arbiters in this world — note arbitration is client-side, so a dedicated server shows none)");
    }

    auto Status = FString::Printf(TEXT("%d arbiter(s) · %d member(s)"), Snapshot.NumArbiters, Snapshot.NumMembers);

    if (Snapshot.NumUnassignedMembers > 0)
    { Status += FString::Printf(TEXT(" · %d unassigned (arbiter tag unresolved)"), Snapshot.NumUnassignedMembers); }

    return FText::FromString(Status);
}

auto
    SCkVisualLodDebuggerWindow::
    DoReset_WorldState()
    -> void
{
    _Collector.Reset();

    // Every FCk_Handle this window owns lives on one of these lines. Handles hold the ECS registry by value, so one
    // that outlives its PIE registry AVs on destruct at the NEXT PIE start — the roster's row items and the marker
    // set's keys are handle containers exactly like the collector's snapshot.
    _MarkerSet.Reset();

    _RosterItems.Reset();
    if (_RosterListView.IsValid())
    {
        _RosterListView->ClearSelection();
        _RosterListView->RequestListRefresh();
    }

    _SelectedMember = FCk_Handle{};

    _MemberEventStates.Reset();
    _EventDomain      = NAME_None;
    _HasEventBaseline = false;
    _EventFrozen      = false;
    if (_EventLog.IsValid())
    { _EventLog->Clear_Entries(); }

    _Live           = FCkVisualLodDebugger_ArbiterInfo{};
    _HasLiveArbiter = false;
    _Tallies        = FTallies{};

    _TotalPromotes = 0;
    _TotalDemotes  = 0;
    _TotalPreempts = 0;

    if (_RingPromotes.IsValid()) { _RingPromotes->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize); }
    if (_RingDemotes.IsValid())  { _RingDemotes->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize); }
    if (_RingPreempts.IsValid()) { _RingPreempts->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize); }

    _LastStructureSignature.Reset();
    _LastAlertSignature.Reset();
}

auto
    SCkVisualLodDebuggerWindow::
    HandleSessionInvalidated()
    -> void
{
    if (_ViewportPicker.IsValid())
    { _ViewportPicker->Deactivate(); }

    DoReset_WorldState();
}

auto
    SCkVisualLodDebuggerWindow::
    HandleWorldInvalidated(
        UWorld* InWorld)
    -> void
{
    // World-scoped teardown: only release when the world going down is the one this window is reading,
    // so a second live world in the same process is not cleared out from under itself.
    if (ck::IsValid(InWorld) && InWorld != DoGet_TargetWorld())
    { return; }

    HandleSessionInvalidated();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    DoUpdate_Live()
    -> void
{
    const auto& Snapshot = _Collector.Get_Snapshot();

    const auto* Selected = Snapshot.Find_ByTabId(_SelectedDomain);

    if (Selected == nullptr && NOT Snapshot.Arbiters.IsEmpty())
    {
        Selected = &Snapshot.Arbiters[0];

        // The selected domain vanished (or this is the first populated tick). Running totals are
        // per-arbiter, so carrying them across a domain switch would attribute one domain's flips to
        // another.
        _SelectedDomain = Selected->TabId;
        _TotalPromotes  = 0;
        _TotalDemotes   = 0;
        _TotalPreempts  = 0;
    }

    if (Selected == nullptr)
    {
        _Live           = FCkVisualLodDebugger_ArbiterInfo{};
        _HasLiveArbiter = false;
        _Tallies        = FTallies{};
        return;
    }

    _Live           = *Selected;
    _HasLiveArbiter = true;

    _Tallies            = FTallies{};
    _Tallies.Members    = _Live.Members.Num();
    _Tallies.InView     = _Live.Get_InViewCount();
    _Tallies.Proxy      = _Live.Get_ProxyCount();
    _Tallies.Far        = _Live.Get_FarCount();
    _Tallies.Fading     = _Live.Get_FadingCount();
    _Tallies.Hidden     = _Live.Get_HiddenCount();
    _Tallies.Unrendered = _Live.Get_UnrenderedCount();
    _Tallies.UsedSlots  = _Live.Get_UsedSlotsTotal();
}

auto
    SCkVisualLodDebuggerWindow::
    DoPush_ActivitySample()
    -> void
{
    const auto Push = [](const TSharedPtr<TArray<float>>& InRing, int32 InValue)
    {
        if (NOT InRing.IsValid())
        { return; }

        InRing->Add(static_cast<float>(InValue));
        while (InRing->Num() > ck_visuallod_debugger_window::k_ActivityRingSize)
        { InRing->RemoveAt(0); }
    };

    // SAMPLED, not accumulated: the debugger has no hook on the arbiter update, so it reads the per-tick
    // counters at ITS refresh rate. Between two gated ticks the counters may have been reset several
    // times over, so both the rings and the totals are a trend rather than an exact flip history — which
    // is why the pane labels them "sampled".
    const auto Promotes = _HasLiveArbiter ? _Live.PromotesThisTick : 0;
    const auto Demotes  = _HasLiveArbiter ? _Live.DemotesThisTick  : 0;
    const auto Preempts = _HasLiveArbiter ? _Live.PreemptsThisTick : 0;

    Push(_RingPromotes, Promotes);
    Push(_RingDemotes,  Demotes);
    Push(_RingPreempts, Preempts);

    _TotalPromotes += Promotes;
    _TotalDemotes  += Demotes;
    _TotalPreempts += Preempts;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_StructureSignature() const
    -> FString
{
    // Arbiter identity set + selected-domain pool and render-band structure. Live values remain
    // attribute-bound; only a changed control topology warrants rebuilding the retained pool host.
    auto Signature = _SelectedDomain.ToString();

    for (const auto& Arbiter : _Collector.Get_Snapshot().Arbiters)
    { Signature.Appendf(TEXT("|%s"), *Arbiter.DomainTagName); }

    Signature.Appendf(TEXT("|pools=%d"), _Live.Crowds.Num());
    for (const auto& Crowd : _Live.Crowds)
    { Signature.Appendf(TEXT("|crowd=%d,bands=%d"), Crowd.CrowdIndex, Crowd.RuntimeTuners.Get_RenderBands().Num()); }

    return Signature;
}

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_AlertSignature() const
    -> FString
{
    if (NOT _HasLiveArbiter)
    { return FString(TEXT("none")); }

    const auto ViewUnresolved = NOT _Live.ViewValid;
    const auto PoolExhausted  = _Live.Get_AnyPoolExhausted();

    return FString::Printf(TEXT("%d%d"),
        ViewUnresolved ? 1 : 0,
        PoolExhausted ? 1 : 0);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_CommandGroups()
    -> TArray<FCkDebug_CommandGroup>
{
    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    const auto Toggles =
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SCkDebug_IconToggle)
            .IconId(ECk_Icon::Freeze)
            .Label(LOCTEXT("FreezeLabel", "Freeze"))
            .ShowLabel(true)
            .ToolTip(LOCTEXT("FreezeTooltip",
                "Freeze arbitration on the selected domain. The arbiter resolves nothing new — no promotes, "
                "demotes, preempts or far-anim updates — so you can eject and inspect the current assignment. "
                "In-flight fades still finish, then everything holds."))
            .IsEnabled_Lambda([WeakPanel]()
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() && Panel->_HasLiveArbiter && ck::IsValid(Panel->_Live.Entity);
            })
            .IsOn_Lambda([WeakPanel]()
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() && Panel->_Live.Frozen;
            })
            .OnStateChanged_Lambda([WeakPanel](const bool InFrozen)
            {
                const auto Panel = WeakPanel.Pin();
                if (NOT Panel.IsValid())
                { return; }

                auto Generic = Panel->_Live.Entity;
                if (ck::Is_NOT_Valid(Generic))
                { return; }

                auto Arbiter = UCk_Utils_VisualLodArbiter_UE::Cast(Generic);
                if (ck::Is_NOT_Valid(Arbiter))
                { return; }

                UCk_Utils_VisualLodArbiter_UE::Request_SetFrozen(Arbiter, InFrozen);
            })
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_IconToggle)
            .IconId(ECk_Icon::Shapes)
            .Label(LOCTEXT("MarkersLabel", "Markers"))
            .ShowLabel(true)
            .ToolTip(LOCTEXT("MarkersTooltip",
                "Draw world marker shapes over each member, coloured by representation — diamond over a "
                "near skel mesh, dot over a far GPU member, ring over an unrendered one."))
            .IsOn_Lambda([WeakPanel]()
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() && Panel->_MarkersEnabled;
            })
            .OnStateChanged_Lambda([WeakPanel](const bool InEnabled)
            {
                const auto Panel = WeakPanel.Pin();
                if (NOT Panel.IsValid())
                { return; }

                Panel->_MarkersEnabled = InEnabled;

                // Toggling off destroys the shapes immediately rather than waiting for the next gated tick — the
                // gate can be capped to a second, and a marker that lingers after the toggle reads as a stuck one.
                if (NOT InEnabled)
                { Panel->_MarkerSet.Reset(); }
            })
        ]
        ;

    return {
        FCkDebug_CommandGroup::Primary(
            TEXT("VisualLodToggles"),
            LOCTEXT("VisualLodTogglesLabel", "Visual LOD arbitration controls"),
            Toggles)
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_ArbiterTuners()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));
    const auto NoMaximum = TOptional<double>{};

    const auto MakeFloatRow = [WeakPanel, &NoMaximum](
        const FText& InLabel,
        const FText& InToolTip,
        TFunction<double(const FCk_VisualLodArbiter_RuntimeTuners&)> InGet,
        TFunction<void(FCk_VisualLodArbiter_RuntimeTuners&, float)> InSet,
        int32 InDigits,
        TOptional<double> InMinimum = TOptional<double>{0.0}) -> TSharedRef<SWidget>
    {
        return ck_visuallod_debugger_window::Make_TunerRow(
            InLabel,
            InToolTip,
            TAttribute<double>::CreateLambda([WeakPanel, Get = MoveTemp(InGet)]() -> double
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() ? Get(Panel->_Live.RuntimeTuners) : 0.0;
            }),
            ECkDebug_NumericKind::Float,
            InMinimum,
            NoMaximum,
            InDigits,
            FOnCkDebug_NumericCommitted::CreateLambda([WeakPanel, Set = MoveTemp(InSet)](double InValue)
            {
                const auto Panel = WeakPanel.Pin();
                if (NOT Panel.IsValid())
                { return; }

                Panel->DoRequest_RuntimeTuners([Set, InValue](FCk_VisualLodArbiter_RuntimeTuners& InTuners)
                { Set(InTuners, static_cast<float>(InValue)); });
            }));
    };

    const auto MakeIntegerRow = [WeakPanel, &NoMaximum](
        const FText& InLabel,
        const FText& InToolTip,
        TFunction<int32(const FCk_VisualLodArbiter_RuntimeTuners&)> InGet,
        TFunction<void(FCk_VisualLodArbiter_RuntimeTuners&, int32)> InSet) -> TSharedRef<SWidget>
    {
        return ck_visuallod_debugger_window::Make_TunerRow(
            InLabel,
            InToolTip,
            TAttribute<double>::CreateLambda([WeakPanel, Get = MoveTemp(InGet)]() -> double
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() ? static_cast<double>(Get(Panel->_Live.RuntimeTuners)) : 0.0;
            }),
            ECkDebug_NumericKind::Integer,
            TOptional<double>{0.0},
            NoMaximum,
            0,
            FOnCkDebug_NumericCommitted::CreateLambda([WeakPanel, Set = MoveTemp(InSet)](double InValue)
            {
                const auto Panel = WeakPanel.Pin();
                if (NOT Panel.IsValid())
                { return; }

                Panel->DoRequest_RuntimeTuners([Set, InValue](FCk_VisualLodArbiter_RuntimeTuners& InTuners)
                { Set(InTuners, FMath::Max(0, FMath::RoundToInt(InValue))); });
            }));
    };

    return SNew(SBox)
        .IsEnabled_Lambda([WeakPanel]()
        {
            const auto Panel = WeakPanel.Pin();
            return Panel.IsValid() && Panel->_HasLiveArbiter && Panel->_Live.HasConfig
                && ck::IsValid(Panel->_Live.Entity);
        })
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            .Padding(CkStyle::SpaceM)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Font_Static(&ck_visuallod_debugger_window::Get_MetaFont)
                        .ColorAndOpacity_Lambda([WeakPanel]()
                        {
                            const auto Panel = WeakPanel.Pin();
                            return Panel.IsValid() && Panel->_Live.Get_RuntimeTunersDifferFromAuthored()
                                ? CkStyle::Warn()
                                : CkStyle::TextMute();
                        })
                        .Text_Lambda([WeakPanel]() -> FText
                        {
                            const auto Panel = WeakPanel.Pin();
                            return Panel.IsValid() && Panel->_Live.Get_RuntimeTunersDifferFromAuthored()
                                ? LOCTEXT("RuntimeTunersOverridden", "session override")
                                : LOCTEXT("RuntimeTunersAuthored", "authored");
                        })
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
                [
                    SNew(STextBlock)
                    .AutoWrapText(true)
                    .Font_Static(&ck_visuallod_debugger_window::Get_MetaFont)
                    .ColorAndOpacity(CkStyle::TextDim())
                    .Text(LOCTEXT("RuntimeTunersDescription",
                        "Commits are deferred to the selected arbiter. They affect this session only; the config "
                        "asset and structural crowd/profile setup remain unchanged."))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeIntegerRow(
                        LOCTEXT("TunerNearBudget", "Near budget"),
                        LOCTEXT("TunerNearBudgetTip",
                            "Maximum ranked near-camera promotes. Lowering below current usage blocks new "
                            "admissions; existing proxies leave through normal demotion or preemption."),
                        [](const auto& InTuners) { return InTuners.Get_NearBudget(); },
                        [](auto& InTuners, int32 InValue) { InTuners.Set_NearBudget(InValue); })
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeIntegerRow(
                        LOCTEXT("TunerLockBudget", "Lock budget"),
                        LOCTEXT("TunerLockBudgetTip",
                            "Reserved capacity for new lock-driven promotes. Existing held locks are never evicted."),
                        [](const auto& InTuners) { return InTuners.Get_LockBudget(); },
                        [](auto& InTuners, int32 InValue) { InTuners.Set_LockBudget(InValue); })
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeFloatRow(
                        LOCTEXT("TunerPromoteDistance", "Promote distance (cm)"),
                        LOCTEXT("TunerPromoteDistanceTip",
                            "Members nearer than this may promote. Values above the current demote distance clamp to it."),
                        [](const auto& InTuners) { return static_cast<double>(InTuners.Get_PromoteDistance()); },
                        [](auto& InTuners, float InValue)
                        { InTuners.Set_PromoteDistance(FMath::Min(InValue, InTuners.Get_DemoteDistance())); }, 0)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeFloatRow(
                        LOCTEXT("TunerDemoteDistance", "Demote distance (cm)"),
                        LOCTEXT("TunerDemoteDistanceTip",
                            "Promoted members beyond this distance demote. Values below the current promote distance clamp to it."),
                        [](const auto& InTuners) { return static_cast<double>(InTuners.Get_DemoteDistance()); },
                        [](auto& InTuners, float InValue)
                        { InTuners.Set_DemoteDistance(FMath::Max(InValue, InTuners.Get_PromoteDistance())); }, 0)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeFloatRow(
                        LOCTEXT("TunerLockDistance", "Lock max distance (cm)"),
                        LOCTEXT("TunerLockDistanceTip", "Maximum distance at which a held promote lock may start a promote."),
                        [](const auto& InTuners) { return static_cast<double>(InTuners.Get_LockPromoteMaxDistance()); },
                        [](auto& InTuners, float InValue) { InTuners.Set_LockPromoteMaxDistance(InValue); }, 0)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeFloatRow(
                        LOCTEXT("TunerAlwaysInView", "Always in view (cm)"),
                        LOCTEXT("TunerAlwaysInViewTip", "Members this close rank as in-view regardless of facing."),
                        [](const auto& InTuners) { return static_cast<double>(InTuners.Get_AlwaysInViewDistance()); },
                        [](auto& InTuners, float InValue) { InTuners.Set_AlwaysInViewDistance(InValue); }, 0)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeFloatRow(
                        LOCTEXT("TunerViewMargin", "View margin (deg)"),
                        LOCTEXT("TunerViewMarginTip", "Extra angle outside the camera FOV that still ranks as in-view."),
                        [](const auto& InTuners) { return static_cast<double>(InTuners.Get_ViewConeMarginDeg()); },
                        [](auto& InTuners, float InValue) { InTuners.Set_ViewConeMarginDeg(InValue); }, 1)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeFloatRow(
                        LOCTEXT("TunerPreemptMargin", "Preempt margin (cm)"),
                        LOCTEXT("TunerPreemptMarginTip", "Distance advantage a challenger needs to displace an incumbent."),
                        [](const auto& InTuners) { return static_cast<double>(InTuners.Get_PreemptDistanceMargin()); },
                        [](auto& InTuners, float InValue) { InTuners.Set_PreemptDistanceMargin(InValue); }, 0)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeIntegerRow(
                        LOCTEXT("TunerMaxPreempts", "Max preempts / tick"),
                        LOCTEXT("TunerMaxPreemptsTip", "Maximum incumbent replacements begun by one arbiter update."),
                        [](const auto& InTuners) { return InTuners.Get_MaxPreemptsPerTick(); },
                        [](auto& InTuners, int32 InValue) { InTuners.Set_MaxPreemptsPerTick(InValue); })
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeFloatRow(
                        LOCTEXT("TunerFadeDuration", "Fade duration (s)"),
                        LOCTEXT("TunerFadeDurationTip", "Duration of newly started promote and demote crossfades."),
                        [](const auto& InTuners) { return InTuners.Get_FadeDuration().Get_Seconds(); },
                        [](auto& InTuners, float InValue) { InTuners.Set_FadeDuration(FCk_Time{InValue}); }, 3)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeFloatRow(
                        LOCTEXT("TunerFadeAnchorLead", "Fade anchor lead (frames)"),
                        LOCTEXT("TunerFadeAnchorLeadTip", "Frame lead used to align the promoted mesh with the far crowd clock."),
                        [](const auto& InTuners) { return InTuners.Get_FadeAnchorLeadFrames(); },
                        [](auto& InTuners, float InValue) { InTuners.Set_FadeAnchorLeadFrames(InValue); }, 2, TOptional<double>{})
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeFloatRow(
                        LOCTEXT("TunerFadeAnchorLag", "Fade anchor bake lag"),
                        LOCTEXT("TunerFadeAnchorLagTip", "Bake-interval lag used to align the promoted mesh with the sampled far pose."),
                        [](const auto& InTuners) { return InTuners.Get_FadeAnchorBakeLagIntervals(); },
                        [](auto& InTuners, float InValue) { InTuners.Set_FadeAnchorBakeLagIntervals(InValue); }, 2, TOptional<double>{})
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SCkDebug_KeyValueRow)
                    .KeyText(LOCTEXT("TunerExhaustion", "Pool exhaustion"))
                    .Tone(ECkDebug_KeyValueTone::Custom)
                    .CustomValueColor(CkStyle::Text())
                    .ValueWidget()
                    [
                        SNew(SSegmentedControl<ECk_VisualLod_PoolExhaustionPolicy>)
                        .Value_Lambda([WeakPanel]()
                        {
                            const auto Panel = WeakPanel.Pin();
                            return Panel.IsValid()
                                ? Panel->_Live.RuntimeTuners.Get_ExhaustionPolicy()
                                : ECk_VisualLod_PoolExhaustionPolicy::PromoteInstead;
                        })
                        .OnValueChanged_Lambda([WeakPanel](ECk_VisualLod_PoolExhaustionPolicy InPolicy)
                        {
                            const auto Panel = WeakPanel.Pin();
                            if (Panel.IsValid())
                            {
                                Panel->DoRequest_RuntimeTuners([InPolicy](auto& InTuners)
                                { InTuners.Set_ExhaustionPolicy(InPolicy); });
                            }
                        })
                        + SSegmentedControl<ECk_VisualLod_PoolExhaustionPolicy>::Slot(
                            ECk_VisualLod_PoolExhaustionPolicy::PromoteInstead)
                            .Text(LOCTEXT("TunerExhaustionPromote", "Promote"))
                        + SSegmentedControl<ECk_VisualLod_PoolExhaustionPolicy>::Slot(
                            ECk_VisualLod_PoolExhaustionPolicy::Unrendered)
                            .Text(LOCTEXT("TunerExhaustionUnrendered", "Unrendered"))
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("RuntimeTunersReset", "Reset to authored"))
                    .ToolTipText(LOCTEXT("RuntimeTunersResetTip", "Restore every runtime tuner from the arbiter config asset."))
                    .IsEnabled_Lambda([WeakPanel]()
                    {
                        const auto Panel = WeakPanel.Pin();
                        return Panel.IsValid() && Panel->_Live.Get_RuntimeTunersDifferFromAuthored();
                    })
                    .OnClicked(this, &SCkVisualLodDebuggerWindow::DoRequest_ResetRuntimeTuners)
                ]
            ]
        ];
}

auto
    SCkVisualLodDebuggerWindow::
    DoRequest_RuntimeTuners(
        TFunctionRef<void(FCk_VisualLodArbiter_RuntimeTuners&)> InMutate)
    -> void
{
    auto Generic = _Live.Entity;
    if (ck::Is_NOT_Valid(Generic))
    { return; }

    auto Arbiter = UCk_Utils_VisualLodArbiter_UE::Cast(Generic);
    if (ck::Is_NOT_Valid(Arbiter))
    { return; }

    auto Tuners = _Live.RuntimeTuners;
    InMutate(Tuners);

    // This is deliberately the handle-based full validator, not the scalar-only helper: band order,
    // sequence bounds, rate ranges, and profile invariants must reject before either enqueue or the
    // optimistic UI copy. A rejected number consequently redraws from the unchanged snapshot.
    if (NOT UCk_Utils_VisualLodArbiter_UE::Get_AreRuntimeTunersValid(Arbiter, Tuners))
    { return; }

    UCk_Utils_VisualLodArbiter_UE::Request_SetRuntimeTuners(
        Arbiter, FCk_Request_VisualLodArbiter_SetRuntimeTuners{Tuners}, {});
    _Live.Set_RuntimeTuners(Tuners);
}

auto
    SCkVisualLodDebuggerWindow::
    DoRequest_CrowdTuners(
        int32 InCrowdIndex,
        TFunctionRef<void(FCk_VisualLod_RuntimeCrowdTuners&)> InMutate)
    -> void
{
    DoRequest_RuntimeTuners([InCrowdIndex, InMutate](FCk_VisualLodArbiter_RuntimeTuners& InTuners)
    {
        auto Crowds = InTuners.Get_CrowdTuners();
        if (NOT Crowds.IsValidIndex(InCrowdIndex))
        { return; }

        auto Crowd = Crowds[InCrowdIndex];
        InMutate(Crowd);
        Crowds[InCrowdIndex] = MoveTemp(Crowd);
        InTuners.Set_CrowdTuners(Crowds);
    });
}

auto
    SCkVisualLodDebuggerWindow::
    DoRequest_ProfileTuners(
        int32 InCrowdIndex,
        int32 InBandIndex,
        TFunctionRef<void(FCk_IskmRenderer_RuntimeProfileTuners&)> InMutate)
    -> void
{
    if (InCrowdIndex == INDEX_NONE || InBandIndex == INDEX_NONE)
    { return; }

    DoRequest_CrowdTuners(InCrowdIndex, [InBandIndex, InMutate](FCk_VisualLod_RuntimeCrowdTuners& InCrowd)
    {
        auto Bands = InCrowd.Get_RenderBands();
        if (NOT Bands.IsValidIndex(InBandIndex))
        { return; }

        auto Band = Bands[InBandIndex];
        auto Profile = Band.Get_ProfileTuners();
        InMutate(Profile);
        Band.Set_ProfileTuners(Profile);
        Bands[InBandIndex] = MoveTemp(Band);
        InCrowd.Set_RenderBands(Bands);
    });
}

auto
    SCkVisualLodDebuggerWindow::
    DoRequest_ResetRuntimeTuners()
    -> FReply
{
    auto Generic = _Live.Entity;
    if (ck::Is_NOT_Valid(Generic))
    { return FReply::Handled(); }

    auto Arbiter = UCk_Utils_VisualLodArbiter_UE::Cast(Generic);
    if (ck::Is_NOT_Valid(Arbiter))
    { return FReply::Handled(); }

    UCk_Utils_VisualLodArbiter_UE::Request_ResetRuntimeTuners(
        Arbiter, FCk_Request_VisualLodArbiter_ResetRuntimeTuners{}, {});
    _Live.Set_RuntimeTuners(_Live.AuthoredRuntimeTuners);
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_Body()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));
    const auto TunerSlotSizeRule = TAttribute<SSplitter::ESizeRule>::CreateLambda([WeakPanel]()
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->_TunersExpanded
            ? SSplitter::FractionOfParent
            : SSplitter::SizeToContent;
    });
    const auto TunerSlotValue = TAttribute<float>::CreateLambda([WeakPanel]()
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->_TunersExpanded ? 0.42f : 0.0f;
    });
    const auto TunerSlotMinSize = TAttribute<float>::CreateLambda([WeakPanel]()
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->_TunersExpanded ? 170.0f : 0.0f;
    });

    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SAssignNew(_AlertBox, SVerticalBox)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SAssignNew(_DomainTabsHost, SBox)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceL, CkStyle::SpaceM, CkStyle::SpaceL, CkStyle::SpaceS)
        [
            DoBuild_ArbiterHeader()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceL, 0.0f, CkStyle::SpaceL, CkStyle::SpaceM)
        [
            DoBuild_StatStrip()
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Vertical)
            .PhysicalSplitterHandleSize(5.0f)
            + SSplitter::Slot()
            .Value(0.30f)
            .MinSize(190.0f)
            [
                DoBuild_OverviewGrid()
            ]
            + SSplitter::Slot()
            .Value(0.70f)
            .MinSize(280.0f)
            [
                // The disclosure owns only the header. Its workspace below joins the investigation
                // row in a retained splitter, so collapse removes both its body and drag handle.
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(CkStyle::SpaceL, 0.0f, CkStyle::SpaceL, CkStyle::SpaceS)
                [
                    DoBuild_TunersRow()
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SSplitter)
                    .Orientation(Orient_Vertical)
                    .PhysicalSplitterHandleSize(5.0f)
                    + SSplitter::Slot()
                    .SizeRule(TunerSlotSizeRule)
                    .Value(TunerSlotValue)
                    .MinSize(TunerSlotMinSize)
                    [
                        SNew(SBox)
                        .Clipping(EWidgetClipping::ClipToBounds)
                        .Visibility_Lambda([WeakPanel]()
                        {
                            const auto Panel = WeakPanel.Pin();
                            return Panel.IsValid() && Panel->_TunersExpanded
                                ? EVisibility::Visible
                                : EVisibility::Collapsed;
                        })
                        [ DoBuild_TunersWorkspace() ]
                    ]
                    + SSplitter::Slot()
                    .Value(0.58f)
                    .MinSize(250.0f)
                    [
                        SNew(SSplitter)
                        .Orientation(Orient_Horizontal)
                        .PhysicalSplitterHandleSize(5.0f)
                        + SSplitter::Slot()
                        .Value(0.72f)
                        .MinSize(360.0f)
                        [
                            SAssignNew(_RosterPaneBox, SVerticalBox)
                            + SVerticalBox::Slot().FillHeight(1.0f)
                            [ DoBuild_RosterPane() ]
                        ]
                        + SSplitter::Slot()
                        .Value(0.28f)
                        .MinSize(260.0f)
                        [
                            SAssignNew(_DetailRailBox, SVerticalBox)
                            + SVerticalBox::Slot().FillHeight(1.0f)
                            [
                                SNew(SSplitter)
                                .Orientation(Orient_Vertical)
                                .PhysicalSplitterHandleSize(5.0f)
                                + SSplitter::Slot().Value(0.62f).MinSize(180.0f)
                                [ SNew(SScrollBox) + SScrollBox::Slot()[ DoBuild_DetailRail() ] ]
                                + SSplitter::Slot().Value(0.38f).MinSize(130.0f)
                                [ DoBuild_EventLog() ]
                            ]
                        ]
                    ]
                ]
            ]
        ];
}

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_TunersRow()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    return SNew(SCkDebug_InspectorPanel)
        .Title(LOCTEXT("TunersWorkspaceHeading", "Tuners"))
        .CountText(LOCTEXT("TunersWorkspaceHint", "session overrides"))
        .StartExpanded(false)
        .OnToggled(FOnCkDebugInspectorToggled::CreateLambda([WeakPanel](bool InExpanded)
        {
            const auto Panel = WeakPanel.Pin();
            if (Panel.IsValid()) { Panel->_TunersExpanded = InExpanded; }
        }))
        .Body()
        [
            SNullWidget::NullWidget
        ];
}

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_TunersWorkspace()
    -> TSharedRef<SWidget>
{
    return SNew(SSplitter)
        .Orientation(Orient_Horizontal)
        .PhysicalSplitterHandleSize(5.0f)
        .Clipping(EWidgetClipping::ClipToBounds)
        + SSplitter::Slot().Value(1.0f).MinSize(280.0f)
        [
            SNew(SBox)
            .Clipping(EWidgetClipping::ClipToBounds)
            [ DoBuild_ArbiterTuners() ]
        ]
        + SSplitter::Slot().Value(1.25f).MinSize(300.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [ SAssignNew(_CrowdTunerBox, SVerticalBox) ]
        ];
}

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_ArbiterHeader()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Font_Static(&ck_visuallod_debugger_window::Get_HeadingFont)
            .ColorAndOpacity(CkStyle::PaneHeadingColor())
            .Text(LOCTEXT("ArbiterHeading", "ARBITER"))
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_EntityRef)
            .ShowName(true)
            .Entity_Lambda([WeakPanel]() -> FCk_Handle
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() ? Panel->_Live.Entity : FCk_Handle{};
            })
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_StatusPill)
            .ShowDot(true)
            .Text_Lambda([WeakPanel]() -> FText
            {
                const auto Panel = WeakPanel.Pin();
                if (NOT Panel.IsValid() || NOT Panel->_HasLiveArbiter)
                { return LOCTEXT("PillNoArbiter", "No arbiter"); }

                // Frozen first: a frozen arbiter resolves nothing, so every other state below it is a
                // stale reading of a domain that is deliberately not deciding anything.
                if (Panel->_Live.Frozen)
                { return LOCTEXT("PillFrozen", "Frozen — flips suspended"); }

                if (NOT Panel->_Live.ViewValid)
                { return LOCTEXT("PillViewInvalid", "View invalid — idle"); }

                if (Panel->_Live.Get_AnyPoolExhausted())
                { return LOCTEXT("PillPoolExhausted", "Pool exhausted"); }

                return LOCTEXT("PillArbitrating", "Arbitrating");
            })
            .Tone_Lambda([WeakPanel]() -> ECk_Tone
            {
                const auto Panel = WeakPanel.Pin();
                if (NOT Panel.IsValid() || NOT Panel->_HasLiveArbiter)
                { return ECk_Tone::Neutral; }

                if (Panel->_Live.Frozen)
                { return ECk_Tone::Info; }

                if (NOT Panel->_Live.ViewValid)
                { return ECk_Tone::Warn; }

                if (Panel->_Live.Get_AnyPoolExhausted())
                { return ECk_Tone::Err; }

                return ECk_Tone::Ok;
            })
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Font_Static(&ck_visuallod_debugger_window::Get_MetaFont)
            .ColorAndOpacity(CkStyle::TextMute())
            .Text_Lambda([WeakPanel]() -> FText
            {
                const auto Panel = WeakPanel.Pin();
                if (NOT Panel.IsValid() || NOT Panel->_HasLiveArbiter)
                { return FText::GetEmpty(); }

                const auto Config = Panel->_Live.HasConfig
                    ? Panel->_Live.ConfigName
                    : FString(TEXT("(config unresolved)"));

                return FText::FromString(FString::Printf(TEXT("%s · %d crowd pool(s)"),
                    *Config, Panel->_Live.Crowds.Num()));
            })
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_StatStrip()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    // One reader for every cell: pin once, read the cached tallies. Bind the value and the meta line
    // separately so a cell's headline number and its explanation can never disagree by a frame.
    const auto MakeCount = [WeakPanel](TFunction<int32(const FTallies&)> InSelector)
    {
        return TAttribute<FText>::CreateLambda([WeakPanel, Selector = MoveTemp(InSelector)]() -> FText
        {
            const auto Panel = WeakPanel.Pin();
            return FText::AsNumber(Panel.IsValid() ? Selector(Panel->_Tallies) : 0);
        });
    };

    const auto MakeMeta = [WeakPanel](TFunction<FString(const SCkVisualLodDebuggerWindow&)> InSelector)
    {
        return TAttribute<FText>::CreateLambda([WeakPanel, Selector = MoveTemp(InSelector)]() -> FText
        {
            const auto Panel = WeakPanel.Pin();
            return Panel.IsValid() ? FText::FromString(Selector(*Panel)) : FText::GetEmpty();
        });
    };

    const auto Strip = SNew(SHorizontalBox);

    const auto AddCard = [&Strip](TSharedRef<SWidget> InCard)
    {
        Strip->AddSlot()
            .FillWidth(1.0f)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                InCard
            ];
    };

    AddCard(ck_visuallod_debugger_window::Make_StatCard(
        LOCTEXT("StatMembers", "Members"),
        MakeCount([](const FTallies& InTallies) { return InTallies.Members; }),
        MakeMeta([](const SCkVisualLodDebuggerWindow& InPanel)
        { return FString::Printf(TEXT("%d in view"), InPanel._Tallies.InView); }),
        FSlateColor(CkStyle::TextStrong()),
        CkStyle::TextMute()));

    AddCard(ck_visuallod_debugger_window::Make_StatCard(
        LOCTEXT("StatProxy", "SkelMesh (near)"),
        MakeCount([](const FTallies& InTallies) { return InTallies.Proxy; }),
        MakeMeta([](const SCkVisualLodDebuggerWindow& InPanel)
        {
            // The charge split is the number that explains a promote the near budget cannot account for:
            // lock and unbudgeted promotes are charged to neither the near budget nor each other.
            return FString::Printf(TEXT("near %d · lock %d · unbud %d"),
                InPanel._Live.NearPromotedCount,
                InPanel._Live.LockedPromotedCount,
                InPanel._Live.UnbudgetedPromotedCount);
        }),
        FSlateColor(CkStyle::Accent()),
        CkStyle::Accent()));

    AddCard(ck_visuallod_debugger_window::Make_StatCard(
        LOCTEXT("StatFar", "GPU crowd (far)"),
        MakeCount([](const FTallies& InTallies) { return InTallies.Far; }),
        MakeMeta([](const SCkVisualLodDebuggerWindow& InPanel)
        { return FString::Printf(TEXT("%d slots used"), InPanel._Tallies.UsedSlots); }),
        FSlateColor(CkStyle::Ok()),
        CkStyle::Ok()));

    AddCard(ck_visuallod_debugger_window::Make_StatCard(
        LOCTEXT("StatFading", "Fading"),
        MakeCount([](const FTallies& InTallies) { return InTallies.Fading; }),
        MakeMeta([](const SCkVisualLodDebuggerWindow& InPanel)
        { return FString::Printf(TEXT("%.2f s dither"), InPanel._Live.FadeDurationSeconds); }),
        TAttribute<FSlateColor>::CreateLambda([WeakPanel]() -> FSlateColor
        {
            const auto Panel = WeakPanel.Pin();
            const auto Fading = Panel.IsValid() && Panel->_Tallies.Fading > 0;
            return FSlateColor(Fading ? CkStyle::Info() : CkStyle::TextStrong());
        }),
        CkStyle::Info()));

    AddCard(ck_visuallod_debugger_window::Make_StatCard(
        LOCTEXT("StatHidden", "Hidden"),
        MakeCount([](const FTallies& InTallies) { return InTallies.Hidden; }),
        MakeMeta([](const SCkVisualLodDebuggerWindow&) { return FString(TEXT("suspended")); }),
        FSlateColor(CkStyle::TextStrong()),
        CkStyle::BorderStrong()));

    AddCard(ck_visuallod_debugger_window::Make_StatCard(
        LOCTEXT("StatUnrendered", "Unrendered"),
        MakeCount([](const FTallies& InTallies) { return InTallies.Unrendered; }),
        MakeMeta([](const SCkVisualLodDebuggerWindow& InPanel)
        { return InPanel._Tallies.Unrendered > 0 ? FString(TEXT("pool starved")) : FString(TEXT("—")); }),
        TAttribute<FSlateColor>::CreateLambda([WeakPanel]() -> FSlateColor
        {
            const auto Panel = WeakPanel.Pin();
            const auto Starved = Panel.IsValid() && Panel->_Tallies.Unrendered > 0;
            return FSlateColor(Starved ? CkStyle::Err() : CkStyle::TextStrong());
        }),
        TAttribute<FLinearColor>::CreateLambda([WeakPanel]() -> FLinearColor
        {
            const auto Panel = WeakPanel.Pin();
            const auto Starved = Panel.IsValid() && Panel->_Tallies.Unrendered > 0;
            return Starved ? CkStyle::Err() : CkStyle::BorderStrong();
        })));

    return Strip;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_OverviewGrid()
    -> TSharedRef<SWidget>
{
    return SNew(SSplitter)
        .Orientation(Orient_Horizontal)
        .PhysicalSplitterHandleSize(5.0f)
        + SSplitter::Slot()
        .Value(1.15f)
        .MinSize(280.0f)
        [
            SNew(SBox)
            .MaxDesiredWidth(280.0f)
            .Clipping(EWidgetClipping::ClipToBounds)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [ DoBuild_BudgetsPane() ]
            ]
        ]
        + SSplitter::Slot()
        .Value(1.0f)
        .MinSize(260.0f)
        [
            SNew(SBox)
            .MaxDesiredWidth(260.0f)
            .Clipping(EWidgetClipping::ClipToBounds)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [ DoBuild_ViewPane() ]
            ]
        ]
        + SSplitter::Slot()
        .Value(1.0f)
        .MinSize(240.0f)
        [
            SNew(SBox)
            .MaxDesiredWidth(240.0f)
            .Clipping(EWidgetClipping::ClipToBounds)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [ DoBuild_ActivityPane() ]
            ]
        ];
}

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_BudgetsPane()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    return SNew(SCkDebug_Card)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
            [
                ck_visuallod_debugger_window::Make_PaneHeading(
                    LOCTEXT("BudgetsHeading", "Budgets"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid())
                        { return FText::GetEmpty(); }

                        const auto Headroom = FMath::Max(0, Panel->_Live.NearBudget - Panel->_Live.NearPromotedCount);
                        return FText::FromString(FString::Printf(TEXT("headroom %d"), Headroom));
                    }))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                ck_visuallod_debugger_window::Make_MeterRow(
                    LOCTEXT("BudgetNear", "Near"),
                    TAttribute<float>::CreateLambda([WeakPanel]()
                    {
                        const auto Panel = WeakPanel.Pin();
                        return Panel.IsValid()
                            ? ck_visuallod_debugger_window::Get_Fraction(Panel->_Live.NearPromotedCount, Panel->_Live.NearBudget)
                            : 0.0f;
                    }),
                    CkStyle::Accent(),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid())
                        { return FText::GetEmpty(); }
                        return FText::FromString(FString::Printf(TEXT("%d / %d"),
                            Panel->_Live.NearPromotedCount, Panel->_Live.NearBudget));
                    }),
                    LOCTEXT("BudgetNearTip", "Ranked near-camera promotes charged to the near budget."))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                ck_visuallod_debugger_window::Make_MeterRow(
                    LOCTEXT("BudgetLock", "Lock"),
                    TAttribute<float>::CreateLambda([WeakPanel]()
                    {
                        const auto Panel = WeakPanel.Pin();
                        return Panel.IsValid()
                            ? ck_visuallod_debugger_window::Get_Fraction(Panel->_Live.LockedPromotedCount, Panel->_Live.LockBudget)
                            : 0.0f;
                    }),
                    CkStyle::Warn(),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid())
                        { return FText::GetEmpty(); }
                        return FText::FromString(FString::Printf(TEXT("%d / %d"),
                            Panel->_Live.LockedPromotedCount, Panel->_Live.LockBudget));
                    }),
                    LOCTEXT("BudgetLockTip",
                        "Lock-driven promotes (ragdoll / montage holders). Reserved independently of the near "
                        "budget in both directions."))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_MeterRow(
                    LOCTEXT("BudgetUnbudgeted", "Unbudgeted"),
                    TAttribute<float>::CreateLambda([WeakPanel]()
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid())
                        { return 0.0f; }

                        // Unbudgeted promotes are charged to no budget, so there is no denominator to
                        // divide by. Scaling against the near budget gives the bar a stable meaning:
                        // "how big is the off-budget cost next to the budgeted one".
                        return ck_visuallod_debugger_window::Get_Fraction(
                            Panel->_Live.UnbudgetedPromotedCount, FMath::Max(1, Panel->_Live.NearBudget));
                    }),
                    CkStyle::Err(),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        return FText::AsNumber(Panel.IsValid() ? Panel->_Live.UnbudgetedPromotedCount : 0);
                    }),
                    LOCTEXT("BudgetUnbudgetedTip",
                        "AlwaysPromoted entities and pool-exhaustion fallbacks — promoted, but charged to no "
                        "budget. Shown against the near budget for scale."))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                // The overview owns the one scroll viewport for this whole card. A second nested scroll box
                // here would receive unbounded height from its parent and fight it for wheel input.
                SAssignNew(_CrowdPoolBox, SVerticalBox)
            ]
        ];
}

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_ViewPane()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    return SNew(SCkDebug_Card)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                ck_visuallod_debugger_window::Make_PaneHeading(
                    LOCTEXT("ViewHeading", "View"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid() || NOT Panel->_HasLiveArbiter)
                        { return FText::GetEmpty(); }

                        if (NOT Panel->_Live.ViewValid)
                        { return LOCTEXT("ViewUnresolvedChip", "unresolved"); }

                        // Which source produced these numbers is half the diagnosis when they look wrong.
                        return Panel->_Live.HasExplicitObserver
                            ? LOCTEXT("ViewExplicitChip", "explicit observer")
                            : LOCTEXT("ViewLocalChip", "local-view discovery");
                    }))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("ViewValid", "Valid"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        return ck_visuallod_debugger_window::Get_BoolText(Panel.IsValid() && Panel->_Live.ViewValid);
                    }),
                    TAttribute<FLinearColor>::CreateLambda([WeakPanel]() -> FLinearColor
                    {
                        const auto Panel = WeakPanel.Pin();
                        const auto Valid = Panel.IsValid() && Panel->_Live.ViewValid;
                        return Valid ? CkStyle::Ok() : CkStyle::Err();
                    }))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("ViewLocation", "Location"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid() || NOT Panel->_Live.ViewValid)
                        { return LOCTEXT("Unset", "—"); }

                        const auto& Loc = Panel->_Live.ViewLocation;
                        return FText::FromString(FString::Printf(TEXT("X %.0f  Y %.0f  Z %.0f"), Loc.X, Loc.Y, Loc.Z));
                    }),
                    CkStyle::Text())
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("ViewForward", "Forward"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid() || NOT Panel->_Live.ViewValid)
                        { return LOCTEXT("Unset", "—"); }

                        const auto& Fwd = Panel->_Live.ViewForward;
                        return FText::FromString(FString::Printf(TEXT("X %.2f  Y %.2f  Z %.2f"), Fwd.X, Fwd.Y, Fwd.Z));
                    }),
                    CkStyle::Text())
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("ViewCone", "Cone"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid() || NOT Panel->_Live.ViewValid)
                        { return LOCTEXT("Unset", "—"); }

                        return FText::FromString(FString::Printf(TEXT("%.0f° full  (cos %.3f · margin %.0f°)"),
                            Panel->_Live.Get_ViewConeDegrees(),
                            Panel->_Live.ViewCosHalfCone,
                            Panel->_Live.ViewConeMarginDeg));
                    }),
                    CkStyle::Text())
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("ViewObserver", "Observer"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid())
                        { return LOCTEXT("Unset", "—"); }

                        return Panel->_Live.HasExplicitObserver
                            ? LOCTEXT("ObserverExplicit", "explicit")
                            : LOCTEXT("ObserverLocalView", "local-view discovery");
                    }),
                    CkStyle::TextDim())
            ]
        ];
}

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_ActivityPane()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    const auto Latest = [WeakPanel](TFunction<int32(const FCkVisualLodDebugger_ArbiterInfo&)> InSelector)
    {
        return TAttribute<FText>::CreateLambda([WeakPanel, Selector = MoveTemp(InSelector)]() -> FText
        {
            const auto Panel = WeakPanel.Pin();
            return FText::AsNumber(Panel.IsValid() ? Selector(Panel->_Live) : 0);
        });
    };

    return SNew(SCkDebug_Card)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
            [
                ck_visuallod_debugger_window::Make_PaneHeading(
                    LOCTEXT("ActivityHeading", "Activity"),
                    LOCTEXT("ActivityRight", "sampled @ refresh · last 60"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
            [
                ck_visuallod_debugger_window::Make_SparkRow(
                    LOCTEXT("ActivityPromotes", "Promotes"),
                    _RingPromotes,
                    CkStyle::Ok(),
                    Latest([](const FCkVisualLodDebugger_ArbiterInfo& InArbiter) { return InArbiter.PromotesThisTick; }))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
            [
                ck_visuallod_debugger_window::Make_SparkRow(
                    LOCTEXT("ActivityDemotes", "Demotes"),
                    _RingDemotes,
                    CkStyle::Info(),
                    Latest([](const FCkVisualLodDebugger_ArbiterInfo& InArbiter) { return InArbiter.DemotesThisTick; }))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_SparkRow(
                    LOCTEXT("ActivityPreempts", "Preempts"),
                    _RingPreempts,
                    CkStyle::Warn(),
                    Latest([](const FCkVisualLodDebugger_ArbiterInfo& InArbiter) { return InArbiter.PreemptsThisTick; }))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_Separator()
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                ck_visuallod_debugger_window::Make_PaneHeading(
                    LOCTEXT("TotalsHeading", "Totals"),
                    LOCTEXT("TotalsRight", "sampled since domain select"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("TotalPromotes", "Promotes"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        return FText::AsNumber(Panel.IsValid() ? Panel->_TotalPromotes : 0);
                    }),
                    CkStyle::Text())
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("TotalDemotes", "Demotes"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        return FText::AsNumber(Panel.IsValid() ? Panel->_TotalDemotes : 0);
                    }),
                    CkStyle::Text())
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("TotalPreempts", "Preempts"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        return FText::AsNumber(Panel.IsValid() ? Panel->_TotalPreempts : 0);
                    }),
                    CkStyle::Text())
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkVisualLodDebuggerWindow::
    DoRebuild_DomainTabs()
    -> void
{
    if (NOT _DomainTabsHost.IsValid())
    { return; }

    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    auto Tabs = TArray<FCkDebug_UnderlineTabDesc>{};

    for (const auto& Arbiter : _Collector.Get_Snapshot().Arbiters)
    {
        auto Desc = FCkDebug_UnderlineTabDesc{};
        Desc.Id    = Arbiter.TabId;
        Desc.Label = FText::FromString(Arbiter.DomainTagName);

        // Bound, not baked: the member count moves every tick and a tab set only changes when a DOMAIN
        // appears or vanishes.
        Desc.CountText = TAttribute<FText>::CreateLambda([WeakPanel, TabId = Arbiter.TabId]() -> FText
        {
            const auto Panel = WeakPanel.Pin();
            if (NOT Panel.IsValid())
            { return FText::GetEmpty(); }

            const auto* Found = Panel->_Collector.Get_Snapshot().Find_ByTabId(TabId);
            return FText::AsNumber(Found != nullptr ? Found->Members.Num() : 0);
        });

        Tabs.Add(MoveTemp(Desc));
    }

    _DomainTabsHost->SetContent(
        SNew(SCkDebug_UnderlineTabs)
        .Tabs(MoveTemp(Tabs))
        .ActiveTabId_Lambda([WeakPanel]() -> FName
        {
            const auto Panel = WeakPanel.Pin();
            return Panel.IsValid() ? Panel->_SelectedDomain : NAME_None;
        })
        .OnTabSelected(FOnCkDebug_TabSelected::CreateLambda([WeakPanel](FName InTabId)
        {
            const auto Panel = WeakPanel.Pin();
            if (NOT Panel.IsValid() || Panel->_SelectedDomain == InTabId)
            { return; }

            Panel->_SelectedDomain = InTabId;

            // Running totals and the rings are per-arbiter. Carrying them across a domain switch would
            // attribute one domain's flips to another.
            Panel->_TotalPromotes = 0;
            Panel->_TotalDemotes  = 0;
            Panel->_TotalPreempts = 0;

            if (Panel->_RingPromotes.IsValid()) { Panel->_RingPromotes->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize); }
            if (Panel->_RingDemotes.IsValid())  { Panel->_RingDemotes->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize); }
            if (Panel->_RingPreempts.IsValid()) { Panel->_RingPreempts->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize); }

            // The new domain may declare a different pool count, so the pool meters must re-emit.
            Panel->_LastStructureSignature.Reset();
        })));
}

auto
    SCkVisualLodDebuggerWindow::
    DoRebuild_CrowdPools()
    -> void
{
    if (NOT _CrowdPoolBox.IsValid() || NOT _CrowdTunerBox.IsValid())
    { return; }

    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    _CrowdPoolBox->ClearChildren();
    _CrowdTunerBox->ClearChildren();

    if (_Live.Crowds.IsEmpty())
    {
        _CrowdPoolBox->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Font_Static(&ck_visuallod_debugger_window::Get_MetaFont)
                .ColorAndOpacity(CkStyle::TextMute())
                .Text(LOCTEXT("NoCrowdPools", "(no crowd pools configured on this domain)"))
            ];
        _CrowdTunerBox->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Font_Static(&ck_visuallod_debugger_window::Get_MetaFont)
                .ColorAndOpacity(CkStyle::TextMute())
                .Text(LOCTEXT("NoCrowdTuners", "(no crowd tuners configured on this domain)"))
            ];
        return;
    }

    for (auto CrowdIndex = 0; CrowdIndex < _Live.Crowds.Num(); ++CrowdIndex)
    {
        // Reads the LIVE array by index rather than capturing the crowd struct: the struct is replaced
        // wholesale every gated tick, and a captured copy would freeze at build time.
        const auto Crowd = [WeakPanel, CrowdIndex]() -> FCkVisualLodDebugger_CrowdInfo
        {
            const auto Panel = WeakPanel.Pin();
            if (NOT Panel.IsValid() || NOT Panel->_Live.Crowds.IsValidIndex(CrowdIndex))
            { return {}; }

            return Panel->_Live.Crowds[CrowdIndex];
        };

        const auto MakeCrowdFloat = [WeakPanel, CrowdIndex](
            const FText& InLabel,
            TFunction<double(const FCk_VisualLod_RuntimeCrowdTuners&)> InGet,
            TFunction<void(FCk_VisualLod_RuntimeCrowdTuners&, float)> InSet) -> TSharedRef<SWidget>
        {
            return ck_visuallod_debugger_window::Make_TunerRow(
                InLabel, LOCTEXT("CrowdTunerTip", "Deferred session-only crowd tuning."),
                TAttribute<double>::CreateLambda([WeakPanel, CrowdIndex, Get = MoveTemp(InGet)]()
                {
                    const auto Panel = WeakPanel.Pin();
                    return Panel.IsValid() && Panel->_Live.RuntimeTuners.Get_CrowdTuners().IsValidIndex(CrowdIndex)
                        ? Get(Panel->_Live.RuntimeTuners.Get_CrowdTuners()[CrowdIndex]) : 0.0;
                }), ECkDebug_NumericKind::Float, TOptional<double>{0.0}, TOptional<double>{}, 2,
                FOnCkDebug_NumericCommitted::CreateLambda([WeakPanel, CrowdIndex, Set = MoveTemp(InSet)](double InValue)
                {
                    const auto Panel = WeakPanel.Pin();
                    if (Panel.IsValid())
                    { Panel->DoRequest_CrowdTuners(CrowdIndex, [Set, InValue](auto& InCrowd) { Set(InCrowd, static_cast<float>(InValue)); }); }
                }));
        };

        const auto MakeCrowdInteger = [WeakPanel, CrowdIndex](
            const FText& InLabel,
            TFunction<int32(const FCk_VisualLod_RuntimeCrowdTuners&)> InGet,
            TFunction<void(FCk_VisualLod_RuntimeCrowdTuners&, int32)> InSet) -> TSharedRef<SWidget>
        {
            return ck_visuallod_debugger_window::Make_TunerRow(
                InLabel, LOCTEXT("CrowdTunerTip", "Deferred session-only crowd tuning."),
                TAttribute<double>::CreateLambda([WeakPanel, CrowdIndex, Get = MoveTemp(InGet)]()
                {
                    const auto Panel = WeakPanel.Pin();
                    return Panel.IsValid() && Panel->_Live.RuntimeTuners.Get_CrowdTuners().IsValidIndex(CrowdIndex)
                        ? static_cast<double>(Get(Panel->_Live.RuntimeTuners.Get_CrowdTuners()[CrowdIndex])) : 0.0;
                }), ECkDebug_NumericKind::Integer, TOptional<double>{0.0}, TOptional<double>{}, 0,
                FOnCkDebug_NumericCommitted::CreateLambda([WeakPanel, CrowdIndex, Set = MoveTemp(InSet)](double InValue)
                {
                    const auto Panel = WeakPanel.Pin();
                    if (Panel.IsValid())
                    { Panel->DoRequest_CrowdTuners(CrowdIndex, [Set, InValue](auto& InCrowd) { Set(InCrowd, FMath::Max(0, FMath::RoundToInt(InValue))); }); }
            }));
        };

        const auto CrowdTunerBox = SNew(SVerticalBox);

        _CrowdPoolBox->AddSlot()
            .AutoHeight()
            .Padding(0.0f, CrowdIndex == 0 ? 0.0f : CkStyle::SpaceM, 0.0f, CkStyle::SpaceS)
            [
                ck_visuallod_debugger_window::Make_PaneHeading(
                    FText::FromString(FString::Printf(TEXT("Crowd pool %d"), CrowdIndex)),
                    TAttribute<FText>::CreateLambda([Crowd]() -> FText
                    {
                        const auto Info = Crowd();
                        return Info.HasCrowdActor
                            ? FText::FromString(FString::Printf(TEXT("%d tile(s) · %d profile bucket(s)"),
                                Info.TileCount, Info.ProfileBucketCount))
                            : LOCTEXT("CrowdNotStoodUp", "not created yet");
                    }))
            ];

        _CrowdTunerBox->AddSlot().AutoHeight().Padding(0.0f, CrowdIndex == 0 ? 0.0f : CkStyle::SpaceM, 0.0f, 0.0f)
        [
            SNew(SCkDebug_InspectorPanel)
            .Title(FText::FromString(FString::Printf(TEXT("Crowd %d tuners"), CrowdIndex)))
            .StartExpanded(false)
            .Body()
            [
                CrowdTunerBox
            ]
        ];

        CrowdTunerBox->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            ck_visuallod_debugger_window::Make_PaneHeading(
                LOCTEXT("CrowdAnimationTuners", "Far animation tuners"),
                LOCTEXT("CrowdAnimationTunersMeta", "session only"))
        ];
        CrowdTunerBox->AddSlot().AutoHeight()
        [ MakeCrowdInteger(LOCTEXT("CrowdIdleSequence", "Idle sequence"),
            [](const auto& T) { return T.Get_IdleSequenceIndex(); },
            [](auto& T, int32 V) { T.Set_IdleSequenceIndex(V); }) ];
        CrowdTunerBox->AddSlot().AutoHeight()
        [ MakeCrowdInteger(LOCTEXT("CrowdMoveSequence", "Move sequence"),
            [](const auto& T) { return T.Get_MoveSequenceIndex(); },
            [](auto& T, int32 V) { T.Set_MoveSequenceIndex(V); }) ];
        CrowdTunerBox->AddSlot().AutoHeight()
        [ MakeCrowdFloat(LOCTEXT("CrowdMoveThreshold", "Move threshold"),
            [](const auto& T) { return static_cast<double>(T.Get_MoveSpeedThreshold()); },
            [](auto& T, float V) { T.Set_MoveSpeedThreshold(V); }) ];
        CrowdTunerBox->AddSlot().AutoHeight()
        [ MakeCrowdFloat(LOCTEXT("CrowdMoveAuthoredSpeed", "Move authored speed"),
            [](const auto& T) { return static_cast<double>(T.Get_MoveAuthoredSpeed()); },
            [](auto& T, float V) { T.Set_MoveAuthoredSpeed(FMath::Max(1.0f, V)); }) ];
        CrowdTunerBox->AddSlot().AutoHeight()
        [ MakeCrowdFloat(LOCTEXT("CrowdMoveRateMin", "Move rate min"),
            [](const auto& T) { return T.Get_MoveRateClamp().Get_Min(); },
            [](auto& T, float V) { T.Set_MoveRateClamp(FCk_FloatRange{V, T.Get_MoveRateClamp().Get_Max()}); }) ];
        CrowdTunerBox->AddSlot().AutoHeight()
        [ MakeCrowdFloat(LOCTEXT("CrowdMoveRateMax", "Move rate max"),
            [](const auto& T) { return T.Get_MoveRateClamp().Get_Max(); },
            [](auto& T, float V) { T.Set_MoveRateClamp(FCk_FloatRange{T.Get_MoveRateClamp().Get_Min(), V}); }) ];

        const auto BandCount = _Live.RuntimeTuners.Get_CrowdTuners().IsValidIndex(CrowdIndex)
            ? _Live.RuntimeTuners.Get_CrowdTuners()[CrowdIndex].Get_RenderBands().Num() : 0;
        for (auto BandIndex = 0; BandIndex < BandCount; ++BandIndex)
        {
            const auto MakeBandFloat = [WeakPanel, CrowdIndex, BandIndex](const FText& InLabel,
                TFunction<double(const FCk_VisualLod_RuntimeRenderBandTuners&)> InGet,
                TFunction<void(FCk_VisualLod_RuntimeRenderBandTuners&, float)> InSet) -> TSharedRef<SWidget>
            {
                return ck_visuallod_debugger_window::Make_TunerRow(InLabel, LOCTEXT("BandTunerTip", "Deferred session-only render-band tuning."),
                    TAttribute<double>::CreateLambda([WeakPanel, CrowdIndex, BandIndex, Get = MoveTemp(InGet)]()
                    {
                        const auto Panel = WeakPanel.Pin();
                        const auto& Crowds = Panel.IsValid() ? Panel->_Live.RuntimeTuners.Get_CrowdTuners() : TArray<FCk_VisualLod_RuntimeCrowdTuners>{};
                        return Crowds.IsValidIndex(CrowdIndex) && Crowds[CrowdIndex].Get_RenderBands().IsValidIndex(BandIndex)
                            ? Get(Crowds[CrowdIndex].Get_RenderBands()[BandIndex]) : 0.0;
                    }), ECkDebug_NumericKind::Float, TOptional<double>{0.0}, TOptional<double>{}, 0,
                    FOnCkDebug_NumericCommitted::CreateLambda([WeakPanel, CrowdIndex, BandIndex, Set = MoveTemp(InSet)](double InValue)
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid()) { return; }
                        Panel->DoRequest_CrowdTuners(CrowdIndex, [BandIndex, Set, InValue](auto& InCrowd)
                        {
                            auto Bands = InCrowd.Get_RenderBands();
                            if (NOT Bands.IsValidIndex(BandIndex)) { return; }
                            auto Band = Bands[BandIndex]; Set(Band, static_cast<float>(InValue)); Bands[BandIndex] = MoveTemp(Band);
                            InCrowd.Set_RenderBands(Bands);
                        });
                    }));
            };
            // Render-profile controls belong to the authored band, not to a member currently occupying it.
            // That keeps every active profile tunable while the crowd is lazy or the band is empty.
            const auto MakeProfileNumber = [WeakPanel, CrowdIndex, BandIndex](const FText& InLabel,
                TFunction<double(const FCk_IskmRenderer_RuntimeProfileTuners&)> InGet,
                TFunction<void(FCk_IskmRenderer_RuntimeProfileTuners&, double)> InSet,
                ECkDebug_NumericKind InKind, int32 InDigits) -> TSharedRef<SWidget>
            {
                return ck_visuallod_debugger_window::Make_TunerRow(
                    InLabel, LOCTEXT("ProfileTunerTip", "Deferred session-only render-profile tuning."),
                    TAttribute<double>::CreateLambda([WeakPanel, CrowdIndex, BandIndex, Get = MoveTemp(InGet)]()
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid()
                            || NOT Panel->_Live.RuntimeTuners.Get_CrowdTuners().IsValidIndex(CrowdIndex))
                        { return 0.0; }

                        const auto& Bands = Panel->_Live.RuntimeTuners.Get_CrowdTuners()[CrowdIndex].Get_RenderBands();
                        return Bands.IsValidIndex(BandIndex) ? Get(Bands[BandIndex].Get_ProfileTuners()) : 0.0;
                    }), InKind, TOptional<double>{0.0}, TOptional<double>{}, InDigits,
                    FOnCkDebug_NumericCommitted::CreateLambda([WeakPanel, CrowdIndex, BandIndex, Set = MoveTemp(InSet)](double InValue)
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (Panel.IsValid())
                        { Panel->DoRequest_ProfileTuners(CrowdIndex, BandIndex, [Set, InValue](auto& InProfile) { Set(InProfile, InValue); }); }
                    }));
            };
            const auto MakeProfileToggle = [WeakPanel, CrowdIndex, BandIndex](const FText& InLabel,
                TFunction<bool(const FCk_IskmRenderer_RuntimeProfileTuners&)> InGet,
                TFunction<void(FCk_IskmRenderer_RuntimeProfileTuners&, bool)> InSet) -> TSharedRef<SWidget>
            {
                return SNew(SCkDebug_KeyValueRow)
                    .KeyText(InLabel)
                    .Tone(ECkDebug_KeyValueTone::Custom)
                    .CustomValueColor(CkStyle::Text())
                    .ValueWidget()
                    [
                        SNew(SSegmentedControl<ECk_EnableDisable>)
                        .Value_Lambda([WeakPanel, CrowdIndex, BandIndex, Get = MoveTemp(InGet)]()
                        {
                            const auto Panel = WeakPanel.Pin();
                            if (NOT Panel.IsValid()
                                || NOT Panel->_Live.RuntimeTuners.Get_CrowdTuners().IsValidIndex(CrowdIndex))
                            { return ECk_EnableDisable::Disable; }

                            const auto& Bands = Panel->_Live.RuntimeTuners.Get_CrowdTuners()[CrowdIndex].Get_RenderBands();
                            return Bands.IsValidIndex(BandIndex) && Get(Bands[BandIndex].Get_ProfileTuners())
                                ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable;
                        })
                        .OnValueChanged_Lambda([WeakPanel, CrowdIndex, BandIndex, Set = MoveTemp(InSet)](ECk_EnableDisable InValue)
                        {
                            const auto Panel = WeakPanel.Pin();
                            if (Panel.IsValid())
                            {
                                Panel->DoRequest_ProfileTuners(CrowdIndex, BandIndex,
                                    [Set, InValue](auto& InProfile) { Set(InProfile, InValue == ECk_EnableDisable::Enable); });
                            }
                        })
                        + SSegmentedControl<ECk_EnableDisable>::Slot(ECk_EnableDisable::Enable).Text(LOCTEXT("TunerOn", "On"))
                        + SSegmentedControl<ECk_EnableDisable>::Slot(ECk_EnableDisable::Disable).Text(LOCTEXT("TunerOff", "Off"))
                    ];
            };
            const auto BandTunerBox = SNew(SVerticalBox);
            CrowdTunerBox->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                SNew(SCkDebug_InspectorPanel)
                .Title(FText::FromString(FString::Printf(TEXT("Render band %d"), BandIndex)))
                .StartExpanded(false)
                .Body()
                [
                    BandTunerBox
                ]
            ];
            if (BandIndex == 0)
            {
                BandTunerBox->AddSlot().AutoHeight()
                [ ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("BandThreshold", "Threshold (cm)"),
                    TAttribute<FText>::CreateLambda([Crowd]()
                    {
                        const auto Info = Crowd();
                        return Info.RuntimeTuners.Get_RenderBands().IsValidIndex(0)
                            ? FText::AsNumber(Info.RuntimeTuners.Get_RenderBands()[0].Get_DistanceThreshold())
                            : FText::GetEmpty();
                    }),
                    CkStyle::TextDim()) ];
            }
            else
            {
                BandTunerBox->AddSlot().AutoHeight()
                [ MakeBandFloat(LOCTEXT("BandThreshold", "Threshold (cm)"), [](const auto& T) { return T.Get_DistanceThreshold(); }, [](auto& T, float V) { T.Set_DistanceThreshold(V); }) ];
            }
            BandTunerBox->AddSlot().AutoHeight()
            [ MakeBandFloat(LOCTEXT("BandHysteresis", "Return hysteresis"), [](const auto& T) { return T.Get_ReturnHysteresis(); }, [](auto& T, float V) { T.Set_ReturnHysteresis(V); }) ];

            const auto AddProfileToggle = [&BandTunerBox, &MakeProfileToggle](const FText& InLabel,
                TFunction<bool(const FCk_IskmRenderer_RuntimeProfileTuners&)> InGet,
                TFunction<void(FCk_IskmRenderer_RuntimeProfileTuners&, bool)> InSet)
            { BandTunerBox->AddSlot().AutoHeight()[MakeProfileToggle(InLabel, MoveTemp(InGet), MoveTemp(InSet))]; };
            const auto AddProfileNumber = [&BandTunerBox, &MakeProfileNumber](const FText& InLabel,
                TFunction<double(const FCk_IskmRenderer_RuntimeProfileTuners&)> InGet,
                TFunction<void(FCk_IskmRenderer_RuntimeProfileTuners&, double)> InSet,
                ECkDebug_NumericKind InKind, int32 InDigits)
            { BandTunerBox->AddSlot().AutoHeight()[MakeProfileNumber(InLabel, MoveTemp(InGet), MoveTemp(InSet), InKind, InDigits)]; };

            AddProfileToggle(LOCTEXT("ProfileCastShadow", "Cast shadow"), [](const auto& P) { return P.Get_RenderingInfo().Get_bCastDynamicShadow() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bCastDynamicShadow(V); P.Set_RenderingInfo(R); });
            AddProfileToggle(LOCTEXT("ProfileMainPass", "Main pass"), [](const auto& P) { return P.Get_RenderingInfo().Get_bRenderInMainPass() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bRenderInMainPass(V); P.Set_RenderingInfo(R); });
            AddProfileToggle(LOCTEXT("ProfileDepthPass", "Depth pass"), [](const auto& P) { return P.Get_RenderingInfo().Get_bRenderInDepthPass() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bRenderInDepthPass(V); P.Set_RenderingInfo(R); });
            AddProfileToggle(LOCTEXT("ProfileDecals", "Receives decals"), [](const auto& P) { return P.Get_RenderingInfo().Get_bReceivesDecals() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bReceivesDecals(V); P.Set_RenderingInfo(R); });
            AddProfileToggle(LOCTEXT("ProfileOccluder", "Use as occluder"), [](const auto& P) { return P.Get_RenderingInfo().Get_bUseAsOccluder() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bUseAsOccluder(V); P.Set_RenderingInfo(R); });
            AddProfileToggle(LOCTEXT("ProfileCustomDepth", "Custom depth"), [](const auto& P) { return P.Get_RenderingInfo().Get_bRenderCustomDepth() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bRenderCustomDepth(V); P.Set_RenderingInfo(R); });
            AddProfileToggle(LOCTEXT("ProfileContactShadow", "Contact shadow"), [](const auto& P) { return P.Get_RenderingInfo().Get_bCastContactShadow() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bCastContactShadow(V); P.Set_RenderingInfo(R); });
            AddProfileToggle(LOCTEXT("ProfileIndirect", "Dynamic indirect"), [](const auto& P) { return P.Get_RenderingInfo().Get_bAffectDynamicIndirectLighting() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bAffectDynamicIndirectLighting(V); P.Set_RenderingInfo(R); });
            AddProfileToggle(LOCTEXT("ProfileDistanceField", "Distance field"), [](const auto& P) { return P.Get_RenderingInfo().Get_bAffectDistanceFieldLighting() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bAffectDistanceFieldLighting(V); P.Set_RenderingInfo(R); });
            AddProfileToggle(LOCTEXT("ProfileRayTracing", "Ray tracing"), [](const auto& P) { return P.Get_RenderingInfo().Get_bVisibleInRayTracing() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bVisibleInRayTracing(V); P.Set_RenderingInfo(R); });
            AddProfileToggle(LOCTEXT("ProfileVelocity", "Output velocity"), [](const auto& P) { return P.Get_RenderingInfo().Get_bOutputVelocity() != 0; }, [](auto& P, bool V) { auto R = P.Get_RenderingInfo(); R.Set_bOutputVelocity(V); P.Set_RenderingInfo(R); });
            AddProfileNumber(LOCTEXT("ProfileMinDraw", "Min draw distance"), [](const auto& P) { return static_cast<double>(P.Get_MinDrawDistance()); }, [](auto& P, double V) { P.Set_MinDrawDistance(static_cast<float>(V)); }, ECkDebug_NumericKind::Float, 0);
            AddProfileNumber(LOCTEXT("ProfileMaxDraw", "Max draw distance"), [](const auto& P) { return static_cast<double>(P.Get_MaxDrawDistance()); }, [](auto& P, double V) { P.Set_MaxDrawDistance(static_cast<float>(V)); }, ECkDebug_NumericKind::Float, 0);
            AddProfileNumber(LOCTEXT("ProfileMinLod", "Min LOD"), [](const auto& P) { return static_cast<double>(P.Get_MinLOD()); }, [](auto& P, double V) { P.Set_MinLOD(FMath::Max(0, FMath::RoundToInt(V))); }, ECkDebug_NumericKind::Integer, 0);
            AddProfileNumber(LOCTEXT("ProfileBounds", "Bounds scale"), [](const auto& P) { return static_cast<double>(P.Get_BoundsScale()); }, [](auto& P, double V) { P.Set_BoundsScale(FMath::Max(0.01f, static_cast<float>(V))); }, ECkDebug_NumericKind::Float, 2);
            AddProfileNumber(LOCTEXT("ProfileFarInterval", "Far update interval (s)"), [](const auto& P) { return P.Get_FarAnimationUpdateInterval().Get_Seconds(); }, [](auto& P, double V) { P.Set_FarAnimationUpdateInterval(FCk_Time{V}); }, ECkDebug_NumericKind::Float, 3);
            AddProfileToggle(LOCTEXT("ProfileFreezeFar", "Freeze far animation"), [](const auto& P) { return P.Get_FreezeFarAnimation() == ECk_EnableDisable::Enable; }, [](auto& P, bool V) { P.Set_FreezeFarAnimation(V ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable); });
            AddProfileToggle(LOCTEXT("ProfileChannel0", "Lighting channel 0"), [](const auto& P) { return P.Get_LightingChannels().bChannel0 != 0; }, [](auto& P, bool V) { auto C = P.Get_LightingChannels(); C.bChannel0 = V; P.Set_LightingChannels(C); });
            AddProfileToggle(LOCTEXT("ProfileChannel1", "Lighting channel 1"), [](const auto& P) { return P.Get_LightingChannels().bChannel1 != 0; }, [](auto& P, bool V) { auto C = P.Get_LightingChannels(); C.bChannel1 = V; P.Set_LightingChannels(C); });
            AddProfileToggle(LOCTEXT("ProfileChannel2", "Lighting channel 2"), [](const auto& P) { return P.Get_LightingChannels().bChannel2 != 0; }, [](auto& P, bool V) { auto C = P.Get_LightingChannels(); C.bChannel2 = V; P.Set_LightingChannels(C); });
        }

        _CrowdPoolBox->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                ck_visuallod_debugger_window::Make_MeterRow(
                    LOCTEXT("CrowdSlots", "Slots"),
                    TAttribute<float>::CreateLambda([Crowd]() { return Crowd().Get_UsedFraction(); }),
                    TAttribute<FLinearColor>::CreateLambda([Crowd]() -> FLinearColor
                    {
                        return Crowd().Get_IsExhausted() ? CkStyle::Err() : CkStyle::Ok();
                    }),
                    TAttribute<FText>::CreateLambda([Crowd]() -> FText
                    {
                        const auto Info = Crowd();
                        return FText::FromString(FString::Printf(TEXT("%d / %d"), Info.Get_UsedSlots(), Info.PoolSize));
                    }),
                    LOCTEXT("CrowdSlotsTip",
                        "Member slots taken in this batched crowd's fixed pool. The pool cannot grow after "
                        "Finalize, so a full bar is the exhaustion policy's trigger."))
            ];

        _CrowdPoolBox->AddSlot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("CrowdRendered", "Rendered"),
                    TAttribute<FText>::CreateLambda([Crowd]() -> FText
                    {
                        const auto Info = Crowd();
                        if (NOT Info.HasCrowdActor)
                        { return LOCTEXT("Unset", "—"); }

                        // Rendered instances drop below used slots whenever a member is hidden for a
                        // Plan-1 flip, so the two numbers disagreeing is information, not a bug.
                        return FText::FromString(FString::Printf(TEXT("%d instance(s) · %d tile(s) · %d profile bucket(s)"),
                            Info.RenderedInstanceCount, Info.TileCount, Info.ProfileBucketCount));
                    }),
                    CkStyle::Text())
            ];

        _CrowdPoolBox->AddSlot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("CrowdExhaustion", "Exhaustion"),
                    TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid() || NOT Panel->_Live.HasConfig)
                        { return LOCTEXT("Unset", "—"); }

                        return FText::FromString(
                            ck_visuallod_debugger_window::Get_PolicyText(Panel->_Live.ExhaustionPolicy));
                    }),
                    CkStyle::TextDim())
            ];

        // Rendering participation — display only. Multiple authored profiles are member-selected;
        // a single pool-level shadow/lighting summary would be false in that case, so the detail
        // rail is the authoritative per-member view.
        _CrowdPoolBox->AddSlot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("CrowdFarShadows", "Far shadows"),
                    TAttribute<FText>::CreateLambda([Crowd]() -> FText
                    {
                        const auto Info = Crowd();
                        if (Info.Get_HasMultipleRenderProfiles())
                        { return LOCTEXT("CrowdPerMember", "per member — see detail"); }
                        const auto& Flags = Info.RenderProfiles.Num() == 1 ? Info.RenderProfiles[0].Render : Info.Render;
                        if (NOT Flags.Resolved)
                        { return LOCTEXT("Unset", "—"); }

                        return Flags.CastShadow
                            ? LOCTEXT("ShadowCast", "cast")
                            : LOCTEXT("ShadowOff", "off");
                    }),
                    TAttribute<FLinearColor>::CreateLambda([Crowd]() -> FLinearColor
                    {
                        const auto Info = Crowd();
                        if (Info.Get_HasMultipleRenderProfiles())
                        { return CkStyle::TextDim(); }
                        const auto& Flags = Info.RenderProfiles.Num() == 1 ? Info.RenderProfiles[0].Render : Info.Render;
                        return Flags.Resolved
                            ? ck_visuallod_debugger_window::Get_BoolColor(Flags.CastShadow)
                            : CkStyle::TextMute();
                    }))
            ];

        _CrowdPoolBox->AddSlot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("CrowdFarLighting", "Far lighting"),
                    TAttribute<FText>::CreateLambda([Crowd]() -> FText
                    {
                        const auto Info = Crowd();
                        if (Info.Get_HasMultipleRenderProfiles())
                        { return LOCTEXT("CrowdPerMember", "per member — see detail"); }
                        const auto& Flags = Info.RenderProfiles.Num() == 1 ? Info.RenderProfiles[0].Render : Info.Render;
                        if (NOT Flags.Resolved)
                        { return LOCTEXT("Unset", "—"); }

                        return FText::FromString(FString::Printf(TEXT("indirect %s · dist-field %s"),
                            Flags.AffectDynamicIndirectLighting ? TEXT("on") : TEXT("off"),
                            Flags.AffectDistanceFieldLighting ? TEXT("on") : TEXT("off")));
                    }),
                    CkStyle::Text())
            ];

        _CrowdPoolBox->AddSlot()
            .AutoHeight()
            [
                ck_visuallod_debugger_window::Make_KvRow(
                    LOCTEXT("CrowdLightChannels", "Light channels"),
                    TAttribute<FText>::CreateLambda([Crowd]() -> FText
                    {
                        const auto Info = Crowd();
                        if (Info.Get_HasMultipleRenderProfiles())
                        { return LOCTEXT("CrowdPerMember", "per member — see detail"); }
                        const auto& Flags = Info.RenderProfiles.Num() == 1 ? Info.RenderProfiles[0].Render : Info.Render;
                        return Flags.Resolved
                            ? FText::FromString(Flags.Get_LightingChannelsText())
                            : LOCTEXT("Unset", "—");
                    }),
                    CkStyle::TextDim())
            ];
    }
}

auto
    SCkVisualLodDebuggerWindow::
    DoRebuild_Alerts()
    -> void
{
    if (NOT _AlertBox.IsValid())
    { return; }

    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    _AlertBox->ClearChildren();

    if (NOT _HasLiveArbiter)
    { return; }

    if (NOT _Live.ViewValid)
    {
        _AlertBox->AddSlot()
            .AutoHeight()
            [
                SNew(SCkDebug_AlertRow)
                .Tone(ECk_Tone::Warn)
                .Glyph(FText::FromString(TEXT("▲")))
                .LeadText(LOCTEXT("AlertViewLead", "View unresolved"))
                .BodyText(LOCTEXT("AlertViewBody",
                    " — the arbiter skips the whole batch: no promotes, no demotes, no far-anim updates."))
                .FixText(LOCTEXT("AlertViewFix", "wire an observer or verify the local player camera"))
            ];
    }

    if (_Live.Get_AnyPoolExhausted())
    {
        _AlertBox->AddSlot()
            .AutoHeight()
            [
                SNew(SCkDebug_AlertRow)
                .Tone(ECk_Tone::Err)
                .Glyph(FText::FromString(TEXT("✕")))
                .LeadText(LOCTEXT("AlertPoolLead", "Crowd pool exhausted"))
                .BodyText_Lambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid())
                    { return FText::GetEmpty(); }

                    // The counts move while the alert stands, so the body is bound rather than baked —
                    // rebuilding the row to update a number would flicker the whole lane.
                    auto Body = FString(TEXT(" — "));
                    for (const auto& Crowd : Panel->_Live.Crowds)
                    {
                        if (NOT Crowd.Get_IsExhausted())
                        { continue; }

                        Body += FString::Printf(TEXT("pool %d at %d/%d.  "),
                            Crowd.CrowdIndex, Crowd.Get_UsedSlots(), Crowd.PoolSize);
                    }

                    Body += FString::Printf(TEXT("Policy %s."),
                        *ck_visuallod_debugger_window::Get_PolicyText(Panel->_Live.ExhaustionPolicy));

                    return FText::FromString(Body);
                })
                .FixText(LOCTEXT("AlertPoolFix", "raise PoolSize or lower the member count"))
            ];
    }
}

// ====================================================================================================================
// Roster
// ====================================================================================================================

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_RosterPane()
    -> TSharedRef<SWidget>
{
    using namespace ck_visuallod_debugger_window;

    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    const auto SortMode = [WeakPanel](FName InColumn)
    {
        return TAttribute<EColumnSortMode::Type>::CreateLambda([WeakPanel, InColumn]()
        {
            const auto Panel = WeakPanel.Pin();
            return Panel.IsValid() ? Panel->DoGet_SortModeFor(InColumn) : EColumnSortMode::None;
        });
    };

    return SNew(SCkDebug_Card)
        .BodyPadding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Font_Static(&Get_HeadingFont)
                    .ColorAndOpacity(CkStyle::PaneHeadingColor())
                    .Text(LOCTEXT("RosterHeading", "MEMBERS"))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebug_CountBadge)
                    .ValueText_Lambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        if (NOT Panel.IsValid())
                        { return FText::GetEmpty(); }

                        // "shown / total" while a filter is active: a roster that silently shrank is the
                        // fastest way to convince a reader that members vanished from the domain.
                        return Panel->_FilterString.IsEmpty()
                            ? FText::AsNumber(Panel->_RosterItems.Num())
                            : FText::FromString(FString::Printf(TEXT("%d / %d"),
                                Panel->_RosterItems.Num(), Panel->_Live.Members.Num()));
                    })
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebug_StatusPill)
                    .ShowDot(false)
                    .Tone(ECk_Tone::Info)
                    .Text_Lambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        return FText::FromString(FString::Printf(TEXT("%d in view"),
                            Panel.IsValid() ? Panel->_Tallies.InView : 0));
                    })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebug_StatusPill)
                    .ShowDot(false)
                    .Tone_Lambda([WeakPanel]()
                    {
                        const auto Panel = WeakPanel.Pin();
                        return Panel.IsValid() && Panel->_Tallies.Fading > 0 ? ECk_Tone::Info : ECk_Tone::Neutral;
                    })
                    .Text_Lambda([WeakPanel]() -> FText
                    {
                        const auto Panel = WeakPanel.Pin();
                        return FText::FromString(FString::Printf(TEXT("%d fading"),
                            Panel.IsValid() ? Panel->_Tallies.Fading : 0));
                    })
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                SAssignNew(_RosterSearchBar, SCkDebug_DualSearchBar)
                .FilterHintText(LOCTEXT("RosterFilterHint", "Filter members\x2026"))
                .HighlightHintText(LOCTEXT("RosterHighlightHint", "Highlight\x2026"))
                .OnFilterTextChanged_Lambda([WeakPanel](const FString& InText)
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid() || Panel->_FilterString == InText)
                    { return; }

                    Panel->_FilterString = InText;
                    Panel->DoApply_RosterPipeline();
                })
                .OnHighlightTextChanged_Lambda([WeakPanel](const FString& InText)
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid() || Panel->_HighlightString == InText)
                    { return; }

                    // Highlight only re-tints rows the filter already kept, and every row's tint is an
                    // attribute lambda reading this string — so a keystroke costs no widget work at all.
                    Panel->_HighlightString = InText;
                })
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(_RosterListView, SListView<FRosterItemPtr>)
                .ListItemsSource(&_RosterItems)
                .SelectionMode(ESelectionMode::Single)
                .OnGenerateRow(this, &SCkVisualLodDebuggerWindow::Handle_GenerateRosterRow)
                .OnSelectionChanged(this, &SCkVisualLodDebuggerWindow::Handle_RosterSelectionChanged)
                .OnContextMenuOpening(this, &SCkVisualLodDebuggerWindow::Handle_RosterContextMenu)
                .HeaderRow(
                    SNew(SHeaderRow)
                    + SHeaderRow::Column(k_Col_Entity)
                        .DefaultLabel(LOCTEXT("ColEntity", "Entity"))
                        .FillWidth(0.34f)
                        .SortMode(SortMode(k_Col_Entity))
                        .OnSort(this, &SCkVisualLodDebuggerWindow::Handle_RosterSortChanged)
                    + SHeaderRow::Column(k_Col_Rep)
                        .DefaultLabel(LOCTEXT("ColRep", "Rep"))
                        .FillWidth(0.14f)
                        .SortMode(SortMode(k_Col_Rep))
                        .OnSort(this, &SCkVisualLodDebuggerWindow::Handle_RosterSortChanged)
                    + SHeaderRow::Column(k_Col_Distance)
                        .DefaultLabel(LOCTEXT("ColDistance", "Distance"))
                        .FixedWidth(74.0f)
                        .SortMode(SortMode(k_Col_Distance))
                        .OnSort(this, &SCkVisualLodDebuggerWindow::Handle_RosterSortChanged)
                    + SHeaderRow::Column(k_Col_View)
                        .DefaultLabel(LOCTEXT("ColView", "View"))
                        .FixedWidth(46.0f)
                        .SortMode(SortMode(k_Col_View))
                        .OnSort(this, &SCkVisualLodDebuggerWindow::Handle_RosterSortChanged)
                    + SHeaderRow::Column(k_Col_Fade)
                        .DefaultLabel(LOCTEXT("ColFade", "Fade \x03B1"))
                        .FixedWidth(96.0f)
                        .SortMode(SortMode(k_Col_Fade))
                        .OnSort(this, &SCkVisualLodDebuggerWindow::Handle_RosterSortChanged)
                    + SHeaderRow::Column(k_Col_Slot)
                        .DefaultLabel(LOCTEXT("ColSlot", "Slot"))
                        .FixedWidth(48.0f)
                        .SortMode(SortMode(k_Col_Slot))
                        .OnSort(this, &SCkVisualLodDebuggerWindow::Handle_RosterSortChanged)
                    + SHeaderRow::Column(k_Col_Flags)
                        .DefaultLabel(LOCTEXT("ColFlags", "Flags"))
                        .FillWidth(0.30f)
                        .SortMode(SortMode(k_Col_Flags))
                        .OnSort(this, &SCkVisualLodDebuggerWindow::Handle_RosterSortChanged))
            ]
        ];
}

auto
    SCkVisualLodDebuggerWindow::
    DoMatches_Query(
        const FCkVisualLodDebugger_MemberInfo& InMember,
        const FString&                         InNeedle) const
    -> bool
{
    if (InNeedle.IsEmpty())
    { return true; }

    using namespace ck_visuallod_debugger_window;

    return InMember.Name.Contains(InNeedle, ESearchCase::IgnoreCase)
        || Get_MemberIdText(InMember).Contains(InNeedle, ESearchCase::IgnoreCase)
        || Get_RepLabel(InMember.Representation).ToString().Contains(InNeedle, ESearchCase::IgnoreCase)
        || InMember.Get_FlagsText().Contains(InNeedle, ESearchCase::IgnoreCase);
}

auto
    SCkVisualLodDebuggerWindow::
    DoApply_RosterPipeline()
    -> void
{
    using namespace ck_visuallod_debugger_window;

    // Filter first, then sort, then reconcile. Sorting before filtering would order rows nobody sees.
    auto Filtered = TArray<const FCkVisualLodDebugger_MemberInfo*>{};
    Filtered.Reserve(_Live.Members.Num());

    for (const auto& Member : _Live.Members)
    {
        if (DoMatches_Query(Member, _FilterString))
        { Filtered.Add(&Member); }
    }

    if (_SortColumn != NAME_None)
    {
        // Sentinels sort LAST regardless of direction only if they are mapped to an extreme; -1 distance means
        // "never ranked" and an empty slot means "holds none", and neither belongs at the top of an ascending list
        // of real values.
        const auto Key = [](const FCkVisualLodDebugger_MemberInfo& InMember, FName InColumn) -> double
        {
            if (InColumn == k_Col_Rep)
            {
                switch (InMember.Representation)
                {
                    case ECk_VisualLod_Representation::PromotedProxy: return 0.0;
                    case ECk_VisualLod_Representation::FarMember:     return 1.0;
                    default:                                          return 2.0;
                }
            }

            if (InColumn == k_Col_Distance)
            {
                return InMember.LastDistance < 0.0f
                    ? TNumericLimits<double>::Max()
                    : static_cast<double>(InMember.LastDistance);
            }

            if (InColumn == k_Col_View)
            { return InMember.LastInView ? 0.0 : 1.0; }

            if (InColumn == k_Col_Fade)
            { return static_cast<double>(InMember.FadeAlpha); }

            if (InColumn == k_Col_Slot)
            {
                return InMember.SlotIndex < 0
                    ? TNumericLimits<double>::Max()
                    : static_cast<double>(InMember.SlotIndex);
            }

            if (InColumn == k_Col_Flags)
            { return static_cast<double>(InMember.Get_FlagWeight()); }

            return static_cast<double>(InMember.Entity.Get_Entity().Get_EntityNumber());
        };

        const auto Ascending = _SortMode != EColumnSortMode::Descending;
        const auto Column    = _SortColumn;

        Filtered.Sort([&Key, Column, Ascending](
            const FCkVisualLodDebugger_MemberInfo& InA,
            const FCkVisualLodDebugger_MemberInfo& InB)
        {
            const auto KeyA = Key(InA, Column);
            const auto KeyB = Key(InB, Column);

            if (KeyA != KeyB)
            { return Ascending ? KeyA < KeyB : KeyB < KeyA; }

            // Entity id, never flipped: a tie-break that follows the sort direction reshuffles equal rows on every
            // header click, which reads as the list churning rather than reordering.
            return InA.Entity.Get_Entity() < InB.Entity.Get_Entity();
        });
    }

    // SListView tracks selection by POINTER identity, so a row's TSharedPtr must survive a refresh: index the
    // existing items by member entity, reuse the pointer when the key matches, and update its contents in place.
    auto Existing = TMap<FCk_Handle, FRosterItemPtr>{};
    Existing.Reserve(_RosterItems.Num());
    for (const auto& Item : _RosterItems)
    {
        if (Item.IsValid())
        { Existing.Add(Item->Entity, Item); }
    }

    auto NewItems = TArray<FRosterItemPtr>{};
    NewItems.Reserve(Filtered.Num());

    auto SetChanged = false;

    for (const auto* Member : Filtered)
    {
        auto Item = FRosterItemPtr{};

        if (auto* Found = Existing.Find(Member->Entity))
        {
            Item   = *Found;
            *Item  = *Member;
            Existing.Remove(Member->Entity);
        }
        else
        {
            Item       = MakeShared<FCkVisualLodDebugger_MemberInfo>(*Member);
            SetChanged = true;
        }

        NewItems.Add(MoveTemp(Item));
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    // A pure REORDER changes no pointer, so the set-change test above misses it — compare positions too, or a sort
    // click would leave the old order painted.
    if (NOT SetChanged && NewItems.Num() == _RosterItems.Num())
    {
        for (auto Index = 0; Index < NewItems.Num(); ++Index)
        {
            if (NewItems[Index] != _RosterItems[Index])
            {
                SetChanged = true;
                break;
            }
        }
    }

    _RosterItems = MoveTemp(NewItems);

    if (SetChanged && _RosterListView.IsValid())
    {
        _RosterListView->RequestListRefresh();
        DoRestore_RosterSelection();
    }
}

auto
    SCkVisualLodDebuggerWindow::
    DoRestore_RosterSelection()
    -> void
{
    if (NOT _RosterListView.IsValid())
    { return; }

    if (ck::Is_NOT_Valid(_SelectedMember))
    {
        _RosterListView->ClearSelection();
        return;
    }

    auto Target = FRosterItemPtr{};
    for (const auto& Item : _RosterItems)
    {
        if (Item.IsValid() && Item->Entity == _SelectedMember)
        {
            Target = Item;
            break;
        }
    }

    if (NOT Target.IsValid())
    { return; }

    // Compare before selecting: SetItemSelection on an already-selected row still fires OnSelectionChanged, and an
    // echo out of a restore is how a debugger ends up fighting the user's clicks.
    const auto Current = _RosterListView->GetSelectedItems();
    if (Current.Num() == 1 && Current[0] == Target)
    { return; }

    _RosterListView->SetItemSelection(Target, true, ESelectInfo::Direct);
}

auto
    SCkVisualLodDebuggerWindow::
    Handle_GenerateRosterRow(
        FRosterItemPtr                    InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_visuallod_debugger_window;

    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));
    const auto WeakItem  = TWeakPtr<FCkVisualLodDebugger_MemberInfo>(InItem);

    return SNew(SRosterRow, InOwnerTable)
        .Item(InItem)
        .DemoteDistance_Lambda([WeakPanel]()
        {
            const auto Panel = WeakPanel.Pin();
            return Panel.IsValid() ? Panel->_Live.DemoteDistance : 0.0f;
        })
        .HasHighlightQuery_Lambda([WeakPanel]()
        {
            const auto Panel = WeakPanel.Pin();
            return Panel.IsValid() && NOT Panel->_HighlightString.IsEmpty();
        })
        .IsHighlightMatch_Lambda([WeakPanel, WeakItem]()
        {
            const auto Panel = WeakPanel.Pin();
            const auto Item  = WeakItem.Pin();
            if (NOT Panel.IsValid() || NOT Item.IsValid())
            { return false; }

            return Panel->DoMatches_Query(*Item, Panel->_HighlightString);
        });
}

auto
    SCkVisualLodDebuggerWindow::
    Handle_RosterSelectionChanged(
        FRosterItemPtr    InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    // Direct is programmatic — a selection restore or an applied sync — and is IGNORED outright,
    // not merely un-broadcast: SetItemSelection signals synchronously, before the list's selection
    // state settles, so acting on the echo re-enters DoRestore_RosterSelection from inside its own
    // call, the compare-guard misses, and the cycle recurses until the stack dies (shipped crash).
    // Programmatic paths already set _SelectedMember themselves; this handler is for user clicks
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    constexpr auto Broadcast = true;
    DoSelect_Member(InItem.IsValid() ? InItem->Entity : FCk_Handle{}, Broadcast);
}

auto
    SCkVisualLodDebuggerWindow::
    Handle_RosterContextMenu()
    -> TSharedPtr<SWidget>
{
    if (NOT _RosterListView.IsValid())
    { return nullptr; }

    const auto Selected = _RosterListView->GetSelectedItems();
    if (Selected.IsEmpty() || NOT Selected[0].IsValid())
    { return nullptr; }

    const auto& Member = *Selected[0];

    constexpr auto CloseAfterSelection = true;
    auto MenuBuilder = FMenuBuilder{CloseAfterSelection, nullptr};

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        LOCTEXT("CopyRow", "Copy Row"),
        LOCTEXT("CopyRowTip", "Copy this member's roster line — representation, distance, view, fade, slot and flags."),
        ck_visuallod_debugger_window::Make_RowCopyText(Member));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        LOCTEXT("CopyEntity", "Copy Entity"),
        LOCTEXT("CopyEntityTip", "Copy the full entity handle (ID|Version + debug name)."),
        ck::Format_UE(TEXT("{}"), Member.Entity));

    if (const auto FocusTarget = Member.Entity;
        ck::IsValid(FocusTarget) && ck::DebugFocus::Get_CanFocus())
    {
        MenuBuilder.AddMenuEntry(
            LOCTEXT("FocusInViewport", "Focus in Viewport (F)"),
            LOCTEXT("FocusInViewportTip", "Glide the editor camera to frame this member (auto-ejects while possessed)."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([FocusTarget]()
            {
                ck::DebugFocus::Focus_Entity(FocusTarget);
            })));
    }

    return MenuBuilder.MakeWidget();
}

auto
    SCkVisualLodDebuggerWindow::
    Handle_RosterSortChanged(
        EColumnSortPriority::Type InPriority,
        const FName&              InColumn,
        EColumnSortMode::Type     InMode)
    -> void
{
    // Cycling a column back to None hands the roster back to the collector's own order — in-view first, then
    // nearest first, which is the order the arbiter itself ranks in.
    _SortColumn = InMode == EColumnSortMode::None ? NAME_None : InColumn;
    _SortMode   = InMode;

    DoApply_RosterPipeline();
}

auto
    SCkVisualLodDebuggerWindow::
    DoGet_SortModeFor(
        FName InColumn) const
    -> EColumnSortMode::Type
{
    return _SortColumn == InColumn ? _SortMode : EColumnSortMode::None;
}

// ====================================================================================================================
// Selection — one concept, four sources
// ====================================================================================================================

auto
    SCkVisualLodDebuggerWindow::
    DoSelect_Member(
        const FCk_Handle& InMember,
        bool              InBroadcast)
    -> void
{
    _SelectedMember = InMember;

    DoRestore_RosterSelection();

    if (NOT InBroadcast || ck::Is_NOT_Valid(InMember))
    { return; }

    ck::DebugSelectionSync::Broadcast(InMember, WindowId);
}

auto
    SCkVisualLodDebuggerWindow::
    DoGet_SelectedMember() const
    -> const FCkVisualLodDebugger_MemberInfo*
{
    if (ck::Is_NOT_Valid(_SelectedMember))
    { return nullptr; }

    for (const auto& Member : _Live.Members)
    {
        if (Member.Entity == _SelectedMember)
        { return &Member; }
    }

    return nullptr;
}

auto
    SCkVisualLodDebuggerWindow::
    TargetEntity(
        const FCk_Handle& InEntity)
    -> void
{
    // The picker and the entity-target route both hand over whatever the user clicked — an owner NPC, a child
    // feature entity, the member itself. Resolve through the SAME predicate the picker filters with, so both
    // resolve the same real target.
    const auto Resolved = ck::DebugSelectionSync::Resolve_ClosestLineageMatch(
        InEntity, &SCkVisualLodDebuggerWindow::Is_VisualLodPickCandidate);

    if (ck::Is_NOT_Valid(Resolved))
    { return; }

    // A member's domain may not be the tab currently shown, and selecting a row that is not in the list would
    // silently do nothing — so switch the tab first, then re-run the pipeline so the row exists to select.
    for (const auto& Arbiter : _Collector.Get_Snapshot().Arbiters)
    {
        const auto* Found = Arbiter.Members.FindByPredicate(
            [&Resolved](const FCkVisualLodDebugger_MemberInfo& InMember) { return InMember.Entity == Resolved; });

        if (Found == nullptr)
        { continue; }

        if (_SelectedDomain != Arbiter.TabId)
        {
            _SelectedDomain = Arbiter.TabId;

            // Same per-domain reset the underline tabs perform: running totals and rings are one arbiter's, and the
            // new domain may declare a different pool count, so the meters must re-emit.
            _TotalPromotes = 0;
            _TotalDemotes  = 0;
            _TotalPreempts = 0;

            if (_RingPromotes.IsValid()) { _RingPromotes->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize); }
            if (_RingDemotes.IsValid())  { _RingDemotes->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize); }
            if (_RingPreempts.IsValid()) { _RingPreempts->Init(0.0f, ck_visuallod_debugger_window::k_ActivityRingSize); }

            _LastStructureSignature.Reset();

            DoUpdate_Live();
            DoUpdate_EventLog();
            DoApply_RosterPipeline();
        }

        break;
    }

    constexpr auto Broadcast = true;
    DoSelect_Member(Resolved, Broadcast);

    if (_RosterListView.IsValid() && _RosterListView->GetSelectedItems().Num() == 1)
    { _RosterListView->RequestScrollIntoView(_RosterListView->GetSelectedItems()[0]); }
}

auto
    SCkVisualLodDebuggerWindow::
    HandleGlobalSelectionSync(
        const FCk_Handle& InSelected,
        FName             InSource)
    -> void
{
    if (InSource == WindowId)
    { return; }

    const auto Resolved = ck::DebugSelectionSync::Resolve_ClosestLineageMatch(
        InSelected, &SCkVisualLodDebuggerWindow::Is_VisualLodPickCandidate);

    if (ck::Is_NOT_Valid(Resolved))
    { return; }

    // An APPLIED selection is not a user selection: the guard suppresses any Broadcast this path could provoke, and
    // the false below keeps this window from echoing the sync straight back at its sender.
    const auto Guard = ck::DebugSelectionSync::FApplyGuard{};

    constexpr auto Broadcast = false;
    DoSelect_Member(Resolved, Broadcast);
}

auto
    SCkVisualLodDebuggerWindow::
    OnKeyDown(
        const FGeometry&  InGeometry,
        const FKeyEvent&  InKeyEvent)
    -> FReply
{
    if (InKeyEvent.GetKey() == EKeys::F
        && ck::IsValid(_SelectedMember)
        && ck::DebugFocus::Focus_Entity(_SelectedMember))
    { return FReply::Handled(); }

    return SCkDebugger_WindowBase::OnKeyDown(InGeometry, InKeyEvent);
}

// ====================================================================================================================
// Detail rail
// ====================================================================================================================

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_DetailRail()
    -> TSharedRef<SWidget>
{
    using namespace ck_visuallod_debugger_window;

    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    // Every row reads the SELECTED member through this, so changing the selection changes what the rail says without
    // rebuilding a single widget — the rail's structure is built once for the window's whole life.
    const auto MemberText = [WeakPanel](TFunction<FString(const FCkVisualLodDebugger_MemberInfo&, const FCkVisualLodDebugger_ArbiterInfo&)> InSelector)
    {
        return TAttribute<FText>::CreateLambda([WeakPanel, Selector = MoveTemp(InSelector)]() -> FText
        {
            const auto Panel = WeakPanel.Pin();
            if (NOT Panel.IsValid())
            { return FText::GetEmpty(); }

            const auto* Member = Panel->DoGet_SelectedMember();
            if (Member == nullptr)
            { return LOCTEXT("Unset", "—"); }

            return FText::FromString(Selector(*Member, Panel->_Live));
        });
    };

    const auto MemberColor = [WeakPanel](TFunction<FLinearColor(const FCkVisualLodDebugger_MemberInfo&)> InSelector)
    {
        return TAttribute<FLinearColor>::CreateLambda([WeakPanel, Selector = MoveTemp(InSelector)]() -> FLinearColor
        {
            const auto Panel = WeakPanel.Pin();
            if (NOT Panel.IsValid())
            { return CkStyle::TextMute(); }

            const auto* Member = Panel->DoGet_SelectedMember();
            return Member == nullptr ? CkStyle::TextMute() : Selector(*Member);
        });
    };

    // Section variants swap by VISIBILITY, not by rebuild: a promoted member and a far one want different Animation
    // and Rendering rows, and a debugger that tears its rail down every time the selection flips flickers.
    const auto VisibleWhen = [WeakPanel](TFunction<bool(const FCkVisualLodDebugger_MemberInfo&)> InPredicate)
    {
        return TAttribute<EVisibility>::CreateLambda([WeakPanel, Predicate = MoveTemp(InPredicate)]()
        {
            const auto Panel = WeakPanel.Pin();
            if (NOT Panel.IsValid())
            { return EVisibility::Collapsed; }

            const auto* Member = Panel->DoGet_SelectedMember();
            return Member != nullptr && Predicate(*Member) ? EVisibility::Visible : EVisibility::Collapsed;
        });
    };

    const auto HasSelection = TAttribute<EVisibility>::CreateLambda([WeakPanel]()
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->DoGet_SelectedMember() != nullptr
            ? EVisibility::Visible
            : EVisibility::Collapsed;
    });

    const auto NoSelection = TAttribute<EVisibility>::CreateLambda([WeakPanel]()
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->DoGet_SelectedMember() != nullptr
            ? EVisibility::Collapsed
            : EVisibility::Visible;
    });

    return SNew(SBox)
        .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceL, CkStyle::SpaceM})
        [
            SNew(SCkDebug_Card)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
                [
                    Make_PaneHeading(LOCTEXT("DetailHeading", "Member detail"), LOCTEXT("DetailHint", "click a row"))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Visibility(NoSelection)
                    .Font_Static(&Get_MetaFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                    .AutoWrapText(true)
                    .Text(LOCTEXT("DetailEmpty",
                        "No member selected. Pick one in the viewport, or click a roster row."))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SVerticalBox)
                    .Visibility(HasSelection)

                    // ---- header: the one place an EntityRef belongs (a pill inside a row is a click-trap) ----
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SCkDebug_EntityRef)
                            .ShowName(true)
                            .Entity_Lambda([WeakPanel]() -> FCk_Handle
                            {
                                const auto Panel = WeakPanel.Pin();
                                return Panel.IsValid() ? Panel->_SelectedMember : FCk_Handle{};
                            })
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(SCkDebug_StatusPill)
                            .ShowDot(true)
                            .Text_Lambda([WeakPanel]() -> FText
                            {
                                const auto Panel = WeakPanel.Pin();
                                if (NOT Panel.IsValid())
                                { return FText::GetEmpty(); }

                                const auto* Member = Panel->DoGet_SelectedMember();
                                return Member == nullptr ? FText::GetEmpty() : Get_RepLabel(Member->Representation);
                            })
                            .Tone_Lambda([WeakPanel]()
                            {
                                const auto Panel = WeakPanel.Pin();
                                if (NOT Panel.IsValid())
                                { return ECk_Tone::Neutral; }

                                const auto* Member = Panel->DoGet_SelectedMember();
                                if (Member == nullptr)
                                { return ECk_Tone::Neutral; }

                                switch (Member->Representation)
                                {
                                    case ECk_VisualLod_Representation::PromotedProxy: return ECk_Tone::Accent;
                                    case ECk_VisualLod_Representation::FarMember:     return ECk_Tone::Ok;
                                    default:                                          return ECk_Tone::Neutral;
                                }
                            })
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                            .Visibility(VisibleWhen([](const auto& InMember) { return InMember.Hidden; }))
                            .Font_Static(&Get_MetaFont)
                            .ColorAndOpacity(CkStyle::TextMute())
                            .Text(LOCTEXT("DetailHidden", "HIDDEN"))
                        ]
                    ]

                    // ---- Representation ----
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                    [
                        Make_PaneHeading(LOCTEXT("DetailRepHeading", "Representation"), FText::GetEmpty())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailCharge", "Promote charge"),
                            MemberText([](const auto& InMember, const auto&) { return InMember.Get_PromoteChargeText(); }),
                            MemberColor([](const auto& InMember) -> FLinearColor
                            {
                                // The charge is the budget the promote is billed to, so its tone is the budget's:
                                // lock reads as the reserved band, unbudgeted as the one nothing constrains.
                                if (NOT InMember.Promoted)       { return CkStyle::TextMute(); }
                                if (InMember.PromotedViaLock)    { return CkStyle::Warn(); }
                                if (InMember.PromotedUnbudgeted) { return CkStyle::Err(); }
                                return CkStyle::Accent();
                            }))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailSlot", "Crowd slot"),
                            MemberText([](const auto& InMember, const auto&)
                            {
                                if (InMember.SlotIndex < 0)
                                { return FString(TEXT("released")); }

                                return InMember.CrowdIndex == INDEX_NONE
                                    ? FString::Printf(TEXT("%d"), InMember.SlotIndex)
                                    : FString::Printf(TEXT("%d · crowd %d"), InMember.SlotIndex, InMember.CrowdIndex);
                            }),
                            CkStyle::Text())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailLocks", "Promote locks"),
                            MemberText([](const auto& InMember, const auto&)
                            {
                                // A counter, not a timer: nothing expires a lock, so a stuck promote is a missing
                                // release and the number is the whole diagnosis.
                                return InMember.PromoteLockCount > 0
                                    ? FString::Printf(TEXT("%d  (counter — no expiry)"), InMember.PromoteLockCount)
                                    : FString(TEXT("0"));
                            }),
                            MemberColor([](const auto& InMember)
                            {
                                return InMember.PromoteLockCount > 0 ? CkStyle::Warn() : CkStyle::TextDim();
                            }))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailPreempt", "Preempt demote"),
                            MemberText([](const auto& InMember, const auto&)
                            { return InMember.PreemptDemote ? FString(TEXT("true")) : FString(TEXT("false")); }),
                            MemberColor([](const auto& InMember)
                            { return InMember.PreemptDemote ? CkStyle::Info() : CkStyle::TextDim(); }))
                    ]

                    // ---- Fade ----
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_Separator()
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                    [
                        Make_PaneHeading(LOCTEXT("DetailFadeHeading", "Fade"), FText::GetEmpty())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailPhase", "Phase"),
                            MemberText([](const auto& InMember, const auto&)
                            { return Get_FadePhaseText(InMember.FadePhase).ToString(); }),
                            MemberColor([](const auto& InMember)
                            { return InMember.Get_IsFading() ? CkStyle::Info() : CkStyle::TextDim(); }))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, CkStyle::SpaceXS, 0.0f, CkStyle::SpaceXS)
                    [
                        SNew(SCkDebug_MeterBar)
                        .DesiredSize(FVector2D(k_DetailMeterWidth, k_MeterHeight))
                        .FillColor(CkStyle::Info())
                        .Fraction_Lambda([WeakPanel]()
                        {
                            const auto Panel = WeakPanel.Pin();
                            if (NOT Panel.IsValid())
                            { return 0.0f; }

                            const auto* Member = Panel->DoGet_SelectedMember();
                            return Member == nullptr ? 0.0f : Member->FadeAlpha;
                        })
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailAlpha", "Member α"),
                            MemberText([](const auto& InMember, const auto&)
                            {
                                // The two are one crossfade seen from both ends — printing only the member's alpha
                                // leaves the reader doing the subtraction to know what the proxy is doing.
                                return FString::Printf(TEXT("%.2f  ·  near dither = %.2f"),
                                    InMember.FadeAlpha, 1.0f - InMember.FadeAlpha);
                            }),
                            CkStyle::Text())
                    ]

                    // ---- Animation ----
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_Separator()
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                    [
                        Make_PaneHeading(LOCTEXT("DetailAnimHeading", "Animation"), FText::GetEmpty())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SVerticalBox)
                        .Visibility(VisibleWhen([](const auto& InMember) { return InMember.Promoted; }))
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            Make_KvRow(
                                LOCTEXT("DetailProxySeq", "SkelMesh sequence"),
                                MemberText([](const auto& InMember, const auto&)
                                {
                                    return InMember.ProxySequenceIndex == INDEX_NONE
                                        ? FString(TEXT("—"))
                                        : FString::Printf(TEXT("[%d]"), InMember.ProxySequenceIndex);
                                }),
                                CkStyle::Text())
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            Make_KvRow(
                                LOCTEXT("DetailProxyRate", "SkelMesh rate"),
                                MemberText([](const auto& InMember, const auto&)
                                { return FString::Printf(TEXT("%.2f×"), InMember.ProxyRate); }),
                                CkStyle::Text())
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SVerticalBox)
                        .Visibility(VisibleWhen([](const auto& InMember) { return NOT InMember.Promoted; }))
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            Make_KvRow(
                                LOCTEXT("DetailFarSeq", "Far sequence"),
                                MemberText([](const auto& InMember, const auto&)
                                {
                                    return InMember.FarSequenceIndex == INDEX_NONE
                                        ? FString(TEXT("—  (no slot)"))
                                        : FString::Printf(TEXT("[%d]  ·  speed-driven"), InMember.FarSequenceIndex);
                                }),
                                CkStyle::Text())
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            Make_KvRow(
                                LOCTEXT("DetailFarRate", "Far rate"),
                                MemberText([](const auto& InMember, const auto&)
                                { return FString::Printf(TEXT("%.2f×"), InMember.FarRate); }),
                                CkStyle::Text())
                        ]
                    ]

                    // ---- Rendering (DISPLAY ONLY) ----
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_Separator()
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                    [
                        Make_PaneHeading(LOCTEXT("DetailRenderHeading", "Rendering"), FText::GetEmpty())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailRenderBand", "Far render band"),
                            MemberText([](const auto& InMember, const auto&)
                            {
                                return InMember.RenderBandIndex == INDEX_NONE
                                    ? FString(TEXT("legacy / no slot"))
                                    : FString::Printf(TEXT("[%d]"), InMember.RenderBandIndex);
                            }),
                            CkStyle::TextDim())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailComponent", "Component"),
                            MemberText([](const auto& InMember, const auto& InArbiter)
                            {
                                const auto Flags = Get_EffectiveRenderFlags(InArbiter, InMember);
                                return Flags.Resolved ? Flags.ComponentDesc : FString(TEXT("not rendered"));
                            }),
                            MemberColor([WeakPanel](const auto& InMember) -> FLinearColor
                            {
                                const auto Panel = WeakPanel.Pin();
                                if (NOT Panel.IsValid())
                                { return CkStyle::TextMute(); }

                                return Get_EffectiveRenderFlags(Panel->_Live, InMember).Resolved
                                    ? CkStyle::Text()
                                    : CkStyle::TextMute();
                            }))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailCastShadow", "Cast shadow"),
                            MemberText([](const auto& InMember, const auto& InArbiter)
                            {
                                const auto Flags = Get_EffectiveRenderFlags(InArbiter, InMember);
                                if (NOT Flags.Resolved)
                                { return FString(TEXT("—")); }

                                return Flags.CastShadow ? FString(TEXT("true")) : FString(TEXT("false"));
                            }),
                            MemberColor([WeakPanel](const auto& InMember) -> FLinearColor
                            {
                                const auto Panel = WeakPanel.Pin();
                                if (NOT Panel.IsValid())
                                { return CkStyle::TextMute(); }

                                const auto Flags = Get_EffectiveRenderFlags(Panel->_Live, InMember);
                                return Flags.Resolved ? Get_BoolColor(Flags.CastShadow) : CkStyle::TextMute();
                            }))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailIndirect", "Dyn. indirect"),
                            MemberText([](const auto& InMember, const auto& InArbiter)
                            {
                                const auto Flags = Get_EffectiveRenderFlags(InArbiter, InMember);
                                if (NOT Flags.Resolved)
                                { return FString(TEXT("—")); }

                                return Flags.AffectDynamicIndirectLighting
                                    ? FString(TEXT("true"))
                                    : FString(TEXT("false"));
                            }),
                            MemberColor([WeakPanel](const auto& InMember) -> FLinearColor
                            {
                                const auto Panel = WeakPanel.Pin();
                                if (NOT Panel.IsValid())
                                { return CkStyle::TextMute(); }

                                const auto Flags = Get_EffectiveRenderFlags(Panel->_Live, InMember);
                                return Flags.Resolved
                                    ? Get_BoolColor(Flags.AffectDynamicIndirectLighting)
                                    : CkStyle::TextMute();
                            }))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailDistanceField", "Distance field"),
                            MemberText([](const auto& InMember, const auto& InArbiter)
                            {
                                const auto Flags = Get_EffectiveRenderFlags(InArbiter, InMember);
                                if (NOT Flags.Resolved)
                                { return FString(TEXT("—")); }

                                return Flags.AffectDistanceFieldLighting
                                    ? FString(TEXT("true"))
                                    : FString(TEXT("false"));
                            }),
                            MemberColor([WeakPanel](const auto& InMember) -> FLinearColor
                            {
                                const auto Panel = WeakPanel.Pin();
                                if (NOT Panel.IsValid())
                                { return CkStyle::TextMute(); }

                                const auto Flags = Get_EffectiveRenderFlags(Panel->_Live, InMember);
                                return Flags.Resolved
                                    ? Get_BoolColor(Flags.AffectDistanceFieldLighting)
                                    : CkStyle::TextMute();
                            }))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailChannels", "Light channels"),
                            MemberText([](const auto& InMember, const auto& InArbiter)
                            {
                                const auto Flags = Get_EffectiveRenderFlags(InArbiter, InMember);
                                return Flags.Resolved ? Flags.Get_LightingChannelsText() : FString(TEXT("—"));
                            }),
                            CkStyle::TextDim())
                    ]

                    // ---- Ranking inputs ----
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_Separator()
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                    [
                        Make_PaneHeading(LOCTEXT("DetailRankHeading", "Ranking inputs"), FText::GetEmpty())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailDistance", "Distance"),
                            MemberText([](const auto& InMember, const auto&)
                            {
                                return InMember.LastDistance < 0.0f
                                    ? FString(TEXT("—  (never ranked)"))
                                    : FString::Printf(TEXT("%.1f"), InMember.LastDistance);
                            }),
                            CkStyle::Text())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Make_KvRow(
                            LOCTEXT("DetailInView", "In view"),
                            MemberText([](const auto& InMember, const auto&)
                            { return InMember.LastInView ? FString(TEXT("true")) : FString(TEXT("false")); }),
                            MemberColor([](const auto& InMember)
                            { return InMember.LastInView ? CkStyle::Ok() : CkStyle::TextMute(); }))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Font_Static(&Get_MetaFont)
                            .ColorAndOpacity(CkStyle::TextDim())
                            .Text(LOCTEXT("DetailArbiter", "Arbiter"))
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(SCkDebug_EntityRef)
                            .ShowName(true)
                            .Entity_Lambda([WeakPanel]() -> FCk_Handle
                            {
                                const auto Panel = WeakPanel.Pin();
                                return Panel.IsValid() ? Panel->_Live.Entity : FCk_Handle{};
                            })
                        ]
                    ]
                ]
            ]
        ];
}

// ====================================================================================================================
// Event log
// ====================================================================================================================

auto
    SCkVisualLodDebuggerWindow::
    DoBuild_EventLog()
    -> TSharedRef<SWidget>
{
    using namespace ck_visuallod_debugger_window;

    const auto WeakPanel = TWeakPtr<SCkVisualLodDebuggerWindow>(SharedThis(this));

    return SNew(SBox)
        .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceL, CkStyle::SpaceL})
        [
            SNew(SCkDebug_Card)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                [
                    Make_PaneHeading(
                        LOCTEXT("EventHeading", "Recent activity"),
                        TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                        {
                            const auto Panel = WeakPanel.Pin();
                            if (NOT Panel.IsValid() || NOT Panel->_EventLog.IsValid())
                            { return FText::GetEmpty(); }

                            // "diffed" rather than "logged": these lines are synthesized from the delta between two
                            // gated snapshots, so the pane says what they actually are.
                            return FText::FromString(FString::Printf(TEXT("%d · diffed @ refresh"),
                                Panel->_EventLog->Get_EntryCount()));
                        }))
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SAssignNew(_EventLog, SCkDebug_EventLog)
                    .MaxEntries(k_EventLogMaxItems)
                    .EmptyText(LOCTEXT("EventEmpty", "No representation changes since the window opened."))
                ]
            ]
        ];
}

auto
    SCkVisualLodDebuggerWindow::
    DoUpdate_EventLog()
    -> void
{
    if (NOT _EventLog.IsValid())
    { return; }

    if (NOT _HasLiveArbiter)
    {
        _MemberEventStates.Reset();
        _HasEventBaseline = false;
        return;
    }

    // Events are per-arbiter. Carrying a previous domain's states across a tab switch would report the whole new
    // membership as if every member had just appeared.
    if (_EventDomain != _Live.TabId)
    {
        _EventDomain      = _Live.TabId;
        _HasEventBaseline = false;
        _MemberEventStates.Reset();
        _EventLog->Clear_Entries();
        _EventFrozen = _Live.Frozen;
    }

    auto Entries = TArray<FCkDebug_EventLogEntry>{};

    const auto Push = [this, &Entries](const FString& InMessage, const FString& InCategory, ECk_Tone InTone)
    {
        auto Entry        = FCkDebug_EventLogEntry{};
        Entry.Message     = InMessage;
        Entry.Category    = InCategory;
        Entry.Tone        = InTone;
        Entry.TimeSeconds = _TickTimeSeconds;
        Entries.Add(MoveTemp(Entry));
    };

    if (_EventFrozen != _Live.Frozen)
    {
        _EventFrozen = _Live.Frozen;
        Push(_Live.Frozen
                ? FString(TEXT("Arbiter frozen — flips suspended, in-flight fades still finish"))
                : FString(TEXT("Arbiter unfrozen — arbitration resumed")),
            FString(TEXT("FREEZE")),
            ECk_Tone::Info);
    }

    auto NextStates = TMap<FCk_Handle, FMemberEventState>{};
    NextStates.Reserve(_Live.Members.Num());

    for (const auto& Member : _Live.Members)
    {
        auto State           = FMemberEventState{};
        State.Representation = Member.Representation;
        State.PreemptDemote  = Member.PreemptDemote;
        State.SlotIndex      = Member.SlotIndex;

        const auto* Previous = _MemberEventStates.Find(Member.Entity);
        NextStates.Add(Member.Entity, State);

        // The FIRST populated tick is a baseline, not a burst of events: reporting every existing member as newly
        // appeared would bury the one transition the reader opened the window for.
        if (NOT _HasEventBaseline)
        { continue; }

        if (Previous == nullptr)
        {
            // A member that appears already starved is the pool-exhaustion outcome the Unrendered stat counts, and
            // it is the only appearance worth a line — a member arriving with a slot or a proxy is business as usual.
            if (Member.Get_IsUnrendered() && Member.SlotIndex < 0)
            {
                Push(FString::Printf(TEXT("%s appeared unrendered — pool starved"), *Member.Name),
                    FString(TEXT("POOL")), ECk_Tone::Err);
            }
            continue;
        }

        if (Previous->Representation != ECk_VisualLod_Representation::PromotedProxy
            && Member.Representation == ECk_VisualLod_Representation::PromotedProxy)
        {
            Push(FString::Printf(TEXT("%s promoted (%s%s)"),
                    *Member.Name,
                    *Member.Get_PromoteChargeText(),
                    Member.LastInView ? TEXT(" · in view") : TEXT("")),
                FString(TEXT("PROMOTE")), ECk_Tone::Ok);
        }
        else if (Previous->Representation == ECk_VisualLod_Representation::PromotedProxy
            && Member.Representation == ECk_VisualLod_Representation::FarMember)
        {
            Push(FString::Printf(TEXT("%s demote finished (slot %s)"),
                    *Member.Name,
                    *ck_visuallod_debugger_window::Get_SlotText(Member)),
                FString(TEXT("DEMOTE")), ECk_Tone::Info);
        }

        if (NOT Previous->PreemptDemote && Member.PreemptDemote)
        {
            Push(FString::Printf(TEXT("%s preempt-demoted — a nearer member took the budget"), *Member.Name),
                FString(TEXT("PREEMPT")), ECk_Tone::Warn);
        }

        if (Previous->SlotIndex >= 0 && Member.SlotIndex < 0 && Member.Get_IsUnrendered())
        {
            Push(FString::Printf(TEXT("%s lost its slot with nothing to fall back to — unrendered"), *Member.Name),
                FString(TEXT("POOL")), ECk_Tone::Err);
        }
    }

    if (_HasEventBaseline)
    {
        for (const auto& Kvp : _MemberEventStates)
        {
            if (NextStates.Contains(Kvp.Key))
            { continue; }

            Push(FString::Printf(TEXT("member released (%s)"), *ck::Format_UE(TEXT("{}"), Kvp.Key)),
                FString(TEXT("RELEASE")), ECk_Tone::Neutral);
        }
    }

    _MemberEventStates = MoveTemp(NextStates);
    _HasEventBaseline  = true;

    if (NOT Entries.IsEmpty())
    { _EventLog->Add_Entries(MoveTemp(Entries)); }
}

// ====================================================================================================================
// World markers
// ====================================================================================================================

auto
    SCkVisualLodDebuggerWindow::
    DoUpdate_Markers()
    -> void
{
    using namespace ck_visuallod_debugger_window;

    if (NOT _MarkersEnabled)
    {
        _MarkerSet.Reset();
        return;
    }

    auto* World = DoGet_TargetWorld();
    if (ck::Is_NOT_Valid(World))
    {
        _MarkerSet.Reset();
        return;
    }

    // Whichever camera the user is looking through — the ejected editor one while inspecting, the player's otherwise.
    // Falling back to the arbiter's own cached view keeps the flat markers facing SOMETHING rather than edge-on.
    const auto ViewerLocation = ck::DebugViewportView::Get_ViewCameraLocation(World)
        .Get(_Live.ViewValid ? _Live.ViewLocation : FVector::ZeroVector);

    auto LiveKeys = TSet<FCk_Handle>{};
    LiveKeys.Reserve(_Live.Members.Num());

    for (const auto& Member : _Live.Members)
    {
        // Hidden members are deliberately not drawn and deliberately not marked: a marker over nothing is the one
        // thing this overlay must never claim.
        if (Member.Hidden || NOT Member.HasWorldLocation || ck::Is_NOT_Valid(Member.Entity))
        { continue; }

        const auto Shape = Member.Promoted
            ? FCkVisualLodDebugger_MarkerSet::EShape::Diamond
            : Member.SlotIndex >= 0
                ? FCkVisualLodDebugger_MarkerSet::EShape::Dot
                : FCkVisualLodDebugger_MarkerSet::EShape::Ring;

        const auto Selected = ck::IsValid(_SelectedMember) && Member.Entity == _SelectedMember;

        LiveKeys.Add(Member.Entity);

        _MarkerSet.Update_Marker(
            World,
            Member.Entity,
            Shape,
            Member.WorldLocation + FVector{0.0, 0.0, k_MarkerHeightCm},
            Get_RepColor(Member.Representation),
            Selected,
            ViewerLocation);
    }

    // Members that vanished, went hidden, or belong to a domain the user switched away from.
    _MarkerSet.Prune(LiveKeys);
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE
