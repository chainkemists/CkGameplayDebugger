#include "CkJoltDebugger/Window/SCkJoltDebugger_DetailPanel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_debugger_detail_panel
{
    auto Get_UnsetText() -> FText
    { return FText::FromString(TEXT("--")); }

    // The referent an unbound panel hands back, so the whole read path can stay by-reference.
    auto Get_NoSelection() -> const TOptional<FCkJoltDebugger_BodySnapshot>&
    {
        static const auto None = TOptional<FCkJoltDebugger_BodySnapshot>{};
        return None;
    }

    auto Get_NoFacts() -> const FCkJoltDebugger_SelectionFacts&
    {
        static const auto None = FCkJoltDebugger_SelectionFacts{};
        return None;
    }

    auto Font_ContactRow() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); }

    auto Get_ContactRowPadding() -> FMargin
    { return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceS, 1.0f}); }

    auto Get_PopulationText(
        ECkJoltDebugger_Population InPopulation) -> FText
    {
        switch (InPopulation)
        {
            case ECkJoltDebugger_Population::JoltBody:    return FText::FromString(TEXT("Jolt Body"));
            case ECkJoltDebugger_Population::BakedStatic: return FText::FromString(TEXT("Baked Static World"));
            case ECkJoltDebugger_Population::Sensor:      return FText::FromString(TEXT("Sensor (Probe)"));
            case ECkJoltDebugger_Population::Character:   return FText::FromString(TEXT("Character"));
            case ECkJoltDebugger_Population::Constraint:  return FText::FromString(TEXT("Constraint"));
            default:                                      return Get_UnsetText();
        }
    }

    auto Get_MotionText(
        ECk_MotionType InMotionType) -> FText
    {
        switch (InMotionType)
        {
            case ECk_MotionType::Kinematic: return FText::FromString(TEXT("Kinematic"));
            case ECk_MotionType::Dynamic:   return FText::FromString(TEXT("Dynamic"));
            default:                        return FText::FromString(TEXT("Static"));
        }
    }

    auto Get_MotionQualityText(
        ECk_MotionQuality InQuality) -> FText
    {
        return FText::FromString(InQuality == ECk_MotionQuality::LinearCast
            ? TEXT("LinearCast (CCD)")
            : TEXT("Discrete"));
    }

    auto Get_GroundStateText(
        ECk_JoltCharacter_GroundState InGroundState) -> FText
    {
        switch (InGroundState)
        {
            case ECk_JoltCharacter_GroundState::OnGround:     return FText::FromString(TEXT("On Ground"));
            case ECk_JoltCharacter_GroundState::OnSteepSlope: return FText::FromString(TEXT("On Steep Slope"));
            case ECk_JoltCharacter_GroundState::NotSupported: return FText::FromString(TEXT("Not Supported"));
            default:                                          return FText::FromString(TEXT("In Air"));
        }
    }

    auto Get_SleepText(
        ECk_Jolt_SleepState InSleepState) -> FText
    {
        return FText::FromString(InSleepState == ECk_Jolt_SleepState::Asleep ? TEXT("Asleep") : TEXT("Awake"));
    }

    auto Get_VectorText(
        const FVector& InVector) -> FText
    {
        return FText::FromString(ck::Format_UE(TEXT("{:.1f}, {:.1f}, {:.1f}"), InVector.X, InVector.Y, InVector.Z));
    }

    auto Get_ScalarText(
        float InValue) -> FText
    {
        return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), InValue));
    }

    /*
     * ZERO MEANS INFINITE on the facility's side — Jolt stores an inverse mass, and a static or explicitly
     * infinite-mass body has an inverse of 0. Rendering that as "0.00 kg" would read as a massless body,
     * which is the exact opposite of what it means.
     */
    auto Get_MassText(
        float InMass) -> FText
    {
        return InMass <= 0.0f
            ? FText::FromString(TEXT("Infinite"))
            : FText::FromString(ck::Format_UE(TEXT("{:.2f} kg"), InMass));
    }

    auto Get_BoundsText(
        const FBox& InBounds) -> FText
    {
        if (InBounds.IsValid == 0)
        { return Get_UnsetText(); }

        const auto Size = InBounds.GetSize();
        return FText::FromString(ck::Format_UE(TEXT("{:.0f} x {:.0f} x {:.0f}"), Size.X, Size.Y, Size.Z));
    }

    auto Get_ContactRowText(
        const FCkJoltDebugger_ContactRow& InRow) -> FString
    {
        return ck::Format_UE(TEXT("Body {}   {} pts   {:.2f} cm"),
            InRow.OtherBodyKey,
            InRow.NumContactPoints,
            InRow.PenetrationDepth);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_DetailPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    using namespace ck_jolt_debugger_detail_panel;

    _GetSelection      = InArgs._GetSelection;
    _GetSelectionFacts = InArgs._GetSelectionFacts;
    _OnContactSelected = InArgs._OnContactSelected;

    const auto WeakPanel = TWeakPtr<SCkJoltDebugger_DetailPanel>{SharedThis(this)};

    const auto Get_HasSelection = [WeakPanel]() -> bool
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->Get_Selection().IsSet();
    };

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
        [
            SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Selected Body")))
        ]

        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
        [
            SNew(STextBlock)
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            .Text(FText::FromString(TEXT("Nothing selected. Click a row or a body in the viewport.")))
            .Visibility_Lambda([Get_HasSelection]() -> EVisibility
            {
                return Get_HasSelection() ? EVisibility::Collapsed : EVisibility::Visible;
            })
        ]

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SVerticalBox)
            .Visibility_Lambda([Get_HasSelection]() -> EVisibility
            {
                return Get_HasSelection() ? EVisibility::Visible : EVisibility::Collapsed;
            })

            // ---- Identity ----

            + SVerticalBox::Slot().AutoHeight()
            [ MakeSection(TEXT("Identity")) ]

            + SVerticalBox::Slot().AutoHeight()
            [ MakeEntityRow() ]

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceM, CkStyle::SpaceS)
            [ MakeIdentitySummary() ]

            // ---- Simulation ----

            + SVerticalBox::Slot().AutoHeight()
            [ MakeSection(TEXT("Simulation")) ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Motion Type"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();
                    return Selection.IsSet() && Selection->HasSimulationState
                        ? Get_MotionText(Selection->MotionType)
                        : Get_UnsetText();
                }), CkStyle::Value_Enum())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Sleep State"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();
                    return Selection.IsSet() && Selection->HasSimulationState
                        ? Get_SleepText(Selection->SleepState)
                        : Get_UnsetText();
                }), CkStyle::Value_Enum())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Mass"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return Get_MassText(InSample.Get_Mass()); }, CkStyle::Value_Numeric())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Motion Quality"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return Get_MotionQualityText(InSample.Get_MotionQuality()); }, CkStyle::Value_Enum())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                // UNSET for a static body, and only for one: Jolt dereferences MotionProperties without a
                // guard, so the facility never asks a static body whether it may sleep.
                MakeSampleRow(TEXT("Allows Sleeping"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                {
                    const auto& Allows = InSample.Get_AllowsSleeping();
                    return Allows.IsSet()
                        ? FText::FromString(*Allows ? TEXT("Yes") : TEXT("No"))
                        : Get_UnsetText();
                })
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Gravity Factor"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return Get_ScalarText(InSample.Get_GravityFactor()); }, CkStyle::Value_Numeric())
            ]

            // ---- Motion ----

            + SVerticalBox::Slot().AutoHeight()
            [ MakeSection(TEXT("Motion")) ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Linear Velocity"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();

                    if (NOT Selection.IsSet() || NOT Selection->HasLinearVelocity)
                    { return Get_UnsetText(); }

                    const auto& Velocity = Selection->LinearVelocity;
                    return FText::FromString(ck::Format_UE(TEXT("{:.1f}, {:.1f}, {:.1f}  ({:.1f})"),
                        Velocity.X, Velocity.Y, Velocity.Z, Velocity.Size()));
                }), CkStyle::Value_Math())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Angular Velocity"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return Get_VectorText(InSample.Get_AngularVelocity()); }, CkStyle::Value_Math())
            ]

            // ---- Collision ----

            + SVerticalBox::Slot().AutoHeight()
            [ MakeSection(TEXT("Collision")) ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Friction"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return Get_ScalarText(InSample.Get_Friction()); }, CkStyle::Value_Numeric())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Restitution"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return Get_ScalarText(InSample.Get_Restitution()); }, CkStyle::Value_Numeric())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Object Layer"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return FText::AsNumber(static_cast<int32>(InSample.Get_ObjectLayer())); }, CkStyle::Value_Numeric())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Broad Phase Layer"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return FText::AsNumber(static_cast<int32>(InSample.Get_BroadPhaseLayer())); }, CkStyle::Value_Numeric())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleStatusRow(TEXT("Sensor"),
                    [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                    { return FText::FromString(InSample.Get_IsSensor() ? TEXT("Sensor") : TEXT("Solid")); },
                    [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                    { return InSample.Get_IsSensor() ? ECk_Tone::Info : ECk_Tone::Neutral; })
            ]

            // ---- Shape ----

            + SVerticalBox::Slot().AutoHeight()
            [ MakeSection(TEXT("Shape")) ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Shape Type"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                {
                    return InSample.Get_ShapeType().IsEmpty()
                        ? Get_UnsetText()
                        : FText::FromString(InSample.Get_ShapeType());
                }, CkStyle::Value_Enum())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Shape Sub Type"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                {
                    return InSample.Get_ShapeSubType().IsEmpty()
                        ? Get_UnsetText()
                        : FText::FromString(InSample.Get_ShapeSubType());
                }, CkStyle::Value_Enum())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("Shape Scale"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return Get_VectorText(InSample.Get_ShapeScale()); }, CkStyle::Value_Math())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("World Bounds"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return Get_BoundsText(InSample.Get_WorldBounds()); }, CkStyle::Value_Math())
            ]

            // ---- Provenance ----

            + SVerticalBox::Slot().AutoHeight()
            [ MakeSection(TEXT("Provenance")) ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeSampleRow(TEXT("User Data"), [](const FCk_Jolt_DebugDraw_BodySample& InSample)
                { return FText::FromString(ck::Format_UE(TEXT("{}"), InSample.Get_UserData())); }, CkStyle::Value_String())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Source Actor"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();
                    return Selection.IsSet() && NOT Selection->SourceActorName.IsEmpty()
                        ? FText::FromString(Selection->SourceActorName)
                        : Get_UnsetText();
                }), CkStyle::Value_String())
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Baked Bodies"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();
                    return Selection.IsSet() && Selection->Population == ECkJoltDebugger_Population::BakedStatic
                        ? FText::AsNumber(Selection->NumBodies)
                        : Get_UnsetText();
                }), CkStyle::Value_Numeric())
            ]

            // ---- Character (collapsed for every rigid body) ----

            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SVerticalBox)
                .Visibility_Lambda([WeakPanel]() -> EVisibility
                {
                    const auto Panel = WeakPanel.Pin();
                    return Panel.IsValid() && Panel->Get_IsCharacterGroupVisible()
                        ? EVisibility::Visible
                        : EVisibility::Collapsed;
                })

                + SVerticalBox::Slot().AutoHeight()
                [ MakeSection(TEXT("Character")) ]

                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeCharacterStatusRow(TEXT("Ground State"), [](const FCk_Jolt_DebugDraw_CharacterSample& InSample)
                    { return Get_GroundStateText(InSample.Get_GroundState()); }, [](const FCk_Jolt_DebugDraw_CharacterSample& InSample)
                    {
                        switch (InSample.Get_GroundState())
                        {
                            case ECk_JoltCharacter_GroundState::OnGround:     return ECk_Tone::Ok;
                            case ECk_JoltCharacter_GroundState::OnSteepSlope:
                            case ECk_JoltCharacter_GroundState::NotSupported: return ECk_Tone::Warn;
                            default:                                          return ECk_Tone::Info;
                        }
                    })
                ]

                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeCharacterRow(TEXT("Character Velocity"), [](const FCk_Jolt_DebugDraw_CharacterSample& InSample)
                    { return Get_VectorText(InSample.Get_Velocity()); }, CkStyle::Value_Math())
                ]

                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeCharacterRow(TEXT("Ground Normal"), [](const FCk_Jolt_DebugDraw_CharacterSample& InSample)
                    { return Get_VectorText(InSample.Get_GroundNormal()); }, CkStyle::Value_Math())
                ]

                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeCharacterRow(TEXT("Ground Velocity"), [](const FCk_Jolt_DebugDraw_CharacterSample& InSample)
                    { return Get_VectorText(InSample.Get_GroundVelocity()); }, CkStyle::Value_Math())
                ]

                + SVerticalBox::Slot().AutoHeight()
                [
                    MakeCharacterRow(TEXT("Up Vector"), [](const FCk_Jolt_DebugDraw_CharacterSample& InSample)
                    { return Get_VectorText(InSample.Get_Up()); }, CkStyle::Value_Math())
                ]

                + SVerticalBox::Slot().AutoHeight()
                [
                    // Unset while the character is unsupported — 0 is a VALID body key and would name the
                    // first body ever created, so the facility hands back a TOptional rather than a sentinel.
                    MakeCharacterRow(TEXT("Ground Body"), [](const FCk_Jolt_DebugDraw_CharacterSample& InSample)
                    {
                        const auto& GroundKey = InSample.Get_GroundBodyKey();
                        return GroundKey.IsSet()
                            ? FText::FromString(ck::Format_UE(TEXT("{}"), *GroundKey))
                            : Get_UnsetText();
                    }, CkStyle::Value_Numeric())
                ]
            ]

            // ---- Contacts ----

            + SVerticalBox::Slot().AutoHeight()
            [ MakeSection(TEXT("Contacts")) ]

            + SVerticalBox::Slot().AutoHeight()
            [ MakeContactsList() ]
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_DetailPanel::
    Get_Selection() const
    -> const TOptional<FCkJoltDebugger_BodySnapshot>&
{
    return _GetSelection.IsBound()
        ? _GetSelection.Execute()
        : ck_jolt_debugger_detail_panel::Get_NoSelection();
}

auto
    SCkJoltDebugger_DetailPanel::
    Get_SelectionFacts() const
    -> const FCkJoltDebugger_SelectionFacts&
{
    return _GetSelectionFacts.IsBound()
        ? _GetSelectionFacts.Execute()
        : ck_jolt_debugger_detail_panel::Get_NoFacts();
}

auto
    SCkJoltDebugger_DetailPanel::
    Get_IsCharacterGroupVisible() const
    -> bool
{
    // Keyed on the SELECTION's population, not on whether a character sample happens to be present: the
    // sample arrives one capture late, and a group that appeared a frame after the row was clicked would
    // shift every row below it exactly once per selection.
    const auto& Selection = Get_Selection();
    return Selection.IsSet() && Selection->Population == ECkJoltDebugger_Population::Character;
}

auto
    SCkJoltDebugger_DetailPanel::
    Get_PopulationStatusText() const
    -> FText
{
    const auto& Selection = Get_Selection();
    return Selection.IsSet()
        ? ck_jolt_debugger_detail_panel::Get_PopulationText(Selection->Population)
        : ck_jolt_debugger_detail_panel::Get_UnsetText();
}

auto
    SCkJoltDebugger_DetailPanel::
    Get_PopulationStatusTone() const
    -> ECk_Tone
{
    const auto& Selection = Get_Selection();

    if (NOT Selection.IsSet())
    { return ECk_Tone::Neutral; }

    switch (Selection->Population)
    {
        case ECkJoltDebugger_Population::JoltBody:    return ECk_Tone::Accent;
        case ECkJoltDebugger_Population::Sensor:      return ECk_Tone::Info;
        case ECkJoltDebugger_Population::Character:   return ECk_Tone::Ok;
        default:                                      return ECk_Tone::Neutral;
    }
}

auto
    SCkJoltDebugger_DetailPanel::
    Get_SimulationStatusText() const
    -> FText
{
    const auto& Selection = Get_Selection();

    if (NOT Selection.IsSet() || NOT Selection->HasSimulationState)
    { return FText::FromString(TEXT("No Simulation State")); }

    return ck_jolt_debugger_detail_panel::Get_SleepText(Selection->SleepState);
}

auto
    SCkJoltDebugger_DetailPanel::
    Get_SimulationStatusTone() const
    -> ECk_Tone
{
    const auto& Selection = Get_Selection();
    return Selection.IsSet() && Selection->HasSimulationState && Selection->SleepState == ECk_Jolt_SleepState::Awake
        ? ECk_Tone::Ok
        : ECk_Tone::Neutral;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_DetailPanel::
    MakeRow(
        const FString&    InKey,
        TAttribute<FText> InValue,
        TAttribute<FLinearColor> InSetValueColor)
    -> TSharedRef<SWidget>
{
    _RowValues.Add(InKey, InValue);

    const auto SetValueColor = InSetValueColor.IsSet() || InSetValueColor.IsBound()
        ? InSetValueColor
        : TAttribute<FLinearColor>{CkStyle::Text()};
    const auto ValueColor = TAttribute<FLinearColor>::CreateLambda([InValue, SetValueColor]() -> FLinearColor
    {
        return InValue.Get().EqualTo(ck_jolt_debugger_detail_panel::Get_UnsetText())
            ? CkStyle::TextMute()
            : SetValueColor.Get();
    });
    _RowValueColors.Add(InKey, ValueColor);

    return SNew(SCkDebug_KeyValueRow)
        .KeyText(FText::FromString(InKey))
        .ValueText(MoveTemp(InValue))
        .Tone(ECkDebug_KeyValueTone::Custom)
        .CustomValueColor(ValueColor);
}

auto
    SCkJoltDebugger_DetailPanel::
    MakeSampleRow(
        const FString& InKey,
        TFunction<FText(const FCk_Jolt_DebugDraw_BodySample&)> InRead,
        TAttribute<FLinearColor> InSetValueColor)
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkJoltDebugger_DetailPanel>{SharedThis(this)};

    return MakeRow(InKey, TAttribute<FText>::CreateLambda([WeakPanel, Read = MoveTemp(InRead)]() -> FText
    {
        const auto Panel = WeakPanel.Pin();

        if (NOT Panel.IsValid())
        { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

        const auto& Sample = Panel->Get_SelectionFacts().BodySample;

        return Sample.IsSet() ? Read(*Sample) : ck_jolt_debugger_detail_panel::Get_UnsetText();
    }), MoveTemp(InSetValueColor));
}

auto
    SCkJoltDebugger_DetailPanel::
    MakeSampleStatusRow(
        const FString& InKey,
        TFunction<FText(const FCk_Jolt_DebugDraw_BodySample&)> InText,
        TFunction<ECk_Tone(const FCk_Jolt_DebugDraw_BodySample&)> InTone)
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkJoltDebugger_DetailPanel>{SharedThis(this)};
    const auto Text = TAttribute<FText>::CreateLambda([WeakPanel, Read = MoveTemp(InText)]() -> FText
    {
        const auto Panel = WeakPanel.Pin();

        if (NOT Panel.IsValid())
        { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

        const auto& Sample = Panel->Get_SelectionFacts().BodySample;
        return Sample.IsSet() ? Read(*Sample) : ck_jolt_debugger_detail_panel::Get_UnsetText();
    });

    _RowValues.Add(InKey, Text);
    const auto Tone = TAttribute<ECk_Tone>::CreateLambda([WeakPanel, Read = MoveTemp(InTone)]() -> ECk_Tone
    {
        const auto Panel = WeakPanel.Pin();

        if (NOT Panel.IsValid())
        { return ECk_Tone::Neutral; }

        const auto& Sample = Panel->Get_SelectionFacts().BodySample;
        return Sample.IsSet() ? Read(*Sample) : ECk_Tone::Neutral;
    });
    _RowStatusTones.Add(InKey, Tone);

    return SNew(SCkDebug_KeyValueRow)
        .KeyText(FText::FromString(InKey))
        .Tone(ECkDebug_KeyValueTone::Custom)
        .ValueWidget()
        [
            SNew(SCkDebug_StatusPill)
            .ShowDot(true)
            .Text(Text)
            .Tone(Tone)
        ];
}

auto
    SCkJoltDebugger_DetailPanel::
    MakeCharacterRow(
        const FString& InKey,
        TFunction<FText(const FCk_Jolt_DebugDraw_CharacterSample&)> InRead,
        TAttribute<FLinearColor> InSetValueColor)
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkJoltDebugger_DetailPanel>{SharedThis(this)};

    return MakeRow(InKey, TAttribute<FText>::CreateLambda([WeakPanel, Read = MoveTemp(InRead)]() -> FText
    {
        const auto Panel = WeakPanel.Pin();

        if (NOT Panel.IsValid())
        { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

        const auto& Sample = Panel->Get_SelectionFacts().CharacterSample;

        return Sample.IsSet() ? Read(*Sample) : ck_jolt_debugger_detail_panel::Get_UnsetText();
    }), MoveTemp(InSetValueColor));
}

auto
    SCkJoltDebugger_DetailPanel::
    MakeCharacterStatusRow(
        const FString& InKey,
        TFunction<FText(const FCk_Jolt_DebugDraw_CharacterSample&)> InText,
        TFunction<ECk_Tone(const FCk_Jolt_DebugDraw_CharacterSample&)> InTone)
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkJoltDebugger_DetailPanel>{SharedThis(this)};
    const auto Text = TAttribute<FText>::CreateLambda([WeakPanel, Read = MoveTemp(InText)]() -> FText
    {
        const auto Panel = WeakPanel.Pin();
        if (NOT Panel.IsValid()) { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

        const auto& Sample = Panel->Get_SelectionFacts().CharacterSample;
        return Sample.IsSet() ? Read(*Sample) : ck_jolt_debugger_detail_panel::Get_UnsetText();
    });
    const auto Tone = TAttribute<ECk_Tone>::CreateLambda([WeakPanel, Read = MoveTemp(InTone)]() -> ECk_Tone
    {
        const auto Panel = WeakPanel.Pin();
        if (NOT Panel.IsValid()) { return ECk_Tone::Neutral; }

        const auto& Sample = Panel->Get_SelectionFacts().CharacterSample;
        return Sample.IsSet() ? Read(*Sample) : ECk_Tone::Neutral;
    });

    _RowValues.Add(InKey, Text);
    _RowStatusTones.Add(InKey, Tone);

    return SNew(SCkDebug_KeyValueRow)
        .KeyText(FText::FromString(InKey))
        .Tone(ECkDebug_KeyValueTone::Custom)
        .ValueWidget()
        [
            SNew(SCkDebug_StatusPill)
            .ShowDot(true)
            .Text(Text)
            .Tone(Tone)
        ];
}

auto
    SCkJoltDebugger_DetailPanel::
    MakeSection(
        const FString& InLabel)
    -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
        [
            SNew(SCkDebug_SectionHeader).Label(FText::FromString(InLabel))
        ];
}

auto
    SCkJoltDebugger_DetailPanel::
    Get_RowValueText(
        const FString& InKey) const
    -> FText
{
    const auto* Value = _RowValues.Find(InKey);
    return Value != nullptr ? Value->Get() : FText::GetEmpty();
}

auto
    SCkJoltDebugger_DetailPanel::
    Get_RowValueColor(
        const FString& InKey) const
    -> FLinearColor
{
    const auto* Color = _RowValueColors.Find(InKey);
    return Color != nullptr ? Color->Get() : CkStyle::TextMute();
}

auto
    SCkJoltDebugger_DetailPanel::
    Get_RowStatusTone(
        const FString& InKey) const
    -> ECk_Tone
{
    const auto* Tone = _RowStatusTones.Find(InKey);
    return Tone != nullptr ? Tone->Get() : ECk_Tone::Neutral;
}

auto
    SCkJoltDebugger_DetailPanel::
    MakeEntityRow()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkJoltDebugger_DetailPanel>{SharedThis(this)};

    return SNew(SCkDebug_KeyValueRow)
        .KeyText(FText::FromString(TEXT("Entity")))
        .Tone(ECkDebug_KeyValueTone::Custom)
        .ValueWidget()
        [
            SNew(SCkDebug_EntityRef)
            .ShowName(true)
            .Entity_Lambda([WeakPanel]() -> FCk_Handle
            {
                const auto Panel = WeakPanel.Pin();

                if (NOT Panel.IsValid())
                { return FCk_Handle{}; }

                const auto& Selection = Panel->Get_Selection();
                return Selection.IsSet() ? Selection->Handle : FCk_Handle{};
            })
        ];
}

auto
    SCkJoltDebugger_DetailPanel::
    MakeIdentitySummary()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkJoltDebugger_DetailPanel>{SharedThis(this)};
    const auto PopulationText = TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
    {
        const auto Panel = WeakPanel.Pin();
        return Panel.IsValid() ? Panel->Get_PopulationStatusText() : ck_jolt_debugger_detail_panel::Get_UnsetText();
    });
    const auto BodyKeyText = TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
    {
        const auto Panel = WeakPanel.Pin();

        if (NOT Panel.IsValid())
        { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

        const auto& Selection = Panel->Get_Selection();
        return Selection.IsSet() && Selection->BodyKey.IsSet()
            ? FText::FromString(ck::Format_UE(TEXT("{}"), *Selection->BodyKey))
            : ck_jolt_debugger_detail_panel::Get_UnsetText();
    });
    const auto BodyKeyColor = TAttribute<FSlateColor>::CreateLambda([BodyKeyText]() -> FSlateColor
    {
        return FSlateColor(BodyKeyText.Get().EqualTo(ck_jolt_debugger_detail_panel::Get_UnsetText())
            ? CkStyle::TextMute()
            : CkStyle::Value_Numeric());
    });

    // These values deliberately remain inspectable through Get_RowValueText: the compact summary is the
    // rendered representation of the same attributes, rather than a second derived copy of identity state.
    _RowValues.Add(TEXT("Population"), PopulationText);
    _RowValues.Add(TEXT("Body Key"), BodyKeyText);

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCkDebug_StatusPill)
            .ShowDot(true)
            .Text(PopulationText)
            .Tone_Lambda([WeakPanel]() -> ECk_Tone
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() ? Panel->Get_PopulationStatusTone() : ECk_Tone::Neutral;
            })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCkDebug_StatusPill)
            .ShowDot(true)
            .Text_Lambda([WeakPanel]() -> FText
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() ? Panel->Get_SimulationStatusText() : ck_jolt_debugger_detail_panel::Get_UnsetText();
            })
            .Tone_Lambda([WeakPanel]() -> ECk_Tone
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() ? Panel->Get_SimulationStatusTone() : ECk_Tone::Neutral;
            })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SCkDebug_StatPair)
            .Value(BodyKeyText)
            .Label(FText::FromString(TEXT("BODY KEY")))
            .ValueColor(BodyKeyColor)
            .Layout(ECkDebug_StatPairLayout::Inline_ValueFirst)
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_DetailPanel::
    MakeContactsList()
    -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkJoltDebugger_DetailPanel>{SharedThis(this)};

    return SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f)
        [
            SNew(STextBlock)
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            .Text(FText::FromString(TEXT("Not touching anything.")))
            .Visibility_Lambda([WeakPanel]() -> EVisibility
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() && Panel->Get_NumContactRows() > 0
                    ? EVisibility::Collapsed
                    : EVisibility::Visible;
            })
        ]

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SBox)
            .MaxDesiredHeight(140.0f)
            [
                SAssignNew(_ContactList, SListView<ContactItemPtr>)
                .ListItemsSource(&_ContactItems)
                .OnGenerateRow(this, &SCkJoltDebugger_DetailPanel::OnGenerateContactRow)
                .OnSelectionChanged(this, &SCkJoltDebugger_DetailPanel::OnContactSelectionChanged)
                .SelectionMode(ESelectionMode::Single)
            ]
        ];
}

/*
 * STextBlock only. An STableRow detects selection from an OnMouseButtonDown that bubbles up out of its
 * content, so any child that returns Handled on a left click traps it and the row renders but never
 * selects (CkDebuggerCommon/CLAUDE.md §"Don't put click-consuming widgets inside STableRow"). Clicking a
 * contact row is the whole point of this list — it selects the other body — so nothing here may consume.
 */
auto
    SCkJoltDebugger_DetailPanel::
    OnGenerateContactRow(
        ContactItemPtr InItem,
        const TSharedRef<STableViewBase>& InTable)
    -> TSharedRef<ITableRow>
{
    const auto WeakItem = TWeakPtr<FCkJoltDebugger_ContactRow>{InItem};

    return SNew(STableRow<ContactItemPtr>, InTable)
        .Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger_detail_panel::Get_ContactRowPadding))
        .ShowSelection(true)
        [
            SNew(STextBlock)
            .Font_Static(&ck_jolt_debugger_detail_panel::Font_ContactRow)
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            .Text_Lambda([WeakItem]() -> FText
            {
                const auto Pinned = WeakItem.Pin();
                return Pinned.IsValid()
                    ? FText::FromString(ck_jolt_debugger_detail_panel::Get_ContactRowText(*Pinned))
                    : FText::GetEmpty();
            })
        ];
}

auto
    SCkJoltDebugger_DetailPanel::
    OnContactSelectionChanged(
        ContactItemPtr InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    // Direct is the programmatic apply — the reconcile re-stamps nothing today, but a Direct echo would
    // re-select the other body every time the contacts list refreshed under the user.
    if (InSelectInfo == ESelectInfo::Direct || NOT InItem.IsValid())
    { return; }

    _OnContactSelected.ExecuteIfBound(InItem->OtherBodyKey);
}

auto
    SCkJoltDebugger_DetailPanel::
    Refresh_Contacts()
    -> void
{
    const auto& Contacts = Get_SelectionFacts().Contacts;

    // Pointer identity by body key, the same contract the outliner keeps: SListView keys selection by
    // pointer, and a contacts array rebuilt every capture would destroy the click that is landing on it.
    auto Existing = TMap<uint64, ContactItemPtr>{};
    Existing.Reserve(_ContactItems.Num());

    for (const auto& Item : _ContactItems)
    {
        if (Item.IsValid())
        { Existing.Add(Item->OtherBodyKey, Item); }
    }

    auto NewItems = TArray<ContactItemPtr>{};
    NewItems.Reserve(Contacts.Num());
    auto SetChanged = false;

    for (const auto& Contact : Contacts)
    {
        auto Row = FCkJoltDebugger_ContactRow{};
        Row.OtherBodyKey     = Contact.Get_OtherBodyKey();
        Row.NumContactPoints = Contact.Get_NumContactPoints();
        Row.PenetrationDepth = Contact.Get_PenetrationDepth();

        auto Item = ContactItemPtr{};

        if (auto* Found = Existing.Find(Row.OtherBodyKey))
        {
            Item = *Found;
            *Item = Row;
            Existing.Remove(Row.OtherBodyKey);
        }
        else
        {
            Item = MakeShared<FCkJoltDebugger_ContactRow>(Row);
            SetChanged = true;
        }

        NewItems.Emplace(MoveTemp(Item));
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    _ContactItems = MoveTemp(NewItems);

    if (SetChanged && _ContactList.IsValid())
    { _ContactList->RequestListRefresh(); }
}

auto
    SCkJoltDebugger_DetailPanel::
    Get_NumContactRows() const
    -> int32
{
    return _ContactItems.Num();
}

// --------------------------------------------------------------------------------------------------------------------
