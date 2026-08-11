#include "CkStyleLabDebugger/Widgets/SCkStyleLab_ControlsPane.h"

#include "CkStyleLabDebugger/Styles/CkStyleLab_AxisMetadata.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "UObject/UnrealType.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Reflection helpers (module-unique namespace name — unity builds concatenate TUs).
// --------------------------------------------------------------------------------------------------------------------

namespace ck_style_lab_controls
{
    constexpr auto ValueLabelWidth = 128.0f;
    constexpr auto AxisNameWidth   = 116.0f;

    auto Get_AxisEnum(const FProperty* InProperty) -> const UEnum*
    {
        if (const auto* EnumProperty = CastField<FEnumProperty>(InProperty))
        { return EnumProperty->GetEnum(); }

        if (const auto* ByteProperty = CastField<FByteProperty>(InProperty))
        { return ByteProperty->GetIntPropertyEnum(); }

        return nullptr;
    }

    // UHT appends a hidden _MAX entry to every UENUM; cycling through it would show a bogus option.
    auto Get_AxisOptions(const UEnum* InEnum) -> TArray<int64>
    {
        auto Options = TArray<int64>{};

        const auto EntryCount = InEnum->NumEnums();
        for (auto Index = 0; Index < EntryCount; ++Index)
        {
            if (InEnum->GetNameStringByIndex(Index).EndsWith(TEXT("_MAX")))
            { continue; }

            Options.Add(InEnum->GetValueByIndex(Index));
        }

        return Options;
    }

    auto Get_AxisValue(const FProperty* InProperty, const void* InContainer) -> int64
    {
        const auto* ValuePtr = InProperty->ContainerPtrToValuePtr<void>(InContainer);

        if (const auto* EnumProperty = CastField<FEnumProperty>(InProperty))
        { return EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr); }

        if (const auto* ByteProperty = CastField<FByteProperty>(InProperty))
        { return ByteProperty->GetSignedIntPropertyValue(ValuePtr); }

        return 0;
    }

    auto Set_AxisValue(const FProperty* InProperty, void* InContainer, int64 InValue) -> void
    {
        auto* ValuePtr = InProperty->ContainerPtrToValuePtr<void>(InContainer);

        if (const auto* EnumProperty = CastField<FEnumProperty>(InProperty))
        {
            EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, InValue);
            return;
        }

        if (const auto* ByteProperty = CastField<FByteProperty>(InProperty))
        { ByteProperty->SetIntPropertyValue(ValuePtr, InValue); }
    }
}

// ====================================================================================================================

auto
    SCkStyleLab_ControlsPane::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _OnSelectionChanged = InArgs._OnSelectionChanged;

    for (auto PropertyIt = TFieldIterator<FProperty>{FCkDebuggerStyleSelection::StaticStruct()};
         PropertyIt;
         ++PropertyIt)
    {
        const auto* AxisEnum = ck_style_lab_controls::Get_AxisEnum(*PropertyIt);

        if (AxisEnum == nullptr)
        { continue; }

        const auto* Metadata = ck::style_lab::Find_AxisMetadata((*PropertyIt)->GetFName());
        checkf(Metadata != nullptr,
               TEXT("Every FCkDebuggerStyleSelection axis requires CkStyleLab runtime metadata: %s"),
               *(*PropertyIt)->GetName());
        if (Metadata == nullptr)
        { continue; }

        auto Axis         = MakeShared<FCkStyleLab_AxisRow>();
        Axis->Property    = *PropertyIt;
        Axis->DisplayName = Metadata->DisplayName;
        Axis->ToolTip     = Metadata->ToolTip;
        Axis->Options     = ck_style_lab_controls::Get_AxisOptions(AxisEnum);

        if (Axis->Options.IsEmpty())
        { continue; }

        for (const auto Option : Axis->Options)
        {
            const auto* OptionLabel =
                ck::style_lab::Find_AxisOptionLabel(Axis->Property->GetFName(), Option);
            checkf(OptionLabel != nullptr,
                   TEXT("Every Style Lab axis option requires runtime metadata: %s=%lld"),
                   *Axis->Property->GetName(),
                   Option);
            if (OptionLabel == nullptr)
            {
                // Do not expose a partly-labelled axis in a packaged build.
                Axis->Options.Reset();
                Axis->OptionLabels.Reset();
                break;
            }
            Axis->OptionLabels.Add(*OptionLabel);
        }
        if (Axis->Options.IsEmpty())
        { continue; }

        _Axes.Add(MoveTemp(Axis));
    }

    const auto& Profiles = ck::debug_axes::Get_StyleProfiles();
    for (auto Index = 0; Index < Profiles.Num(); ++Index)
    { _ProfileItems.Add(MakeShared<int32>(Index)); }

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush_Large())
            .BorderBackgroundColor(FSlateColor{CkStyle::Bg1()})
            .Padding(FMargin{CkStyle::SpaceM})
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()
                    [
                        Build_ProfileControls()
                    ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM, 0.0f, CkStyle::SpaceS)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("AXES")))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
                            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                    ]

                + SVerticalBox::Slot().AutoHeight()
                    [
                        Build_AxisRows()
                    ]
            ]
    ];
}

// ====================================================================================================================

auto
    SCkStyleLab_ControlsPane::
    Build_AxisRows()
    -> TSharedRef<SWidget>
{
    auto Rows = SNew(SVerticalBox);

    for (const auto& Axis : _Axes)
    {
        Rows->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
            [
                Build_AxisRow(Axis)
            ];
    }

    return Rows;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    Build_AxisRow(
        const TSharedPtr<FCkStyleLab_AxisRow>& InAxis)
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        .ToolTipText(InAxis->ToolTip)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox)
                    .WidthOverride(ck_style_lab_controls::AxisNameWidth)
                    [
                        SNew(STextBlock)
                            .Text(InAxis->DisplayName)
                            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                    ]
            ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("\x25C0")))   // U+25C0
                    .ToolTipText(FText::FromString(TEXT("Previous option")))
                    .OnClicked(FOnClicked::CreateSP(
                        this, &SCkStyleLab_ControlsPane::OnCycleAxis, InAxis, -1))
            ]

        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                SNew(SBox)
                    .MinDesiredWidth(ck_style_lab_controls::ValueLabelWidth)
                    [
                        SNew(STextBlock)
                            .Text(TAttribute<FText>::CreateSP(
                                this, &SCkStyleLab_ControlsPane::Get_AxisValueLabel, InAxis))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                            .Justification(ETextJustify::Center)
                            .ColorAndOpacity(FSlateColor{CkStyle::Text()})
                    ]
            ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("\x25B6")))   // U+25B6
                    .ToolTipText(FText::FromString(TEXT("Next option")))
                    .OnClicked(FOnClicked::CreateSP(
                        this, &SCkStyleLab_ControlsPane::OnCycleAxis, InAxis, 1))
            ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    Build_ProfileControls()
    -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("PROFILE")))
                    .Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            ]

        + SVerticalBox::Slot().AutoHeight()
            [
                SAssignNew(_ProfileCombo, SComboBox<TSharedPtr<int32>>)
                    .OptionsSource(&_ProfileItems)
                    .OnGenerateWidget_Lambda([](TSharedPtr<int32> InItem) -> TSharedRef<SWidget>
                    {
                        const auto& Profiles = ck::debug_axes::Get_StyleProfiles();

                        if (NOT InItem.IsValid() || NOT Profiles.IsValidIndex(*InItem))
                        { return SNullWidget::NullWidget; }

                        const auto& Profile = Profiles[*InItem];

                        // The active profile carries a marker so the open list shows which point in
                        // axis space is currently applied.
                        const auto* Settings = UCkDebuggerStyleSettings::Get();
                        const auto IsActive = Settings != nullptr && Settings->ActiveProfileName == Profile.Name;
                        const auto Label = IsActive
                            ? ck::Format_UE(TEXT("* {}"), Profile.Name)
                            : Profile.Name;

                        return SNew(STextBlock)
                            .Text(FText::FromString(Label))
                            .ToolTipText(FText::FromString(Profile.Blurb))
                            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                            .ColorAndOpacity(FSlateColor{IsActive ? CkStyle::Accent() : CkStyle::Text()});
                    })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<int32> InItem, ESelectInfo::Type)
                    {
                        if (NOT InItem.IsValid())
                        { return; }

                        Apply_Profile(*InItem);
                    })
                    [
                        SNew(STextBlock)
                            .Text_Lambda([]() -> FText
                            {
                                const auto* Settings = UCkDebuggerStyleSettings::Get();
                                return FText::FromString(Settings != nullptr
                                    ? Settings->ActiveProfileName
                                    : FString{});
                            })
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                            .ColorAndOpacity(FSlateColor{CkStyle::Accent()})
                    ]
            ]

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("Reset to Classic")))
                    .ToolTipText(FText::FromString(TEXT("Restore every axis to its default — identical to applying the Classic profile.")))
                    .OnClicked(FOnClicked::CreateSP(this, &SCkStyleLab_ControlsPane::OnResetToClassic))
            ];
}

// ====================================================================================================================

auto
    SCkStyleLab_ControlsPane::
    Get_AxisValueLabel(
        TSharedPtr<FCkStyleLab_AxisRow> InAxis) const
    -> FText
{
    const auto* Settings = UCkDebuggerStyleSettings::Get();

    if (NOT InAxis.IsValid() || Settings == nullptr)
    { return FText::GetEmpty(); }

    const auto Value = ck_style_lab_controls::Get_AxisValue(InAxis->Property, &Settings->Selection);
    const auto Index = InAxis->Options.IndexOfByKey(Value);

    return InAxis->OptionLabels.IsValidIndex(Index)
        ? InAxis->OptionLabels[Index]
        : FText::FromString(TEXT("(unknown)"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    OnCycleAxis(
        TSharedPtr<FCkStyleLab_AxisRow> InAxis,
        int32                           InDirection)
    -> FReply
{
    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();

    if (NOT InAxis.IsValid() || Settings == nullptr)
    { return FReply::Handled(); }

    const auto Current      = ck_style_lab_controls::Get_AxisValue(InAxis->Property, &Settings->Selection);
    const auto CurrentIndex = InAxis->Options.IndexOfByKey(Current);
    const auto OptionCount  = InAxis->Options.Num();

    // An unknown stored value (older config, renamed option) lands on the first option rather
    // than leaving the axis stuck.
    const auto NextIndex = CurrentIndex == INDEX_NONE
        ? 0
        : ((CurrentIndex + InDirection) % OptionCount + OptionCount) % OptionCount;

    ck_style_lab_controls::Set_AxisValue(
        InAxis->Property, &Settings->Selection, InAxis->Options[NextIndex]);

    Settings->ActiveProfileName = TEXT("Custom");
    Settings->SaveConfig();
    Settings->NotifyChanged();

    Notify_SelectionChanged();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    OnResetToClassic()
    -> FReply
{
    constexpr auto ClassicProfileIndex = 0;
    Apply_Profile(ClassicProfileIndex);

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    Apply_Profile(
        int32 InProfileIndex)
    -> void
{
    const auto& Profiles = ck::debug_axes::Get_StyleProfiles();
    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();

    if (NOT Profiles.IsValidIndex(InProfileIndex) || Settings == nullptr)
    { return; }

    const auto& Profile = Profiles[InProfileIndex];

    Settings->Selection         = Profile.Selection;
    Settings->ActiveProfileName = Profile.Name;
    Settings->SaveConfig();
    Settings->NotifyChanged();

    Notify_SelectionChanged();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    Notify_SelectionChanged()
    -> void
{
    if (_OnSelectionChanged.IsBound())
    { _OnSelectionChanged.Execute(); }
}

// ====================================================================================================================
