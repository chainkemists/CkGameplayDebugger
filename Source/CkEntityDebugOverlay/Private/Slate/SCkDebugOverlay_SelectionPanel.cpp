#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_SelectionPanel.h"

#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_SelectionSettings.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"

#include "HAL/PlatformApplicationMisc.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_debugoverlay_selection_panel
{
    auto Get_HierarchyText(ECk_DebugOverlay_SelectionHierarchy InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots: return FText::FromString(TEXT("Meaningful roots"));
            case ECk_DebugOverlay_SelectionHierarchy::LiteralRoots: return FText::FromString(TEXT("Literal roots"));
            case ECk_DebugOverlay_SelectionHierarchy::AllEntities: return FText::FromString(TEXT("All entities"));
        }
        return FText::FromString(TEXT("Invalid"));
    }

    template<typename EnumType>
    auto Next(EnumType InValue, std::initializer_list<EnumType> InValues) -> EnumType
    {
        for (auto It = InValues.begin(); It != InValues.end(); ++It)
        {
            if (*It == InValue)
            {
                ++It;
                return It == InValues.end() ? *InValues.begin() : *It;
            }
        }
        return *InValues.begin();
    }

    auto Invoke(TFunction<void()>& InCallback) -> FReply
    {
        if (InCallback)
        { InCallback(); }
        return FReply::Handled();
    }

    auto Label(const FText& InText) -> TSharedRef<STextBlock>
    {
        return SNew(STextBlock)
            .Text(InText)
            .Font(ck::debug_axes::ScaledFont("Regular", 9))
            .ColorAndOpacity(CkStyle::TextMute());
    }

    auto Is_ProhibitedBindingKey(const FKey& InKey) -> bool
    {
        return !InKey.IsValid() || InKey.IsModifierKey() || InKey == EKeys::Semicolon ||
            InKey.IsMouseButton() || InKey == EKeys::MouseWheelAxis ||
            InKey == EKeys::MouseScrollUp || InKey == EKeys::MouseScrollDown;
    }
}

// ====================================================================================================================

auto SCkDebugOverlay_SelectionPanel::Construct(const FArguments& InArgs) -> void
{
    _OnClose = InArgs._OnClose;
    _OnSelect = InArgs._OnSelect;
    _OnPrevious = InArgs._OnPrevious;
    _OnNext = InArgs._OnNext;
    _OnFamily = InArgs._OnFamily;
    _StatusText = InArgs._StatusText;
    _ExplanationText = InArgs._ExplanationText;

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(390.0f)
        .MaxDesiredHeight(640.0f)
        [
            SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor(CkStyle::OverlayOf(CkStyle::BgRoot(), 0.96f))
            .Padding(FMargin{ CkStyle::SpaceM })
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                    [ SNew(STextBlock).Text(FText::FromString(TEXT("Selection"))).Font(ck::debug_axes::ScaledFont("Bold", 12)).ColorAndOpacity(CkStyle::TextStrong()) ]
                    + SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right)
                    [ Make_ActionButton(FText::FromString(TEXT("Close")), [this](){ _OnClose.ExecuteIfBound(); }) ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, CkStyle::SpaceS)
                [ SNew(STextBlock).Text(_ExplanationText).AutoWrapText(true).Font(ck::debug_axes::ScaledFont("Regular", 8)).ColorAndOpacity(CkStyle::TextMute()) ]
                + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()[ Build_PolicySection() ]
                    + SScrollBox::Slot()[ Build_TuningSection() ]
                    + SScrollBox::Slot()[ Build_PresentationSection() ]
                    + SScrollBox::Slot()[ Build_NamedPresetSection() ]
                    + SScrollBox::Slot()[ Build_InputSection() ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                [ SNew(STextBlock).Text_Lambda([this](){ return Get_StatusText(); }).AutoWrapText(true).Font(ck::debug_axes::ScaledFont("Regular", 8)).ColorAndOpacity(CkStyle::Accent()) ]
            ]
        ]
    ];
}

auto SCkDebugOverlay_SelectionPanel::Make_Section(const FText& InTitle, const TSharedRef<SWidget>& InContent) const -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor(CkStyle::OverlayOf(CkStyle::Bg2(), 0.85f))
        .Padding(FMargin{ CkStyle::SpaceS })
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
            [ SNew(STextBlock).Text(InTitle).Font(ck::debug_axes::ScaledFont("Bold", 9)).ColorAndOpacity(CkStyle::TextStrong()) ]
            + SVerticalBox::Slot().AutoHeight()[ InContent ]
        ];
}

auto SCkDebugOverlay_SelectionPanel::Make_CycleRow(const FText& InLabel, TAttribute<FText> InValue, TFunction<void()> InOnClicked) const -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[ ck_debugoverlay_selection_panel::Label(InLabel) ]
        + SHorizontalBox::Slot().AutoWidth()
        [ SNew(SButton).ContentPadding(FMargin{ CkStyle::SpaceS, 2.0f }).OnClicked_Lambda([Action = MoveTemp(InOnClicked)]() mutable { return ck_debugoverlay_selection_panel::Invoke(Action); })
            [ SNew(STextBlock).Text(InValue).Font(ck::debug_axes::ScaledFont("Regular", 8)).ColorAndOpacity(CkStyle::TextStrong()) ] ];
}

auto SCkDebugOverlay_SelectionPanel::Make_ToggleRow(const FText& InLabel, TAttribute<bool> InValue, TFunction<void(bool)> InOnChanged) const -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[ ck_debugoverlay_selection_panel::Label(InLabel) ]
        + SHorizontalBox::Slot().AutoWidth()
        [ SNew(SCkDebug_ToggleSurface)
            .IsOn(InValue)
            .AccessibleText(InLabel)
            .OnStateChanged_Lambda([Action = MoveTemp(InOnChanged)](bool InIsOn) mutable { Action(InIsOn); })
            [ SNew(STextBlock).Text_Lambda([InValue](){ return FText::FromString(InValue.Get() ? TEXT("On") : TEXT("Off")); }).Font(ck::debug_axes::ScaledFont("Regular", 8)).ColorAndOpacity(CkStyle::TextStrong()) ] ];
}

auto SCkDebugOverlay_SelectionPanel::Make_SliderRow(const FText& InLabel, TAttribute<float> InValue, float InMin, float InMax, TFunction<void(float)> InOnChanged) const -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[ ck_debugoverlay_selection_panel::Label(InLabel) ]
        + SHorizontalBox::Slot().AutoWidth()
        [ SNew(SCkDebug_NumericEditor)
            .Value_Lambda([InValue](){ return static_cast<double>(InValue.Get()); })
            .Kind(ECkDebug_NumericKind::Float)
            .MinValue(static_cast<double>(InMin))
            .MaxValue(static_cast<double>(InMax))
            .FractionalDigits(2)
            .OnValueCommitted(FOnCkDebug_NumericCommitted::CreateLambda([Action = MoveTemp(InOnChanged)](double InValue) mutable { Action(static_cast<float>(InValue)); })) ];
}

auto SCkDebugOverlay_SelectionPanel::Make_ActionButton(const FText& InLabel, TFunction<void()> InAction) const -> TSharedRef<SWidget>
{
    return SNew(SButton)
        .ContentPadding(FMargin{ CkStyle::SpaceS, 2.0f })
        .OnClicked_Lambda([Action = MoveTemp(InAction)]() mutable { return ck_debugoverlay_selection_panel::Invoke(Action); })
        [ SNew(STextBlock).Text(InLabel).Font(ck::debug_axes::ScaledFont("Regular", 8)).ColorAndOpacity(CkStyle::TextStrong()) ];
}

auto SCkDebugOverlay_SelectionPanel::Mutate_Config(TFunction<void(FCk_DebugOverlay_SelectionConfig&)> InMutation, bool InSave) const -> void
{
    auto* Settings = UCk_DebugOverlay_SelectionSettings::Get_Mutable();
    if (Settings == nullptr)
    { return; }
    auto Candidate = Settings->Get_Config();
    InMutation(Candidate);
    Settings->TrySet_Config(Candidate, InSave);
}

auto SCkDebugOverlay_SelectionPanel::Build_PolicySection() -> TSharedRef<SWidget>
{
    auto Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()[Make_CycleRow(FText::FromString(TEXT("Hierarchy")), TAttribute<FText>::CreateLambda([](){ return ck_debugoverlay_selection_panel::Get_HierarchyText(UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().Hierarchy); }), [this](){ Mutate_Config([](auto& C){ C.Hierarchy = ck_debugoverlay_selection_panel::Next(C.Hierarchy, { ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots, ECk_DebugOverlay_SelectionHierarchy::LiteralRoots, ECk_DebugOverlay_SelectionHierarchy::AllEntities }); }); })];
    Content->AddSlot().AutoHeight()[Make_CycleRow(FText::FromString(TEXT("Anchor")), TAttribute<FText>::CreateLambda([](){ return FText::FromString(UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().RootAnchor == ECk_DebugOverlay_SelectionRootAnchor::Member ? TEXT("Member") : TEXT("Root")); }), [this](){ Mutate_Config([](auto& C){ C.RootAnchor = ck_debugoverlay_selection_panel::Next(C.RootAnchor, { ECk_DebugOverlay_SelectionRootAnchor::Member, ECk_DebugOverlay_SelectionRootAnchor::Root }); }); })];
    Content->AddSlot().AutoHeight()[Make_CycleRow(FText::FromString(TEXT("Aim scope")), TAttribute<FText>::CreateLambda([](){ const auto V=UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().Scope; return FText::FromString(V==ECk_DebugOverlay_SelectionScope::InView ? TEXT("In view") : V==ECk_DebugOverlay_SelectionScope::Nearby ? TEXT("Nearby") : TEXT("View + nearby")); }), [this](){ Mutate_Config([](auto& C){ C.Scope = ck_debugoverlay_selection_panel::Next(C.Scope, { ECk_DebugOverlay_SelectionScope::ViewWithNearbyFallback, ECk_DebugOverlay_SelectionScope::InView, ECk_DebugOverlay_SelectionScope::Nearby }); }); })];
    Content->AddSlot().AutoHeight()[Make_CycleRow(FText::FromString(TEXT("Aim targeting")), TAttribute<FText>::CreateLambda([](){ return FText::FromString(UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().Targeting == ECk_DebugOverlay_SelectionTargeting::Weighted ? TEXT("Weighted") : TEXT("Cone")); }), [this](){ Mutate_Config([](auto& C){ C.Targeting = ck_debugoverlay_selection_panel::Next(C.Targeting, { ECk_DebugOverlay_SelectionTargeting::Weighted, ECk_DebugOverlay_SelectionTargeting::Cone }); }); })];
    return Make_Section(FText::FromString(TEXT("Policy")), Content);
}

auto SCkDebugOverlay_SelectionPanel::Build_TuningSection() -> TSharedRef<SWidget>
{
    auto Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()[Make_SliderRow(FText::FromString(TEXT("View bias")), TAttribute<float>::CreateLambda([](){ return UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().ViewBias; }), 0.0f, 1.0f, [this](float V){ Mutate_Config([V](auto& C){ C.ViewBias = V; }); })];
    Content->AddSlot().AutoHeight()[Make_SliderRow(FText::FromString(TEXT("Discovery radius")), TAttribute<float>::CreateLambda([](){ return UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().SearchRadius; }), 0.0f, 10000.0f, [this](float V){ Mutate_Config([V](auto& C){ C.SearchRadius = V; }); })];
    Content->AddSlot().AutoHeight()[Make_SliderRow(FText::FromString(TEXT("Cone half angle")), TAttribute<float>::CreateLambda([](){ return UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().ConeHalfAngle; }), 1.0f, 90.0f, [this](float V){ Mutate_Config([V](auto& C){ C.ConeHalfAngle = V; }); })];
    return Make_Section(FText::FromString(TEXT("Tuning")), Content);
}

auto SCkDebugOverlay_SelectionPanel::Build_PresentationSection() -> TSharedRef<SWidget>
{
    auto Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()[Make_CycleRow(FText::FromString(TEXT("Order")), TAttribute<FText>::CreateLambda([](){ return FText::FromString(UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().Order == ECk_DebugOverlay_SelectionOrder::Score ? TEXT("Score") : TEXT("Screen")); }), [this](){ Mutate_Config([](auto& C){ C.Order = ck_debugoverlay_selection_panel::Next(C.Order, { ECk_DebugOverlay_SelectionOrder::Score, ECk_DebugOverlay_SelectionOrder::Screen }); }); })];
    Content->AddSlot().AutoHeight()[Make_CycleRow(FText::FromString(TEXT("Ordering")), TAttribute<FText>::CreateLambda([](){ return FText::FromString(UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().Stability == ECk_DebugOverlay_SelectionStability::Frozen ? TEXT("Stable order") : TEXT("Live rerank")); }), [this](){ Mutate_Config([](auto& C){ C.Stability = ck_debugoverlay_selection_panel::Next(C.Stability, { ECk_DebugOverlay_SelectionStability::Frozen, ECk_DebugOverlay_SelectionStability::Live }); }); })];
    Content->AddSlot().AutoHeight()[Make_CycleRow(FText::FromString(TEXT("Family")), TAttribute<FText>::CreateLambda([](){ return FText::FromString(UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().Family == ECk_DebugOverlay_SelectionFamily::Hold ? TEXT("Hold") : TEXT("Toggle")); }), [this](){ Mutate_Config([](auto& C){ C.Family = ck_debugoverlay_selection_panel::Next(C.Family, { ECk_DebugOverlay_SelectionFamily::Hold, ECk_DebugOverlay_SelectionFamily::Toggle }); }); })];
    Content->AddSlot().AutoHeight()[Make_ToggleRow(FText::FromString(TEXT("Show drawer shortlist")), TAttribute<bool>::CreateLambda([](){ return UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().Labels == ECk_DebugOverlay_SelectionLabels::Shortlist; }), [this](bool InShow){ Mutate_Config([InShow](auto& C){ C.Labels = InShow ? ECk_DebugOverlay_SelectionLabels::Shortlist : ECk_DebugOverlay_SelectionLabels::Numbers; }); })];
    Content->AddSlot().AutoHeight()[Make_ToggleRow(FText::FromString(TEXT("Show aim cone")), TAttribute<bool>::CreateLambda([](){ return UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().ShowAimCone; }), [this](bool V){ Mutate_Config([V](auto& C){ C.ShowAimCone = V; }); })];
    Content->AddSlot().AutoHeight()[Make_ToggleRow(FText::FromString(TEXT("Include occluded")), TAttribute<bool>::CreateLambda([](){ return UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().IncludeOccluded; }), [this](bool V){ Mutate_Config([V](auto& C){ C.IncludeOccluded = V; }); })];
    Content->AddSlot().AutoHeight()[Make_SliderRow(FText::FromString(TEXT("Diamond scale")), TAttribute<float>::CreateLambda([](){ return UCk_DebugOverlay_SelectionSettings::Get()->Get_Config().DiamondScale; }), 0.1f, 5.0f, [this](float V){ Mutate_Config([V](auto& C){ C.DiamondScale = V; }); })];
    Content->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,CkStyle::SpaceXS,0.0f)[Make_ActionButton(FText::FromString(TEXT("Focus family")), [](){ UCk_DebugOverlay_SelectionSettings::Get_Mutable()->ApplyPreset(ECk_DebugOverlay_SelectionPreset::FocusFamily); })]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,CkStyle::SpaceXS,0.0f)[Make_ActionButton(FText::FromString(TEXT("Spatial sweep")), [](){ UCk_DebugOverlay_SelectionSettings::Get_Mutable()->ApplyPreset(ECk_DebugOverlay_SelectionPreset::SpatialSweep); })]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,CkStyle::SpaceXS,0.0f)[Make_ActionButton(FText::FromString(TEXT("Aim cone")), [](){ UCk_DebugOverlay_SelectionSettings::Get_Mutable()->ApplyPreset(ECk_DebugOverlay_SelectionPreset::AimCone); })]
        + SHorizontalBox::Slot().AutoWidth()[Make_ActionButton(FText::FromString(TEXT("Reset")), [](){ UCk_DebugOverlay_SelectionSettings::Get_Mutable()->Reset_Config(); })]];
    return Make_Section(FText::FromString(TEXT("Presentation")), Content);
}

auto SCkDebugOverlay_SelectionPanel::Build_InputSection() -> TSharedRef<SWidget>
{
    auto Content = SNew(SVerticalBox);
    for (const auto Binding : { FName(TEXT("Select")), FName(TEXT("Previous")), FName(TEXT("Next")), FName(TEXT("Family")), FName(TEXT("Settings")) })
    {
        Content->AddSlot().AutoHeight()[Make_CycleRow(FText::FromName(Binding), TAttribute<FText>::CreateLambda([this, Binding](){ return Get_BindingText(Binding); }), [this, Binding](){ Begin_BindingCapture(Binding); })];
    }
    Content->AddSlot().AutoHeight()[Make_SliderRow(FText::FromString(TEXT("Hold Select to lock (seconds)")),
        TAttribute<float>::CreateLambda([](){ return GetDefault<UCk_DebugOverlay_InputSettings>()->HoldSelectSeconds; }),
        0.05f, 3.0f, [](float InSeconds)
        {
            if (NOT FMath::IsFinite(InSeconds))
            { return; }
            auto* Input = GetMutableDefault<UCk_DebugOverlay_InputSettings>();
            Input->HoldSelectSeconds = FMath::Clamp(InSeconds, 0.05f, 3.0f);
            Input->SaveConfig();
        })];
    Content->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,CkStyle::SpaceXS,0.0f)[Make_ActionButton(FText::FromString(TEXT("Select")), [this](){ if (_OnSelect) { _OnSelect(); } })]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,CkStyle::SpaceXS,0.0f)[Make_ActionButton(FText::FromString(TEXT("Previous")), [this](){ if (_OnPrevious) { _OnPrevious(); } })]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,CkStyle::SpaceXS,0.0f)[Make_ActionButton(FText::FromString(TEXT("Next")), [this](){ if (_OnNext) { _OnNext(); } })]
        + SHorizontalBox::Slot().AutoWidth()[Make_ActionButton(FText::FromString(TEXT("Family")), [this](){ if (_OnFamily) { _OnFamily(); } })]];
    return Make_Section(FText::FromString(TEXT("Bindings (click, then press a key)")), Content);
}

auto SCkDebugOverlay_SelectionPanel::Build_NamedPresetSection() -> TSharedRef<SWidget>
{
    auto Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()
    [
        SAssignNew(_PresetNameText, SEditableTextBox)
        .HintText(FText::FromString(TEXT("Preset name")))
        .Font(ck::debug_axes::ScaledFont("Regular", 8))
    ];
    Content->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
        [ Make_ActionButton(FText::FromString(TEXT("Save")), [this]()
            {
                const auto Name = _PresetNameText.IsValid() ? FName{_PresetNameText->GetText().ToString()} : NAME_None;
                if (auto* Settings = UCk_DebugOverlay_SelectionSettings::Get_Mutable(); Settings != nullptr && !Settings->TrySave_NamedPreset(Name))
                { _PanelError = Settings->Get_LastError(); }
                else { _PanelError.Reset(); }
            }) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
        [ Make_ActionButton(FText::FromString(TEXT("Apply")), [this]()
            {
                const auto Name = _PresetNameText.IsValid() ? FName{_PresetNameText->GetText().ToString()} : NAME_None;
                if (auto* Settings = UCk_DebugOverlay_SelectionSettings::Get_Mutable(); Settings != nullptr && !Settings->TryApply_NamedPreset(Name))
                { _PanelError = Settings->Get_LastError(); }
                else { _PanelError.Reset(); }
            }) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
        [ Make_ActionButton(FText::FromString(TEXT("Copy JSON")), [this]()
            {
                FString Json;
                const auto Name = _PresetNameText.IsValid() ? FName{_PresetNameText->GetText().ToString()} : NAME_None;
                const auto* Settings = UCk_DebugOverlay_SelectionSettings::Get();
                if (Settings != nullptr && Settings->Export_NamedPresetJson(Name, Json))
                { FPlatformApplicationMisc::ClipboardCopy(*Json); _PanelError.Reset(); }
                else { _PanelError = TEXT("Save a named preset before exporting it."); }
            }) ]
        + SHorizontalBox::Slot().AutoWidth()
        [ Make_ActionButton(FText::FromString(TEXT("Paste JSON")), [this]()
            {
                FString Json;
                FPlatformApplicationMisc::ClipboardPaste(Json);
                FName ImportedName;
                if (auto* Settings = UCk_DebugOverlay_SelectionSettings::Get_Mutable(); Settings != nullptr && Settings->TryImport_NamedPresetJson(Json, ImportedName))
                {
                    _PresetNameText->SetText(FText::FromName(ImportedName));
                    _PanelError.Reset();
                }
                else { _PanelError = Settings != nullptr ? Settings->Get_LastError() : TEXT("Selection settings are unavailable."); }
            }) ]
    ];
    return Make_Section(FText::FromString(TEXT("Named presets")), Content);
}

auto SCkDebugOverlay_SelectionPanel::Begin_BindingCapture(FName InBinding) -> FReply
{
    _BindingToCapture = InBinding;
    FSlateApplication::Get().SetKeyboardFocus(AsShared(), EFocusCause::SetDirectly);
    return FReply::Handled();
}

auto SCkDebugOverlay_SelectionPanel::Set_Binding(FName InBinding, const FKey& InKey) -> void
{
    auto* Input = GetMutableDefault<UCk_DebugOverlay_InputSettings>();
    if (Input == nullptr)
    { _PanelError = TEXT("Selection bindings are unavailable."); return; }
    if (ck_debugoverlay_selection_panel::Is_ProhibitedBindingKey(InKey))
    { _PanelError = TEXT("Use a keyboard key other than semicolon; mouse and wheel bindings are not supported."); return; }
    const auto Duplicate = [&InBinding, &InKey, Input]()
    {
        const auto OtherUnmodified = (InBinding != TEXT("Select") && Input->SelectKey == InKey) ||
            (InBinding != TEXT("Previous") && Input->PreviousKey == InKey) ||
            (InBinding != TEXT("Next") && Input->NextKey == InKey) ||
            (InBinding != TEXT("Family") && Input->FamilyKey == InKey);
        const auto SettingsCollision = InBinding != TEXT("Settings") && !Input->SettingsRequireControl && Input->SettingsKey == InKey;
        const auto UnmodifiedSettings = InBinding == TEXT("Settings") && !Input->SettingsRequireControl &&
            (Input->SelectKey == InKey || Input->PreviousKey == InKey || Input->NextKey == InKey || Input->FamilyKey == InKey);
        return OtherUnmodified || SettingsCollision || UnmodifiedSettings;
    }();
    if (Duplicate)
    { _PanelError = TEXT("Selection bindings must be unique. Ctrl+Settings is the only allowed overlap."); return; }
    if (InBinding == TEXT("Select")) { Input->SelectKey = InKey; }
    else if (InBinding == TEXT("Previous")) { Input->PreviousKey = InKey; }
    else if (InBinding == TEXT("Next")) { Input->NextKey = InKey; }
    else if (InBinding == TEXT("Family")) { Input->FamilyKey = InKey; }
    else if (InBinding == TEXT("Settings")) { Input->SettingsKey = InKey; }
    else { _PanelError = TEXT("Unknown selection binding."); return; }
    _PanelError.Reset();
    Input->SaveConfig();
}

auto SCkDebugOverlay_SelectionPanel::Get_BindingText(FName InBinding) const -> FText
{
    if (_BindingToCapture == InBinding)
    { return FText::FromString(TEXT("Press a key…")); }
    const auto* Input = GetDefault<UCk_DebugOverlay_InputSettings>();
    if (Input == nullptr)
    { return FText::FromString(TEXT("Unavailable")); }
    const auto Key = InBinding == TEXT("Select") ? Input->SelectKey : InBinding == TEXT("Previous") ? Input->PreviousKey : InBinding == TEXT("Next") ? Input->NextKey : InBinding == TEXT("Family") ? Input->FamilyKey : Input->SettingsKey;
    const auto Prefix = InBinding == TEXT("Settings") && Input->SettingsRequireControl ? TEXT("Ctrl+") : TEXT("");
    return FText::FromString(FString{Prefix} + Key.GetDisplayName().ToString());
}

auto SCkDebugOverlay_SelectionPanel::Get_StatusText() const -> FText
{ return _PanelError.IsEmpty() ? _StatusText.Get() : FText::FromString(_PanelError); }

auto SCkDebugOverlay_SelectionPanel::OnPreviewKeyDown(const FGeometry&, const FKeyEvent& InKeyEvent) -> FReply
{
    const auto Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape)
    {
        if (_BindingToCapture.IsNone()) { _OnClose.ExecuteIfBound(); }
        else { _BindingToCapture = NAME_None; }
        return FReply::Handled();
    }
    const auto* Input = GetDefault<UCk_DebugOverlay_InputSettings>();
    const auto SettingsClose = Input != nullptr && Input->SettingsRequireControl && InKeyEvent.IsControlDown() &&
        !InKeyEvent.IsAltDown() && !InKeyEvent.IsShiftDown() && !InKeyEvent.IsCommandDown() && Key == Input->SettingsKey;
    if (SettingsClose)
    { _BindingToCapture = NAME_None; _OnClose.ExecuteIfBound(); return FReply::Handled(); }
    return FReply::Unhandled();
}

auto SCkDebugOverlay_SelectionPanel::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply
{
    const auto Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape)
    {
        if (_BindingToCapture.IsNone()) { _OnClose.ExecuteIfBound(); }
        else { _BindingToCapture = NAME_None; }
        return FReply::Handled();
    }
    const auto* Input = GetDefault<UCk_DebugOverlay_InputSettings>();
    const auto SettingsClose = Input != nullptr && Input->SettingsRequireControl && InKeyEvent.IsControlDown() &&
        !InKeyEvent.IsAltDown() && !InKeyEvent.IsShiftDown() && !InKeyEvent.IsCommandDown() && Key == Input->SettingsKey;
    if (SettingsClose)
    { _BindingToCapture = NAME_None; _OnClose.ExecuteIfBound(); return FReply::Handled(); }
    if (_BindingToCapture.IsNone())
    { return SCompoundWidget::OnKeyDown(InGeometry, InKeyEvent); }
    const auto ValidKey = !ck_debugoverlay_selection_panel::Is_ProhibitedBindingKey(Key);
    if (NOT ValidKey)
    { _PanelError = TEXT("Use a keyboard key other than semicolon; mouse and wheel bindings are not supported."); return FReply::Handled(); }
    Set_Binding(_BindingToCapture, Key);
    if (_PanelError.IsEmpty())
    { _BindingToCapture = NAME_None; }
    return FReply::Handled();
}

// ====================================================================================================================
