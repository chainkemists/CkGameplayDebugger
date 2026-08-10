// --------------------------------------------------------------------------------------------------------------------
// Search inputs and the palette-derived domain ramps:
//   - SCkDebug_SearchBar   single-mode debounced filter, next to its dual sibling for comparison
//   - Ramps                Get_HeatColor / Get_ScoreColor / Get_CategoricalColor swatch strips
//
// The ramp strips are the visual audit for "no debugger writes its own hex" — if a stop looks
// wrong here, fix the CkStyle role, not the call site. Helpers live in a NAMED namespace because
// gallery .cpp files are unity-built together.
// --------------------------------------------------------------------------------------------------------------------

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Gallery/CkDebuggerGallery_Registry.h"
#include "CkGallery_SectionUtils.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#include <limits>

using ck::gallery::Caption;

// ====================================================================================================================

namespace ck_gallery_ramps
{
    constexpr auto SwatchWidth = 46.0f;
    constexpr auto SwatchHeight = 26.0f;
    constexpr auto RampSteps = 11;

    auto Make_Swatch(const FLinearColor& InColor, const FString& InLabel) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(SwatchWidth)
            .HeightOverride(SwatchHeight)
            [
                SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush_Small())
                .BorderBackgroundColor(FSlateColor{InColor})
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(InLabel))
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                    // Contrast against an arbitrary ramp stop: pick by perceived brightness.
                    .ColorAndOpacity(FSlateColor{
                        (0.299f * InColor.R + 0.587f * InColor.G + 0.114f * InColor.B) > 0.55f
                            ? CkStyle::BgRoot()
                            : CkStyle::TextStrong()})
                ]
            ];
    }

    auto Make_RampRow(TFunctionRef<FLinearColor(float)> InRamp) -> TSharedRef<SWidget>
    {
        auto Row = SNew(SHorizontalBox);

        for (auto Step = 0; Step < RampSteps; ++Step)
        {
            const auto Alpha = static_cast<float>(Step) / static_cast<float>(RampSteps - 1);

            Row->AddSlot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
                [
                    Make_Swatch(InRamp(Alpha), FString::Printf(TEXT("%.1f"), Alpha))
                ];
        }

        return Row;
    }

    // Out-of-range and NaN inputs must produce the clamped endpoints, never an undefined color.
    auto Make_ClampRow(TFunctionRef<FLinearColor(float)> InRamp) -> TSharedRef<SWidget>
    {
        const auto Cases = TArray<TPair<float, FString>>{
            {-5.0f, FString{TEXT("-5")}},
            {0.0f,  FString{TEXT("0")}},
            {1.0f,  FString{TEXT("1")}},
            {9.0f,  FString{TEXT("9")}},
            {std::numeric_limits<float>::quiet_NaN(), FString{TEXT("NaN")}},
        };

        auto Row = SNew(SHorizontalBox);

        for (const auto& Case : Cases)
        {
            Row->AddSlot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
                [
                    Make_Swatch(InRamp(Case.Key), Case.Value)
                ];
        }

        return Row;
    }
}

// ====================================================================================================================
// SEARCH BARS
// ====================================================================================================================

class SCkGallery_SearchBarDemo : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGallery_SearchBarDemo) {}
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
                Caption(TEXT("SCkDebug_SearchBar — one query, 0.3 s debounce, clear button appears once there is text. Type and watch the echo lag by the debounce; press Enter to commit immediately."))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                SNew(SBox)
                .MaxDesiredWidth(360.0f)
                [
                    SNew(SCkDebug_SearchBar)
                    .HintText(FText::FromString(TEXT("Filter inspectors...")))
                    .OnSearchTextChanged_Lambda([this](const FString& InText) { _SingleText = InText; })
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Left)
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
            [
                SNew(SCkDebug_StatusPill)
                .Text_Lambda([this]()
                {
                    return FText::FromString(_SingleText.IsEmpty()
                        ? FString{TEXT("(no filter)")}
                        : ck::Format_UE(TEXT("filter: {}"), _SingleText));
                })
                .Tone_Lambda([this]() { return _SingleText.IsEmpty() ? ECk_Tone::Neutral : ECk_Tone::Info; })
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                Caption(TEXT("SCkDebug_DualSearchBar — the two-query sibling. Reach for this one whenever the surface has a list the user both narrows AND highlights."))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                SNew(SBox)
                .MaxDesiredWidth(360.0f)
                [
                    SNew(SCkDebug_DualSearchBar)
                    .OnFilterTextChanged_Lambda([this](const FString& InText) { _FilterText = InText; })
                    .OnHighlightTextChanged_Lambda([this](const FString& InText) { _HighlightText = InText; })
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Left)
            [
                SNew(SCkDebug_StatusPill)
                .Text_Lambda([this]()
                {
                    return FText::FromString(ck::Format_UE(TEXT("filter '{}' · highlight '{}'"),
                        _FilterText, _HighlightText));
                })
                .Tone(ECk_Tone::Neutral)
            ]
        ];
    }

private:
    FString _SingleText;
    FString _FilterText;
    FString _HighlightText;
};

class FCkGallery_SearchBars : public ICkDebuggerGallery_Section
{
public:
    virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Search Bars")); }
    virtual auto Get_Description() const -> FText override
    {
        return FText::FromString(TEXT("The single-mode search bar promoted from the ECS debugger, shown next to its dual-query sibling so the choice between them is obvious."));
    }
    virtual auto Get_SortPriority() const -> int32 override { return 50; }

    virtual auto Build_Widget() -> TSharedRef<SWidget> override
    {
        return SNew(SCkGallery_SearchBarDemo);
    }
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_SearchBars)

// ====================================================================================================================
// RAMPS
// ====================================================================================================================

class FCkGallery_Ramps : public ICkDebuggerGallery_Section
{
public:
    virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Domain Ramps")); }
    virtual auto Get_Description() const -> FText override
    {
        return FText::FromString(TEXT("ck::debug_axes heat / score / categorical helpers. Every stop is a CkStyle role, so editing the palette moves all of them; the clamp rows prove out-of-range and NaN inputs are total."));
    }
    virtual auto Get_SortPriority() const -> int32 override { return 55; }

    virtual auto Build_Widget() -> TSharedRef<SWidget> override
    {
        using namespace ck_gallery_ramps;

        auto Column = SNew(SVerticalBox);

        const auto AddBlock = [&Column](const FString& InCaption, TSharedRef<SWidget> InContent)
        {
            Column->AddSlot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                [ Caption(InCaption) ];

            Column->AddSlot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
                [ InContent ];
        };

        AddBlock(
            TEXT("Get_HeatColor — Ok at 0.0, Warn exactly at 0.5, Err at 1.0 (Scheduler timing heat, Insights budget banding)."),
            Make_RampRow([](float InAlpha) { return ck::debug_axes::Get_HeatColor(InAlpha); }));

        AddBlock(
            TEXT("Get_HeatColor — clamping and NaN totality."),
            Make_ClampRow([](float InAlpha) { return ck::debug_axes::Get_HeatColor(InAlpha); }));

        AddBlock(
            TEXT("Get_ScoreColor — Info at 0.0 to Ok at 1.0 (Eqs candidate gradient)."),
            Make_RampRow([](float InAlpha) { return ck::debug_axes::Get_ScoreColor(InAlpha); }));

        AddBlock(
            TEXT("Get_ScoreColor — clamping and NaN totality."),
            Make_ClampRow([](float InAlpha) { return ck::debug_axes::Get_ScoreColor(InAlpha); }));

        // Deliberately runs past the palette size so the wrap is visible: index N repeats index 0.
        {
            const auto PaletteSize = ck::debug_axes::Get_CategoricalPaletteSize();
            auto Row = SNew(SHorizontalBox);

            for (auto Index = -2; Index < PaletteSize + 2; ++Index)
            {
                Row->AddSlot()
                    .AutoWidth()
                    .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
                    [
                        Make_Swatch(
                            ck::debug_axes::Get_CategoricalColor(Index),
                            FString::FromInt(Index))
                    ];
            }

            AddBlock(
                ck::Format_UE(
                    TEXT("Get_CategoricalColor — {} distinct entries; the index wraps in both directions, so -1 matches {}."),
                    PaletteSize, PaletteSize - 1),
                Row);
        }

        return Column;
    }
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_Ramps)

// --------------------------------------------------------------------------------------------------------------------
