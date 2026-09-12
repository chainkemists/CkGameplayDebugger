#include "CkStyleLabDebugger/Widgets/SCkStyleLab_ControlsPane.h"

#include "CkStyleLabDebugger/Styles/CkStyleLab_AxisMetadata.h"
#include "CkStyleLabDebugger/Widgets/SCkStyleLab_SamplePane.h"

#include "CkInputHudOverlay/Settings/CkInputHud_Settings.h"
#include "CkInputHudOverlay/Settings/CkInputHud_UserSettings.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Reflection helpers (module-unique namespace name — unity builds concatenate TUs).
// --------------------------------------------------------------------------------------------------------------------

namespace ck_style_lab_controls
{
    template <typename TEnum>
    auto Cycle(TEnum InValue, const int32 InDirection, const int32 InCount) -> TEnum
    {
        const int32 Current = static_cast<int32>(InValue);
        return static_cast<TEnum>(((Current + InDirection) % InCount + InCount) % InCount);
    }

    auto GetPaletteLabel(const ECk_InputHud_Palette InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_Palette::ArcticSignal:  return FText::FromString(TEXT("Arctic Signal"));
            case ECk_InputHud_Palette::EmberTerminal: return FText::FromString(TEXT("Ember Terminal"));
            case ECk_InputHud_Palette::OrchidSynth:   return FText::FromString(TEXT("Orchid Synth"));
            case ECk_InputHud_Palette::TacticalMint:  return FText::FromString(TEXT("Tactical Mint"));
            default:                                  return FText::FromString(TEXT("Arctic Signal"));
        }
    }

    auto GetDensityLabel(const ECk_InputHud_Density InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_Density::Compact:  return FText::FromString(TEXT("Compact"));
            case ECk_InputHud_Density::Standard: return FText::FromString(TEXT("Standard"));
            case ECk_InputHud_Density::Readable: return FText::FromString(TEXT("Readable"));
            default:                             return FText::FromString(TEXT("Compact"));
        }
    }

    auto GetCornerLabel(const ECk_InputHud_CornerStyle InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_CornerStyle::Sharp:   return FText::FromString(TEXT("Sharp"));
            case ECk_InputHud_CornerStyle::Soft:    return FText::FromString(TEXT("Soft"));
            case ECk_InputHud_CornerStyle::Rounded: return FText::FromString(TEXT("Rounded"));
            default:                                return FText::FromString(TEXT("Rounded"));
        }
    }

    auto GetAnchorLabel(const ECk_InputHud_AnchorCorner InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_AnchorCorner::TopLeft:     return FText::FromString(TEXT("Top left"));
            case ECk_InputHud_AnchorCorner::BottomLeft:  return FText::FromString(TEXT("Bottom left"));
            case ECk_InputHud_AnchorCorner::BottomRight: return FText::FromString(TEXT("Bottom right"));
            case ECk_InputHud_AnchorCorner::TopRight:
            default:                                     return FText::FromString(TEXT("Top right"));
        }
    }

    auto GetMetadataLabel(const ECk_InputHud_MetadataMode InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_MetadataMode::Keys:    return FText::FromString(TEXT("Keys"));
            case ECk_InputHud_MetadataMode::Compact: return FText::FromString(TEXT("Compact"));
            case ECk_InputHud_MetadataMode::Full:    return FText::FromString(TEXT("Full"));
            default:                                 return FText::FromString(TEXT("Compact"));
        }
    }

    auto GetFrameLabel(const ECk_InputHud_FrameNotation InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_FrameNotation::Press: return FText::FromString(TEXT("Press"));
            case ECk_InputHud_FrameNotation::Delta: return FText::FromString(TEXT("Delta"));
            case ECk_InputHud_FrameNotation::Range: return FText::FromString(TEXT("Range"));
            default:                                return FText::FromString(TEXT("Press"));
        }
    }

    auto GetInputHudColor(const ECk_InputHud_ColorRole InRole) -> FLinearColor
    {
        const FCk_InputHud_PaletteSnapshot Palette = UCk_InputHud_UserSettings::Get_PaletteSnapshot();
        switch (InRole)
        {
            case ECk_InputHud_ColorRole::ContainerOutline: return Palette.ContainerOutline;
            case ECk_InputHud_ColorRole::KeyBorder:        return Palette.KeyBorder;
            case ECk_InputHud_ColorRole::Active:           return Palette.Active;
            case ECk_InputHud_ColorRole::Resolved:         return Palette.Resolved;
            case ECk_InputHud_ColorRole::Unrouted:         return Palette.Unrouted;
        }
        return FLinearColor::Transparent;
    }

    auto ProfileUiSchema() -> TArray<FCkUiFieldSchema>
    {
        return {
            {TEXT("name"), ECkUiFieldKind::Text},
            {TEXT("blurb"), ECkUiFieldKind::Text},
            {TEXT("active-color"), ECkUiFieldKind::Color},
        };
    }

    auto AxisUiSchema() -> TArray<FCkUiFieldSchema>
    {
        return {
            {TEXT("label"), ECkUiFieldKind::Text},
            {TEXT("tooltip"), ECkUiFieldKind::Text},
            {TEXT("value"), ECkUiFieldKind::Text},
        };
    }

    auto GroupKey(const ECkStyleLab_Group InGroup) -> FString
    {
        switch (InGroup)
        {
            case ECkStyleLab_Group::WorkbenchSurfaces: return TEXT("workbench-surfaces");
            case ECkStyleLab_Group::TokensLegend: return TEXT("tokens-legend");
            case ECkStyleLab_Group::EntityValues: return TEXT("entity-values");
            case ECkStyleLab_Group::HierarchyEditing: return TEXT("hierarchy-editing");
            case ECkStyleLab_Group::Icons: return TEXT("icons");
            case ECkStyleLab_Group::GraphTelemetry: return TEXT("graph-telemetry");
            case ECkStyleLab_Group::InputHud: return TEXT("input-hud");
        }
        return TEXT("unknown");
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

        _AxesByProperty.Add(Axis->Property->GetName(), Axis);
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
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT RegistryResult.Succeeded)
    {
        _ControlsPublicationError = FString::Join(RegistryResult.Errors, TEXT("\n"));
        return SNew(STextBlock).Text(Get_ControlsLayoutError());
    }

    auto Collections = TMap<ECkStyleLab_Group, TSharedPtr<FCkUiCollection>>{};
    for (const FCkStyleLab_GroupMetadata& Group : ck::style_lab::Get_GroupMetadata())
    {
        if (Group.Group == ECkStyleLab_Group::InputHud) { continue; }
        TSharedPtr<FCkUiCollection> Collection;
        const FCkUiLoadResult Result = FCkUiCollection::TryCreate(ck_style_lab_controls::AxisUiSchema(), Collection);
        if (NOT Result.Succeeded)
        {
            _ControlsPublicationError = FString::Join(Result.Errors, TEXT("\n"));
            return SNew(STextBlock).Text(Get_ControlsLayoutError());
        }
        Collections.Add(Group.Group, MoveTemp(Collection));
    }

    const TWeakPtr<SCkStyleLab_ControlsPane> WeakPane{SharedThis(this)};
    auto Bindings = FCkUiView::FNativeBindings{};
    auto Previews = TArray<TSharedPtr<SCkStyleLab_SamplePane>>{};
    for (const FCkStyleLab_GroupMetadata& Group : ck::style_lab::Get_GroupMetadata())
    {
        const TSharedPtr<SCkStyleLab_SamplePane> Preview = SNew(SCkStyleLab_SamplePane).Group(Group.Group);
        Previews.Add(Preview);
        Bindings.Add(TEXT("preview-") + ck_style_lab_controls::GroupKey(Group.Group), Preview);
    }
    auto Tokens = FCkUiView::FTokens{};
    Tokens.Add(TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS));
    Tokens.Add(TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM));
    Tokens.Add(TEXT("--text-dim"), TEXT("#") + CkStyle::TextDim().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--err"), TEXT("#") + CkStyle::Err().ToFColorSRGB().ToHex());
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("controls-layout-error"), TAttribute<FText>::CreateLambda([WeakPane]()
    {
        const TSharedPtr<SCkStyleLab_ControlsPane> Pane = WeakPane.Pin();
        return Pane.IsValid() ? Pane->Get_ControlsLayoutError() : FText::GetEmpty();
    }));
    Data.Visibility.Add(TEXT("controls-layout-error-visible"), TAttribute<bool>::CreateLambda([WeakPane]()
    {
        const TSharedPtr<SCkStyleLab_ControlsPane> Pane = WeakPane.Pin();
        return Pane.IsValid() && !Pane->Get_ControlsLayoutError().IsEmpty();
    }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakPane]()
    {
        const TSharedPtr<SCkStyleLab_ControlsPane> Pane = WeakPane.Pin();
        return Pane.IsValid() && Pane->_ControlsPublicationError.IsEmpty();
    });
    Data.SlateUserIndex = 0;
    for (const FCkStyleLab_GroupMetadata& Group : ck::style_lab::Get_GroupMetadata())
    {
        const FString Key = ck_style_lab_controls::GroupKey(Group.Group);
        Data.Text.Add(TEXT("group-") + Key + TEXT("-title"), Group.DisplayName);
        Data.Text.Add(TEXT("group-") + Key + TEXT("-description"), Group.Description);
        if (const TSharedPtr<FCkUiCollection>* Collection = Collections.Find(Group.Group))
        { Data.Collections.Add(TEXT("axes-") + Key, *Collection); }
    }
    Data.ItemActions.Add(TEXT("cycle-axis-previous"), FCkUiOnItemAction::CreateLambda([WeakPane](const FString& PropertyName)
    {
        if (const TSharedPtr<SCkStyleLab_ControlsPane> Pane = WeakPane.Pin())
        { Pane->OnCycleAxis(Pane->_AxesByProperty.FindRef(PropertyName), -1); }
    }));
    Data.ItemActions.Add(TEXT("cycle-axis-next"), FCkUiOnItemAction::CreateLambda([WeakPane](const FString& PropertyName)
    {
        if (const TSharedPtr<SCkStyleLab_ControlsPane> Pane = WeakPane.Pin())
        { Pane->OnCycleAxis(Pane->_AxesByProperty.FindRef(PropertyName), 1); }
    }));

    auto Actions = FCkUiView::FActions{};
    Configure_InputHudBindings(Data, Actions, WeakPane);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid())
    {
        _ControlsPublicationError = TEXT("CkDebugger plugin is unavailable; Style Lab controls cannot load their authored layout.");
        return SNew(STextBlock).Text(Get_ControlsLayoutError());
    }

    const TSharedRef<FCkUiView> View = FCkUiView::Create(MoveTemp(Bindings), MoveTemp(Actions), MoveTemp(Tokens),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Region = View->GetRegion(TEXT("main"));
    const FString UiDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    View->SetFiles(FPaths::Combine(UiDirectory, TEXT("StyleLabControls.ui.html")),
        FPaths::Combine(UiDirectory, TEXT("StyleLabControls.ui.css")));

    _AxisCollections = Collections;
    Publish_AxisRecords();
    if (NOT _ControlsPublicationError.IsEmpty())
    {
        _AxisCollections.Reset();
        return SNew(STextBlock).Text(Get_ControlsLayoutError());
    }
    View->PollFiles();
    if (NOT View->GetLastResult().Succeeded)
    {
        _ControlsPublicationError = FString::Join(View->GetLastResult().Errors, TEXT("\n"));
        _AxisCollections.Reset();
        return SNew(STextBlock).Text(Get_ControlsLayoutError());
    }

    _ControlsView = View;
    _GroupPreviews.Append(MoveTemp(Previews));
    _ControlsPublicationError.Reset();
    return Region;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_ControlsPane::
    Configure_InputHudBindings(
        FCkUiView::FDataBindings& InOutData,
        FCkUiView::FActions& InOutActions,
        TWeakPtr<SCkStyleLab_ControlsPane> InWeakPane) -> void
{
    const auto AddCycle = [&InOutData, &InOutActions, InWeakPane](
        const TCHAR* InName,
        TFunction<FText()> InLabel,
        TFunction<void(int32)> InCycle)
    {
        const FString Name{InName};
        InOutData.Text.Add(Name + TEXT("-value"), TAttribute<FText>::CreateLambda([InWeakPane, Label = MoveTemp(InLabel)]()
        {
            return InWeakPane.IsValid() ? Label() : FText::GetEmpty();
        }));
        InOutActions.Add(Name + TEXT("-previous"), FSimpleDelegate::CreateLambda([InWeakPane, Cycle = InCycle]()
        {
            if (InWeakPane.IsValid()) { Cycle(-1); }
        }));
        InOutActions.Add(Name + TEXT("-next"), FSimpleDelegate::CreateLambda([InWeakPane, Cycle = MoveTemp(InCycle)]()
        {
            if (InWeakPane.IsValid()) { Cycle(1); }
        }));
    };
    AddCycle(TEXT("input-hud-palette"), [] { return ck_style_lab_controls::GetPaletteLabel(UCk_InputHud_UserSettings::Get()->Palette); },
        [InWeakPane](const int32 Direction) { auto* Settings = UCk_InputHud_UserSettings::Get_Mutable(); Settings->Set_Palette(ck_style_lab_controls::Cycle(Settings->Palette, Direction, 4)); if (const auto Pane = InWeakPane.Pin()) { Pane->Notify_SelectionChanged(); } });
    AddCycle(TEXT("input-hud-density"), [] { return ck_style_lab_controls::GetDensityLabel(UCk_InputHud_UserSettings::Get_Density()); },
        [InWeakPane](const int32 Direction) { auto* Settings = UCk_InputHud_UserSettings::Get_Mutable(); Settings->Set_Density(ck_style_lab_controls::Cycle(Settings->Density, Direction, 3)); if (const auto Pane = InWeakPane.Pin()) { Pane->Notify_SelectionChanged(); } });
    AddCycle(TEXT("input-hud-corners"), [] { return ck_style_lab_controls::GetCornerLabel(UCk_InputHud_UserSettings::Get_CornerStyle()); },
        [InWeakPane](const int32 Direction) { auto* Settings = UCk_InputHud_UserSettings::Get_Mutable(); Settings->Set_CornerStyle(ck_style_lab_controls::Cycle(Settings->CornerStyle, Direction, 3)); if (const auto Pane = InWeakPane.Pin()) { Pane->Notify_SelectionChanged(); } });
    AddCycle(TEXT("input-hud-anchor"), [] { return ck_style_lab_controls::GetAnchorLabel(UCk_InputHud_UserSettings::Get_AnchorCorner()); },
        [InWeakPane](const int32 Direction) { UCk_InputHud_UserSettings::Get_Mutable()->Set_AnchorCorner(ck_style_lab_controls::Cycle(UCk_InputHud_UserSettings::Get_AnchorCorner(), Direction, 4)); if (const auto Pane = InWeakPane.Pin()) { Pane->Notify_SelectionChanged(); } });
    AddCycle(TEXT("input-hud-metadata"), [] { return ck_style_lab_controls::GetMetadataLabel(UCk_InputHud_UserSettings::Get_MetadataMode()); },
        [InWeakPane](const int32 Direction) { auto* Settings = UCk_InputHud_UserSettings::Get_Mutable(); Settings->Set_MetadataMode(ck_style_lab_controls::Cycle(Settings->MetadataMode, Direction, 3)); if (const auto Pane = InWeakPane.Pin()) { Pane->Notify_SelectionChanged(); } });
    AddCycle(TEXT("input-hud-frame"), [] { return ck_style_lab_controls::GetFrameLabel(UCk_InputHud_UserSettings::Get_FrameNotation()); },
        [InWeakPane](const int32 Direction) { auto* Settings = UCk_InputHud_UserSettings::Get_Mutable(); Settings->Set_FrameNotation(ck_style_lab_controls::Cycle(Settings->FrameNotation, Direction, 3)); if (const auto Pane = InWeakPane.Pin()) { Pane->Notify_SelectionChanged(); } });

    const auto AddNumber = [&InOutData, InWeakPane](const TCHAR* InName, TFunction<float()> InGetter, TFunction<void(float)> InSetter)
    {
        InOutData.Number.Add(InName, TAttribute<float>::CreateLambda([Getter = InGetter] { return Getter(); }));
        InOutData.NumberCommitted.Add(InName, FCkUiOnNumberCommitted::CreateLambda([InWeakPane, Getter = InGetter, Setter = MoveTemp(InSetter)](const float Value, ETextCommit::Type)
        {
            if (const TSharedPtr<SCkStyleLab_ControlsPane> Pane = InWeakPane.Pin())
            {
                const uint32 Before = UCk_InputHud_UserSettings::Get_Revision();
                if (!FMath::IsNearlyEqual(Getter(), Value)) { Setter(Value); }
                if (UCk_InputHud_UserSettings::Get_Revision() != Before) { Pane->Notify_SelectionChanged(); }
            }
        }));
    };
    AddNumber(TEXT("input-hud-padding-x"), [] { return UCk_InputHud_UserSettings::Get_KeyPaddingX(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyPaddingX(Value); });
    AddNumber(TEXT("input-hud-padding-y"), [] { return UCk_InputHud_UserSettings::Get_KeyPaddingY(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyPaddingY(Value); });
    AddNumber(TEXT("input-hud-corner-radius"), [] { return UCk_InputHud_UserSettings::Get_KeyCornerRadius(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyCornerRadius(Value); });
    AddNumber(TEXT("input-hud-overall-opacity"), [] { return UCk_InputHud_UserSettings::Get_OverallOpacity(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_OverallOpacity(Value); });
    AddNumber(TEXT("input-hud-anchor-x"), [] { return UCk_InputHud_UserSettings::Get_AnchorOffsetX(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_AnchorOffsetX(Value); });
    AddNumber(TEXT("input-hud-anchor-y"), [] { return UCk_InputHud_UserSettings::Get_AnchorOffsetY(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_AnchorOffsetY(Value); });
    AddNumber(TEXT("input-hud-border-width"), [] { return UCk_InputHud_UserSettings::Get_KeyBorderWidth(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyBorderWidth(Value); });
    AddNumber(TEXT("input-hud-border-opacity"), [] { return UCk_InputHud_UserSettings::Get_KeyBorderOpacity(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyBorderOpacity(Value); });
    AddNumber(TEXT("input-hud-active-fill-opacity"), [] { return UCk_InputHud_UserSettings::Get_ActiveFillOpacity(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_ActiveFillOpacity(Value); });
    AddNumber(TEXT("input-hud-active-glow-opacity"), [] { return UCk_InputHud_UserSettings::Get_ActiveGlowOpacity(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_ActiveGlowOpacity(Value); });
    AddNumber(TEXT("input-hud-panel-opacity"), [] { return UCk_InputHud_UserSettings::Get_PanelOpacity(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_PanelOpacity(Value); });
    AddNumber(TEXT("input-hud-pulse-scale"), [] { return UCk_InputHud_UserSettings::Get_PulseScale(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_PulseScale(Value); });
    AddNumber(TEXT("input-hud-history-brightness"), [] { return UCk_InputHud_UserSettings::Get_HistoryBrightness(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_HistoryBrightness(Value); });
    AddNumber(TEXT("input-hud-press-pop-scale"), [] { return UCk_InputHud_UserSettings::Get_PressPopScale(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_PressPopScale(Value); });
    AddNumber(TEXT("input-hud-press-pop-ms"), [] { return UCk_InputHud_UserSettings::Get_PressPopDurationMs(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_PressPopDurationMs(Value); });
    AddNumber(TEXT("input-hud-release-ease-ms"), [] { return UCk_InputHud_UserSettings::Get_ReleaseEaseMs(); }, [](const float Value) { UCk_InputHud_UserSettings::Get_Mutable()->Set_ReleaseEaseMs(Value); });
    InOutData.Number.Add(TEXT("input-hud-hold-bar-max"), TAttribute<float>::CreateLambda([] { return UCk_InputHud_Settings::Get_HoldBarMaxPx(); }));
    InOutData.NumberCommitted.Add(TEXT("input-hud-hold-bar-max"), FCkUiOnNumberCommitted::CreateLambda([InWeakPane](const float Value, ETextCommit::Type)
    {
        const float Sanitized = FMath::Clamp(Value, 8.0f, 64.0f);
        if (const auto Pane = InWeakPane.Pin(); Pane.IsValid() && !FMath::IsNearlyEqual(UCk_InputHud_Settings::Get_HoldBarMaxPx(), Sanitized))
        {
            auto* Settings = GetMutableDefault<UCk_InputHud_Settings>();
            Settings->HoldBarMaxPx = Sanitized;
            Settings->SaveConfig();
            UCk_InputHud_UserSettings::Get_Mutable()->NotifyChanged();
            Pane->Notify_SelectionChanged();
        }
    }));
    const auto AddColor = [&InOutData, InWeakPane](const TCHAR* InName, const ECk_InputHud_ColorRole Role)
    {
        InOutData.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([InWeakPane, Role]
        { return InWeakPane.IsValid() ? ck_style_lab_controls::GetInputHudColor(Role) : FLinearColor::Transparent; }));
        InOutData.ColorCommitted.Add(FString(InName) + TEXT("-committed"), FCkUiOnColorCommitted::CreateLambda([InWeakPane, Role](const FLinearColor Value)
        {
            if (const auto Pane = InWeakPane.Pin())
            {
                auto* Settings = UCk_InputHud_UserSettings::Get_Mutable();
                if (!Settings->UseCustomColors || !ck_style_lab_controls::GetInputHudColor(Role).Equals(Value.GetClamped()))
                { Settings->Set_CustomColor(Role, Value.GetClamped()); Pane->Notify_SelectionChanged(); }
            }
        }));
    };
    AddColor(TEXT("input-hud-container-outline-color"), ECk_InputHud_ColorRole::ContainerOutline);
    AddColor(TEXT("input-hud-key-outline-color"), ECk_InputHud_ColorRole::KeyBorder);
    AddColor(TEXT("input-hud-active-color"), ECk_InputHud_ColorRole::Active);
    AddColor(TEXT("input-hud-resolved-color"), ECk_InputHud_ColorRole::Resolved);
    AddColor(TEXT("input-hud-unrouted-color"), ECk_InputHud_ColorRole::Unrouted);
    InOutActions.Add(TEXT("input-hud-reset-visuals"), FSimpleDelegate::CreateLambda([InWeakPane]
    {
        if (const auto Pane = InWeakPane.Pin())
        {
            const uint32 Before = UCk_InputHud_UserSettings::Get_Revision();
            UCk_InputHud_UserSettings::Get_Mutable()->Reset_VisualTuning();
            if (UCk_InputHud_UserSettings::Get_Revision() != Before) { Pane->Notify_SelectionChanged(); }
        }
    }));

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

auto SCkStyleLab_ControlsPane::Publish_AxisRecords() -> void
{
    if (_AxisCollections.IsEmpty()) { return; }

    auto RecordsByGroup = TMap<ECkStyleLab_Group, TArray<FCkUiRecordData>>{};
    for (const TSharedPtr<FCkStyleLab_AxisRow>& Axis : _Axes)
    {
        if (NOT Axis.IsValid()) { continue; }
        auto& Records = RecordsByGroup.FindOrAdd(Axis->Group);
        auto Record = FCkUiRecordData{};
        Record.Key = Axis->Property->GetName();
        Record.Fields.Add(TEXT("label"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = Axis->DisplayName});
        Record.Fields.Add(TEXT("tooltip"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = Axis->ToolTip});
        Record.Fields.Add(TEXT("value"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = Get_AxisValueLabel(Axis)});
        Records.Add(MoveTemp(Record));
    }

    auto Updates = TArray<FCkUiCollectionUpdate>{};
    Updates.Reserve(_AxisCollections.Num());
    for (const auto& Pair : _AxisCollections)
    {
        Updates.Add({.Collection = Pair.Value, .Records = MoveTemp(RecordsByGroup.FindOrAdd(Pair.Key))});
    }

    const FCkUiLoadResult Result = FCkUiCollection::TrySetRecordsBatch(MoveTemp(Updates));
    if (NOT Result.Succeeded)
    {
        _ControlsPublicationError = FString::Join(Result.Errors, TEXT("\n"));
        return;
    }
    _ControlsPublicationError.Reset();
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

auto SCkStyleLab_ControlsPane::Get_ControlsLayoutError() const -> FText
{
    if (NOT _ControlsPublicationError.IsEmpty()) { return FText::FromString(_ControlsPublicationError); }
    return NOT _ControlsView.IsValid() || _ControlsView->GetLastResult().Succeeded
        ? FText::GetEmpty()
        : FText::FromString(FString::Join(_ControlsView->GetLastResult().Errors, TEXT("\n")));
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkStyleLab_ControlsPane::Tick_ProfileFiles(double, float) -> EActiveTimerReturnType
{
    if (_ProfileView.IsValid()) { _ProfileView->PollFiles(); }
    if (_ControlsView.IsValid()) { _ControlsView->PollFiles(); }
    Publish_ProfileRecords();
    Publish_AxisRecords();
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
    Publish_AxisRecords();
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
