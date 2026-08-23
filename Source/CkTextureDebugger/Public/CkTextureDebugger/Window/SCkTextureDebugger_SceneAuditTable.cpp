#include "CkTextureDebugger/Window/SCkTextureDebugger_SceneAuditTable.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Components/PrimitiveComponent.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkTextureDebugger_SceneAuditTable"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_texture_audit
{
    const auto Actor = FName{TEXT("Actor")};
    const auto Component = FName{TEXT("Component")};
    const auto Kind = FName{TEXT("Kind")};
    const auto Slots = FName{TEXT("Slots")};
    const auto Textures = FName{TEXT("Textures")};
    const auto State = FName{TEXT("State")};

    constexpr auto ShowSelection = true;
    constexpr auto ShowStatusDot = false;
    constexpr auto ClearSelectionOnClick = true;

    auto
        Get_KindText(
            ECkTextureDebugger_ComponentKind InKind) -> FString
    {
        switch (InKind)
        {
            case ECkTextureDebugger_ComponentKind::StaticMesh:
            {
                return TEXT("Static mesh");
            }
            case ECkTextureDebugger_ComponentKind::SkeletalMesh:
            {
                return TEXT("Skeletal mesh");
            }
            case ECkTextureDebugger_ComponentKind::InstancedStaticMesh:
            {
                return TEXT("Instanced mesh");
            }
            case ECkTextureDebugger_ComponentKind::HierarchicalInstancedStaticMesh:
            {
                return TEXT("HISM");
            }
            case ECkTextureDebugger_ComponentKind::FoliageInstancedStaticMesh:
            {
                return TEXT("Foliage ISM");
            }
            default:
            {
                return TEXT("Other primitive");
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_Tone(
            const FCkTextureDebugger_ComponentRow& InComponent) -> ECk_Tone
    {
        if (InComponent.HasComponentSlotOverlay)
        {
            return ECk_Tone::Err;
        }

        if (InComponent.InstanceCount > 0)
        {
            return ECk_Tone::Warn;
        }

        return InComponent.SupportsCheckerOverride
            ? ECk_Tone::Ok
            : ECk_Tone::Neutral;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_StateText(
            const FCkTextureDebugger_ComponentRow& InComponent) -> FString
    {
        auto Parts = TArray<FString>{};
        if (InComponent.HasComponentSlotOverlay)
        {
            Parts.Add(TEXT("Blocked: slot overlay"));
        }

        if (InComponent.InstanceCount > 0)
        {
            Parts.Add(FString::Printf(
                TEXT("Component-wide: %d instances"),
                InComponent.InstanceCount));
        }

        if (Parts.IsEmpty())
        {
            Parts.Add(InComponent.SupportsCheckerOverride
                ? TEXT("Checker-capable")
                : TEXT("Not a mesh target"));
        }

        return FString::Join(Parts, TEXT(" · "));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_Row(
            const FCkTextureDebugger_ComponentRow& InComponent)
        -> SCkTextureDebugger_SceneAuditTable::FRow
    {
        auto Result = SCkTextureDebugger_SceneAuditTable::FRow{};
        Result.Key.Component = FObjectKey{InComponent.NavigationTarget.Get()};
        Result.Key.ComponentPath = InComponent.NavigationTarget.IsValid()
            ? InComponent.NavigationTarget->GetPathName()
            : InComponent.ActorPath.ToString();
        Result.ActorPath = InComponent.ActorPath.ToString();
        Result.Actor = InComponent.ActorDisplayName;
        Result.Component = InComponent.ComponentDisplayName;
        Result.ClassName = InComponent.ComponentClassName;
        Result.Kind = Get_KindText(InComponent.Kind);
        Result.State = Get_StateText(InComponent);
        Result.Tone = Get_Tone(InComponent);
        Result.Slots = InComponent.MaterialSlots.Num();
        Result.Target = InComponent.NavigationTarget;

        for (const auto& Slot : InComponent.MaterialSlots)
        {
            Result.Textures += Slot.Textures.Num();
        }

        Result.Search = FString::Printf(
            TEXT("%s\n%s\n%s\n%s\n%s\n%s\n%s"),
            *Result.Actor,
            *Result.Component,
            *Result.ClassName,
            *Result.ActorPath,
            *Result.Key.ComponentPath,
            *Result.Kind,
            *Result.State);
        return Result;
    }

    // ----------------------------------------------------------------------------------------------------------------

    class SRow final : public SMultiColumnTableRow<TSharedPtr<SCkTextureDebugger_SceneAuditTable::FRow>>
    {
    public:
        SLATE_BEGIN_ARGS(SRow) {}
            SLATE_ARGUMENT(TSharedPtr<SCkTextureDebugger_SceneAuditTable::FRow>, Row)
        SLATE_END_ARGS()

        auto Construct(
            const FArguments& InArgs,
            const TSharedRef<STableViewBase>& InOwnerTable) -> void;

        virtual auto GenerateWidgetForColumn(
            const FName& InColumnName) -> TSharedRef<SWidget> override;

    private:
        TSharedPtr<SCkTextureDebugger_SceneAuditTable::FRow> _Row;
    };

    auto
        SRow::
        Construct(
            const FArguments& InArgs,
            const TSharedRef<STableViewBase>& InOwnerTable) -> void
    {
        _Row = InArgs._Row;
        FSuperRowType::Construct(
            FSuperRowType::FArguments()
                .ShowSelection(ShowSelection)
                .Padding(FMargin{0.0f, 1.0f}),
            InOwnerTable);
    }

    auto
        SRow::
        GenerateWidgetForColumn(
            const FName& InColumnName) -> TSharedRef<SWidget>
    {
        const auto WeakRow = TWeakPtr<SCkTextureDebugger_SceneAuditTable::FRow>{_Row};
        if (InColumnName == State)
        {
            return SNew(SCkDebug_StatusPill)
                .Text_Lambda([WeakRow]()
                {
                    const auto Row = WeakRow.Pin();
                    return Row.IsValid()
                        ? FText::FromString(Row->State)
                        : FText::GetEmpty();
                })
                .Tone_Lambda([WeakRow]()
                {
                    const auto Row = WeakRow.Pin();
                    return Row.IsValid()
                        ? Row->Tone
                        : ECk_Tone::Neutral;
                })
                .ShowDot(ShowStatusDot);
        }

        const auto GetText = [WeakRow, InColumnName]() -> FText
        {
            const auto Row = WeakRow.Pin();
            if (NOT Row.IsValid())
            {
                return FText::GetEmpty();
            }

            if (InColumnName == Actor)
            {
                return FText::FromString(Row->Actor);
            }

            if (InColumnName == Component)
            {
                return FText::FromString(Row->Component);
            }

            if (InColumnName == Kind)
            {
                return FText::FromString(Row->Kind);
            }

            if (InColumnName == Slots)
            {
                return FText::AsNumber(Row->Slots);
            }

            return FText::AsNumber(Row->Textures);
        };

        const auto GetTooltip = [WeakRow, InColumnName]() -> FText
        {
            const auto Row = WeakRow.Pin();
            if (NOT Row.IsValid())
            {
                return FText::GetEmpty();
            }

            if (InColumnName == Actor)
            {
                return FText::FromString(Row->ActorPath);
            }

            if (InColumnName == Component)
            {
                return FText::FromString(Row->Key.ComponentPath);
            }

            return FText::FromString(Row->ClassName);
        };

        const auto GetTextColor = [WeakRow]() -> FSlateColor
        {
            const auto Row = WeakRow.Pin();
            return FSlateColor{Row.IsValid() && NOT Row->bHighlight
                ? CkStyle::TextMute()
                : CkStyle::Text()};
        };

        return SNew(STextBlock)
            .Text_Lambda(GetText)
            .ToolTipText_Lambda(GetTooltip)
            .ColorAndOpacity_Lambda(GetTextColor);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_SceneAuditTable::
    Construct(
        const FArguments& InArgs) -> void
{
    _OnSelected = InArgs._OnComponentSelected;

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SNew(SCkDebug_DualSearchBar)
                .FilterHintText(LOCTEXT("Filter", "Filter loaded components…"))
                .HighlightHintText(LOCTEXT("Highlight", "Highlight…"))
                .OnFilterTextChanged(this, &SCkTextureDebugger_SceneAuditTable::OnFilter)
                .OnHighlightTextChanged(this, &SCkTextureDebugger_SceneAuditTable::OnHighlight)
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_CountBadge)
                .ValueText_Lambda([this]()
                {
                    return GetCountText();
                })
                .SuffixText(LOCTEXT("Shown", "shown"))
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("Clear", "Clear selection"))
                .IsEnabled_Lambda([this]()
                {
                    return _Selected.IsValid();
                })
                .OnClicked(this, &SCkTextureDebugger_SceneAuditTable::OnClear)
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [
            SNew(SCkDebug_StatusPill)
            .Text_Lambda([this]()
            {
                return GetSelectionText();
            })
            .Tone_Lambda([this]()
            {
                return _Selected.IsValid()
                    ? ECk_Tone::Ok
                    : ECk_Tone::Neutral;
            })
            .ShowDot(ck_texture_audit::ShowStatusDot)
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                SAssignNew(_List, SListView<TSharedPtr<FRow>>)
                .ListItemsSource(&_Visible)
                .OnGenerateRow(this, &SCkTextureDebugger_SceneAuditTable::OnGenerateRow)
                .OnSelectionChanged(this, &SCkTextureDebugger_SceneAuditTable::OnSelectionChanged)
                .OnContextMenuOpening(this, &SCkTextureDebugger_SceneAuditTable::OnContextMenu)
                .SelectionMode(ESelectionMode::Single)
                .ClearSelectionOnClick(ck_texture_audit::ClearSelectionOnClick)
                .HeaderRow(
                    SNew(SHeaderRow)
                    + SHeaderRow::Column(ck_texture_audit::Actor)
                        .DefaultLabel(LOCTEXT("Actor", "Actor"))
                        .FillWidth(0.22f)
                    + SHeaderRow::Column(ck_texture_audit::Component)
                        .DefaultLabel(LOCTEXT("Component", "Component"))
                        .FillWidth(0.23f)
                    + SHeaderRow::Column(ck_texture_audit::Kind)
                        .DefaultLabel(LOCTEXT("Kind", "Kind"))
                        .FillWidth(0.16f)
                    + SHeaderRow::Column(ck_texture_audit::Slots)
                        .DefaultLabel(LOCTEXT("Slots", "Slots"))
                        .FixedWidth(56.0f)
                    + SHeaderRow::Column(ck_texture_audit::Textures)
                        .DefaultLabel(LOCTEXT("Textures", "Textures"))
                        .FixedWidth(76.0f)
                    + SHeaderRow::Column(ck_texture_audit::State)
                        .DefaultLabel(LOCTEXT("State", "Audit state"))
                        .FillWidth(0.39f))
            ]
            + SOverlay::Slot()
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([this]()
                {
                    return GetEmptyText();
                })
                .Visibility_Lambda([this]()
                {
                    return _Visible.IsEmpty()
                        ? EVisibility::Visible
                        : EVisibility::Collapsed;
                })
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            ]
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_SceneAuditTable::
    SetSnapshot(
        const FCkTextureDebugger_LoadedWorldSnapshot& InSnapshot) -> void
{
    Reconcile(InSnapshot);
}

auto
    SCkTextureDebugger_SceneAuditTable::
    SetSelectedComponent(
        TWeakObjectPtr<UPrimitiveComponent> InComponent) -> void
{
    _Selected = InComponent;
    RestoreSelection();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_SceneAuditTable::
    Get_VisibleRowCount() const -> int32
{
    return _Visible.Num();
}

auto
    SCkTextureDebugger_SceneAuditTable::
    Get_TotalRowCount() const -> int32
{
    return _All.Num();
}

auto
    SCkTextureDebugger_SceneAuditTable::
    Get_SelectedRowCount() const -> int32
{
    const auto* SelectedComponent = _Selected.Get();
    if (SelectedComponent == nullptr)
    {
        return 0;
    }

    const auto ContainsSelectedComponent = _Visible.ContainsByPredicate(
        [SelectedComponent](const TSharedPtr<FRow>& InItem)
        {
            return InItem.IsValid() && InItem->Target.Get() == SelectedComponent;
        });
    return ContainsSelectedComponent ? 1 : 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_SceneAuditTable::
    Reconcile(
        const FCkTextureDebugger_LoadedWorldSnapshot& InSnapshot) -> void
{
    _HasWorld = InSnapshot.World.IsValid();

    auto ExistingRows = TMap<FRowKey, TSharedPtr<FRow>>{};
    for (const auto& Item : _All)
    {
        if (Item.IsValid())
        {
            ExistingRows.Add(Item->Key, Item);
        }
    }

    auto NewRows = TArray<TSharedPtr<FRow>>{};
    for (const auto& ComponentRow : InSnapshot.Components)
    {
        auto NewValue = ck_texture_audit::Make_Row(ComponentRow);
        const auto* Existing = ExistingRows.Find(NewValue.Key);
        auto Item = Existing != nullptr
            ? *Existing
            : MakeShared<FRow>();
        *Item = MoveTemp(NewValue);
        NewRows.Add(Item);
    }

    NewRows.Sort([](const TSharedPtr<FRow>& InLeft, const TSharedPtr<FRow>& InRight)
    {
        const auto ActorComparison = InLeft->ActorPath.Compare(
            InRight->ActorPath,
            ESearchCase::CaseSensitive);
        if (ActorComparison != 0)
        {
            return ActorComparison < 0;
        }

        return InLeft->Key.ComponentPath.Compare(
            InRight->Key.ComponentPath,
            ESearchCase::CaseSensitive) < 0;
    });

    _All = MoveTemp(NewRows);
    RebuildVisible();
}

auto
    SCkTextureDebugger_SceneAuditTable::
    RebuildVisible() -> void
{
    auto NewVisible = TArray<TSharedPtr<FRow>>{};
    for (const auto& Item : _All)
    {
        Item->bHighlight = _Highlight.IsEmpty() || Matches(*Item, _Highlight);
        if (Matches(*Item, _Filter))
        {
            NewVisible.Add(Item);
        }
    }

    auto StructureChanged = _Visible.Num() != NewVisible.Num();
    if (NOT StructureChanged)
    {
        for (auto Index = 0; Index < _Visible.Num(); ++Index)
        {
            if (_Visible[Index] != NewVisible[Index])
            {
                StructureChanged = true;
                break;
            }
        }
    }

    _Visible = MoveTemp(NewVisible);
    if (StructureChanged && _List.IsValid())
    {
        _List->RequestListRefresh();
    }

    RestoreSelection();
}

auto
    SCkTextureDebugger_SceneAuditTable::
    RestoreSelection() -> void
{
    if (NOT _List.IsValid())
    {
        return;
    }

    auto* SelectedComponent = _Selected.Get();
    const auto* SelectedItem = SelectedComponent != nullptr
        ? _Visible.FindByPredicate([SelectedComponent](const TSharedPtr<FRow>& InItem)
        {
            return InItem.IsValid() && InItem->Target.Get() == SelectedComponent;
        })
        : nullptr;

    if (SelectedItem == nullptr)
    {
        _List->ClearSelection();
        return;
    }

    const auto CurrentSelection = _List->GetSelectedItems();
    const auto IsAlreadySelected = CurrentSelection.Num() == 1 && CurrentSelection[0] == *SelectedItem;
    if (NOT IsAlreadySelected)
    {
        _List->SetSelection(*SelectedItem, ESelectInfo::Direct);
    }
}

auto
    SCkTextureDebugger_SceneAuditTable::
    ClearSelection(
        bool InNotify) -> void
{
    const auto HadSelection = _Selected.IsValid();
    _Selected.Reset();

    if (_List.IsValid())
    {
        _List->ClearSelection();
    }

    if (InNotify && HadSelection && _OnSelected)
    {
        _OnSelected({});
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_SceneAuditTable::
    Matches(
        const FRow& InRow,
        const FString& InQuery) const -> bool
{
    return InQuery.IsEmpty() || InRow.Search.Contains(InQuery, ESearchCase::IgnoreCase);
}

auto
    SCkTextureDebugger_SceneAuditTable::
    GetEmptyText() const -> FText
{
    if (NOT _HasWorld)
    {
        return LOCTEXT("NoWorld", "No active Editor, PIE, or game world is selected.");
    }

    if (_All.IsEmpty())
    {
        return LOCTEXT("NoRows", "No registered primitive components are loaded.");
    }

    return LOCTEXT("NoMatches", "No loaded components match the filter.");
}

auto
    SCkTextureDebugger_SceneAuditTable::
    GetCountText() const -> FText
{
    return FText::FromString(FString::Printf(
        TEXT("%d / %d"),
        _Visible.Num(),
        _All.Num()));
}

auto
    SCkTextureDebugger_SceneAuditTable::
    GetSelectionText() const -> FText
{
    const auto* SelectedComponent = _Selected.Get();
    if (SelectedComponent == nullptr)
    {
        return LOCTEXT("None", "No component selected");
    }

    const auto* SelectedRow = _All.FindByPredicate(
        [SelectedComponent](const TSharedPtr<FRow>& InItem)
        {
            return InItem.IsValid() && InItem->Target.Get() == SelectedComponent;
        });
    return SelectedRow != nullptr && SelectedRow->IsValid()
        ? FText::FromString(FString::Printf(
            TEXT("Selected: %s"),
            *(*SelectedRow)->Component))
        : LOCTEXT(
            "SelectedUnavailable",
            "Selected component is no longer in the loaded-world audit.");
}

auto
    SCkTextureDebugger_SceneAuditTable::
    MakeCopyText(
        const FRow& InRow) const -> FString
{
    return FString::Printf(
        TEXT("Actor: %s\nActor path: %s\nComponent: %s\nComponent path: %s\nClass: %s\nKind: %s\nAudit state: %s\nMaterial slots: %d\nRuntime-used textures: %d"),
        *InRow.Actor,
        *InRow.ActorPath,
        *InRow.Component,
        *InRow.Key.ComponentPath,
        *InRow.ClassName,
        *InRow.Kind,
        *InRow.State,
        InRow.Slots,
        InRow.Textures);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkTextureDebugger_SceneAuditTable::
    OnFilter(
        const FString& InText) -> void
{
    if (_Filter == InText)
    {
        return;
    }

    _Filter = InText;
    RebuildVisible();
}

auto
    SCkTextureDebugger_SceneAuditTable::
    OnHighlight(
        const FString& InText) -> void
{
    if (_Highlight == InText)
    {
        return;
    }

    _Highlight = InText;
    RebuildVisible();
}

auto
    SCkTextureDebugger_SceneAuditTable::
    OnClear() -> FReply
{
    constexpr auto NotifySelectionChanged = true;
    ClearSelection(NotifySelectionChanged);
    return FReply::Handled();
}

auto
    SCkTextureDebugger_SceneAuditTable::
    OnContextMenu() -> TSharedPtr<SWidget>
{
    if (NOT _List.IsValid())
    {
        return nullptr;
    }

    const auto SelectedItems = _List->GetSelectedItems();
    if (SelectedItems.Num() != 1 || NOT SelectedItems[0].IsValid())
    {
        return nullptr;
    }

    constexpr auto CloseAfterSelection = true;
    auto Menu = FMenuBuilder{CloseAfterSelection, nullptr};
    ck::DebugCopyMenu::AddCopyEntry(
        Menu,
        LOCTEXT("CopyPath", "Copy component path"),
        LOCTEXT("CopyPathTip", "Copy the runtime component path."),
        SelectedItems[0]->Key.ComponentPath);
    ck::DebugCopyMenu::AddCopyEntry(
        Menu,
        LOCTEXT("CopyAll", "Copy audit summary"),
        LOCTEXT("CopyAllTip", "Copy copied loaded-world facts."),
        MakeCopyText(*SelectedItems[0]));
    return Menu.MakeWidget();
}

auto
    SCkTextureDebugger_SceneAuditTable::
    OnGenerateRow(
        TSharedPtr<FRow> InItem,
        const TSharedRef<STableViewBase>& InOwner) -> TSharedRef<ITableRow>
{
    return SNew(ck_texture_audit::SRow, InOwner)
        .Row(MoveTemp(InItem));
}

auto
    SCkTextureDebugger_SceneAuditTable::
    OnSelectionChanged(
        TSharedPtr<FRow> InItem,
        ESelectInfo::Type InType) -> void
{
    if (InType == ESelectInfo::Direct)
    {
        return;
    }

    if (NOT InItem.IsValid())
    {
        constexpr auto NotifySelectionChanged = true;
        ClearSelection(NotifySelectionChanged);
        return;
    }

    _Selected = InItem->Target;
    if (_OnSelected)
    {
        _OnSelected(_Selected);
    }
}

#undef LOCTEXT_NAMESPACE
