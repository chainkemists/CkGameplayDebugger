#include "CkTextureDebugger/Window/SCkTextureDebugger_DiagnosticPages.h"

#include "CkTextureDebugger/Analysis/CkTextureDebugger_MaterialAnalysis.h"
#include "CkTextureDebugger/Analysis/CkTextureDebugger_SurfaceAnalysis.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ValuePill.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Components/MeshComponent.h"
#include "MaterialShaderType.h"
#include "RHIStrings.h"
#include "UObject/ObjectKey.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkTextureDebugger_DiagnosticPages"

namespace ck_texture_debugger_diagnostic_pages
{
    const auto ParameterColumn = FName{TEXT("Parameter")};
    const auto TextureColumn = FName{TEXT("Texture")};
    const auto ProvenanceColumn = FName{TEXT("Provenance")};
    const auto SlotColumn = FName{TEXT("Slot")};
    const auto VariantColumn = FName{TEXT("Variant")};

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

    auto MakePurposeText(const FText& InText) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush_Large())
            .BorderBackgroundColor(FSlateColor{CkStyle::Bg2()})
            .Padding(CkStyle::SpaceL)
            [
                SNew(STextBlock)
                .Text(InText)
                .AutoWrapText(true)
                .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            ];
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

    auto MakeFactRow(FText InLabel, TAttribute<FText> InValue) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_KeyValueRow)
            .KeyText(MoveTemp(InLabel))
            .ValueText(MoveTemp(InValue))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::TextStrong());
    }

    auto MakeBoolFactRow(FText InLabel, TAttribute<bool> InValue) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_KeyValueRow)
            .KeyText(MoveTemp(InLabel))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::TextStrong())
            .ValueWidget()
            [
                SNew(SCkDebug_ValuePill)
                .Value(MoveTemp(InValue))
                .Editable(false)
                .TrueText(LOCTEXT("Enabled", "YES"))
                .FalseText(LOCTEXT("Disabled", "NO"))
            ];
    }

    class SMaterialInputRow final
        : public SMultiColumnTableRow<TSharedPtr<SCkTextureDebugger_MaterialInputsPage::FRow>>
    {
    public:
        SLATE_BEGIN_ARGS(SMaterialInputRow) {}
            SLATE_ARGUMENT(TSharedPtr<SCkTextureDebugger_MaterialInputsPage::FRow>, Row)
        SLATE_END_ARGS()

        auto
        Construct(
            const FArguments& InArgs,
            const TSharedRef<STableViewBase>& InOwnerTable) -> void
        {
            _Row = InArgs._Row;
            FSuperRowType::Construct(
                FSuperRowType::FArguments()
                    .Padding(FMargin{0.0f, 2.0f})
                    .ShowSelection(false),
                InOwnerTable);
        }

        virtual auto
        GenerateWidgetForColumn(
            const FName& InColumnName) -> TSharedRef<SWidget> override
        {
            const auto WeakRow = TWeakPtr<SCkTextureDebugger_MaterialInputsPage::FRow>{_Row};
            if (InColumnName == ProvenanceColumn)
            {
                return SNew(SBox)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Left)
                    .Padding(FMargin{CkStyle::SpaceS, 0.0f})
                    [
                        SNew(SCkDebug_StatusPill)
                        .Text_Lambda([WeakRow]()
                        {
                            const auto Row = WeakRow.Pin();
                            return Row.IsValid() ? FText::FromString(Row->Provenance) : FText::GetEmpty();
                        })
                        .Tone_Lambda([WeakRow]()
                        {
                            const auto Row = WeakRow.Pin();
                            return Row.IsValid() ? ProvenanceTone(Row->ProvenanceKind) : ECk_Tone::Neutral;
                        })
                        .ShowDot(false)
                    ];
            }

            const auto Text = TAttribute<FText>::CreateLambda([WeakRow, InColumnName]() -> FText
            {
                const auto Row = WeakRow.Pin();
                if (NOT Row.IsValid()) { return FText::GetEmpty(); }
                if (InColumnName == ParameterColumn) { return FText::FromString(Row->Parameter); }
                if (InColumnName == TextureColumn) { return FText::FromString(Row->Texture); }
                if (InColumnName == SlotColumn) { return FText::FromString(Row->Slot); }
                if (InColumnName == VariantColumn) { return FText::FromString(Row->Variant); }
                return FText::GetEmpty();
            });
            const auto Tooltip = TAttribute<FText>::CreateLambda([WeakRow, InColumnName]() -> FText
            {
                const auto Row = WeakRow.Pin();
                if (NOT Row.IsValid()) { return FText::GetEmpty(); }
                if (InColumnName == ParameterColumn) { return FText::FromString(Row->Detail); }
                if (InColumnName == TextureColumn) { return FText::FromString(Row->TexturePath); }
                return FText::GetEmpty();
            });

            return SNew(SBox)
                .VAlign(VAlign_Center)
                .Padding(FMargin{CkStyle::SpaceS, 0.0f})
                [
                    SNew(STextBlock)
                    .Text(Text)
                    .ToolTipText(Tooltip)
                    .ColorAndOpacity_Lambda([WeakRow]()
                    {
                        const auto Row = WeakRow.Pin();
                        return Row.IsValid()
                            ? TextColor(Row->IsHighlighted, Row->IsDimmed)
                            : FSlateColor{CkStyle::TextMute()};
                    })
                ];
        }

    private:
        TSharedPtr<SCkTextureDebugger_MaterialInputsPage::FRow> _Row;
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_UvDensityPage::
    Construct(
        const FArguments&) -> void
{
    _Result.Availability = ECkTextureDebugger_UvDensityAvailability::InvalidComponent;
    _Result.UnavailableReason = TEXT("Select a checker-capable mesh component and an explicit material slot.");

    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot().Padding(CkStyle::SpaceM)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                ck_texture_debugger_diagnostic_pages::MakePurposeText(
                    LOCTEXT("UvPurpose",
                        "Measures texels per centimetre only when the selected triangle, UV area, texture binding, texture transform, and cooked dimensions are authoritative. Missing proof is reported instead of estimated."))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM)
            [
                SNew(SWrapBox).UseAllottedSize(true)
                + SWrapBox::Slot().Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceS)
                [
                    SNew(SCkDebug_StatusPill)
                    .Text_Lambda([this] { return Get_ComponentContextText(); })
                    .Tone_Lambda([this]
                    {
                        return _Component.IsSet() && _Component->NavigationTarget.IsValid()
                            ? ECk_Tone::Info
                            : ECk_Tone::Neutral;
                    })
                ]
                + SWrapBox::Slot().Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceS)
                [
                    SNew(SCkDebug_StatusPill)
                    .Text_Lambda([this] { return Get_SlotContextText(); })
                    .Tone_Lambda([this] { return _ExplicitSlotIndices.IsEmpty() ? ECk_Tone::Neutral : ECk_Tone::Accent; })
                ]
                + SWrapBox::Slot().Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceS)
                [
                    SNew(SCkDebug_StatusPill)
                    .Text_Lambda([this] { return Get_TextureContextText(); })
                    .Tone_Lambda([this] { return _SelectedTexture.IsSet() ? ECk_Tone::Info : ECk_Tone::Neutral; })
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
            [
                SNew(SCkDebug_InspectorPanel)
                .Title(LOCTEXT("UvInputs", "Measurement inputs"))
                .StartExpanded(true)
                .Body()
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        ck_texture_debugger_diagnostic_pages::MakeFactRow(
                            LOCTEXT("UvComponent", "Component"),
                            TAttribute<FText>::CreateLambda([this] { return Get_ComponentContextText(); }))
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        ck_texture_debugger_diagnostic_pages::MakeFactRow(
                            LOCTEXT("UvSlot", "Material slot"),
                            TAttribute<FText>::CreateLambda([this] { return Get_SlotContextText(); }))
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        ck_texture_debugger_diagnostic_pages::MakeFactRow(
                            LOCTEXT("UvChannel", "UV channel"), FText::FromString(TEXT("UV0")))
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        ck_texture_debugger_diagnostic_pages::MakeFactRow(
                            LOCTEXT("UvTriangle", "Triangle / section"),
                            FText::FromString(TEXT("Unavailable — no authoritative triangle mapping")))
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        ck_texture_debugger_diagnostic_pages::MakeFactRow(
                            LOCTEXT("UvTexture", "Selected texture"),
                            TAttribute<FText>::CreateLambda([this] { return Get_TextureContextText(); }))
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SCkDebug_InspectorPanel)
                .Title(LOCTEXT("UvResult", "Authoritative result"))
                .StartExpanded(true)
                .Body()
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
                    [
                        SNew(SCkDebug_StatusPill)
                        .Text_Lambda([this] { return Get_ResultStatusText(); })
                        .Tone_Lambda([this] { return Get_ResultTone(); })
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM)
                    [
                        SNew(SCkDebug_StatPair)
                        .Value_Lambda([this] { return Get_ResultValueText(); })
                        .Label(LOCTEXT("TexelsPerCm", "Texels / cm"))
                        .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                        .ValueColor_Lambda([this]
                        {
                            return FSlateColor{Get_ResultTone() == ECk_Tone::Ok ? CkStyle::Ok() : CkStyle::Warn()};
                        })
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this] { return Get_ResultExplanationText(); })
                        .AutoWrapText(true)
                        .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                    ]
                ]
            ]
        ]
    ];
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

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_MaterialInputsPage::
    Construct(
        const FArguments&) -> void
{
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM)
        [
            ck_texture_debugger_diagnostic_pages::MakePurposeText(
                LOCTEXT("MaterialPurpose",
                    "Shows texture parameters resolved through the active material-instance chain and separately labels active-quality/platform used textures as potential references. Potential rows do not claim a sampler or slot binding."))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f)
        [
            SNew(SWrapBox).UseAllottedSize(true)
            + SWrapBox::Slot().Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceS)
            [
                SNew(SCkDebug_StatusPill)
                .Text_Lambda([this]
                {
                    return _Component.IsSet()
                        ? FText::FromString(FString::Printf(TEXT("%s · %s"),
                            *_Component->ActorDisplayName, *_Component->ComponentDisplayName))
                        : LOCTEXT("MaterialContextNone", "No component");
                })
                .Tone_Lambda([this]
                {
                    return _Component.IsSet() && _Component->NavigationTarget.IsValid()
                        ? ECk_Tone::Info
                        : ECk_Tone::Neutral;
                })
            ]
            + SWrapBox::Slot().Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceS)
            [
                SNew(SCkDebug_StatusPill)
                .Text_Lambda([this]
                {
                    return _SelectedTexture.IsSet() && NOT _SelectedTexture->DisplayName.IsEmpty()
                        ? FText::FromString(FString::Printf(TEXT("Selected texture · %s"), *_SelectedTexture->DisplayName))
                        : LOCTEXT("MaterialTextureNone", "Selected texture · none");
                })
                .Tone_Lambda([this] { return _SelectedTexture.IsSet() ? ECk_Tone::Accent : ECk_Tone::Neutral; })
                .ShowDot(false)
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SNew(SCkDebug_DualSearchBar)
                .FilterHintText(LOCTEXT("MaterialFilter", "Filter parameter, texture, slot, or provenance…"))
                .HighlightHintText(LOCTEXT("MaterialHighlight", "Highlight matches…"))
                .OnFilterTextChanged(this, &SCkTextureDebugger_MaterialInputsPage::OnFilterTextChanged)
                .OnHighlightTextChanged(this, &SCkTextureDebugger_MaterialInputsPage::OnHighlightTextChanged)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_CountBadge)
                .ValueText_Lambda([this]()
                {
                    return FText::FromString(FString::Printf(
                        TEXT("%d/%d"),
                        _VisibleRows.Num(),
                        _AllRows.Num()));
                })
                .SuffixText(LOCTEXT("MaterialRows", "rows"))
            ]
        ]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(CkStyle::SpaceM)
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                SAssignNew(_ListView, SListView<TSharedPtr<FRow>>)
                .ListItemsSource(&_VisibleRows)
                .SelectionMode(ESelectionMode::None)
                .OnGenerateRow(this, &SCkTextureDebugger_MaterialInputsPage::OnGenerateRow)
                .HeaderRow(
                    SNew(SHeaderRow)
                    + SHeaderRow::Column(ck_texture_debugger_diagnostic_pages::ParameterColumn).DefaultLabel(LOCTEXT("ParameterColumn", "Parameter")).FillWidth(0.22f)
                    + SHeaderRow::Column(ck_texture_debugger_diagnostic_pages::TextureColumn).DefaultLabel(LOCTEXT("TextureColumn", "Texture")).FillWidth(0.25f)
                    + SHeaderRow::Column(ck_texture_debugger_diagnostic_pages::ProvenanceColumn).DefaultLabel(LOCTEXT("ProvenanceColumn", "Provenance")).FillWidth(0.20f)
                    + SHeaderRow::Column(ck_texture_debugger_diagnostic_pages::SlotColumn).DefaultLabel(LOCTEXT("SlotColumn", "Slot")).FillWidth(0.12f)
                    + SHeaderRow::Column(ck_texture_debugger_diagnostic_pages::VariantColumn).DefaultLabel(LOCTEXT("VariantColumn", "Quality / platform")).FillWidth(0.21f))
            ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([this] { return Get_EmptyStateText(); })
                .AutoWrapText(true)
                .Justification(ETextJustify::Center)
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                .Visibility_Lambda([this] { return _VisibleRows.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
            ]
        ]
    ];
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

    auto StructureChanged = _VisibleRows.Num() != NewVisible.Num();
    if (NOT StructureChanged)
    {
        for (auto Index = 0; Index < _VisibleRows.Num(); ++Index)
        {
            if (_VisibleRows[Index] != NewVisible[Index])
            {
                StructureChanged = true;
                break;
            }
        }
    }

    _VisibleRows = MoveTemp(NewVisible);
    if (StructureChanged && _ListView.IsValid()) { _ListView->RequestListRefresh(); }
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

auto
    SCkTextureDebugger_MaterialInputsPage::
    OnGenerateRow(
        TSharedPtr<FRow> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>
{
    return SNew(ck_texture_debugger_diagnostic_pages::SMaterialInputRow, InOwnerTable)
        .Row(MoveTemp(InItem));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_SurfaceLightingPage::
    Construct(
        const FArguments&) -> void
{
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM)
        [
            ck_texture_debugger_diagnostic_pages::MakePurposeText(
                LOCTEXT("SurfacePurpose",
                    "Reports public runtime material and component facts for the selected slots. These facts do not diagnose Lumen, VSM, light leaks, blurry textures, or final rendered appearance."))
        ]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceM, CkStyle::SpaceM)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                SNew(SWrapBox).UseAllottedSize(true)
                + SWrapBox::Slot().Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceS)
                [
                    SNew(SCkDebug_StatusPill)
                    .Text_Lambda([this]
                    {
                        return _Component.IsSet()
                            ? FText::FromString(FString::Printf(TEXT("%s · %s"),
                                *_Component->ActorDisplayName, *_Component->ComponentDisplayName))
                            : LOCTEXT("SurfaceContextNone", "No component");
                    })
                    .Tone_Lambda([this]
                    {
                        return _Component.IsSet() && _Component->NavigationTarget.IsValid()
                            ? ECk_Tone::Info
                            : ECk_Tone::Neutral;
                    })
                ]
                + SWrapBox::Slot().Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceS)
                [
                    SNew(SCkDebug_StatusPill)
                    .Text_Lambda([this]
                    {
                        return _SelectedTexture.IsSet() && NOT _SelectedTexture->DisplayName.IsEmpty()
                            ? FText::FromString(FString::Printf(TEXT("Selected texture · %s"), *_SelectedTexture->DisplayName))
                            : LOCTEXT("SurfaceTextureNone", "Selected texture · none");
                    })
                    .Tone_Lambda([this] { return _SelectedTexture.IsSet() ? ECk_Tone::Accent : ECk_Tone::Neutral; })
                    .ShowDot(false)
                ]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [
                        SAssignNew(_SlotBox, SVerticalBox)
                    ]
                ]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this] { return Get_EmptyStateText(); })
                    .AutoWrapText(true)
                    .Justification(ETextJustify::Center)
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                    .Visibility_Lambda([this] { return _SlotFacts.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
                ]
            ]
        ]
    ];
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
        const auto HadFacts = NOT _SlotFacts.IsEmpty();
        _SlotFacts.Reset();
        if (HadFacts) { Rebuild_SlotWidgets(); }
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

    auto StructureChanged = _SlotFacts.Num() != NewFacts.Num();
    if (NOT StructureChanged)
    {
        for (auto Index = 0; Index < _SlotFacts.Num(); ++Index)
        {
            if (_SlotFacts[Index] != NewFacts[Index])
            {
                StructureChanged = true;
                break;
            }
        }
    }

    _SlotFacts = MoveTemp(NewFacts);
    if (StructureChanged) { Rebuild_SlotWidgets(); }
}

auto SCkTextureDebugger_SurfaceLightingPage::Rebuild_SlotWidgets() -> void
{
    if (NOT _SlotBox.IsValid()) { return; }
    _SlotBox->ClearChildren();
    for (const auto& Facts : _SlotFacts)
    {
        _SlotBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
        [
            Build_SlotWidget(Facts)
        ];
    }
}

auto
    SCkTextureDebugger_SurfaceLightingPage::
    Build_SlotWidget(
        TSharedPtr<FSlotFacts> Facts) -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_Card)
        .StripeColor_Lambda([Facts] { return Facts.IsValid() && Facts->HasMaterial ? CkStyle::Info() : CkStyle::Warn(); })
        [
            SNew(SCkDebug_InspectorPanel)
            .Title(FText::FromString(FString::Printf(TEXT("Slot %d · %s"), Facts->SlotIndex, *Facts->MaterialName)))
            .CountText(FText::FromString(Facts->MaterialPath))
            .StatusPillText(Facts->HasMaterial ? LOCTEXT("SurfaceResolved", "RESOLVED") : LOCTEXT("SurfaceUnavailable", "UNAVAILABLE"))
            .StatusPillTone(Facts->HasMaterial ? ECk_Tone::Ok : ECk_Tone::Warn)
            .StartExpanded(true)
            .Body()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SCkDebug_SectionHeader)
                    .Label(LOCTEXT("MaterialFacts", "Material facts"))
                    .Underline(true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                [
                    SNew(SWrapBox).UseAllottedSize(true)
                    + SWrapBox::Slot().Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceS)
                    [
                        SNew(SCkDebug_StatusPill)
                        .Text_Lambda([Facts] { return FText::FromString(FString::Printf(TEXT("Blend · %s"), *Facts->BlendMode)); })
                        .Tone(Facts->HasMaterial ? ECk_Tone::Info : ECk_Tone::Warn)
                        .ShowDot(false)
                    ]
                    + SWrapBox::Slot().Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceS)
                    [
                        SNew(SCkDebug_StatusPill)
                        .Text_Lambda([Facts] { return FText::FromString(FString::Printf(TEXT("Shading · %s"), *Facts->ShadingModels)); })
                        .Tone(Facts->HasMaterial ? ECk_Tone::Info : ECk_Tone::Warn)
                        .ShowDot(false)
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    ck_texture_debugger_diagnostic_pages::MakeBoolFactRow(
                        LOCTEXT("TwoSided", "Two-sided"),
                        TAttribute<bool>::CreateLambda([Facts] { return Facts->IsTwoSided; }))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    ck_texture_debugger_diagnostic_pages::MakeBoolFactRow(
                        LOCTEXT("Masked", "Masked"),
                        TAttribute<bool>::CreateLambda([Facts] { return Facts->IsMasked; }))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    ck_texture_debugger_diagnostic_pages::MakeBoolFactRow(
                        LOCTEXT("Translucent", "Translucent"),
                        TAttribute<bool>::CreateLambda([Facts] { return Facts->IsTranslucent; }))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM, 0.0f, CkStyle::SpaceS)
                [
                    SNew(SCkDebug_SectionHeader)
                    .Label(LOCTEXT("LightingFacts", "Component lighting and shadow facts"))
                    .Underline(true)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    ck_texture_debugger_diagnostic_pages::MakeBoolFactRow(
                        LOCTEXT("CastShadow", "Cast shadow"),
                        TAttribute<bool>::CreateLambda([Facts] { return Facts->CastsShadow; }))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    ck_texture_debugger_diagnostic_pages::MakeBoolFactRow(
                        LOCTEXT("DynamicShadow", "Dynamic shadow"),
                        TAttribute<bool>::CreateLambda([Facts] { return Facts->CastsDynamicShadow; }))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    ck_texture_debugger_diagnostic_pages::MakeBoolFactRow(
                        LOCTEXT("StaticShadow", "Static shadow"),
                        TAttribute<bool>::CreateLambda([Facts] { return Facts->CastsStaticShadow; }))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    ck_texture_debugger_diagnostic_pages::MakeBoolFactRow(
                        LOCTEXT("VolumetricShadow", "Volumetric translucent shadow"),
                        TAttribute<bool>::CreateLambda([Facts] { return Facts->CastsVolumetricTranslucentShadow; }))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    ck_texture_debugger_diagnostic_pages::MakeBoolFactRow(
                        LOCTEXT("ReceivesDecals", "Receives decals"),
                        TAttribute<bool>::CreateLambda([Facts] { return Facts->ReceivesDecals; }))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    ck_texture_debugger_diagnostic_pages::MakeBoolFactRow(
                        LOCTEXT("StaticLighting", "Has static lighting"),
                        TAttribute<bool>::CreateLambda([Facts] { return Facts->HasStaticLighting; }))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
                    [
                        SNew(SCkDebug_StatPair)
                        .Value_Lambda([Facts]
                        {
                            return Facts->IsMasked
                                ? FText::AsNumber(Facts->OpacityMaskClipValue)
                                : FText::FromString(TEXT("N/A"));
                        })
                        .Label(LOCTEXT("OpacityClip", "Opacity mask clip"))
                        .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
                    [
                        SNew(SCkDebug_StatPair)
                        .Value_Lambda([Facts] { return FText::FromString(Facts->LightMapResolution); })
                        .Label(LOCTEXT("Lightmap", "Lightmap resolution"))
                        .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
                    [
                        SNew(SCkDebug_StatPair)
                        .Value_Lambda([Facts] { return FText::FromString(Facts->Nanite); })
                        .Label(LOCTEXT("Nanite", "Nanite data"))
                        .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("NoSurfaceDiagnosis",
                        "These values are direct runtime facts. They do not prove whether a lighting artifact, shadow leak, blurry texture, or final surface appearance is correct."))
                    .AutoWrapText(true)
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ]
        ];
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
