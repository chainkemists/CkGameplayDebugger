#include "CkStyleLabDebugger/Widgets/SCkStyleLab_ControlsPane.h"

#include "CkStyleLabDebugger/Styles/CkStyleLab_AxisMetadata.h"
#include "CkStyleLabDebugger/Widgets/SCkStyleLab_InputHudControls.h"
#include "CkStyleLabDebugger/Widgets/SCkStyleLab_SamplePane.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Reflection helpers (module-unique namespace name — unity builds concatenate TUs).
// --------------------------------------------------------------------------------------------------------------------

namespace ck_style_lab_controls
{
    constexpr auto ValueLabelWidth = 128.0f;
    constexpr auto AxisNameWidth   = 116.0f;

    auto ProfileUiSchema() -> TArray<FCkUiFieldSchema>
    {
        return {
            {TEXT("name"), ECkUiFieldKind::Text},
            {TEXT("blurb"), ECkUiFieldKind::Text},
            {TEXT("active-color"), ECkUiFieldKind::Color},
        };
    }

    auto Get_AxisEnum(const FProperty* InProperty) -> const UEnum*
    {
        if (const auto* EnumProperty = CastField<FEnumProperty>(InProperty))
        { return EnumProperty->GetEnum(); }

        if (const auto* ByteProperty = CastField<FByteProperty>(InProperty))
        { return ByteProperty->GetIntPropertyEnum(); }

        return nullptr;
    }

    // UHT appends a hidden _MAX entry to every UENUM. The authored runtime metadata filter below owns
    // option exposure, including legacy wire values, because UEnum metadata is stripped from packaged builds.
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
        Axis->Group       = Metadata->Group;
        Axis->Options     = ck_style_lab_controls::Get_AxisOptions(AxisEnum);
        Axis->Options.RemoveAll([PropertyName = Axis->Property->GetFName()](const int64 InOption)
        {
            // Authored runtime metadata is the packaged-safe exposure contract. This also keeps
            // retired wire values hidden when UEnum editor metadata is stripped from a build.
            return ck::style_lab::Find_AxisOptionLabel(PropertyName, InOption) == nullptr;
        });

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

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight()
            [
                Build_ProfileControls()
            ]

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
            [
                Build_GroupedAxes()
            ]
    ];
}

// ====================================================================================================================

auto
    SCkStyleLab_ControlsPane::
    Build_GroupedAxes()
    -> TSharedRef<SWidget>
{
    auto Groups = SNew(SVerticalBox);

    for (const auto& Group : ck::style_lab::Get_GroupMetadata())
    {
        Groups->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
            [
                Group.Group == ECkStyleLab_Group::InputHud
                    ? Build_InputHudGroup(Group)
                    : Build_AxisGroup(Group)
            ];
    }

    return Groups;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    Build_AxisGroup(
        const FCkStyleLab_GroupMetadata& InGroup)
    -> TSharedRef<SWidget>
{
    auto Rows = SNew(SVerticalBox);
    auto AxisCount = 0;

    for (const auto& Axis : _Axes)
    {
        if (NOT Axis.IsValid() || Axis->Group != InGroup.Group)
        { continue; }

        ++AxisCount;
        Rows->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
            [Build_AxisRow(Axis)];
    }

    TSharedPtr<SCkStyleLab_SamplePane> Preview;

    auto Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(STextBlock)
                    .Text(InGroup.Description)
                    .AutoWrapText(true)
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            ]
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceM, CkStyle::SpaceS)
            [Rows]
        + SVerticalBox::Slot().AutoHeight()
            [SNew(SSeparator)]
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM)
            [
                SAssignNew(Preview, SCkStyleLab_SamplePane)
                    .Group(InGroup.Group)
            ];

    _GroupPreviews.Add(Preview);

    return SNew(SCkDebug_InspectorPanel)
        .Title(InGroup.DisplayName)
        .CountText(FText::AsNumber(AxisCount))
        .StartExpanded(InGroup.bStartExpanded)
        .Body()
        [Body];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    Build_InputHudGroup(
        const FCkStyleLab_GroupMetadata& InGroup)
    -> TSharedRef<SWidget>
{
    TSharedPtr<SCkStyleLab_SamplePane> Preview;

    auto Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(STextBlock)
                    .Text(InGroup.Description)
                    .AutoWrapText(true)
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            ]
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(SCkStyleLab_InputHudControls)
                    .OnChanged(FOnCkStyleLab_InputHudChanged::CreateSP(
                        this, &SCkStyleLab_ControlsPane::Notify_SelectionChanged))
            ]
        + SVerticalBox::Slot().AutoHeight()
            [SNew(SSeparator)]
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM)
            [
                SAssignNew(Preview, SCkStyleLab_SamplePane)
                    .Group(ECkStyleLab_Group::InputHud)
            ];

    _GroupPreviews.Add(Preview);

    return SNew(SCkDebug_InspectorPanel)
        .Title(InGroup.DisplayName)
        .StartExpanded(InGroup.bStartExpanded)
        .Body()
        [Body];
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
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const FCkUiLoadResult CollectionResult = FCkUiCollection::TryCreate(
        ck_style_lab_controls::ProfileUiSchema(), _ProfileCollection);
    if (NOT RegistryResult.Succeeded || NOT CollectionResult.Succeeded)
    {
        _ProfilePublicationError = FString::Join(RegistryResult.Errors, TEXT("\n"))
            + FString::Join(CollectionResult.Errors, TEXT("\n"));
        _ProfileCollection.Reset();
        return SNew(STextBlock).Text(Get_ProfileLayoutError());
    }

    const TWeakPtr<SCkStyleLab_ControlsPane> WeakPane{SharedThis(this)};
    auto Tokens = FCkUiView::FTokens{};
    Tokens.Add(TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS));
    Tokens.Add(TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM));
    Tokens.Add(TEXT("--text-dim"), TEXT("#") + CkStyle::TextDim().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--accent"), TEXT("#") + CkStyle::Accent().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--err"), TEXT("#") + CkStyle::Err().ToFColorSRGB().ToHex());
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("title"), FText::FromString(TEXT("Curated profiles")));
    Data.Text.Add(TEXT("purpose"), FText::FromString(TEXT("Profiles apply a complete curated style. Editing any individual control below changes the profile to Custom.")));
    Data.Text.Add(TEXT("current-profile"), TAttribute<FText>::CreateSP(this, &SCkStyleLab_ControlsPane::Get_ProfileLabel));
    Data.Text.Add(TEXT("layout-error"), TAttribute<FText>::CreateSP(this, &SCkStyleLab_ControlsPane::Get_ProfileLayoutError));
    Data.Color.Add(TEXT("current-profile-color"), TAttribute<FLinearColor>::CreateLambda([WeakPane]
    {
        return WeakPane.IsValid() ? CkStyle::Accent() : CkStyle::TextDim();
    }));
    Data.Visibility.Add(TEXT("layout-error-visible"), TAttribute<bool>::CreateLambda([WeakPane]
    {
        const TSharedPtr<SCkStyleLab_ControlsPane> Pane = WeakPane.Pin();
        return Pane.IsValid() && NOT Pane->Get_ProfileLayoutError().IsEmpty();
    }));
    Data.Collections.Add(TEXT("profiles"), _ProfileCollection);
    Data.ItemActions.Add(TEXT("apply-profile"), FCkUiOnItemAction::CreateLambda([WeakPane](const FString& InProfileName)
    {
        if (const TSharedPtr<SCkStyleLab_ControlsPane> Pane = WeakPane.Pin())
        { Pane->Apply_ProfileByName(InProfileName); }
    }));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid())
    {
        _ProfilePublicationError = TEXT("CkDebugger plugin is unavailable; Style Lab profiles cannot load their authored layout.");
        _ProfileCollection.Reset();
        return SNew(STextBlock).Text(Get_ProfileLayoutError());
    }

    _ProfileView = FCkUiView::Create({}, {}, MoveTemp(Tokens), CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> ProfileRegion = _ProfileView->GetRegion(TEXT("main"));
    const FString UiDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _ProfileView->SetFiles(
        FPaths::Combine(UiDirectory, TEXT("StyleLabProfiles.ui.html")),
        FPaths::Combine(UiDirectory, TEXT("StyleLabProfiles.ui.css")));
    Publish_ProfileRecords();
    _ProfileView->PollFiles();
    if (NOT _ProfileView->GetLastResult().Succeeded)
    { return SNew(STextBlock).Text(Get_ProfileLayoutError()); }
    RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SCkStyleLab_ControlsPane::Tick_ProfileFiles));

    return ProfileRegion;
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkStyleLab_ControlsPane::Publish_ProfileRecords() -> void
{
    if (NOT _ProfileCollection.IsValid()) { return; }

    const UCkDebuggerStyleSettings* Settings = UCkDebuggerStyleSettings::Get();
    const FString ActiveProfileName = Settings != nullptr ? Settings->ActiveProfileName : FString{};
    const FLinearColor Accent = CkStyle::Accent();
    const FLinearColor Text = CkStyle::Text();
    if (_HasPublishedProfileRecords && _PublishedProfileName == ActiveProfileName
        && _PublishedProfileAccent.Equals(Accent) && _PublishedProfileText.Equals(Text))
    { return; }

    auto Records = TArray<FCkUiRecordData>{};
    const TArray<FCkDebuggerStyleProfile>& Profiles = ck::debug_axes::Get_StyleProfiles();
    Records.Reserve(Profiles.Num());

    for (const FCkDebuggerStyleProfile& Profile : Profiles)
    {
        const bool IsActive = Settings != nullptr && Settings->ActiveProfileName == Profile.Name;
        auto Record = FCkUiRecordData{};
        Record.Key = Profile.Name;
        Record.Fields.Add(TEXT("name"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Profile.Name)});
        Record.Fields.Add(TEXT("blurb"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Profile.Blurb)});
        Record.Fields.Add(TEXT("active-color"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = IsActive ? Accent : Text});
        Records.Add(MoveTemp(Record));
    }

    const FCkUiLoadResult Result = _ProfileCollection->TrySetRecords(MoveTemp(Records));
    if (NOT Result.Succeeded)
    {
        _ProfilePublicationError = FString::Join(Result.Errors, TEXT("\n"));
        return;
    }
    _PublishedProfileName = ActiveProfileName;
    _PublishedProfileAccent = Accent;
    _PublishedProfileText = Text;
    _HasPublishedProfileRecords = true;
    _ProfilePublicationError.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkStyleLab_ControlsPane::Apply_ProfileByName(const FString& InProfileName) -> void
{
    const TArray<FCkDebuggerStyleProfile>& Profiles = ck::debug_axes::Get_StyleProfiles();
    const int32 ProfileIndex = Profiles.IndexOfByPredicate([&InProfileName](const FCkDebuggerStyleProfile& InProfile)
    {
        return InProfile.Name == InProfileName;
    });
    if (ProfileIndex != INDEX_NONE) { Apply_Profile(ProfileIndex); }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkStyleLab_ControlsPane::Get_ProfileLabel() const -> FText
{
    const UCkDebuggerStyleSettings* Settings = UCkDebuggerStyleSettings::Get();
    return FText::FromString(ck::Format_UE(TEXT("Current: {}"),
        Settings != nullptr ? Settings->ActiveProfileName : FString{TEXT("Unavailable")}));
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkStyleLab_ControlsPane::Get_ProfileLayoutError() const -> FText
{
    if (NOT _ProfilePublicationError.IsEmpty()) { return FText::FromString(_ProfilePublicationError); }
    return NOT _ProfileView.IsValid() || _ProfileView->GetLastResult().Succeeded
        ? FText::GetEmpty()
        : FText::FromString(FString::Join(_ProfileView->GetLastResult().Errors, TEXT("\n")));
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkStyleLab_ControlsPane::Tick_ProfileFiles(double, float) -> EActiveTimerReturnType
{
    if (_ProfileView.IsValid()) { _ProfileView->PollFiles(); }
    Publish_ProfileRecords();
    return EActiveTimerReturnType::Continue;
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
    Publish_ProfileRecords();
    RequestPreviewRebuilds();

    if (_OnSelectionChanged.IsBound())
    { _OnSelectionChanged.Execute(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    RequestPreviewRebuilds()
    -> void
{
    for (const auto& Preview : _GroupPreviews)
    {
        if (Preview.IsValid())
        { Preview->RequestRebuild(); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    Set_ShowAllTones(
        bool InShowAllTones)
    -> void
{
    _ShowAllTones = InShowAllTones;
    for (const auto& Preview : _GroupPreviews)
    {
        if (Preview.IsValid())
        { Preview->Set_ShowAllTones(InShowAllTones); }
    }
}

// ====================================================================================================================
