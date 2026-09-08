#include "CkTextureDebugger/Window/SCkTextureDebugger_DiagnosticPages.h"

#include "CkTextureDebugger/Analysis/CkTextureDebugger_MaterialAnalysis.h"
#include "CkTextureDebugger/Analysis/CkTextureDebugger_SurfaceAnalysis.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiTable.h"

#include "Components/MeshComponent.h"
#include "MaterialShaderType.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "RHIStrings.h"
#include "UObject/ObjectKey.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkTextureDebugger_DiagnosticPages"

namespace ck_texture_debugger_diagnostic_pages
{
    auto NormalizeSlots(TArray<int32> InSlots) -> TArray<int32>
    {
        InSlots.RemoveAll([](int32 InSlot) { return InSlot < 0; });
        InSlots.Sort();

        auto Result = TArray<int32>{};
        Result.Reserve(InSlots.Num());
        for (const auto Slot : InSlots)
        {
            if (Result.IsEmpty() || Result.Last() != Slot)
            { Result.Add(Slot); }
        }
        return Result;
    }

    auto FindSlot(
        const TOptional<FCkTextureDebugger_ComponentRow>& InComponent,
        int32 InSlotIndex) -> const FCkTextureDebugger_MaterialSlotRow*
    {
        if (NOT InComponent.IsSet())
        { return nullptr; }

        return InComponent->MaterialSlots.FindByPredicate([InSlotIndex](const FCkTextureDebugger_MaterialSlotRow& InSlot)
        {
            return InSlot.SlotIndex == InSlotIndex;
        });
    }

    auto ProvenanceText(ECkTextureDebugger_MaterialTextureProvenance InProvenance) -> FString
    {
        switch (InProvenance)
        {
            case ECkTextureDebugger_MaterialTextureProvenance::Parameter: return TEXT("Resolved parameter");
            case ECkTextureDebugger_MaterialTextureProvenance::UsedTexture: return TEXT("Potential used texture");
            case ECkTextureDebugger_MaterialTextureProvenance::Unavailable: return TEXT("Unavailable");
        }
        return TEXT("Unavailable");
    }

    auto ProvenanceTone(ECkTextureDebugger_MaterialTextureProvenance InProvenance) -> ECk_Tone
    {
        switch (InProvenance)
        {
            case ECkTextureDebugger_MaterialTextureProvenance::Parameter: return ECk_Tone::Ok;
            case ECkTextureDebugger_MaterialTextureProvenance::UsedTexture: return ECk_Tone::Info;
            case ECkTextureDebugger_MaterialTextureProvenance::Unavailable: return ECk_Tone::Warn;
        }
        return ECk_Tone::Neutral;
    }

    auto MatchesQuery(const FString& InHaystack, const FString& InQuery) -> bool
    {
        return InQuery.IsEmpty() || InHaystack.Contains(InQuery, ESearchCase::IgnoreCase);
    }

    auto TextColor(bool InHighlighted, bool InDimmed) -> FSlateColor
    {
        if (InHighlighted) { return FSlateColor{CkStyle::Accent()}; }
        if (InDimmed) { return FSlateColor{CkStyle::TextMute()}; }
        return FSlateColor{CkStyle::Text()};
    }

    auto MaterialInputsUiSchema() -> TArray<FCkUiFieldSchema>
    {
        return {{TEXT("parameter"), ECkUiFieldKind::Text}, {TEXT("detail"), ECkUiFieldKind::Text}, {TEXT("texture"), ECkUiFieldKind::Text},
            {TEXT("texture-tip"), ECkUiFieldKind::Text}, {TEXT("provenance"), ECkUiFieldKind::Text},
            {TEXT("slot"), ECkUiFieldKind::Text}, {TEXT("variant"), ECkUiFieldKind::Text},
            {TEXT("text-color"), ECkUiFieldKind::Color}, {TEXT("provenance-foreground"), ECkUiFieldKind::Color},
            {TEXT("provenance-background"), ECkUiFieldKind::Color}};
    }

    auto MaterialInputsUiRecord(const SCkTextureDebugger_MaterialInputsPage::FRow& InRow) -> FCkUiRecordData
    {
        auto Result = FCkUiRecordData{};
        Result.Key = InRow.StableKey;
        const auto Text = [&Result](const TCHAR* InName, FText InValue)
        { Result.Fields.Add(InName, FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = MoveTemp(InValue)}); };
        const auto Color = [&Result](const TCHAR* InName, FLinearColor InValue)
        { Result.Fields.Add(InName, FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = InValue}); };
        Text(TEXT("parameter"), FText::FromString(InRow.Parameter));
        Text(TEXT("detail"), FText::FromString(InRow.Detail));
        Text(TEXT("texture"), FText::FromString(InRow.Texture));
        Text(TEXT("texture-tip"), FText::FromString(InRow.TexturePath));
        Text(TEXT("provenance"), FText::FromString(InRow.Provenance));
        Text(TEXT("slot"), FText::FromString(InRow.Slot));
        Text(TEXT("variant"), FText::FromString(InRow.Variant));
        Color(TEXT("text-color"), TextColor(InRow.IsHighlighted, InRow.IsDimmed).GetSpecifiedColor());
        const auto Tone = ProvenanceTone(InRow.ProvenanceKind);
        Color(TEXT("provenance-foreground"), CkStyle::GetToneColor(Tone));
        Color(TEXT("provenance-background"), CkStyle::GetToneDimColor(Tone));
        return Result;
    }

    auto SurfaceLightingUiSchema() -> TArray<FCkUiFieldSchema>
    {
        return {
            {TEXT("heading"), ECkUiFieldKind::Text},
            {TEXT("material"), ECkUiFieldKind::Text},
            {TEXT("material-path"), ECkUiFieldKind::Text},
            {TEXT("status"), ECkUiFieldKind::Text},
            {TEXT("blend"), ECkUiFieldKind::Text},
            {TEXT("shading"), ECkUiFieldKind::Text},
            {TEXT("two-sided"), ECkUiFieldKind::Text},
            {TEXT("masked"), ECkUiFieldKind::Text},
            {TEXT("translucent"), ECkUiFieldKind::Text},
            {TEXT("cast-shadow"), ECkUiFieldKind::Text},
            {TEXT("dynamic-shadow"), ECkUiFieldKind::Text},
            {TEXT("static-shadow"), ECkUiFieldKind::Text},
            {TEXT("volumetric-shadow"), ECkUiFieldKind::Text},
            {TEXT("receives-decals"), ECkUiFieldKind::Text},
            {TEXT("static-lighting"), ECkUiFieldKind::Text},
            {TEXT("opacity-clip"), ECkUiFieldKind::Text},
            {TEXT("lightmap"), ECkUiFieldKind::Text},
            {TEXT("nanite"), ECkUiFieldKind::Text},
            {TEXT("caveat"), ECkUiFieldKind::Text},
            {TEXT("expanded"), ECkUiFieldKind::Bool},
            {TEXT("has-material"), ECkUiFieldKind::Bool},
            {TEXT("missing-material"), ECkUiFieldKind::Bool},
            {TEXT("status-foreground"), ECkUiFieldKind::Color},
            {TEXT("status-background"), ECkUiFieldKind::Color},
            {TEXT("fact-foreground"), ECkUiFieldKind::Color},
            {TEXT("fact-background"), ECkUiFieldKind::Color},
            {TEXT("two-sided-foreground"), ECkUiFieldKind::Color},
            {TEXT("masked-foreground"), ECkUiFieldKind::Color},
            {TEXT("translucent-foreground"), ECkUiFieldKind::Color},
            {TEXT("cast-shadow-foreground"), ECkUiFieldKind::Color},
            {TEXT("dynamic-shadow-foreground"), ECkUiFieldKind::Color},
            {TEXT("static-shadow-foreground"), ECkUiFieldKind::Color},
            {TEXT("volumetric-shadow-foreground"), ECkUiFieldKind::Color},
            {TEXT("receives-decals-foreground"), ECkUiFieldKind::Color},
            {TEXT("static-lighting-foreground"), ECkUiFieldKind::Color},
        };
    }


}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_UvDensityPage::
    Construct(
        const FArguments&) -> void
{
    _Result.Availability = ECkTextureDebugger_UvDensityAvailability::InvalidComponent;
    _Result.UnavailableReason = TEXT("Select a checker-capable mesh component and an explicit material slot.");
    auto Registry = TSharedPtr<const FCkUiWidgetRegistrySnapshot>{};
    const auto RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT RegistryResult.Succeeded)
    {
        _PublicationError = FString::Join(RegistryResult.Errors, TEXT("\n"));
        ChildSlot[SNew(STextBlock).Text(Get_LayoutError())];
        return;
    }

    const auto WeakPage = TWeakPtr<SCkTextureDebugger_UvDensityPage>{SharedThis(this)};
    auto Tokens = FCkUiView::FTokens{};
    Tokens.Add(TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS));
    Tokens.Add(TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM));
    Tokens.Add(TEXT("--space-l"), FString::SanitizeFloat(CkStyle::SpaceL));
    Tokens.Add(TEXT("--text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--text-strong"), TEXT("#") + CkStyle::TextStrong().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--text-dim"), TEXT("#") + CkStyle::TextDim().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--surface"), TEXT("#") + CkStyle::Bg2().ToFColorSRGB().ToHex());

    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("purpose"), LOCTEXT("UvPurpose", "Measures texels per centimetre only when the selected triangle, UV area, texture binding, texture transform, and cooked dimensions are authoritative. Missing proof is reported instead of estimated."));
    Data.Text.Add(TEXT("component-label"), LOCTEXT("UvComponent", "Component"));
    Data.Text.Add(TEXT("slot-label"), LOCTEXT("UvSlot", "Material slot"));
    Data.Text.Add(TEXT("channel-label"), LOCTEXT("UvChannel", "UV channel"));
    Data.Text.Add(TEXT("triangle-label"), LOCTEXT("UvTriangle", "Triangle / section"));
    Data.Text.Add(TEXT("texture-label"), LOCTEXT("UvTexture", "Selected texture"));
    Data.Text.Add(TEXT("channel-value"), FText::FromString(TEXT("UV0")));
    Data.Text.Add(TEXT("triangle-value"), FText::FromString(TEXT("Unavailable — no authoritative triangle mapping")));
    Data.Text.Add(TEXT("result-unit"), LOCTEXT("TexelsPerCm", "Texels / cm"));
    Data.Text.Add(TEXT("component"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Get_ComponentContextText));
    Data.Text.Add(TEXT("slot"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Get_SlotContextText));
    Data.Text.Add(TEXT("texture"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Get_TextureContextText));
    Data.Text.Add(TEXT("inputs-title"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Get_InputsTitleText));
    Data.Text.Add(TEXT("result-title"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Get_ResultTitleText));
    Data.Text.Add(TEXT("result-status"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Get_ResultStatusText));
    Data.Text.Add(TEXT("result-value"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Get_ResultValueText));
    Data.Text.Add(TEXT("result-explanation"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Get_ResultExplanationText));
    Data.Color.Add(TEXT("component-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        const auto Tone = Page.IsValid() && Page->_Component.IsSet() && Page->_Component->NavigationTarget.IsValid() ? ECk_Tone::Info : ECk_Tone::Neutral;
        return CkStyle::GetToneColor(Tone);
    }));
    Data.Color.Add(TEXT("component-background"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        const auto Tone = Page.IsValid() && Page->_Component.IsSet() && Page->_Component->NavigationTarget.IsValid() ? ECk_Tone::Info : ECk_Tone::Neutral;
        return CkStyle::GetToneDimColor(Tone);
    }));
    Data.Color.Add(TEXT("slot-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        const auto Tone = Page.IsValid() && NOT Page->_ExplicitSlotIndices.IsEmpty() ? ECk_Tone::Accent : ECk_Tone::Neutral;
        return CkStyle::GetToneColor(Tone);
    }));
    Data.Color.Add(TEXT("slot-background"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        const auto Tone = Page.IsValid() && NOT Page->_ExplicitSlotIndices.IsEmpty() ? ECk_Tone::Accent : ECk_Tone::Neutral;
        return CkStyle::GetToneDimColor(Tone);
    }));
    Data.Color.Add(TEXT("texture-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        const auto Tone = Page.IsValid() && Page->_SelectedTexture.IsSet() ? ECk_Tone::Info : ECk_Tone::Neutral;
        return CkStyle::GetToneColor(Tone);
    }));
    Data.Color.Add(TEXT("texture-background"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        const auto Tone = Page.IsValid() && Page->_SelectedTexture.IsSet() ? ECk_Tone::Info : ECk_Tone::Neutral;
        return CkStyle::GetToneDimColor(Tone);
    }));
    Data.Color.Add(TEXT("result-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return CkStyle::GetToneColor(Page.IsValid() ? Page->Get_ResultTone() : ECk_Tone::Neutral);
    }));
    Data.Color.Add(TEXT("result-background"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return CkStyle::GetToneDimColor(Page.IsValid() ? Page->Get_ResultTone() : ECk_Tone::Neutral);
    }));
    Data.Visibility.Add(TEXT("inputs-expanded"), TAttribute<bool>::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Are_InputsExpanded));
    Data.Visibility.Add(TEXT("result-expanded"), TAttribute<bool>::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Is_ResultExpanded));

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("toggle-inputs"), FSimpleDelegate::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Toggle_InputsExpanded));
    Actions.Add(TEXT("toggle-result"), FSimpleDelegate::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Toggle_ResultExpanded));
    _LayoutView = FCkUiView::Create({}, MoveTemp(Actions), MoveTemp(Tokens), CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    ChildSlot[SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(STextBlock)
            .Tag(TEXT("Ck.UvDensity.LayoutError"))
            .Text(this, &SCkTextureDebugger_UvDensityPage::Get_LayoutError)
            .ToolTipText(this, &SCkTextureDebugger_UvDensityPage::Get_LayoutError)
            .AutoWrapText(true)
            .ColorAndOpacity(FSlateColor{CkStyle::Err()})
            .Visibility_Lambda([WeakPage]
            {
                const auto Page = WeakPage.Pin();
                return NOT Page.IsValid() || Page->Get_LayoutError().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
            })]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [_LayoutView->GetRegion(TEXT("main"))]];

    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    const auto UiDirectory = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    Reload_LayoutFiles(FPaths::Combine(UiDirectory, TEXT("UvDensity.ui.html")), FPaths::Combine(UiDirectory, TEXT("UvDensity.ui.css")));
    RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SCkTextureDebugger_UvDensityPage::Tick_LayoutFiles));
}

auto
    SCkTextureDebugger_UvDensityPage::
    TryReload_Layout(
        const FString& InMarkup,
        const FString& InStylesheet)
    -> FCkUiLoadResult
{
    if (NOT _LayoutView.IsValid()) { return {false, {TEXT("Authored layout is disabled for UV & Density.")}}; }
    return _LayoutView->TryReload(InMarkup, InStylesheet, TEXT("UvDensity"));
}

auto
    SCkTextureDebugger_UvDensityPage::
    Reload_LayoutFiles(
        const FString& InMarkupPath,
        const FString& InStylesheetPath)
    -> FCkUiLoadResult
{
    if (NOT _LayoutView.IsValid()) { return {false, {TEXT("Authored layout is disabled for UV & Density.")}}; }
    return _LayoutView->ReloadFiles(InMarkupPath, InStylesheetPath);
}

auto
    SCkTextureDebugger_UvDensityPage::
    Poll_LayoutFiles()
    -> bool
{
    return _LayoutView.IsValid() && _LayoutView->PollFiles();
}

auto
    SCkTextureDebugger_UvDensityPage::
    Get_LayoutRevision() const
    -> int64
{
    return _LayoutView.IsValid() ? _LayoutView->GetRevision() : 0;
}

auto
    SCkTextureDebugger_UvDensityPage::
    Get_LayoutError() const
    -> FText
{
    if (NOT _PublicationError.IsEmpty()) { return FText::FromString(_PublicationError); }
    return NOT _LayoutView.IsValid() || _LayoutView->GetLastResult().Succeeded
        ? FText::GetEmpty()
        : FText::FromString(FString::Join(_LayoutView->GetLastResult().Errors, TEXT("\n")));
}

auto
    SCkTextureDebugger_UvDensityPage::
    Tick_LayoutFiles(
        double,
        float)
    -> EActiveTimerReturnType
{
    Poll_LayoutFiles();
    return EActiveTimerReturnType::Continue;
}

auto
    SCkTextureDebugger_UvDensityPage::
    Set_Context(
        TOptional<FCkTextureDebugger_ComponentRow> InComponent,
        TOptional<FCkTextureDebugger_TextureHealthSelection> InSelectedTexture,
        TArray<int32> InExplicitSlotIndices) -> void
{
    _Component = MoveTemp(InComponent);
    _SelectedTexture = MoveTemp(InSelectedTexture);
    _ExplicitSlotIndices = ck_texture_debugger_diagnostic_pages::NormalizeSlots(MoveTemp(InExplicitSlotIndices));
    Refresh_Result();
}

auto SCkTextureDebugger_UvDensityPage::Refresh_Result() -> void
{
    if (NOT _Component.IsSet())
    {
        _Result = {};
        _Result.Availability = ECkTextureDebugger_UvDensityAvailability::InvalidComponent;
        _Result.UnavailableReason = TEXT("No component is selected.");
        return;
    }

    if (_ExplicitSlotIndices.IsEmpty())
    {
        _Result = {};
        _Result.Availability = ECkTextureDebugger_UvDensityAvailability::InvalidMaterialSlot;
        _Result.UnavailableReason = TEXT("No explicit material slot is selected.");
        return;
    }

    if (NOT _SelectedTexture.IsSet())
    {
        _Result = {};
        _Result.Availability = ECkTextureDebugger_UvDensityAvailability::UnprovenTextureBinding;
        _Result.UnavailableReason = TEXT("No texture is selected from Texture Health.");
        return;
    }

    if (_SelectedTexture->Health.CookedWidth <= 0 || _SelectedTexture->Health.CookedHeight <= 0)
    {
        _Result = {};
        _Result.Availability = ECkTextureDebugger_UvDensityAvailability::InvalidTextureDimensions;
        _Result.UnavailableReason = TEXT("The selected texture has no usable cooked dimensions.");
        return;
    }

    auto* Component = _Component->NavigationTarget.Get();
    if ((_SelectedTexture->Component.IsValid() && _SelectedTexture->Component.Get() != Component) ||
        (_SelectedTexture->SlotIndex != INDEX_NONE && _SelectedTexture->SlotIndex != _ExplicitSlotIndices[0]))
    {
        _Result = {};
        _Result.Availability = ECkTextureDebugger_UvDensityAvailability::UnprovenTextureBinding;
        _Result.UnavailableReason = TEXT("The selected texture comes from a different component or material slot.");
        return;
    }

    auto* MeshComponent = Cast<UMeshComponent>(Component);
    _Result = ck::texture_debugger::uv_density::InspectComponentCapability(
        MeshComponent, _ExplicitSlotIndices[0], 0, INDEX_NONE, INDEX_NONE);
}

auto SCkTextureDebugger_UvDensityPage::Get_ComponentContextText() const -> FText
{
    return _Component.IsSet()
        ? FText::FromString(FString::Printf(TEXT("%s · %s"), *_Component->ActorDisplayName, *_Component->ComponentDisplayName))
        : LOCTEXT("NoUvComponent", "No component");
}

auto SCkTextureDebugger_UvDensityPage::Get_SlotContextText() const -> FText
{
    if (_ExplicitSlotIndices.IsEmpty()) { return LOCTEXT("NoUvSlot", "No slot"); }
    const auto Slots = FString::JoinBy(_ExplicitSlotIndices, TEXT(", "), [](int32 InSlot) { return FString::FromInt(InSlot); });
    return FText::FromString(FString::Printf(TEXT("Slot %s"), *Slots));
}

auto SCkTextureDebugger_UvDensityPage::Get_TextureContextText() const -> FText
{
    return _SelectedTexture.IsSet() && NOT _SelectedTexture->DisplayName.IsEmpty()
        ? FText::FromString(_SelectedTexture->DisplayName)
        : LOCTEXT("NoUvTexture", "No texture selected");
}

auto SCkTextureDebugger_UvDensityPage::Get_InputsTitleText() const -> FText
{
    return _InputsExpanded
        ? LOCTEXT("UvInputs", "Measurement inputs")
        : LOCTEXT("ExpandMeasurementInputs", "Measurement inputs (collapsed)");
}

auto SCkTextureDebugger_UvDensityPage::Get_ResultTitleText() const -> FText
{
    return _ResultExpanded
        ? LOCTEXT("UvResult", "Authoritative result")
        : LOCTEXT("ExpandAuthoritativeResult", "Authoritative result (collapsed)");
}

auto SCkTextureDebugger_UvDensityPage::Get_ResultStatusText() const -> FText
{
    return _Result.Availability == ECkTextureDebugger_UvDensityAvailability::Available
        ? LOCTEXT("UvMeasured", "MEASURED")
        : LOCTEXT("UvMissingProof", "MISSING PREREQUISITE");
}

auto SCkTextureDebugger_UvDensityPage::Get_ResultTone() const -> ECk_Tone
{
    return _Result.Availability == ECkTextureDebugger_UvDensityAvailability::Available
        ? ECk_Tone::Ok
        : ECk_Tone::Warn;
}

auto SCkTextureDebugger_UvDensityPage::Get_ResultValueText() const -> FText
{
    return _Result.Availability == ECkTextureDebugger_UvDensityAvailability::Available
        ? FText::AsNumber(_Result.TexelsPerCm, &FNumberFormattingOptions::DefaultWithGrouping())
        : FText::FromString(TEXT("—"));
}

auto SCkTextureDebugger_UvDensityPage::Get_ResultExplanationText() const -> FText
{
    return _Result.Availability == ECkTextureDebugger_UvDensityAvailability::Available
        ? LOCTEXT("UvMeasuredExplanation", "The value is backed by authoritative selected-triangle geometry, UV, texture binding, transform, and cooked dimensions.")
        : FText::FromString(_Result.UnavailableReason.IsEmpty()
            ? TEXT("An authoritative prerequisite is unavailable.")
            : _Result.UnavailableReason);
}

auto SCkTextureDebugger_UvDensityPage::Are_InputsExpanded() const -> bool
{
    return _InputsExpanded;
}

auto SCkTextureDebugger_UvDensityPage::Is_ResultExpanded() const -> bool
{
    return _ResultExpanded;
}

auto SCkTextureDebugger_UvDensityPage::Toggle_InputsExpanded() -> void
{
    _InputsExpanded = NOT _InputsExpanded;
}

auto SCkTextureDebugger_UvDensityPage::Toggle_ResultExpanded() -> void
{
    _ResultExpanded = NOT _ResultExpanded;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_MaterialInputsPage::
    Construct(
        const FArguments&) -> void
{
    using namespace ck_texture_debugger_diagnostic_pages;
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const FCkUiLoadResult CollectionResult = FCkUiCollection::TryCreate(MaterialInputsUiSchema(), _UiCollection);
    if (!RegistryResult.Succeeded || !CollectionResult.Succeeded)
    {
        _PublicationError = FString::Join(RegistryResult.Errors, TEXT("\n")) + FString::Join(CollectionResult.Errors, TEXT("\n"));
        _UiCollection.Reset();
        ChildSlot[SNew(STextBlock).Text(Get_LayoutError())];
        return;
    }
    const auto WeakPage = TWeakPtr<SCkTextureDebugger_MaterialInputsPage>{SharedThis(this)};
    auto Tokens = FCkUiView::FTokens{};
    Tokens.Add(TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS));
    Tokens.Add(TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM));
    Tokens.Add(TEXT("--text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--surface"), TEXT("#") + CkStyle::Bg2().ToFColorSRGB().ToHex());
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("filter-hint"), LOCTEXT("MaterialFilter", "Filter parameter, texture, slot, or provenance…"));
    Data.Text.Add(TEXT("highlight-hint"), LOCTEXT("MaterialHighlight", "Highlight matches…"));
    Data.Text.Add(TEXT("parameter-header"), LOCTEXT("ParameterColumn", "Parameter"));
    Data.Text.Add(TEXT("texture-header"), LOCTEXT("TextureColumn", "Texture"));
    Data.Text.Add(TEXT("provenance-header"), LOCTEXT("ProvenanceColumn", "Provenance"));
    Data.Text.Add(TEXT("slot-header"), LOCTEXT("SlotColumn", "Slot"));
    Data.Text.Add(TEXT("variant-header"), LOCTEXT("VariantColumn", "Quality / platform"));
    Data.Text.Add(TEXT("purpose"), LOCTEXT("MaterialPurpose",
        "Shows texture parameters resolved through the active material-instance chain and separately labels active-quality/platform used textures as potential references. Potential rows do not claim a sampler or slot binding."));
    Data.Text.Add(TEXT("component"), TAttribute<FText>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid() && Page->_Component.IsSet()
            ? FText::FromString(FString::Printf(TEXT("%s · %s"), *Page->_Component->ActorDisplayName, *Page->_Component->ComponentDisplayName))
            : LOCTEXT("MaterialContextNone", "No component");
    }));
    Data.Text.Add(TEXT("texture-context"), TAttribute<FText>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid() && Page->_SelectedTexture.IsSet() && !Page->_SelectedTexture->DisplayName.IsEmpty()
            ? FText::FromString(FString::Printf(TEXT("Selected texture · %s"), *Page->_SelectedTexture->DisplayName))
            : LOCTEXT("MaterialTextureNone", "Selected texture · none");
    }));
    Data.Text.Add(TEXT("filter"), TAttribute<FText>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid() ? FText::FromString(Page->_FilterText) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("highlight"), TAttribute<FText>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid() ? FText::FromString(Page->_HighlightText) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("count"), TAttribute<FText>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid()
            ? FText::Format(LOCTEXT("MaterialRowCountFormat", "{0}/{1} {2}"), Page->_VisibleRows.Num(), Page->_AllRows.Num(), LOCTEXT("MaterialRows", "rows"))
            : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("empty-state"), TAttribute<FText>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid() ? Page->Get_EmptyStateText() : FText::GetEmpty();
    }));
    Data.Visibility.Add(TEXT("material-inputs-empty"), TAttribute<bool>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid() && Page->_VisibleRows.IsEmpty();
    }));
    Data.Color.Add(TEXT("component-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        const bool HasLiveComponent = Page.IsValid() && Page->_Component.IsSet() && Page->_Component->NavigationTarget.IsValid();
        return CkStyle::GetToneColor(HasLiveComponent ? ECk_Tone::Info : ECk_Tone::Neutral);
    }));
    Data.Color.Add(TEXT("component-background"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        const bool HasLiveComponent = Page.IsValid() && Page->_Component.IsSet() && Page->_Component->NavigationTarget.IsValid();
        return CkStyle::GetToneDimColor(HasLiveComponent ? ECk_Tone::Info : ECk_Tone::Neutral);
    }));
    Data.Color.Add(TEXT("texture-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return CkStyle::GetToneColor(Page.IsValid() && Page->_SelectedTexture.IsSet() ? ECk_Tone::Accent : ECk_Tone::Neutral);
    }));
    Data.Color.Add(TEXT("texture-background"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return CkStyle::GetToneDimColor(Page.IsValid() && Page->_SelectedTexture.IsSet() ? ECk_Tone::Accent : ECk_Tone::Neutral);
    }));
    Data.Collections.Add(TEXT("material-inputs"), _UiCollection);
    Data.TextChanged.Add(TEXT("filter"), FOnTextChanged::CreateLambda([WeakPage](const FText& Text)
    {
        if (const auto Page = WeakPage.Pin()) { Page->OnFilterTextChanged(Text.ToString()); }
    }));
    Data.TextChanged.Add(TEXT("highlight"), FOnTextChanged::CreateLambda([WeakPage](const FText& Text)
    {
        if (const auto Page = WeakPage.Pin()) { Page->OnHighlightTextChanged(Text.ToString()); }
    }));
    _LayoutView = FCkUiView::Create({}, {}, MoveTemp(Tokens), CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Tag(TEXT("Ck.MaterialInputs.LayoutError"))
            .Text(this, &SCkTextureDebugger_MaterialInputsPage::Get_LayoutError)
            .AutoWrapText(true)
            .ColorAndOpacity(FSlateColor{CkStyle::Err()})
            .Visibility_Lambda([WeakPage]
            {
                const auto Page = WeakPage.Pin();
                return !Page.IsValid() || Page->Get_LayoutError().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
            })
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            _LayoutView->GetRegion(TEXT("main"))
        ]
    ];
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    const auto UiDirectory = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    Reload_LayoutFiles(FPaths::Combine(UiDirectory, TEXT("MaterialInputs.ui.html")), FPaths::Combine(UiDirectory, TEXT("MaterialInputs.ui.css")));
    RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SCkTextureDebugger_MaterialInputsPage::Tick_LayoutFiles));
}

auto
    SCkTextureDebugger_MaterialInputsPage::
    Set_Context(
        TOptional<FCkTextureDebugger_ComponentRow> InComponent,
        TOptional<FCkTextureDebugger_TextureHealthSelection> InSelectedTexture,
        TArray<int32> InExplicitSlotIndices) -> void
{
    _Component = MoveTemp(InComponent);
    _SelectedTexture = MoveTemp(InSelectedTexture);
    _ExplicitSlotIndices = ck_texture_debugger_diagnostic_pages::NormalizeSlots(MoveTemp(InExplicitSlotIndices));
    Rebuild_Rows();
}

auto SCkTextureDebugger_MaterialInputsPage::TryReload_Layout(const FString& InMarkup, const FString& InStylesheet) -> FCkUiLoadResult
{
    return _LayoutView.IsValid() ? _LayoutView->TryReload(InMarkup, InStylesheet, TEXT("MaterialInputs"))
                                 : FCkUiLoadResult{false, {TEXT("Authored layout is disabled for Material Inputs.")}};
}

auto SCkTextureDebugger_MaterialInputsPage::Reload_LayoutFiles(const FString& InMarkupPath, const FString& InStylesheetPath) -> FCkUiLoadResult
{
    return _LayoutView.IsValid() ? _LayoutView->ReloadFiles(InMarkupPath, InStylesheetPath)
                                 : FCkUiLoadResult{false, {TEXT("Authored layout is disabled for Material Inputs.")}};
}

auto SCkTextureDebugger_MaterialInputsPage::Poll_LayoutFiles() -> bool { return _LayoutView.IsValid() && _LayoutView->PollFiles(); }
auto SCkTextureDebugger_MaterialInputsPage::Get_LayoutRevision() const -> int64 { return _LayoutView.IsValid() ? _LayoutView->GetRevision() : 0; }
auto SCkTextureDebugger_MaterialInputsPage::Get_LayoutError() const -> FText
{
    if (!_PublicationError.IsEmpty()) { return FText::FromString(_PublicationError); }
    return !_LayoutView.IsValid() || _LayoutView->GetLastResult().Succeeded ? FText::GetEmpty() : FText::FromString(FString::Join(_LayoutView->GetLastResult().Errors, TEXT("\n")));
}
auto SCkTextureDebugger_MaterialInputsPage::Get_AuthoredTable() const -> TSharedPtr<SCkUiTable>
{
    return _LayoutView.IsValid() ? _LayoutView->GetTable(TEXT("material-inputs-control")) : nullptr;
}
auto SCkTextureDebugger_MaterialInputsPage::Tick_LayoutFiles(double, float) -> EActiveTimerReturnType
{
    Poll_LayoutFiles();
    return EActiveTimerReturnType::Continue;
}

auto SCkTextureDebugger_MaterialInputsPage::Rebuild_Rows() -> void
{
    if (NOT _Component.IsSet() || NOT _Component->NavigationTarget.IsValid())
    {
        _AllRows.Reset();
        Apply_Search();
        return;
    }

    auto Existing = TMap<FString, TSharedPtr<FRow>>{};
    for (const auto& Row : _AllRows)
    {
        if (Row.IsValid()) { Existing.Add(Row->StableKey, Row); }
    }

    auto NewRows = TArray<TSharedPtr<FRow>>{};
    for (const auto SlotIndex : _ExplicitSlotIndices)
    {
        const auto* Slot = ck_texture_debugger_diagnostic_pages::FindSlot(_Component, SlotIndex);
        if (Slot == nullptr) { continue; }

        const auto Analysis = ck::texture_debugger::material_analysis::Analyze(Slot->NavigationTarget.Get());
        for (const auto& AnalysisRow : Analysis.Rows)
        {
            const auto Parameter = AnalysisRow.ParameterInfo.Name.IsNone()
                ? FString{TEXT("—")}
                : AnalysisRow.ParameterInfo.Association == EMaterialParameterAssociation::GlobalParameter
                    ? AnalysisRow.ParameterInfo.Name.ToString()
                    : FString::Printf(TEXT("%s [%d:%d]"),
                        *AnalysisRow.ParameterInfo.Name.ToString(),
                        static_cast<int32>(AnalysisRow.ParameterInfo.Association),
                        AnalysisRow.ParameterInfo.Index);
            const auto Texture = AnalysisRow.DisplayName.IsEmpty()
                ? FString{TEXT("Unavailable")}
                : AnalysisRow.DisplayName;
            const auto Provenance = ck_texture_debugger_diagnostic_pages::ProvenanceText(AnalysisRow.Provenance);
            const auto Variant = FString::Printf(TEXT("%s · %s"),
                *LexToString(Analysis.ActiveQualityLevel),
                *LexToString(Analysis.ActiveShaderPlatform, false));
            const auto StableKey = FString::Printf(TEXT("%d|%s|%d|%d|%d|%s|%s|%s"),
                SlotIndex,
                *Slot->MaterialPath.ToString(),
                static_cast<int32>(AnalysisRow.Provenance),
                static_cast<int32>(AnalysisRow.ParameterInfo.Association),
                AnalysisRow.ParameterInfo.Index,
                *Parameter,
                *AnalysisRow.TexturePath.ToString(),
                *Texture);

            auto Presentation = TSharedPtr<FRow>{};
            if (const auto* Found = Existing.Find(StableKey))
            {
                Presentation = *Found;
                Existing.Remove(StableKey);
            }
            else
            {
                Presentation = MakeShared<FRow>();
            }

            Presentation->StableKey = StableKey;
            Presentation->Parameter = Parameter;
            Presentation->Texture = Texture;
            Presentation->TexturePath = AnalysisRow.TexturePath.ToString();
            Presentation->Provenance = Provenance;
            Presentation->Detail = AnalysisRow.UnavailableReason;
            Presentation->Slot = FString::Printf(TEXT("%d · %s"), SlotIndex, *Slot->DisplayName);
            Presentation->Variant = Variant;
            Presentation->SlotIndex = SlotIndex;
            Presentation->ProvenanceKind = AnalysisRow.Provenance;
            Presentation->SearchText = FString::Printf(TEXT("%s %s %s %s %s %s %s"),
                *Presentation->Parameter,
                *Presentation->Texture,
                *Presentation->TexturePath,
                *Presentation->Provenance,
                *Presentation->Detail,
                *Presentation->Slot,
                *Presentation->Variant);
            NewRows.Add(MoveTemp(Presentation));
        }
    }

    NewRows.Sort([](const TSharedPtr<FRow>& InLeft, const TSharedPtr<FRow>& InRight)
    {
        if (InLeft->SlotIndex != InRight->SlotIndex) { return InLeft->SlotIndex < InRight->SlotIndex; }
        if (InLeft->Parameter != InRight->Parameter) { return InLeft->Parameter < InRight->Parameter; }
        if (InLeft->Texture != InRight->Texture) { return InLeft->Texture < InRight->Texture; }
        if (InLeft->ProvenanceKind != InRight->ProvenanceKind)
        { return static_cast<uint8>(InLeft->ProvenanceKind) < static_cast<uint8>(InRight->ProvenanceKind); }
        return InLeft->StableKey < InRight->StableKey;
    });

    _AllRows = MoveTemp(NewRows);
    Apply_Search();
}

auto SCkTextureDebugger_MaterialInputsPage::Apply_Search() -> void
{
    auto NewVisible = TArray<TSharedPtr<FRow>>{};
    for (const auto& Row : _AllRows)
    {
        if (NOT Row.IsValid()) { continue; }
        if (NOT ck_texture_debugger_diagnostic_pages::MatchesQuery(Row->SearchText, _FilterText)) { continue; }

        Row->IsHighlighted = NOT _HighlightText.IsEmpty() &&
            ck_texture_debugger_diagnostic_pages::MatchesQuery(Row->SearchText, _HighlightText);
        Row->IsDimmed = NOT _HighlightText.IsEmpty() && NOT Row->IsHighlighted;
        NewVisible.Add(Row);
    }

    _VisibleRows = MoveTemp(NewVisible);
    Publish_Rows();
}

auto SCkTextureDebugger_MaterialInputsPage::OnFilterTextChanged(const FString& InText) -> void
{
    if (_FilterText == InText) { return; }
    _FilterText = InText;
    Apply_Search();
}

auto SCkTextureDebugger_MaterialInputsPage::OnHighlightTextChanged(const FString& InText) -> void
{
    if (_HighlightText == InText) { return; }
    _HighlightText = InText;
    Apply_Search();
}

auto SCkTextureDebugger_MaterialInputsPage::Get_EmptyStateText() const -> FText
{
    if (NOT _Component.IsSet())
    { return LOCTEXT("MaterialNoComponent", "Select a component to inspect its active material inputs."); }
    if (NOT _Component->NavigationTarget.IsValid())
    { return LOCTEXT("MaterialStaleComponent", "The selected component is no longer live."); }
    if (_ExplicitSlotIndices.IsEmpty())
    { return LOCTEXT("MaterialNoSlots", "Select at least one explicit material slot."); }
    if (NOT _FilterText.IsEmpty() && _VisibleRows.IsEmpty())
    { return LOCTEXT("MaterialNoMatches", "No material inputs match the current filter."); }
    return LOCTEXT("MaterialNoRows", "No runtime material-input rows are available for the selected slots.");
}

auto SCkTextureDebugger_MaterialInputsPage::Publish_Rows() -> void
{
    if (!_UiCollection.IsValid()) { return; }
    auto Records = TArray<FCkUiRecordData>{};
    Records.Reserve(_VisibleRows.Num());
    for (const TSharedPtr<FRow>& Row : _VisibleRows)
    {
        if (Row.IsValid()) { Records.Add(ck_texture_debugger_diagnostic_pages::MaterialInputsUiRecord(*Row)); }
    }
    const FCkUiLoadResult Result = _UiCollection->TrySetRecords(MoveTemp(Records));
    if (!Result.Succeeded)
    { _PublicationError = FString::Join(Result.Errors, TEXT("\n")); }
    else
    { _PublicationError.Reset(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_SurfaceLightingPage::
    Construct(
        const FArguments&) -> void
{
    using namespace ck_texture_debugger_diagnostic_pages;
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const FCkUiLoadResult CollectionResult = FCkUiCollection::TryCreate(SurfaceLightingUiSchema(), _UiCollection);
    if (NOT RegistryResult.Succeeded || NOT CollectionResult.Succeeded)
    {
        _PublicationError = FString::Join(RegistryResult.Errors, TEXT("\n")) + FString::Join(CollectionResult.Errors, TEXT("\n"));
        _UiCollection.Reset();
        ChildSlot[SNew(STextBlock).Text(Get_LayoutError())];
        return;
    }

    const auto WeakPage = TWeakPtr<SCkTextureDebugger_SurfaceLightingPage>{SharedThis(this)};
    auto Tokens = FCkUiView::FTokens{};
    Tokens.Add(TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS));
    Tokens.Add(TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM));
    Tokens.Add(TEXT("--space-l"), FString::SanitizeFloat(CkStyle::SpaceL));
    Tokens.Add(TEXT("--text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--text-strong"), TEXT("#") + CkStyle::TextStrong().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--text-dim"), TEXT("#") + CkStyle::TextDim().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--surface"), TEXT("#") + CkStyle::Bg2().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--info"), TEXT("#") + CkStyle::Info().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--warn"), TEXT("#") + CkStyle::Warn().ToFColorSRGB().ToHex());

    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("purpose"), LOCTEXT("SurfacePurpose",
        "Reports public runtime material and component facts for the selected slots. These facts do not diagnose Lumen, VSM, light leaks, blurry textures, or final rendered appearance."));
    Data.Text.Add(TEXT("material-facts-label"), LOCTEXT("MaterialFacts", "Material facts"));
    Data.Text.Add(TEXT("two-sided-label"), LOCTEXT("TwoSided", "Two-sided"));
    Data.Text.Add(TEXT("masked-label"), LOCTEXT("Masked", "Masked"));
    Data.Text.Add(TEXT("translucent-label"), LOCTEXT("Translucent", "Translucent"));
    Data.Text.Add(TEXT("lighting-facts-label"), LOCTEXT("LightingFacts", "Component lighting and shadow facts"));
    Data.Text.Add(TEXT("cast-shadow-label"), LOCTEXT("CastShadow", "Cast shadow"));
    Data.Text.Add(TEXT("dynamic-shadow-label"), LOCTEXT("DynamicShadow", "Dynamic shadow"));
    Data.Text.Add(TEXT("static-shadow-label"), LOCTEXT("StaticShadow", "Static shadow"));
    Data.Text.Add(TEXT("volumetric-shadow-label"), LOCTEXT("VolumetricShadow", "Volumetric translucent shadow"));
    Data.Text.Add(TEXT("receives-decals-label"), LOCTEXT("ReceivesDecals", "Receives decals"));
    Data.Text.Add(TEXT("static-lighting-label"), LOCTEXT("StaticLighting", "Has static lighting"));
    Data.Text.Add(TEXT("opacity-label"), LOCTEXT("OpacityClip", "Opacity mask clip"));
    Data.Text.Add(TEXT("lightmap-label"), LOCTEXT("Lightmap", "Lightmap resolution"));
    Data.Text.Add(TEXT("nanite-label"), LOCTEXT("Nanite", "Nanite data"));
    Data.Text.Add(TEXT("component"), TAttribute<FText>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid() && Page->_Component.IsSet()
            ? FText::FromString(FString::Printf(TEXT("%s · %s"), *Page->_Component->ActorDisplayName, *Page->_Component->ComponentDisplayName))
            : LOCTEXT("SurfaceContextNone", "No component");
    }));
    Data.Text.Add(TEXT("texture-context"), TAttribute<FText>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid() && Page->_SelectedTexture.IsSet() && NOT Page->_SelectedTexture->DisplayName.IsEmpty()
            ? FText::FromString(FString::Printf(TEXT("Selected texture · %s"), *Page->_SelectedTexture->DisplayName))
            : LOCTEXT("SurfaceTextureNone", "Selected texture · none");
    }));
    Data.Text.Add(TEXT("empty-state"), TAttribute<FText>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid() ? Page->Get_EmptyStateText() : FText::GetEmpty();
    }));
    Data.Visibility.Add(TEXT("surface-lighting-empty"), TAttribute<bool>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return Page.IsValid() && Page->_SlotFacts.IsEmpty();
    }));
    Data.Color.Add(TEXT("component-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return CkStyle::GetToneColor(Page.IsValid() && Page->_Component.IsSet() && Page->_Component->NavigationTarget.IsValid() ? ECk_Tone::Info : ECk_Tone::Neutral);
    }));
    Data.Color.Add(TEXT("component-background"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return CkStyle::GetToneDimColor(Page.IsValid() && Page->_Component.IsSet() && Page->_Component->NavigationTarget.IsValid() ? ECk_Tone::Info : ECk_Tone::Neutral);
    }));
    Data.Color.Add(TEXT("texture-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return CkStyle::GetToneColor(Page.IsValid() && Page->_SelectedTexture.IsSet() ? ECk_Tone::Accent : ECk_Tone::Neutral);
    }));
    Data.Color.Add(TEXT("texture-background"), TAttribute<FLinearColor>::CreateLambda([WeakPage]
    {
        const auto Page = WeakPage.Pin();
        return CkStyle::GetToneDimColor(Page.IsValid() && Page->_SelectedTexture.IsSet() ? ECk_Tone::Accent : ECk_Tone::Neutral);
    }));
    Data.Collections.Add(TEXT("surface-lighting"), _UiCollection);
    Data.ItemActions.Add(TEXT("toggle-slot"), FCkUiOnItemAction::CreateLambda([WeakPage](const FString& InStableKey)
    {
        if (const auto Page = WeakPage.Pin()) { Page->Toggle_Slot(InStableKey); }
    }));
    _LayoutView = FCkUiView::Create({}, {}, MoveTemp(Tokens), CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Tag(TEXT("Ck.SurfaceLighting.LayoutError"))
            .Text(this, &SCkTextureDebugger_SurfaceLightingPage::Get_LayoutError)
            .AutoWrapText(true)
            .ColorAndOpacity(FSlateColor{CkStyle::Err()})
            .Visibility_Lambda([WeakPage]
            {
                const auto Page = WeakPage.Pin();
                return NOT Page.IsValid() || Page->Get_LayoutError().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
            })
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            _LayoutView->GetRegion(TEXT("main"))
        ]
    ];
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    const auto UiDirectory = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    Reload_LayoutFiles(FPaths::Combine(UiDirectory, TEXT("SurfaceLighting.ui.html")), FPaths::Combine(UiDirectory, TEXT("SurfaceLighting.ui.css")));
    RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SCkTextureDebugger_SurfaceLightingPage::Tick_LayoutFiles));
}

auto
    SCkTextureDebugger_SurfaceLightingPage::
    Set_Context(
        TOptional<FCkTextureDebugger_ComponentRow> InComponent,
        TOptional<FCkTextureDebugger_TextureHealthSelection> InSelectedTexture,
        TArray<int32> InExplicitSlotIndices) -> void
{
    _Component = MoveTemp(InComponent);
    _SelectedTexture = MoveTemp(InSelectedTexture);
    _ExplicitSlotIndices = ck_texture_debugger_diagnostic_pages::NormalizeSlots(MoveTemp(InExplicitSlotIndices));
    Rebuild_Facts();
}

auto SCkTextureDebugger_SurfaceLightingPage::Rebuild_Facts() -> void
{
    auto Existing = TMap<FString, TSharedPtr<FSlotFacts>>{};
    for (const auto& Facts : _SlotFacts)
    {
        if (Facts.IsValid()) { Existing.Add(Facts->StableKey, Facts); }
    }

    auto NewFacts = TArray<TSharedPtr<FSlotFacts>>{};
    auto* Component = _Component.IsSet() ? _Component->NavigationTarget.Get() : nullptr;
    if (Component == nullptr)
    {
        _SlotFacts.Reset();
        Publish_Facts();
        return;
    }

    for (const auto SlotIndex : _ExplicitSlotIndices)
    {
        const auto* Slot = ck_texture_debugger_diagnostic_pages::FindSlot(_Component, SlotIndex);
        if (Slot == nullptr) { continue; }

        const auto Surface = ck::texture_debugger::surface_analysis::Describe(Component, Slot->NavigationTarget.Get());
        const auto MaterialKey = FObjectKey{Slot->NavigationTarget.Get()};
        const auto StableKey = FString::Printf(TEXT("%d|%u|%s|%s"),
            SlotIndex,
            GetTypeHash(MaterialKey),
            *Slot->MaterialPath.ToString(),
            *Slot->DisplayName);

        auto Facts = TSharedPtr<FSlotFacts>{};
        if (const auto* Found = Existing.Find(StableKey))
        {
            Facts = *Found;
            Existing.Remove(StableKey);
        }
        else
        {
            Facts = MakeShared<FSlotFacts>();
        }

        Facts->StableKey = StableKey;
        Facts->SlotIndex = SlotIndex;
        Facts->MaterialName = Slot->DisplayName.IsEmpty() ? TEXT("(empty)") : Slot->DisplayName;
        Facts->MaterialPath = Slot->MaterialPath.ToString();
        Facts->BlendMode = Surface.HasMaterial ? GetBlendModeString(Surface.BlendMode) : TEXT("Unavailable");
        Facts->ShadingModels = Surface.HasMaterial ? GetShadingModelFieldString(Surface.ShadingModels) : TEXT("Unavailable");
        Facts->LightMapResolution = Surface.LightMapResolution.IsSet()
            ? FString::Printf(TEXT("%d × %d"), Surface.LightMapResolution->X, Surface.LightMapResolution->Y)
            : TEXT("Unavailable");
        Facts->Nanite = Surface.HasNaniteData.IsSet()
            ? (Surface.HasNaniteData.GetValue() ? TEXT("Yes") : TEXT("No"))
            : TEXT("Unavailable for this component type");
        Facts->HasMaterial = Surface.HasMaterial;
        Facts->IsTwoSided = Surface.IsTwoSided;
        Facts->IsMasked = Surface.IsMasked;
        Facts->IsTranslucent = Surface.IsTranslucent;
        Facts->OpacityMaskClipValue = Surface.OpacityMaskClipValue;
        Facts->CastsShadow = Surface.CastsShadow;
        Facts->CastsDynamicShadow = Surface.CastsDynamicShadow;
        Facts->CastsStaticShadow = Surface.CastsStaticShadow;
        Facts->CastsVolumetricTranslucentShadow = Surface.CastsVolumetricTranslucentShadow;
        Facts->ReceivesDecals = Surface.ReceivesDecals;
        Facts->HasStaticLighting = Surface.HasStaticLighting;
        NewFacts.Add(MoveTemp(Facts));
    }

    NewFacts.Sort([](const TSharedPtr<FSlotFacts>& InLeft, const TSharedPtr<FSlotFacts>& InRight)
    {
        return InLeft->SlotIndex < InRight->SlotIndex;
    });

    _SlotFacts = MoveTemp(NewFacts);
    Publish_Facts();
}

auto SCkTextureDebugger_SurfaceLightingPage::TryReload_Layout(const FString& InMarkup, const FString& InStylesheet) -> FCkUiLoadResult
{
    return _LayoutView.IsValid() ? _LayoutView->TryReload(InMarkup, InStylesheet, TEXT("SurfaceLighting"))
                                 : FCkUiLoadResult{false, {TEXT("Authored layout is disabled for Surface & Lighting.")}};
}

auto SCkTextureDebugger_SurfaceLightingPage::Reload_LayoutFiles(const FString& InMarkupPath, const FString& InStylesheetPath) -> FCkUiLoadResult
{
    return _LayoutView.IsValid() ? _LayoutView->ReloadFiles(InMarkupPath, InStylesheetPath)
                                 : FCkUiLoadResult{false, {TEXT("Authored layout is disabled for Surface & Lighting.")}};
}

auto SCkTextureDebugger_SurfaceLightingPage::Poll_LayoutFiles() -> bool { return _LayoutView.IsValid() && _LayoutView->PollFiles(); }
auto SCkTextureDebugger_SurfaceLightingPage::Get_LayoutRevision() const -> int64 { return _LayoutView.IsValid() ? _LayoutView->GetRevision() : 0; }
auto SCkTextureDebugger_SurfaceLightingPage::Get_LayoutError() const -> FText
{
    if (NOT _PublicationError.IsEmpty()) { return FText::FromString(_PublicationError); }
    return NOT _LayoutView.IsValid() || _LayoutView->GetLastResult().Succeeded ? FText::GetEmpty()
        : FText::FromString(FString::Join(_LayoutView->GetLastResult().Errors, TEXT("\n")));
}

auto SCkTextureDebugger_SurfaceLightingPage::Get_AuthoredRepeat() const -> TSharedPtr<SCkUiRepeat>
{
    return _LayoutView.IsValid() ? _LayoutView->GetRepeat(TEXT("surface-lighting-control")) : nullptr;
}

auto SCkTextureDebugger_SurfaceLightingPage::Tick_LayoutFiles(double, float) -> EActiveTimerReturnType
{
    Poll_LayoutFiles();
    return EActiveTimerReturnType::Continue;
}

auto SCkTextureDebugger_SurfaceLightingPage::Toggle_Slot(const FString& InStableKey) -> void
{
    const TSharedPtr<FSlotFacts>* Found = _SlotFacts.FindByPredicate([&InStableKey](const TSharedPtr<FSlotFacts>& InFacts)
    {
        return InFacts.IsValid() && InFacts->StableKey == InStableKey;
    });
    if (Found == nullptr || NOT Found->IsValid()) { return; }
    (*Found)->Expanded = NOT (*Found)->Expanded;
    Publish_Facts();
}

auto SCkTextureDebugger_SurfaceLightingPage::Publish_Facts() -> void
{
    if (NOT _UiCollection.IsValid()) { return; }
    auto Records = TArray<FCkUiRecordData>{};
    Records.Reserve(_SlotFacts.Num());
    const auto Text = [](FCkUiRecordData& InRecord, const TCHAR* InName, const FText& InValue)
    {
        InRecord.Fields.Add(InName, FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = InValue});
    };
    const auto Color = [](FCkUiRecordData& InRecord, const TCHAR* InName, const FLinearColor& InValue)
    {
        InRecord.Fields.Add(InName, FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = InValue});
    };
    const auto Bool = [](bool InValue) -> FText { return InValue ? LOCTEXT("Enabled", "YES") : LOCTEXT("Disabled", "NO"); };
    const auto BoolColor = [](bool InValue) -> FLinearColor { return InValue ? CkStyle::Ok() : CkStyle::TextMute(); };
    for (const TSharedPtr<FSlotFacts>& Facts : _SlotFacts)
    {
        if (NOT Facts.IsValid()) { continue; }
        auto Record = FCkUiRecordData{};
        Record.Key = Facts->StableKey;
        const ECk_Tone Tone = Facts->HasMaterial ? ECk_Tone::Ok : ECk_Tone::Warn;
        Text(Record, TEXT("heading"), FText::FromString(FString::Printf(TEXT("Slot %d · %s"), Facts->SlotIndex, *Facts->MaterialName)));
        Text(Record, TEXT("material"), FText::FromString(Facts->MaterialName));
        Text(Record, TEXT("material-path"), FText::FromString(Facts->MaterialPath));
        Text(Record, TEXT("status"), Facts->HasMaterial ? LOCTEXT("SurfaceResolved", "RESOLVED") : LOCTEXT("SurfaceUnavailable", "UNAVAILABLE"));
        Text(Record, TEXT("blend"), FText::FromString(FString::Printf(TEXT("Blend · %s"), *Facts->BlendMode)));
        Text(Record, TEXT("shading"), FText::FromString(FString::Printf(TEXT("Shading · %s"), *Facts->ShadingModels)));
        Text(Record, TEXT("two-sided"), Bool(Facts->IsTwoSided));
        Text(Record, TEXT("masked"), Bool(Facts->IsMasked));
        Text(Record, TEXT("translucent"), Bool(Facts->IsTranslucent));
        Text(Record, TEXT("cast-shadow"), Bool(Facts->CastsShadow));
        Text(Record, TEXT("dynamic-shadow"), Bool(Facts->CastsDynamicShadow));
        Text(Record, TEXT("static-shadow"), Bool(Facts->CastsStaticShadow));
        Text(Record, TEXT("volumetric-shadow"), Bool(Facts->CastsVolumetricTranslucentShadow));
        Text(Record, TEXT("receives-decals"), Bool(Facts->ReceivesDecals));
        Text(Record, TEXT("static-lighting"), Bool(Facts->HasStaticLighting));
        Text(Record, TEXT("opacity-clip"), Facts->IsMasked ? FText::AsNumber(Facts->OpacityMaskClipValue) : FText::FromString(TEXT("N/A")));
        Text(Record, TEXT("lightmap"), FText::FromString(Facts->LightMapResolution));
        Text(Record, TEXT("nanite"), FText::FromString(Facts->Nanite));
        Text(Record, TEXT("caveat"), LOCTEXT("NoSurfaceDiagnosis",
            "These values are direct runtime facts. They do not prove whether a lighting artifact, shadow leak, blurry texture, or final surface appearance is correct."));
        Record.Fields.Add(TEXT("expanded"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = Facts->Expanded});
        Record.Fields.Add(TEXT("has-material"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = Facts->HasMaterial});
        Record.Fields.Add(TEXT("missing-material"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = NOT Facts->HasMaterial});
        Color(Record, TEXT("status-foreground"), CkStyle::GetToneColor(Tone));
        Color(Record, TEXT("status-background"), CkStyle::GetToneDimColor(Tone));
        Color(Record, TEXT("fact-foreground"), CkStyle::TextStrong());
        Color(Record, TEXT("fact-background"), CkStyle::Bg2());
        Color(Record, TEXT("two-sided-foreground"), BoolColor(Facts->IsTwoSided));
        Color(Record, TEXT("masked-foreground"), BoolColor(Facts->IsMasked));
        Color(Record, TEXT("translucent-foreground"), BoolColor(Facts->IsTranslucent));
        Color(Record, TEXT("cast-shadow-foreground"), BoolColor(Facts->CastsShadow));
        Color(Record, TEXT("dynamic-shadow-foreground"), BoolColor(Facts->CastsDynamicShadow));
        Color(Record, TEXT("static-shadow-foreground"), BoolColor(Facts->CastsStaticShadow));
        Color(Record, TEXT("volumetric-shadow-foreground"), BoolColor(Facts->CastsVolumetricTranslucentShadow));
        Color(Record, TEXT("receives-decals-foreground"), BoolColor(Facts->ReceivesDecals));
        Color(Record, TEXT("static-lighting-foreground"), BoolColor(Facts->HasStaticLighting));
        Records.Add(MoveTemp(Record));
    }
    const FCkUiLoadResult Result = _UiCollection->TrySetRecords(MoveTemp(Records));
    if (NOT Result.Succeeded) { _PublicationError = FString::Join(Result.Errors, TEXT("\n")); }
    else { _PublicationError.Reset(); }
}

auto SCkTextureDebugger_SurfaceLightingPage::Get_EmptyStateText() const -> FText
{
    if (NOT _Component.IsSet())
    { return LOCTEXT("SurfaceNoComponent", "Select a component to inspect surface and lighting facts."); }
    if (NOT _Component->NavigationTarget.IsValid())
    { return LOCTEXT("SurfaceStaleComponent", "The selected component is no longer live."); }
    if (_ExplicitSlotIndices.IsEmpty())
    { return LOCTEXT("SurfaceNoSlots", "Select at least one explicit material slot."); }
    return LOCTEXT("SurfaceNoFacts", "No selected slot resolves in the current component snapshot.");
}

#undef LOCTEXT_NAMESPACE
