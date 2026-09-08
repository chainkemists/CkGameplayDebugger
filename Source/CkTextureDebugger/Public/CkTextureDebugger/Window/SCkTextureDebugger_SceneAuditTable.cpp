#include "CkTextureDebugger/Window/SCkTextureDebugger_SceneAuditTable.h"

#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/SCkUiTable.h"

#include "Components/PrimitiveComponent.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCkTextureDebugger_SceneAuditTable"

namespace ck_texture_audit
{
    auto Get_KindText(const ECkTextureDebugger_ComponentKind InKind) -> FString
    {
        switch (InKind)
        {
        case ECkTextureDebugger_ComponentKind::StaticMesh:
            return TEXT("Static mesh");
        case ECkTextureDebugger_ComponentKind::SkeletalMesh:
            return TEXT("Skeletal mesh");
        case ECkTextureDebugger_ComponentKind::InstancedStaticMesh:
            return TEXT("Instanced mesh");
        case ECkTextureDebugger_ComponentKind::HierarchicalInstancedStaticMesh:
            return TEXT("HISM");
        case ECkTextureDebugger_ComponentKind::FoliageInstancedStaticMesh:
            return TEXT("Foliage ISM");
        default:
            return TEXT("Other primitive");
        }
    }

    auto Get_Tone(const FCkTextureDebugger_ComponentRow& InComponent) -> ECk_Tone
    {
        if (InComponent.HasComponentSlotOverlay)
        {
            return ECk_Tone::Err;
        }
        if (InComponent.InstanceCount > 0)
        {
            return ECk_Tone::Warn;
        }
        return InComponent.SupportsCheckerOverride ? ECk_Tone::Ok : ECk_Tone::Neutral;
    }

    auto Get_StateText(const FCkTextureDebugger_ComponentRow& InComponent) -> FString
    {
        auto Parts = TArray<FString>{};
        if (InComponent.HasComponentSlotOverlay)
        {
            Parts.Add(TEXT("Blocked: slot overlay"));
        }
        if (InComponent.InstanceCount > 0)
        {
            Parts.Add(FString::Printf(TEXT("Component-wide: %d instances"), InComponent.InstanceCount));
        }
        if (Parts.IsEmpty())
        {
            Parts.Add(InComponent.SupportsCheckerOverride ? TEXT("Checker-capable") : TEXT("Not a mesh target"));
        }
        return FString::Join(Parts, TEXT(" · "));
    }

    auto Make_Row(const FCkTextureDebugger_ComponentRow& InComponent) -> SCkTextureDebugger_SceneAuditTable::FRow
    {
        auto Result = SCkTextureDebugger_SceneAuditTable::FRow{};
        Result.Key.Component = FObjectKey{InComponent.NavigationTarget.Get()};
        Result.Key.ComponentPath =
            InComponent.NavigationTarget.IsValid() ? InComponent.NavigationTarget->GetPathName() : InComponent.ActorPath.ToString();
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
        Result.Search = FString::Printf(TEXT("%s\n%s\n%s\n%s\n%s\n%s\n%s"), *Result.Actor, *Result.Component, *Result.ClassName,
                                        *Result.ActorPath, *Result.Key.ComponentPath, *Result.Kind, *Result.State);
        return Result;
    }

    auto UiSchema() -> TArray<FCkUiFieldSchema>
    {
        return {{TEXT("actor"), ECkUiFieldKind::Text},
                {TEXT("component"), ECkUiFieldKind::Text},
                {TEXT("kind"), ECkUiFieldKind::Text},
                {TEXT("slots"), ECkUiFieldKind::Text},
                {TEXT("textures"), ECkUiFieldKind::Text},
                {TEXT("state"), ECkUiFieldKind::Text},
                {TEXT("actor-tip"), ECkUiFieldKind::Text},
                {TEXT("component-tip"), ECkUiFieldKind::Text},
                {TEXT("class-tip"), ECkUiFieldKind::Text},
                {TEXT("text-color"), ECkUiFieldKind::Color},
                {TEXT("state-foreground"), ECkUiFieldKind::Color},
                {TEXT("state-background"), ECkUiFieldKind::Color}};
    }

    auto UiRecord(const SCkTextureDebugger_SceneAuditTable::FRow& InRow) -> FCkUiRecordData
    {
        auto Result = FCkUiRecordData{};
        Result.Key = InRow.UiKey;
        const auto Text = [&Result](const TCHAR* InName, FText InValue)
        { Result.Fields.Add(InName, FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = MoveTemp(InValue)}); };
        const auto Color = [&Result](const TCHAR* InName, const FLinearColor InValue)
        { Result.Fields.Add(InName, FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = InValue}); };
        Text(TEXT("actor"), FText::FromString(InRow.Actor));
        Text(TEXT("component"), FText::FromString(InRow.Component));
        Text(TEXT("kind"), FText::FromString(InRow.Kind));
        Text(TEXT("slots"), FText::AsNumber(InRow.Slots));
        Text(TEXT("textures"), FText::AsNumber(InRow.Textures));
        Text(TEXT("state"), FText::FromString(InRow.State));
        Text(TEXT("actor-tip"), FText::FromString(InRow.ActorPath));
        Text(TEXT("component-tip"), FText::FromString(InRow.Key.ComponentPath));
        Text(TEXT("class-tip"), FText::FromString(InRow.ClassName));
        Color(TEXT("text-color"), InRow.bHighlight ? CkStyle::Text() : CkStyle::TextMute());
        Color(TEXT("state-foreground"), CkStyle::GetToneColor(InRow.Tone));
        Color(TEXT("state-background"), CkStyle::GetToneDimColor(InRow.Tone));
        return Result;
    }
} // namespace ck_texture_audit

auto SCkTextureDebugger_SceneAuditTable::Construct(const FArguments& InArgs) -> void
{
    _OnSelected = InArgs._OnComponentSelected;
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const FCkUiLoadResult CollectionResult = FCkUiCollection::TryCreate(ck_texture_audit::UiSchema(), _UiCollection);
    if (!RegistryResult.Succeeded || !CollectionResult.Succeeded)
    {
        _PublicationError = FString::Join(RegistryResult.Errors, TEXT("\n")) + FString::Join(CollectionResult.Errors, TEXT("\n"));
        _UiCollection.Reset();
        ChildSlot[SNew(STextBlock).Text(Get_LayoutError())];
        return;
    }
    const auto WeakTable = TWeakPtr<SCkTextureDebugger_SceneAuditTable>{SharedThis(this)};
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("clear-selection"), FSimpleDelegate::CreateLambda(
                                             [WeakTable]()
                                             {
                                                 if (const auto Table = WeakTable.Pin())
                                                 {
                                                     Table->ClearSelection(true);
                                                 }
                                             }));
    auto Tokens = FCkUiView::FTokens{};
    Tokens.Add(TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS));
    Tokens.Add(TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM));
    Tokens.Add(TEXT("--text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--text-strong"), TEXT("#") + CkStyle::TextStrong().ToFColorSRGB().ToHex());
    Tokens.Add(TEXT("--surface"), TEXT("#") + CkStyle::Bg2().ToFColorSRGB().ToHex());
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("filter"), TAttribute<FText>::CreateLambda(
                                      [WeakTable]
                                      {
                                          const auto Table = WeakTable.Pin();
                                          return Table.IsValid() ? FText::FromString(Table->_Filter) : FText::GetEmpty();
                                      }));
    Data.Text.Add(TEXT("highlight"), TAttribute<FText>::CreateLambda(
                                         [WeakTable]
                                         {
                                             const auto Table = WeakTable.Pin();
                                             return Table.IsValid() ? FText::FromString(Table->_Highlight) : FText::GetEmpty();
                                         }));
    Data.Text.Add(TEXT("count"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_SceneAuditTable::GetCountText));
    Data.Text.Add(TEXT("selection"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_SceneAuditTable::GetSelectionText));
    Data.Text.Add(TEXT("empty-state"), TAttribute<FText>::CreateSP(this, &SCkTextureDebugger_SceneAuditTable::GetEmptyText));
    Data.Text.Add(TEXT("filter-placeholder"), LOCTEXT("Filter", "Filter loaded components…"));
    Data.Text.Add(TEXT("highlight-placeholder"), LOCTEXT("Highlight", "Highlight…"));
    Data.Text.Add(TEXT("actor-column-label"), LOCTEXT("Actor", "Actor"));
    Data.Text.Add(TEXT("component-column-label"), LOCTEXT("Component", "Component"));
    Data.Text.Add(TEXT("kind-column-label"), LOCTEXT("Kind", "Kind"));
    Data.Text.Add(TEXT("slots-column-label"), LOCTEXT("Slots", "Slots"));
    Data.Text.Add(TEXT("textures-column-label"), LOCTEXT("Textures", "Textures"));
    Data.Text.Add(TEXT("state-column-label"), LOCTEXT("State", "Audit state"));
    Data.Text.Add(TEXT("copy-component-path-label"), LOCTEXT("CopyPath", "Copy component path"));
    Data.Text.Add(TEXT("copy-component-path-tip"), LOCTEXT("CopyPathTip", "Copy the runtime component path."));
    Data.Text.Add(TEXT("copy-audit-summary-label"), LOCTEXT("CopyAll", "Copy audit summary"));
    Data.Text.Add(TEXT("copy-audit-summary-tip"), LOCTEXT("CopyAllTip", "Copy copied loaded-world facts."));
    Data.Color.Add(TEXT("selection-foreground"),
                   TAttribute<FLinearColor>::CreateLambda(
                       [WeakTable]
                       {
                           const auto Table = WeakTable.Pin();
                           return CkStyle::GetToneColor(Table.IsValid() && Table->_Selected.IsValid() ? ECk_Tone::Ok : ECk_Tone::Neutral);
                       }));
    Data.Color.Add(TEXT("selection-background"), TAttribute<FLinearColor>::CreateLambda(
                                                     [WeakTable]
                                                     {
                                                         const auto Table = WeakTable.Pin();
                                                         return CkStyle::GetToneDimColor(Table.IsValid() && Table->_Selected.IsValid()
                                                                                             ? ECk_Tone::Ok
                                                                                             : ECk_Tone::Neutral);
                                                     }));
    Data.Visibility.Add(TEXT("has-selection"), TAttribute<bool>::CreateLambda(
                                                   [WeakTable]
                                                   {
                                                       const auto Table = WeakTable.Pin();
                                                       return Table.IsValid() && Table->_Selected.IsValid();
                                                   }));
    Data.Visibility.Add(TEXT("audit-empty"), TAttribute<bool>::CreateLambda(
                                                 [WeakTable]
                                                 {
                                                     const auto Table = WeakTable.Pin();
                                                     return Table.IsValid() && Table->_Visible.IsEmpty();
                                                 }));
    Data.Collections.Add(TEXT("audit"), _UiCollection);
    Data.TextChanged.Add(TEXT("filter"), FOnTextChanged::CreateLambda(
                                             [WeakTable](const FText& InText)
                                             {
                                                 if (const auto Table = WeakTable.Pin())
                                                 {
                                                     Table->OnFilter(InText.ToString());
                                                 }
                                             }));
    Data.TextChanged.Add(TEXT("highlight"), FOnTextChanged::CreateLambda(
                                                [WeakTable](const FText& InText)
                                                {
                                                    if (const auto Table = WeakTable.Pin())
                                                    {
                                                        Table->OnHighlight(InText.ToString());
                                                    }
                                                }));
    Data.TableSelectionChanged.Add(TEXT("select-component"), FOnCkUiTableSelectionChanged::CreateSP(
                                                                 this, &SCkTextureDebugger_SceneAuditTable::On_AuthoredSelectionChanged));
    Data.ContextActions.Add(TEXT("copy-component-path"),
                            FOnCkUiContextAction::CreateSP(this, &SCkTextureDebugger_SceneAuditTable::On_CopyComponentPath));
    Data.ContextActions.Add(TEXT("copy-audit-summary"),
                            FOnCkUiContextAction::CreateSP(this, &SCkTextureDebugger_SceneAuditTable::On_CopyAuditSummary));
    _LayoutView =
        FCkUiView::Create({}, MoveTemp(Actions), MoveTemp(Tokens), CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                                                    .Tag(TEXT("Ck.SceneAudit.LayoutError"))
                                                    .Text(this, &SCkTextureDebugger_SceneAuditTable::Get_LayoutError)
                                                    .ToolTipText(this, &SCkTextureDebugger_SceneAuditTable::Get_LayoutError)
                                                    .AutoWrapText(true)
                                                    .ColorAndOpacity(FSlateColor{CkStyle::Err()})
                                                    .Visibility_Lambda(
                                                        [WeakTable]
                                                        {
                                                            const auto Table = WeakTable.Pin();
                                                            return !Table.IsValid() || Table->Get_LayoutError().IsEmpty()
                                                                       ? EVisibility::Collapsed
                                                                       : EVisibility::Visible;
                                                        })] +
              SVerticalBox::Slot().FillHeight(1.0f)[_LayoutView->GetRegion(TEXT("main"))]];
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    const auto UiDirectory = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    Reload_LayoutFiles(FPaths::Combine(UiDirectory, TEXT("SceneAudit.ui.html")), FPaths::Combine(UiDirectory, TEXT("SceneAudit.ui.css")));
    RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SCkTextureDebugger_SceneAuditTable::Tick_LayoutFiles));
}

auto SCkTextureDebugger_SceneAuditTable::TryReload_Layout(const FString& InMarkup, const FString& InStylesheet) -> FCkUiLoadResult
{
    return _LayoutView.IsValid() ? _LayoutView->TryReload(InMarkup, InStylesheet, TEXT("SceneAudit"))
                                 : FCkUiLoadResult{false, {TEXT("Authored layout is disabled for this table.")}};
}
auto SCkTextureDebugger_SceneAuditTable::Reload_LayoutFiles(const FString& InMarkupPath, const FString& InStylesheetPath) -> FCkUiLoadResult
{
    return _LayoutView.IsValid() ? _LayoutView->ReloadFiles(InMarkupPath, InStylesheetPath)
                                 : FCkUiLoadResult{false, {TEXT("Authored layout is disabled for this table.")}};
}
auto SCkTextureDebugger_SceneAuditTable::Poll_LayoutFiles() -> bool { return _LayoutView.IsValid() && _LayoutView->PollFiles(); }
auto SCkTextureDebugger_SceneAuditTable::Tick_LayoutFiles(double, float) -> EActiveTimerReturnType
{
    Poll_LayoutFiles();
    return EActiveTimerReturnType::Continue;
}
auto SCkTextureDebugger_SceneAuditTable::Get_LayoutRevision() const -> int64
{
    return _LayoutView.IsValid() ? _LayoutView->GetRevision() : 0;
}
auto SCkTextureDebugger_SceneAuditTable::Get_LayoutError() const -> FText
{
    if (!_PublicationError.IsEmpty())
    {
        return FText::FromString(_PublicationError);
    }
    return !_LayoutView.IsValid() || _LayoutView->GetLastResult().Succeeded
               ? FText::GetEmpty()
               : FText::FromString(FString::Join(_LayoutView->GetLastResult().Errors, TEXT("\n")));
}
auto SCkTextureDebugger_SceneAuditTable::Get_AuthoredTable() const -> TSharedPtr<SCkUiTable>
{
    return _LayoutView.IsValid() ? _LayoutView->GetTable(TEXT("audit-control")) : nullptr;
}

auto SCkTextureDebugger_SceneAuditTable::SetSnapshot(const FCkTextureDebugger_LoadedWorldSnapshot& InSnapshot) -> void
{
    Reconcile(InSnapshot);
}
auto SCkTextureDebugger_SceneAuditTable::SetSelectedComponent(TWeakObjectPtr<UPrimitiveComponent> InComponent) -> void
{
    _Selected = InComponent;
    RestoreSelection();
}
auto SCkTextureDebugger_SceneAuditTable::Get_VisibleRowCount() const -> int32 { return _Visible.Num(); }
auto SCkTextureDebugger_SceneAuditTable::Get_TotalRowCount() const -> int32 { return _All.Num(); }
auto SCkTextureDebugger_SceneAuditTable::Get_SelectedRowCount() const -> int32
{
    const auto* Component = _Selected.Get();
    return Component != nullptr && _Visible.ContainsByPredicate([Component](const TSharedPtr<FRow>& InRow)
                                                                { return InRow.IsValid() && InRow->Target.Get() == Component; })
               ? 1
               : 0;
}

auto SCkTextureDebugger_SceneAuditTable::Reconcile(const FCkTextureDebugger_LoadedWorldSnapshot& InSnapshot) -> void
{
    _HasWorld = InSnapshot.World.IsValid();
    auto Existing = TMap<FRowKey, TSharedPtr<FRow>>{};
    for (const auto& Row : _All)
    {
        if (Row.IsValid())
        {
            Existing.Add(Row->Key, Row);
        }
    }
    auto Values = TArray<FRow>{};
    Values.Reserve(InSnapshot.Components.Num());
    for (const auto& Component : InSnapshot.Components)
    {
        auto Value = ck_texture_audit::Make_Row(Component);
        Value.UiKey =
            Existing.Contains(Value.Key) ? Existing.FindChecked(Value.Key)->UiKey : FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Value.bHighlight = _Highlight.IsEmpty() || Matches(Value, _Highlight);
        Values.Add(MoveTemp(Value));
    }
    Values.Sort(
        [](const FRow& A, const FRow& B)
        {
            const auto ActorComparison = A.ActorPath.Compare(B.ActorPath, ESearchCase::CaseSensitive);
            return ActorComparison != 0 ? ActorComparison < 0
                                        : A.Key.ComponentPath.Compare(B.Key.ComponentPath, ESearchCase::CaseSensitive) < 0;
        });
    auto Records = TArray<FCkUiRecordData>{};
    for (const auto& Value : Values)
    {
        if (Matches(Value, _Filter))
        {
            Records.Add(ck_texture_audit::UiRecord(Value));
        }
    }
    if (_UiCollection.IsValid())
    {
        const FCkUiLoadResult Result = _UiCollection->TrySetRecords(MoveTemp(Records));
        if (!Result.Succeeded)
        {
            _PublicationError = FString::Join(Result.Errors, TEXT("\n"));
            return;
        }
    }
    _PublicationError.Reset();
    auto NewAll = TArray<TSharedPtr<FRow>>{};
    auto NewVisible = TArray<TSharedPtr<FRow>>{};
    auto NewUiRows = TMap<FString, TWeakPtr<FRow>>{};
    for (FRow& Value : Values)
    {
        TSharedPtr<FRow> Row = Existing.FindRef(Value.Key);
        if (Row.IsValid())
        {
            *Row = MoveTemp(Value);
        }
        else
        {
            Row = MakeShared<FRow>(MoveTemp(Value));
        }
        NewAll.Add(Row);
        if (Matches(*Row, _Filter))
        {
            NewVisible.Add(Row);
            NewUiRows.Add(Row->UiKey, Row);
        }
    }
    _All = MoveTemp(NewAll);
    _Visible = MoveTemp(NewVisible);
    _UiRows = MoveTemp(NewUiRows);
    if (const auto Table = Get_AuthoredTable(); Table.IsValid())
    {
        Table->TryRefresh();
    }
    RestoreSelection();
}

auto SCkTextureDebugger_SceneAuditTable::RebuildVisible() -> void
{
    auto Records = TArray<FCkUiRecordData>{};
    auto NewVisible = TArray<TSharedPtr<FRow>>{};
    for (const auto& Row : _All)
    {
        if (!Row.IsValid())
        {
            continue;
        }
        auto Value = *Row;
        Value.bHighlight = _Highlight.IsEmpty() || Matches(Value, _Highlight);
        if (Matches(Value, _Filter))
        {
            Records.Add(ck_texture_audit::UiRecord(Value));
            NewVisible.Add(Row);
        }
    }
    if (_UiCollection.IsValid())
    {
        const FCkUiLoadResult Result = _UiCollection->TrySetRecords(MoveTemp(Records));
        if (!Result.Succeeded)
        {
            _PublicationError = FString::Join(Result.Errors, TEXT("\n"));
            return;
        }
    }
    _PublicationError.Reset();
    _Visible = MoveTemp(NewVisible);
    _UiRows.Reset();
    for (const auto& Row : _Visible)
    {
        Row->bHighlight = _Highlight.IsEmpty() || Matches(*Row, _Highlight);
        _UiRows.Add(Row->UiKey, Row);
    }
    if (const auto Table = Get_AuthoredTable(); Table.IsValid())
    {
        Table->TryRefresh();
    }
    RestoreSelection();
}

auto SCkTextureDebugger_SceneAuditTable::RestoreSelection() -> void
{
    const auto* Component = _Selected.Get();
    const auto* Selected = Component != nullptr ? _Visible.FindByPredicate([Component](const TSharedPtr<FRow>& Row)
                                                                           { return Row.IsValid() && Row->Target.Get() == Component; })
                                                : nullptr;
    if (const auto Table = Get_AuthoredTable(); Table.IsValid())
    {
        Table->TrySelectKey(Selected != nullptr ? TOptional<FString>{(*Selected)->UiKey} : TOptional<FString>{});
    }
}
auto SCkTextureDebugger_SceneAuditTable::ClearSelection(const bool bNotify) -> void
{
    const bool bHadSelection = _Selected.IsValid();
    _Selected.Reset();
    if (const auto Table = Get_AuthoredTable(); Table.IsValid())
    {
        Table->TrySelectKey({});
    }
    if (bNotify && bHadSelection && _OnSelected)
    {
        _OnSelected({});
    }
}
auto SCkTextureDebugger_SceneAuditTable::Matches(const FRow& InRow, const FString& InQuery) const -> bool
{
    return InQuery.IsEmpty() || InRow.Search.Contains(InQuery, ESearchCase::IgnoreCase);
}
auto SCkTextureDebugger_SceneAuditTable::GetEmptyText() const -> FText
{
    if (!_HasWorld)
    {
        return LOCTEXT("NoWorld", "No active Editor, PIE, or game world is selected.");
    }
    if (_All.IsEmpty())
    {
        return LOCTEXT("NoRows", "No registered primitive components are loaded.");
    }
    return LOCTEXT("NoMatches", "No loaded components match the filter.");
}
auto SCkTextureDebugger_SceneAuditTable::GetCountText() const -> FText
{
    return FText::FromString(FString::Printf(TEXT("%d / %d"), _Visible.Num(), _All.Num()));
}
auto SCkTextureDebugger_SceneAuditTable::GetSelectionText() const -> FText
{
    const auto* Component = _Selected.Get();
    if (Component == nullptr)
    {
        return LOCTEXT("None", "No component selected");
    }
    const auto* Row =
        _All.FindByPredicate([Component](const TSharedPtr<FRow>& Item) { return Item.IsValid() && Item->Target.Get() == Component; });
    return Row != nullptr && Row->IsValid() ? FText::FromString(FString::Printf(TEXT("Selected: %s"), *(*Row)->Component))
                                            : LOCTEXT("SelectedUnavailable", "Selected component is no longer in the loaded-world audit.");
}
auto SCkTextureDebugger_SceneAuditTable::MakeCopyText(const FRow& InRow) const -> FString
{
    return FString::Printf(TEXT("Actor: %s\nActor path: %s\nComponent: %s\nComponent path: %s\nClass: %s\nKind: %s\nAudit state: "
                                "%s\nMaterial slots: %d\nRuntime-used textures: %d"),
                           *InRow.Actor, *InRow.ActorPath, *InRow.Component, *InRow.Key.ComponentPath, *InRow.ClassName, *InRow.Kind,
                           *InRow.State, InRow.Slots, InRow.Textures);
}
auto SCkTextureDebugger_SceneAuditTable::OnFilter(const FString& InText) -> void
{
    if (_Filter != InText)
    {
        _Filter = InText;
        RebuildVisible();
    }
}
auto SCkTextureDebugger_SceneAuditTable::OnHighlight(const FString& InText) -> void
{
    if (_Highlight != InText)
    {
        _Highlight = InText;
        RebuildVisible();
    }
}
auto SCkTextureDebugger_SceneAuditTable::Find_Row(const FRowKey& InKey) const -> TSharedPtr<FRow>
{
    const auto* Row = _All.FindByPredicate([&InKey](const TSharedPtr<FRow>& Item) { return Item.IsValid() && Item->Key == InKey; });
    return Row != nullptr ? *Row : nullptr;
}
auto SCkTextureDebugger_SceneAuditTable::Find_RowByUiKey(const FString& InKey) const -> TSharedPtr<FRow>
{
    const TWeakPtr<FRow>* Row = _UiRows.Find(InKey);
    return Row != nullptr ? Row->Pin() : nullptr;
}
auto SCkTextureDebugger_SceneAuditTable::On_AuthoredSelectionChanged(TOptional<FString> InKey, const ESelectInfo::Type InType) -> void
{
    if (InType == ESelectInfo::Direct)
    {
        return;
    }
    if (!InKey.IsSet())
    {
        ClearSelection(true);
        return;
    }
    const TSharedPtr<FRow> Row = Find_RowByUiKey(InKey.GetValue());
    if (!Row.IsValid())
    {
        return;
    }
    _Selected = Row->Target;
    if (_OnSelected)
    {
        _OnSelected(_Selected);
    }
}
auto SCkTextureDebugger_SceneAuditTable::On_CopyComponentPath(const FString& InKey) -> void
{
    if (const TSharedPtr<FRow> Row = Find_RowByUiKey(InKey); Row.IsValid())
    {
        FPlatformApplicationMisc::ClipboardCopy(*Row->Key.ComponentPath);
    }
}
auto SCkTextureDebugger_SceneAuditTable::On_CopyAuditSummary(const FString& InKey) -> void
{
    if (const TSharedPtr<FRow> Row = Find_RowByUiKey(InKey); Row.IsValid())
    {
        FPlatformApplicationMisc::ClipboardCopy(*MakeCopyText(*Row));
    }
}

#undef LOCTEXT_NAMESPACE
