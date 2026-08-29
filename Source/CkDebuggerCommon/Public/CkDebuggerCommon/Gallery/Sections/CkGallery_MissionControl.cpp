// --------------------------------------------------------------------------------------------------------------------
// Showcases for the Mission Control widget set (added for the GOAP debugger
// port, shared by every debugger):
//   Chip, ValuePill, Switch, MeterBar, Card, AlertRow, UnderlineTabs, Stepper,
//   GlowWrap.
// --------------------------------------------------------------------------------------------------------------------

#include "CkDebuggerCommon/Gallery/CkDebuggerGallery_Registry.h"
#include "CkGallery_SectionUtils.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_AlertRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_GlowWrap.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Stepper.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_UnderlineTabs.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ValuePill.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

using ck::gallery::Caption;

namespace ck_gallery_mission_control
{
    auto LabeledRow(const FString& InLabel, TSharedRef<SWidget> InWidget) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
            [
                SNew(SBox)
                .MinDesiredWidth(150.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(InLabel))
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                InWidget
            ];
    }

    auto Pad(TSharedRef<SWidget> InWidget) -> TSharedRef<SWidget>
    {
        return SNew(SBox).Padding(FMargin(0.0f, CkStyle::SpaceS)) [ InWidget ];
    }
}

// ====================================================================================================================
// Chips + value pills
// ====================================================================================================================

class FCkGallery_MissionControl_Chips : public ICkDebuggerGallery_Section
{
public:
    virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Chips & Value Pills")); }
    virtual auto Get_Description() const -> FText override
    {
        return FText::FromString(TEXT("Condition/effect chips (satisfied / unsatisfied / effect / neutral, trace glow) and TRUE/FALSE value pills (read-only vs editable)."));
    }
    virtual auto Get_SortPriority() const -> int32 override { return 62; }

    virtual auto Build_Widget() -> TSharedRef<SWidget> override
    {
        using namespace ck_gallery_mission_control;

        return SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Satisfied:"),
                SNew(SCkDebug_Chip).Text(FText::FromString(TEXT("Has Ammo"))).Kind(ECkDebug_ChipKind::Satisfied)
                    .CopyText(TEXT("Gym.GoapFEAR.WS.Combatant.HasAmmo")))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Unsatisfied:"),
                SNew(SCkDebug_Chip).Text(FText::FromString(TEXT("At Cover"))).Kind(ECkDebug_ChipKind::Unsatisfied))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Effect:"),
                SNew(SCkDebug_Chip).Text(FText::FromString(TEXT("Enemy Neutralized"))).Kind(ECkDebug_ChipKind::Effect))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Neutral:"),
                SNew(SCkDebug_Chip).Text(FText::FromString(TEXT("no preconditions"))).Kind(ECkDebug_ChipKind::Neutral))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Trace-highlighted:"),
                SNew(SCkDebug_Chip).Text(FText::FromString(TEXT("Behind Enemy"))).Kind(ECkDebug_ChipKind::Satisfied).Highlighted(true))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Value TRUE:"),
                SNew(SCkDebug_ValuePill).Value(true))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Value FALSE:"),
                SNew(SCkDebug_ValuePill).Value(false))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Editable (hover):"),
                SNew(SCkDebug_ValuePill).Value(true).Editable(true))) ]

            + SVerticalBox::Slot().AutoHeight() [ Caption(TEXT("Chips flip kind live via attribute; Highlighted draws the key-trace glow ring. Editable pills only consume clicks when editable — keep them out of list rows.")) ];
    }
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_MissionControl_Chips)

// ====================================================================================================================
// Switch, stepper, meter
// ====================================================================================================================

class FCkGallery_MissionControl_Controls : public ICkDebuggerGallery_Section
{
public:
    virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Switch, Stepper & Meter")); }
    virtual auto Get_Description() const -> FText override
    {
        return FText::FromString(TEXT("Sliding toggle (Sandbox/Enabled), −/+ cost stepper, and thin meter bars (cost, budget, key budget)."));
    }
    virtual auto Get_SortPriority() const -> int32 override { return 63; }

    virtual auto Build_Widget() -> TSharedRef<SWidget> override
    {
        using namespace ck_gallery_mission_control;

        return SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Switch ON / OFF:"),
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, CkStyle::SpaceM, 0)
                    [ SNew(SCkDebug_Switch).IsOn_Lambda([this] { return _SwitchState; })
                        .OnStateChanged_Lambda([this](bool InNew) { _SwitchState = InNew; }) ]
                + SHorizontalBox::Slot().AutoWidth()
                    [ SNew(SCkDebug_Switch).IsOn(false) ])) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Stepper:"),
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, CkStyle::SpaceM, 0)
                    [ SNew(STextBlock).Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                        .Text_Lambda([this] { return FText::FromString(FString::Printf(TEXT("%.1f"), _StepperValue)); })
                        .ColorAndOpacity(FSlateColor(CkStyle::Text())) ]
                + SHorizontalBox::Slot().AutoWidth()
                    [ SNew(SCkDebug_Stepper).OnDelta_Lambda([this](int32 InDelta) { _StepperValue = FMath::Max(0.0f, _StepperValue + InDelta * 0.5f); }) ])) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Meter (accent):"),
                SNew(SCkDebug_MeterBar).Fraction(0.35f).FillColor(CkStyle::Accent()))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Meter (ok, key budget):"),
                SNew(SCkDebug_MeterBar).Fraction(0.125f).FillColor(CkStyle::Ok()))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(LabeledRow(TEXT("Meter (warn, near cap):"),
                SNew(SCkDebug_MeterBar).Fraction(0.92f).FillColor(CkStyle::Warn()))) ];
    }

private:
    bool  _SwitchState  = true;
    float _StepperValue = 2.0f;
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_MissionControl_Controls)

// ====================================================================================================================
// Cards, glow, alert rows, underline tabs
// ====================================================================================================================

class FCkGallery_MissionControl_Surfaces : public ICkDebuggerGallery_Section
{
public:
    virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Cards, Glow, Alerts & Tabs")); }
    virtual auto Get_Description() const -> FText override
    {
        return FText::FromString(TEXT("Accent-stripe cards, the soft glow halo, alert strip rows, and underline tab bars."));
    }
    virtual auto Get_SortPriority() const -> int32 override { return 64; }

    virtual auto Build_Widget() -> TSharedRef<SWidget> override
    {
        using namespace ck_gallery_mission_control;

        const auto CardBody = [](const TCHAR* InTitle, const TCHAR* InWhy) -> TSharedRef<SWidget>
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ SNew(STextBlock).Text(FText::FromString(InTitle))
                        .Font(CkStyle::BoldFont(CkStyle::FontSizeBody())).ColorAndOpacity(FSlateColor(CkStyle::Text())) ]
                + SHorizontalBox::Slot().FillWidth(1.0f) [ SNew(SBox) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ SNew(STextBlock).Text(FText::FromString(InWhy))
                        .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall())).ColorAndOpacity(FSlateColor(CkStyle::TextDim())) ];
        };

        return SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight() [ Pad(
                SNew(SCkDebug_Card).StripeColor(CkStyle::Accent())
                    [ CardBody(TEXT("Attack Enemy   1.0"), TEXT("in the plan — step 1")) ]) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(
                SNew(SCkDebug_Card).StripeColor(CkStyle::Err())
                    [ CardBody(TEXT("Attack From Cover   1.0"), TEXT("blocked — needs At Cover")) ]) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(
                SNew(SCkDebug_Card).StripeColor(CkStyle::Warn())
                    [ CardBody(TEXT("Wait For Enemy   999.0"), TEXT("fallback — only wins when nothing else can")) ]) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(
                SNew(SCkDebug_Card).GlowColor(CkStyle::Accent()).Selected(true)
                    [ CardBody(TEXT("Attack From Flank   0.5"), TEXT("active chain leaf — glowing")) ]) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(
                SNew(SCkDebug_AlertRow)
                    .Tone(ECk_Tone::Warn)
                    .Glyph(FText::FromString(TEXT("⚠")))
                    .LeadText(FText::FromString(TEXT("Fallback plan active")))
                    .BodyText(FText::FromString(TEXT("— no affordable path to the goal from the current world state.")))
                    .FixText(FText::FromString(TEXT("Check the Decision panel: what's blocked?")))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(
                SNew(SCkDebug_AlertRow)
                    .Tone(ECk_Tone::Accent)
                    .Glyph(FText::FromString(TEXT("🧪")))
                    .LeadText(FText::FromString(TEXT("Sandbox active")))
                    .BodyText(FText::FromString(TEXT("— \"DebugUI\" override layer (2 keys). Reads are shadowed; base store untouched.")))
                    .ActionText(FText::FromString(TEXT("Pop layer")))) ]

            + SVerticalBox::Slot().AutoHeight() [ Pad(
                SNew(SCkDebug_UnderlineTabs)
                    .Tabs({
                        FCkDebug_UnderlineTabDesc{FName{TEXT("Squad")},     FText::FromString(TEXT("Squad")), ECk_Icon::None, 0, FText::FromString(TEXT("6")), false},
                        FCkDebug_UnderlineTabDesc{FName{TEXT("Inspector")}, FText::FromString(TEXT("Agent Inspector"))},
                        FCkDebug_UnderlineTabDesc{FName{TEXT("Catalog")},   FText::FromString(TEXT("Catalog Audit")), ECk_Icon::None, 0, TAttribute<FText>{}, true},
                    })
                    .ActiveTabId_Lambda([this] { return _ActiveTab; })
                    .OnTabSelected_Lambda([this](FName InTab) { _ActiveTab = InTab; })) ]

            + SVerticalBox::Slot().AutoHeight() [ Caption(TEXT("The glow halo extends past the card — treat it as shadow breathing-room in layouts. Tabs: count chip + warn dot are live attributes.")) ];
    }

private:
    FName _ActiveTab = FName{TEXT("Inspector")};
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_MissionControl_Surfaces)

// ====================================================================================================================
