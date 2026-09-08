#include "CkTextureDebugger/Window/SCkTextureDebugger_TextureHealthTable.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkFlex.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Widgets/Layout/SSpacer.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Texture.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/StyleDefaults.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkTextureDebugger_TextureHealthTable"

namespace ck_texture_debugger_texture_health_table
{
    const auto TextureColumn = FName{TEXT("Texture")};
    const auto ComponentColumn = FName{TEXT("Component")};
    const auto MaterialColumn = FName{TEXT("Material")};
    const auto CookedColumn = FName{TEXT("Cooked")};
    const auto ResidencyColumn = FName{TEXT("Residency")};
    const auto StateColumn = FName{TEXT("State")};

    auto Get_StreamingText(const FCkTextureDebugger_TextureHealth& Health) -> FText
    {
        if (Health.HasStreamingMetrics && Health.RequestedMipCount > Health.ResidentMipCount)
        {
            return FText::FromString(FString::Printf(
                TEXT("Mip deficit · %d"),
                Health.RequestedMipCount - Health.ResidentMipCount));
        }

        if (Health.HasStreamingMetrics && Health.RequestedMipCount == Health.ResidentMipCount)
        { return LOCTEXT("FullyResident", "Fully resident"); }

        switch (Health.StreamingAvailability)
        {
            case ECkTextureDebugger_StreamingAvailability::Available: return LOCTEXT("Streaming", "Streaming active");
            case ECkTextureDebugger_StreamingAvailability::ManagerUnavailable: return LOCTEXT("ManagerUnavailable", "Manager unavailable");
            case ECkTextureDebugger_StreamingAvailability::StreamingDisabled: return LOCTEXT("Disabled", "Streaming disabled");
            case ECkTextureDebugger_StreamingAvailability::NotStreamable: return LOCTEXT("NotStreamable", "Not streamable");
            case ECkTextureDebugger_StreamingAvailability::ResourceNotCreated: return LOCTEXT("NoResource", "No render resource");
            default: return LOCTEXT("Unavailable", "Unavailable");
        }
    }

    auto Get_StreamingTone(const FCkTextureDebugger_TextureHealth& Health) -> ECk_Tone
    {
        if (Health.HasStreamingMetrics && Health.RequestedMipCount > Health.ResidentMipCount)
        { return ECk_Tone::Warn; }

        if (Health.StreamingAvailability == ECkTextureDebugger_StreamingAvailability::Available) { return ECk_Tone::Ok; }
        return Health.StreamingAvailability == ECkTextureDebugger_StreamingAvailability::NotStreamable ? ECk_Tone::Neutral : ECk_Tone::Warn;
    }

    auto Make_TextCell(TAttribute<FText> Text, TAttribute<FText> Tooltip, TAttribute<FLinearColor> Color, EHorizontalAlignment Alignment = HAlign_Left) -> TSharedRef<SWidget>
    {
        return SNew(SBox).VAlign(VAlign_Center).HAlign(Alignment).Padding(FMargin{CkStyle::SpaceS, 0.0f})
        [SNew(STextBlock).Text(MoveTemp(Text)).ToolTipText(MoveTemp(Tooltip)).Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity_Lambda([Color]() { return FSlateColor{Color.Get(FLinearColor::White)}; })];
    }

    auto UiSchema() -> TArray<FCkUiFieldSchema>
    {
        return {{TEXT("texture"), ECkUiFieldKind::Text}, {TEXT("component"), ECkUiFieldKind::Text},
            {TEXT("material"), ECkUiFieldKind::Text}, {TEXT("cooked"), ECkUiFieldKind::Text},
            {TEXT("residency"), ECkUiFieldKind::Text}, {TEXT("state"), ECkUiFieldKind::Text},
            {TEXT("texture-tip"), ECkUiFieldKind::Text}, {TEXT("component-tip"), ECkUiFieldKind::Text},
            {TEXT("material-tip"), ECkUiFieldKind::Text}, {TEXT("text-color"), ECkUiFieldKind::Color},
            {TEXT("state-foreground"), ECkUiFieldKind::Color}, {TEXT("state-background"), ECkUiFieldKind::Color},
            {TEXT("meter-color"), ECkUiFieldKind::Color}, {TEXT("fraction"), ECkUiFieldKind::Number}};
    }

    auto UiRecord(const SCkTextureDebugger_TextureHealthTable::FRow& Row) -> FCkUiRecordData
    {
        auto Result = FCkUiRecordData{};
        Result.Key = Row.UiKey;
        const auto Text = [&Result](const TCHAR* Name, FText Value)
        { Result.Fields.Add(Name, FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = MoveTemp(Value)}); };
        const auto Color = [&Result](const TCHAR* Name, FLinearColor Value)
        { Result.Fields.Add(Name, FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = Value}); };
        Text(TEXT("texture"), FText::FromString(Row.ExactDuplicateCount > 1
            ? FString::Printf(TEXT("%s  x%d"), *Row.Health.DisplayName, Row.ExactDuplicateCount) : Row.Health.DisplayName));
        Text(TEXT("component"), FText::FromString(Row.ComponentLabel));
        Text(TEXT("material"), FText::FromString(Row.MaterialLabel));
        Text(TEXT("cooked"), FText::FromString(FString::Printf(TEXT("%d x %d"), Row.Health.CookedWidth, Row.Health.CookedHeight)));
        Text(TEXT("residency"), Row.Health.HasStreamingMetrics
            ? FText::FromString(FString::Printf(TEXT("%d / %d mips"), Row.Health.ResidentMipCount, Row.Health.RequestedMipCount)) : LOCTEXT("ResidencyNone", "—"));
        Text(TEXT("state"), Get_StreamingText(Row.Health));
        Text(TEXT("texture-tip"), FText::FromString(Row.Health.AssetPath.ToString()));
        Text(TEXT("component-tip"), FText::FromString(Row.Key.ComponentPath.ToString()));
        Text(TEXT("material-tip"), FText::FromString(Row.Key.MaterialPath.ToString()));
        Color(TEXT("text-color"), Row.IsHighlightMatch ? (Row.IsContextComponent ? CkStyle::TextStrong() : CkStyle::Text()) : CkStyle::TextMute());
        Color(TEXT("state-foreground"), CkStyle::GetToneColor(Get_StreamingTone(Row.Health)));
        Color(TEXT("state-background"), CkStyle::GetToneDimColor(Get_StreamingTone(Row.Health)));
        Color(TEXT("meter-color"), CkStyle::Accent());
        const float Fraction = Row.Health.HasStreamingMetrics && Row.Health.RequestedMipCount > 0
            ? FMath::Clamp(static_cast<float>(Row.Health.ResidentMipCount) / Row.Health.RequestedMipCount, 0.0f, 1.0f) : 0.0f;
        Result.Fields.Add(TEXT("fraction"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Number, .Number = Fraction});
        return Result;
    }

    class SRow final : public SMultiColumnTableRow<TSharedPtr<SCkTextureDebugger_TextureHealthTable::FRow>>
    {
    public:
        SLATE_BEGIN_ARGS(SRow) {}
            SLATE_ARGUMENT(TSharedPtr<SCkTextureDebugger_TextureHealthTable::FRow>, Row)
        SLATE_END_ARGS()

        auto Construct(const FArguments& Args, const TSharedRef<STableViewBase>& Owner) -> void
        {
            _Row = Args._Row;
            SMultiColumnTableRow<TSharedPtr<SCkTextureDebugger_TextureHealthTable::FRow>>::Construct(
                FSuperRowType::FArguments().Padding(FMargin{0.0f, 1.0f}).ShowSelection(true), Owner);
            // SMultiColumnTableRow does not forward the mode argument to STableRow.
            // Publish mouse-down selection before a snapshot can reconcile the previous key.
            SignalSelectionMode = ETableRowSignalSelectionMode::Instantaneous;
            SetToolTipText(FText::FromString(_Row.IsValid() ? _Row->Health.AssetPath.ToString() : FString{}));
        }

        virtual auto GenerateWidgetForColumn(const FName& Column) -> TSharedRef<SWidget> override
        {
            const auto WeakRow = TWeakPtr<SCkTextureDebugger_TextureHealthTable::FRow>{_Row};
            const auto Text = [WeakRow, Column]() -> FText
            {
                const auto Row = WeakRow.Pin();
                if (NOT Row.IsValid()) { return FText::GetEmpty(); }
                if (Column == TextureColumn) { return FText::FromString(Row->ExactDuplicateCount > 1 ? FString::Printf(TEXT("%s  x%d"), *Row->Health.DisplayName, Row->ExactDuplicateCount) : Row->Health.DisplayName); }
                if (Column == ComponentColumn) { return FText::FromString(Row->ComponentLabel); }
                if (Column == MaterialColumn) { return FText::FromString(Row->MaterialLabel); }
                if (Column == CookedColumn) { return FText::FromString(FString::Printf(TEXT("%d x %d"), Row->Health.CookedWidth, Row->Health.CookedHeight)); }
                if (Column == ResidencyColumn) { return Row->Health.HasStreamingMetrics ? FText::FromString(FString::Printf(TEXT("%d / %d mips"), Row->Health.ResidentMipCount, Row->Health.RequestedMipCount)) : LOCTEXT("ResidencyNone", "—"); }
                return FText::GetEmpty();
            };
            const auto Tooltip = [WeakRow, Column]() -> FText
            {
                const auto Row = WeakRow.Pin();
                if (NOT Row.IsValid()) { return FText::GetEmpty(); }
                if (Column == TextureColumn) { return FText::FromString(Row->Health.AssetPath.ToString()); }
                if (Column == ComponentColumn) { return FText::FromString(Row->Key.ComponentPath.ToString()); }
                if (Column == MaterialColumn) { return FText::FromString(Row->Key.MaterialPath.ToString()); }
                return FText::GetEmpty();
            };
            const auto Color = [WeakRow]() -> FLinearColor
            {
                const auto Row = WeakRow.Pin();
                if (NOT Row.IsValid()) { return CkStyle::TextMute(); }
                return Row->IsHighlightMatch ? (Row->IsContextComponent ? CkStyle::TextStrong() : CkStyle::Text()) : CkStyle::TextMute();
            };

            if (Column == StateColumn)
            {
                return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin{CkStyle::SpaceS, 0.0f})
                [SNew(SCkDebug_StatusPill)
                    .Text_Lambda([WeakRow]() { const auto Row = WeakRow.Pin(); return Row.IsValid() ? Get_StreamingText(Row->Health) : FText::GetEmpty(); })
                    .Tone_Lambda([WeakRow]() { const auto Row = WeakRow.Pin(); return Row.IsValid() ? Get_StreamingTone(Row->Health) : ECk_Tone::Neutral; })];
            }
            if (Column == ResidencyColumn)
            {
                return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin{CkStyle::SpaceS, 0.0f})
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                    [Make_TextCell(TAttribute<FText>::CreateLambda(Text), TAttribute<FText>::CreateLambda(Tooltip), TAttribute<FLinearColor>::CreateLambda(Color), HAlign_Right)]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 1.0f, 0.0f, 0.0f)
                    [SNew(SCkDebug_MeterBar).Fraction_Lambda([WeakRow]() -> float
                    {
                        const auto Row = WeakRow.Pin();
                        if (NOT Row.IsValid() || NOT Row->Health.HasStreamingMetrics || Row->Health.RequestedMipCount <= 0) { return 0.0f; }
                        return FMath::Clamp(static_cast<float>(Row->Health.ResidentMipCount) / static_cast<float>(Row->Health.RequestedMipCount), 0.0f, 1.0f);
                    }).FillColor(CkStyle::Accent()).DesiredSize(FVector2D{96.0f, 4.0f})]];
            }
            return Make_TextCell(TAttribute<FText>::CreateLambda(Text), TAttribute<FText>::CreateLambda(Tooltip), TAttribute<FLinearColor>::CreateLambda(Color));
        }

    private:
        TSharedPtr<SCkTextureDebugger_TextureHealthTable::FRow> _Row;
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkTextureDebugger_TextureHealthTable::Construct(const FArguments& InArgs) -> void
{
    if (InArgs._UseYogaLayout)
    { Construct_Yoga(InArgs); }
    else
    { Construct_Native(InArgs); }
}
auto SCkTextureDebugger_TextureHealthTable::Construct_Yoga(const FArguments& Args) -> void
{
    _OnSelectionChanged = Args._OnSelectionChanged;
    _PreviewBrush.DrawAs = ESlateBrushDrawType::Image;
    _PreviewBrush.ImageSize = FVector2D{192.0f, 192.0f};

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const FCkUiLoadResult CollectionResult = FCkUiCollection::TryCreate(ck_texture_debugger_texture_health_table::UiSchema(), _UiCollection);
    if (!RegistryResult.Succeeded || !CollectionResult.Succeeded)
    {
        _PublicationError = FString::Join(RegistryResult.Errors, TEXT("\n")) + FString::Join(CollectionResult.Errors, TEXT("\n"));
        _UiCollection.Reset();
        ChildSlot[SNew(STextBlock).Text(Get_LayoutError())];
        return;
    }
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("clear-selection"), FSimpleDelegate::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::Clear_Selection));
    auto Tokens = FCkUiView::FTokens{};
    Tokens.Add(TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS));
    Tokens.Add(TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM));
    Tokens.Add(TEXT("--font-heading"), FString::FromInt(CkStyle::FontSizeH4()));
    Tokens.Add(TEXT("--text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--text-strong"), TEXT("#") + CkStyle::TextStrong().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--surface"), TEXT("#") + CkStyle::Bg2().ToFColorSRGB().ToHex());
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("filter"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::Get_FilterText));
    Data.Text.Add(TEXT("highlight"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::Get_HighlightText));
    Data.Text.Add(TEXT("count"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::Get_RowCountText));
    Data.Text.Add(TEXT("selected-details"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::Get_SelectedDetailsText));
    Data.Text.Add(TEXT("empty-state"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::Get_EmptyStateText));
    Data.Text.Add(TEXT("filter-placeholder"), LOCTEXT("FilterHint", "Filter textures, components, materials…"));
    Data.Text.Add(TEXT("highlight-placeholder"), LOCTEXT("HighlightHint", "Highlight…"));
    Data.Text.Add(TEXT("texture-column-label"), LOCTEXT("TextureColumn", "Texture"));
    Data.Text.Add(TEXT("component-column-label"), LOCTEXT("ComponentColumn", "Component"));
    Data.Text.Add(TEXT("material-column-label"), LOCTEXT("MaterialColumn", "Material / slot"));
    Data.Text.Add(TEXT("cooked-column-label"), LOCTEXT("CookedColumn", "Cooked"));
    Data.Text.Add(TEXT("residency-column-label"), LOCTEXT("ResidencyColumn", "Resident / requested"));
    Data.Text.Add(TEXT("state-column-label"), LOCTEXT("StateColumn", "State"));
    Data.Collections.Add(TEXT("inventory"), _UiCollection);
    Data.TableSelectionChanged.Add(TEXT("select-texture"), FOnCkUiTableSelectionChanged::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::On_AuthoredSelectionChanged));
    Data.Text.Add(TEXT("copy-details-label"), LOCTEXT("CopyDetails", "Copy texture details"));
    Data.Text.Add(TEXT("copy-details-tooltip"), LOCTEXT("CopyDetailsTip", "Copy the selected texture's loaded-world health facts."));
    Data.ContextActions.Add(TEXT("copy-texture-details"), FOnCkUiContextAction::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::On_CopyTextureDetails));
    Data.TextChanged.Add(TEXT("filter"), FOnTextChanged::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::On_FilterTextChanged));
    Data.TextChanged.Add(TEXT("highlight"), FOnTextChanged::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::On_HighlightTextChanged));
    Data.Images.Add(TEXT("preview"), TAttribute<const FSlateBrush*>::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::Get_PreviewBrush));
    Data.Visibility.Add(TEXT("has-selection"), TAttribute<bool>::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::Has_Selection));
    Data.Visibility.Add(TEXT("inventory-empty"), TAttribute<bool>::CreateLambda([WeakTable = TWeakPtr<SCkTextureDebugger_TextureHealthTable>(SharedThis(this))]()
    { const auto Table = WeakTable.Pin(); return Table.IsValid() && Table->_Rows.IsEmpty(); }));
    _LayoutView = FCkUiView::Create({}, MoveTemp(Actions), MoveTemp(Tokens),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);

    // The diagnostic host remains available even when the first authored document cannot load.
    ChildSlot[SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(STextBlock).Tag(TEXT("Ck.TextureHealth.LayoutError"))
            .Text(this, &SCkTextureDebugger_TextureHealthTable::Get_LayoutError)
            .ToolTipText(this, &SCkTextureDebugger_TextureHealthTable::Get_LayoutError)
            .AutoWrapText(true).ColorAndOpacity(FSlateColor{CkStyle::Err()})
            .Visibility_Lambda([this]() { return Get_LayoutError().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [_LayoutView->GetRegion(TEXT("main"))]];

    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    const auto UiDirectory = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    Reload_LayoutFiles(FPaths::Combine(UiDirectory, TEXT("TextureHealth.ui.html")),
        FPaths::Combine(UiDirectory, TEXT("TextureHealth.ui.css")));
    RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SCkTextureDebugger_TextureHealthTable::Tick_LayoutFiles));
}

auto SCkTextureDebugger_TextureHealthTable::TryReload_Layout(const FString& InMarkup, const FString& InStylesheet) -> FCkUiLoadResult
{
    if (!_LayoutView.IsValid()) { return {false, {TEXT("Authored layout is disabled for this table.")}}; }
    return _LayoutView->TryReload(InMarkup, InStylesheet, TEXT("TextureHealth"));
}

auto SCkTextureDebugger_TextureHealthTable::Reload_LayoutFiles(const FString& InMarkupPath, const FString& InStylesheetPath) -> FCkUiLoadResult
{
    if (!_LayoutView.IsValid()) { return {false, {TEXT("Authored layout is disabled for this table.")}}; }
    return _LayoutView->ReloadFiles(InMarkupPath, InStylesheetPath);
}

auto SCkTextureDebugger_TextureHealthTable::Poll_LayoutFiles() -> bool
{
    return _LayoutView.IsValid() && _LayoutView->PollFiles();
}

auto SCkTextureDebugger_TextureHealthTable::Tick_LayoutFiles(double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType
{
    Poll_LayoutFiles();
    return EActiveTimerReturnType::Continue;
}

auto SCkTextureDebugger_TextureHealthTable::Get_LayoutRevision() const -> int64
{
    return _LayoutView.IsValid() ? _LayoutView->GetRevision() : 0;
}

auto SCkTextureDebugger_TextureHealthTable::Get_LayoutError() const -> FText
{
    if (!_PublicationError.IsEmpty()) { return FText::FromString(_PublicationError); }
    if (!_LayoutView.IsValid() || _LayoutView->GetLastResult().Succeeded) { return FText::GetEmpty(); }
    return FText::FromString(FString::Join(_LayoutView->GetLastResult().Errors, TEXT("\n")));
}

auto SCkTextureDebugger_TextureHealthTable::Get_AuthoredTable() const -> TSharedPtr<SCkUiTable>
{
    return _LayoutView.IsValid() ? _LayoutView->GetTable(TEXT("inventory-control")) : nullptr;
}

auto SCkTextureDebugger_TextureHealthTable::Get_AuthoredSplitter() const -> TSharedPtr<SCkUiSplitter>
{
    return _LayoutView.IsValid() ? _LayoutView->GetSplitter(TEXT("health-splitter")) : nullptr;
}

auto
    SCkTextureDebugger_TextureHealthTable::
    Construct_Native(
        const FArguments& Args)
    -> void
{
    _OnSelectionChanged = Args._OnSelectionChanged;
    _PreviewBrush.DrawAs = ESlateBrushDrawType::Image;
    _PreviewBrush.ImageSize = FVector2D{192.0f, 192.0f};

    ChildSlot
    [
        SNew(SSplitter)
        + SSplitter::Slot().Value(0.64f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SAssignNew(_SearchBar, SCkDebug_DualSearchBar)
                    .FilterHintText(LOCTEXT("FilterHint", "Filter textures, components, materials…"))
                    .HighlightHintText(LOCTEXT("HighlightHint", "Highlight…"))
                    .OnFilterTextChanged_Lambda([this](const FString& Text)
                    {
                        if (_FilterString == Text) { return; }
                        _FilterString = Text;
                        Rebuild_Rows();
                        Reconcile_Selection();
                    })
                    .OnHighlightTextChanged_Lambda([this](const FString& Text)
                    {
                        if (_HighlightString == Text) { return; }
                        _HighlightString = Text;
                        Rebuild_Rows();
                        Reconcile_Selection();
                    })
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_CountBadge)
                    .ValueText_Lambda([this]()
                    {
                        return FText::FromString(FString::Printf(
                            TEXT("%d/%d"),
                            Get_VisibleRowCount(),
                            Get_TotalRowCount()));
                    })
                    .SuffixText(LOCTEXT("Rows", "rows"))
                ]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SAssignNew(_ListView, SListView<TSharedPtr<FRow>>)
                    .Tag(TEXT("Ck.TextureHealth.List"))
                    .ListItemsSource(&_Rows)
                    .SelectionMode(ESelectionMode::Single)
                    .ClearSelectionOnClick(true)
                    .OnGenerateRow(this, &SCkTextureDebugger_TextureHealthTable::OnGenerateRow)
                    .OnSelectionChanged(this, &SCkTextureDebugger_TextureHealthTable::OnSelectionChanged)
                    .OnContextMenuOpening(this, &SCkTextureDebugger_TextureHealthTable::OnContextMenuOpening)
                    .HeaderRow(
                        SNew(SHeaderRow)
                        + SHeaderRow::Column(ck_texture_debugger_texture_health_table::TextureColumn)
                          .DefaultLabel(LOCTEXT("TextureColumn", "Texture"))
                          .FillWidth(0.25f)
                        + SHeaderRow::Column(ck_texture_debugger_texture_health_table::ComponentColumn)
                          .DefaultLabel(LOCTEXT("ComponentColumn", "Component"))
                          .FillWidth(0.23f)
                        + SHeaderRow::Column(ck_texture_debugger_texture_health_table::MaterialColumn)
                          .DefaultLabel(LOCTEXT("MaterialColumn", "Material / slot"))
                          .FillWidth(0.20f)
                        + SHeaderRow::Column(ck_texture_debugger_texture_health_table::CookedColumn)
                          .DefaultLabel(LOCTEXT("CookedColumn", "Cooked"))
                          .FillWidth(0.10f)
                          .HAlignHeader(HAlign_Right)
                        + SHeaderRow::Column(ck_texture_debugger_texture_health_table::ResidencyColumn)
                          .DefaultLabel(LOCTEXT("ResidencyColumn", "Resident / requested"))
                          .FillWidth(0.13f)
                          .HAlignHeader(HAlign_Right)
                        + SHeaderRow::Column(ck_texture_debugger_texture_health_table::StateColumn)
                          .DefaultLabel(LOCTEXT("StateColumn", "State"))
                          .FillWidth(0.16f))
                ]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    SNew(SBorder)
                    .Visibility_Lambda([this]()
                    {
                        return _Rows.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    .Padding(CkStyle::SpaceL)
                    .BorderImage(CkStyle::GetRoundedBrush_Large())
                    .BorderBackgroundColor(FSlateColor{CkStyle::Bg2()})
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() { return Get_EmptyStateText(); })
                        .AutoWrapText(true)
                        .Justification(ETextJustify::Center)
                        .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                    ]
                ]
            ]
        ]
        + SSplitter::Slot().Value(0.36f)
        [
            SNew(SBorder)
            .Padding(CkStyle::SpaceM)
            .BorderImage(CkStyle::GetRoundedBrush_Large())
            .BorderBackgroundColor(FSlateColor{CkStyle::Bg2()})
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("SelectedTexture", "Selected texture"))
                        .Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
                        .ColorAndOpacity(FSlateColor{CkStyle::TextStrong()})
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Clear", "Clear"))
                        .ToolTipText(LOCTEXT("ClearTip", "Clear the selected texture and release its preview root."))
                        .OnClicked_Lambda([this]()
                        {
                            Clear_Selection();
                            return FReply::Handled();
                        })
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM, 0.0f, CkStyle::SpaceM)
                [
                    SNew(SBox)
                    .WidthOverride(192.0f)
                    .HeightOverride(192.0f)
                    [
                        SNew(SImage)
                        .Image_Lambda([this]() { return Get_PreviewBrush(); })
                    ]
                ]
                + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() { return Get_SelectedDetailsText(); })
                        .AutoWrapText(true)
                        .ColorAndOpacity(FSlateColor{CkStyle::Text()})
                    ]
                ]
            ]
        ]
    ];
}

auto
    SCkTextureDebugger_TextureHealthTable::
    Set_Snapshot(
        const FCkTextureDebugger_LoadedWorldSnapshot& Snapshot,
        TWeakObjectPtr<UPrimitiveComponent> SelectedComponent)
    -> void
{
    _Snapshot = Snapshot;
    _SelectedComponent = SelectedComponent;
    Rebuild_Rows();
    Reconcile_Selection();
}

auto SCkTextureDebugger_TextureHealthTable::Clear_Selection() -> void
{
    const auto HadSelection = _SelectedKey.IsSet();
    _SelectedKey.Reset();
    Clear_PreviewTexture();
    if (_ListView.IsValid())
    { _ListView->ClearSelection(); }
    if (const auto Table = Get_AuthoredTable(); Table.IsValid()) { Table->TrySelectKey({}); }
    if (HadSelection)
    { _OnSelectionChanged.ExecuteIfBound({}); }
}

auto SCkTextureDebugger_TextureHealthTable::Reconcile_Selection() -> void
{
    if (NOT _SelectedKey.IsSet()) { return; }
    const auto Row = Find_Row(_SelectedKey.GetValue());
    if (NOT Row.IsValid()) { Clear_Selection(); return; }
    Set_PreviewTexture(Row->Texture);
    if (_ListView.IsValid())
    {
        const auto Selected = _ListView->GetSelectedItems();
        if (Selected.Num() != 1 || Selected[0] != Row) { _ListView->SetSelection(Row, ESelectInfo::Direct); }
    }
    if (const auto Table = Get_AuthoredTable(); Table.IsValid()) { Table->TrySelectKey(Row->UiKey); }
}

auto SCkTextureDebugger_TextureHealthTable::Get_VisibleRowCount() const -> int32 { return _Rows.Num(); }
auto SCkTextureDebugger_TextureHealthTable::Get_TotalRowCount() const -> int32 { return _TotalRowCount; }

auto
    SCkTextureDebugger_TextureHealthTable::
    Get_Selection() const
    -> TOptional<FCkTextureDebugger_TextureHealthSelection>
{
    if (NOT _SelectedKey.IsSet())
    { return {}; }

    const auto Row = Find_Row(_SelectedKey.GetValue());
    return Row.IsValid()
        ? TOptional<FCkTextureDebugger_TextureHealthSelection>{Make_Selection(*Row)}
        : TOptional<FCkTextureDebugger_TextureHealthSelection>{};
}

auto SCkTextureDebugger_TextureHealthTable::Rebuild_Rows() -> void
{
    auto Existing = TMap<FRowKey, TSharedPtr<FRow>>{};
    Existing.Reserve(_AllRows.Num());
    for (const auto& Row : _AllRows)
    {
        if (Row.IsValid())
        { Existing.Add(Row->Key, Row); }
    }

    auto Aggregate = TMap<FRowKey, FRow>{};
    for (const auto& Component : _Snapshot.Components) for (const auto& Slot : Component.MaterialSlots) for (const auto& Texture : Slot.Textures)
    {
        auto Value = Make_Row(Component, Slot, Texture);
        if (auto* Found = Aggregate.Find(Value.Key)) { ++Found->ExactDuplicateCount; }
        else { Aggregate.Add(Value.Key, MoveTemp(Value)); }
    }

    // Prepare domain values and the UI projection before mutating any accepted row.
    auto Values = TArray<FRow>{};
    Values.Reserve(Aggregate.Num());
    for (auto& Pair : Aggregate)
    {
        if (const auto* Found = Existing.Find(Pair.Key))
        { Pair.Value.UiKey = (*Found)->UiKey; }
        else
        { Pair.Value.UiKey = FGuid::NewGuid().ToString(EGuidFormats::Digits); }
        Pair.Value.IsHighlightMatch = MatchesSearch(Pair.Value, _HighlightString);
        Values.Add(MoveTemp(Pair.Value));
    }

    Values.Sort([](const FRow& A, const FRow& B)
    {
        if (A.Key.ComponentPath != B.Key.ComponentPath) { return A.Key.ComponentPath.ToString() < B.Key.ComponentPath.ToString(); }
        if (A.Key.SlotIndex != B.Key.SlotIndex) { return A.Key.SlotIndex < B.Key.SlotIndex; }
        if (A.Key.MaterialPath != B.Key.MaterialPath) { return A.Key.MaterialPath.ToString() < B.Key.MaterialPath.ToString(); }
        if (A.Key.TexturePath != B.Key.TexturePath) { return A.Key.TexturePath.ToString() < B.Key.TexturePath.ToString(); }
        return A.UiKey < B.UiKey;
    });

    if (_UiCollection.IsValid())
    {
        auto Records = TArray<FCkUiRecordData>{};
        Records.Reserve(Values.Num());
        for (const FRow& Value : Values)
        { if (MatchesSearch(Value, _FilterString)) { Records.Add(ck_texture_debugger_texture_health_table::UiRecord(Value)); } }
        const FCkUiLoadResult Result = _UiCollection->TrySetRecords(MoveTemp(Records));
        if (!Result.Succeeded)
        { _PublicationError = FString::Join(Result.Errors, TEXT("\n")); return; }
    }
    _PublicationError.Reset();
    _TotalRowCount = Values.Num();
    auto NewAllRows = TArray<TSharedPtr<FRow>>{};
    auto NewRows = TArray<TSharedPtr<FRow>>{};
    auto UiRows = TMap<FString, TWeakPtr<FRow>>{};
    NewAllRows.Reserve(Values.Num());
    NewRows.Reserve(Values.Num());
    for (FRow& Value : Values)
    {
        TSharedPtr<FRow> Row = Existing.FindRef(Value.Key);
        if (Row.IsValid()) { *Row = MoveTemp(Value); }
        else { Row = MakeShared<FRow>(MoveTemp(Value)); }
        NewAllRows.Add(Row);
        if (MatchesSearch(*Row, _FilterString))
        {
            NewRows.Add(Row);
            UiRows.Add(Row->UiKey, Row);
        }
    }

    auto StructureChanged = _Rows.Num() != NewRows.Num();
    if (NOT StructureChanged)
    {
        for (auto Index = 0; Index < _Rows.Num(); ++Index)
        {
            if (_Rows[Index] != NewRows[Index])
            { StructureChanged = true; break; }
        }
    }
    _AllRows = MoveTemp(NewAllRows);
    _Rows = MoveTemp(NewRows);
    _UiRows = MoveTemp(UiRows);
    if (const auto Table = Get_AuthoredTable(); Table.IsValid()) { Table->TryRefresh(); }
    if (_ListView.IsValid() && StructureChanged) { _ListView->RequestListRefresh(); }
}

auto SCkTextureDebugger_TextureHealthTable::MatchesSearch(const FRow& Row, const FString& Needle) const -> bool
{
    if (Needle.IsEmpty()) { return true; }
    return Row.Health.DisplayName.Contains(Needle, ESearchCase::IgnoreCase) || Row.Health.AssetPath.ToString().Contains(Needle, ESearchCase::IgnoreCase)
        || Row.ComponentLabel.Contains(Needle, ESearchCase::IgnoreCase) || Row.MaterialLabel.Contains(Needle, ESearchCase::IgnoreCase)
        || Row.Provenance.Contains(Needle, ESearchCase::IgnoreCase) || ck_texture_debugger_texture_health_table::Get_StreamingText(Row.Health).ToString().Contains(Needle, ESearchCase::IgnoreCase);
}

auto SCkTextureDebugger_TextureHealthTable::Make_Row(const FCkTextureDebugger_ComponentRow& Component, const FCkTextureDebugger_MaterialSlotRow& Slot, const FCkTextureDebugger_TextureRow& Texture) const -> FRow
{
    using namespace ck_texture_debugger_texture_health_table;
    auto Result = FRow{};
    Result.Key.ComponentKey = FObjectKey{Component.NavigationTarget.Get()};
    Result.Key.ComponentPath = FSoftObjectPath{Component.NavigationTarget.Get()};
    Result.Key.SlotIndex = Slot.SlotIndex;
    Result.Key.MaterialKey = FObjectKey{Slot.NavigationTarget.Get()};
    Result.Key.MaterialPath = Slot.MaterialPath;
    Result.Key.TextureKey = FObjectKey{Texture.NavigationTarget.Get()};
    Result.Key.TexturePath = Texture.Health.AssetPath;
    Result.Component = Component.NavigationTarget; Result.Texture = Texture.NavigationTarget;
    Result.ComponentLabel = FString::Printf(TEXT("%s · %s"), *Component.ActorDisplayName, *Component.ComponentDisplayName);
    Result.MaterialLabel = FString::Printf(TEXT("%s · slot %d"), *Slot.DisplayName, Slot.SlotIndex);
    Result.Provenance = FString::Printf(TEXT("Runtime-used · slot %d · %s"), Slot.SlotIndex, *Slot.DisplayName);
    Result.Health = Texture.Health; Result.IsContextComponent = _SelectedComponent.IsValid() && Component.NavigationTarget.Get() == _SelectedComponent.Get();
    return Result;
}

auto SCkTextureDebugger_TextureHealthTable::Make_Selection(const FRow& Row) const -> FCkTextureDebugger_TextureHealthSelection
{
    auto Result = FCkTextureDebugger_TextureHealthSelection{};
    Result.Component = Row.Component; Result.Texture = Row.Texture; Result.SlotIndex = Row.Key.SlotIndex; Result.DisplayName = Row.Health.DisplayName;
    Result.TexturePath = Row.Health.AssetPath; Result.Provenance = Row.Provenance; Result.Details = Make_Details(Row); Result.Health = Row.Health;
    return Result;
}

auto SCkTextureDebugger_TextureHealthTable::Make_Details(const FRow& Row) const -> FString
{
    const auto& Health = Row.Health;
    const auto Streaming = Health.HasStreamingMetrics ? FString::Printf(TEXT("Resident %d / requested %d / max %d mips"), Health.ResidentMipCount, Health.RequestedMipCount, Health.MaxMipCount) : TEXT("Streaming metrics unavailable for this texture");
    return FString::Printf(TEXT("%s\n%s\n%s\nCooked %d x %d · %d mips · %s\nFormat %s · group %s\nResident %lld bytes · dedicated video %lld bytes\n%s"), *Health.DisplayName, *Health.AssetPath.ToString(), *Row.Provenance, Health.CookedWidth, Health.CookedHeight, Health.MipCount, *Health.ClassName, *Health.FormatName, *Health.LodGroupName, Health.ResidentBytes, Health.DedicatedVideoBytes, *Streaming);
}

auto SCkTextureDebugger_TextureHealthTable::Set_PreviewTexture(TWeakObjectPtr<UTexture> Texture) -> void
{
    _PreviewBrush.SetResourceObject(nullptr); _PreviewTextureRoot.Reset();
    if (Texture.IsValid()) { _PreviewTextureRoot = TStrongObjectPtr<UTexture>{Texture.Get()}; _PreviewBrush.SetResourceObject(_PreviewTextureRoot.Get()); }
}

auto SCkTextureDebugger_TextureHealthTable::Clear_PreviewTexture() -> void { _PreviewBrush.SetResourceObject(nullptr); _PreviewTextureRoot.Reset(); }

auto SCkTextureDebugger_TextureHealthTable::Find_Row(const FRowKey& Key) const -> TSharedPtr<FRow>
{
    const auto* Found = _Rows.FindByPredicate([&Key](const TSharedPtr<FRow>& Row) { return Row.IsValid() && Row->Key == Key; });
    return Found == nullptr ? TSharedPtr<FRow>{} : *Found;
}

auto SCkTextureDebugger_TextureHealthTable::Get_EmptyStateText() const -> FText
{
    return _TotalRowCount == 0
        ? LOCTEXT("EmptyWorld", "No runtime-used textures are present in the active loaded world.\nWorld Partition cells and assets are never loaded for this audit.")
        : LOCTEXT("EmptyFilter", "No texture rows match the current filter.\nClear or broaden Filter to return to the loaded-world inventory.");
}

auto SCkTextureDebugger_TextureHealthTable::Get_FilterText() const -> FText
{
    return FText::FromString(_FilterString);
}

auto SCkTextureDebugger_TextureHealthTable::Get_HighlightText() const -> FText
{
    return FText::FromString(_HighlightString);
}

auto SCkTextureDebugger_TextureHealthTable::Get_RowCountText() const -> FText
{
    return FText::FromString(FString::Printf(TEXT("%d/%d rows"), Get_VisibleRowCount(), Get_TotalRowCount()));
}

auto SCkTextureDebugger_TextureHealthTable::Get_SelectedDetailsText() const -> FText
{
    if (NOT _SelectedKey.IsSet()) { return LOCTEXT("NoSelection", "Select a loaded runtime texture to preview its current resource and streaming facts."); }
    const auto Row = Find_Row(_SelectedKey.GetValue());
    return Row.IsValid() ? FText::FromString(Make_Details(*Row)) : LOCTEXT("SelectionGone", "The selected texture is no longer part of the active loaded-world snapshot.");
}

auto SCkTextureDebugger_TextureHealthTable::Get_PreviewBrush() const -> const FSlateBrush* { return _PreviewTextureRoot.Get() != nullptr ? &_PreviewBrush : FStyleDefaults::GetNoBrush(); }

auto SCkTextureDebugger_TextureHealthTable::Has_Selection() const -> bool
{
    return _SelectedKey.IsSet();
}

auto SCkTextureDebugger_TextureHealthTable::On_FilterTextChanged(const FText& InText) -> void
{
    const FString Text = InText.ToString();
    if (_FilterString == Text) { return; }
    _FilterString = Text;
    Rebuild_Rows();
    Reconcile_Selection();
}

auto SCkTextureDebugger_TextureHealthTable::On_HighlightTextChanged(const FText& InText) -> void
{
    const FString Text = InText.ToString();
    if (_HighlightString == Text) { return; }
    _HighlightString = Text;
    Rebuild_Rows();
    Reconcile_Selection();
}

auto SCkTextureDebugger_TextureHealthTable::OnGenerateRow(TSharedPtr<FRow> Item, const TSharedRef<STableViewBase>& Owner) -> TSharedRef<ITableRow>
{ return SNew(ck_texture_debugger_texture_health_table::SRow, Owner).Row(MoveTemp(Item)); }

auto SCkTextureDebugger_TextureHealthTable::OnSelectionChanged(TSharedPtr<FRow> Item, ESelectInfo::Type SelectInfo) -> void
{
    if (SelectInfo == ESelectInfo::Direct) { return; }
    if (NOT Item.IsValid()) { Clear_Selection(); return; }
    _SelectedKey = Item->Key; Set_PreviewTexture(Item->Texture); _OnSelectionChanged.ExecuteIfBound(Make_Selection(*Item));
}

auto SCkTextureDebugger_TextureHealthTable::OnContextMenuOpening() -> TSharedPtr<SWidget>
{
    if (!_SelectedKey.IsSet()) { return nullptr; }
    const TSharedPtr<FRow> Selected = Find_Row(_SelectedKey.GetValue());
    if (!Selected.IsValid()) { return nullptr; }
    auto Menu = FMenuBuilder(true, nullptr);
    ck::DebugCopyMenu::AddCopyEntry(Menu, LOCTEXT("CopyDetails", "Copy texture details"), LOCTEXT("CopyDetailsTip", "Copy the selected texture's loaded-world health facts."), Make_Details(*Selected));
    return Menu.MakeWidget();
}

auto SCkTextureDebugger_TextureHealthTable::On_CopyTextureDetails(const FString& InKey) -> void
{
    const TWeakPtr<FRow>* Found = _UiRows.Find(InKey);
    const TSharedPtr<FRow> Row = Found != nullptr ? Found->Pin() : nullptr;
    if (!Row.IsValid() || Find_Row(Row->Key) != Row) { return; }
    FPlatformApplicationMisc::ClipboardCopy(*Make_Details(*Row));
}

auto SCkTextureDebugger_TextureHealthTable::On_AuthoredSelectionChanged(TOptional<FString> InKey, ESelectInfo::Type InSelectInfo) -> void
{
    if (!InKey.IsSet()) { Clear_Selection(); return; }
    const TWeakPtr<FRow>* Found = _UiRows.Find(InKey.GetValue());
    const TSharedPtr<FRow> Row = Found != nullptr ? Found->Pin() : nullptr;
    if (!Row.IsValid()) { return; }
    // Direct notifications may represent a generic table selection clear; selecting a key here
    // is a user action or an explicitly notifying caller, never an echo from Reconcile_Selection.
    _SelectedKey = Row->Key;
    Set_PreviewTexture(Row->Texture);
    _OnSelectionChanged.ExecuteIfBound(Make_Selection(*Row));
}

#undef LOCTEXT_NAMESPACE
