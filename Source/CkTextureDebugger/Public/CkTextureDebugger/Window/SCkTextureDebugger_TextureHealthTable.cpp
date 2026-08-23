#include "CkTextureDebugger/Window/SCkTextureDebugger_TextureHealthTable.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Texture.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
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

auto
    SCkTextureDebugger_TextureHealthTable::
    Construct(
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

    _TotalRowCount = Aggregate.Num();
    auto NewAllRows = TArray<TSharedPtr<FRow>>{};
    NewAllRows.Reserve(Aggregate.Num());
    for (auto& Pair : Aggregate)
    {
        auto Row = TSharedPtr<FRow>{};
        if (const auto* Found = Existing.Find(Pair.Key))
        {
            Row = *Found;
            *Row = Pair.Value;
            Existing.Remove(Pair.Key);
        }
        else
        { Row = MakeShared<FRow>(MoveTemp(Pair.Value)); }
        NewAllRows.Add(MoveTemp(Row));
    }

    NewAllRows.Sort([](const TSharedPtr<FRow>& A, const TSharedPtr<FRow>& B)
    {
        if (A->Key.ComponentPath != B->Key.ComponentPath) { return A->Key.ComponentPath.ToString() < B->Key.ComponentPath.ToString(); }
        if (A->Key.SlotIndex != B->Key.SlotIndex) { return A->Key.SlotIndex < B->Key.SlotIndex; }
        if (A->Key.MaterialPath != B->Key.MaterialPath) { return A->Key.MaterialPath.ToString() < B->Key.MaterialPath.ToString(); }
        return A->Key.TexturePath.ToString() < B->Key.TexturePath.ToString();
    });

    auto NewRows = TArray<TSharedPtr<FRow>>{};
    NewRows.Reserve(NewAllRows.Num());
    for (const auto& Row : NewAllRows)
    {
        if (Row.IsValid() && MatchesSearch(*Row, _FilterString))
        {
            Row->IsHighlightMatch = MatchesSearch(*Row, _HighlightString);
            NewRows.Add(Row);
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

auto SCkTextureDebugger_TextureHealthTable::Get_SelectedDetailsText() const -> FText
{
    if (NOT _SelectedKey.IsSet()) { return LOCTEXT("NoSelection", "Select a loaded runtime texture to preview its current resource and streaming facts."); }
    const auto Row = Find_Row(_SelectedKey.GetValue());
    return Row.IsValid() ? FText::FromString(Make_Details(*Row)) : LOCTEXT("SelectionGone", "The selected texture is no longer part of the active loaded-world snapshot.");
}

auto SCkTextureDebugger_TextureHealthTable::Get_PreviewBrush() const -> const FSlateBrush* { return _PreviewTextureRoot.Get() != nullptr ? &_PreviewBrush : FStyleDefaults::GetNoBrush(); }

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
    if (NOT _ListView.IsValid()) { return nullptr; }
    const auto Selected = _ListView->GetSelectedItems();
    if (Selected.Num() != 1 || NOT Selected[0].IsValid()) { return nullptr; }
    auto Menu = FMenuBuilder(true, nullptr);
    ck::DebugCopyMenu::AddCopyEntry(Menu, LOCTEXT("CopyDetails", "Copy texture details"), LOCTEXT("CopyDetailsTip", "Copy the selected texture's loaded-world health facts."), Make_Details(*Selected[0]));
    return Menu.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
