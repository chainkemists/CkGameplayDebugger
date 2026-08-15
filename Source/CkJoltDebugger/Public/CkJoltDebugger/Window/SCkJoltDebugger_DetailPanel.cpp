#include "CkJoltDebugger/Window/SCkJoltDebugger_DetailPanel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

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

    auto Get_PopulationText(
        ECkJoltDebugger_Population InPopulation) -> FText
    {
        switch (InPopulation)
        {
            case ECkJoltDebugger_Population::JoltBody:    return FText::FromString(TEXT("Jolt Body"));
            case ECkJoltDebugger_Population::BakedStatic: return FText::FromString(TEXT("Baked Static World"));
            case ECkJoltDebugger_Population::Sensor:      return FText::FromString(TEXT("Sensor (Probe)"));
            case ECkJoltDebugger_Population::Character:   return FText::FromString(TEXT("Character"));
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

    auto Get_SleepText(
        ECk_Jolt_SleepState InSleepState) -> FText
    {
        return FText::FromString(InSleepState == ECk_Jolt_SleepState::Asleep ? TEXT("Asleep") : TEXT("Awake"));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_DetailPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _GetSelection = InArgs._GetSelection;

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

            + SVerticalBox::Slot().AutoHeight()
            [ MakeEntityRow() ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Population"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();
                    return Selection.IsSet()
                        ? ck_jolt_debugger_detail_panel::Get_PopulationText(Selection->Population)
                        : ck_jolt_debugger_detail_panel::Get_UnsetText();
                }))
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Motion Type"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();
                    return Selection.IsSet() && Selection->HasSimulationState
                        ? ck_jolt_debugger_detail_panel::Get_MotionText(Selection->MotionType)
                        : ck_jolt_debugger_detail_panel::Get_UnsetText();
                }))
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Sleep State"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();
                    return Selection.IsSet() && Selection->HasSimulationState
                        ? ck_jolt_debugger_detail_panel::Get_SleepText(Selection->SleepState)
                        : ck_jolt_debugger_detail_panel::Get_UnsetText();
                }))
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Body Key"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();
                    return Selection.IsSet() && Selection->BodyKey.IsSet()
                        ? FText::FromString(ck::Format_UE(TEXT("{}"), *Selection->BodyKey))
                        : ck_jolt_debugger_detail_panel::Get_UnsetText();
                }))
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Linear Velocity"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();

                    if (NOT Selection.IsSet() || NOT Selection->HasLinearVelocity)
                    { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

                    const auto& Velocity = Selection->LinearVelocity;
                    return FText::FromString(ck::Format_UE(TEXT("{:.1f}, {:.1f}, {:.1f}  ({:.1f})"),
                        Velocity.X, Velocity.Y, Velocity.Z, Velocity.Size()));
                }))
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Source Actor"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();
                    return Selection.IsSet() && NOT Selection->SourceActorName.IsEmpty()
                        ? FText::FromString(Selection->SourceActorName)
                        : ck_jolt_debugger_detail_panel::Get_UnsetText();
                }))
            ]

            + SVerticalBox::Slot().AutoHeight()
            [
                MakeRow(TEXT("Baked Bodies"), TAttribute<FText>::CreateLambda([WeakPanel]() -> FText
                {
                    const auto Panel = WeakPanel.Pin();

                    if (NOT Panel.IsValid())
                    { return ck_jolt_debugger_detail_panel::Get_UnsetText(); }

                    const auto& Selection = Panel->Get_Selection();
                    return Selection.IsSet() && Selection->Population == ECkJoltDebugger_Population::BakedStatic
                        ? FText::AsNumber(Selection->NumBodies)
                        : ck_jolt_debugger_detail_panel::Get_UnsetText();
                }))
            ]
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

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_DetailPanel::
    MakeRow(
        const FString&    InKey,
        TAttribute<FText> InValue)
    -> TSharedRef<SWidget>
{
    _RowValues.Add(InKey, InValue);

    return SNew(SCkDebug_KeyValueRow)
        .KeyText(FText::FromString(InKey))
        .ValueText(MoveTemp(InValue))
        .Tone(ECkDebug_KeyValueTone::Custom)
        .CustomValueColor(CkStyle::Text());
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

// --------------------------------------------------------------------------------------------------------------------
