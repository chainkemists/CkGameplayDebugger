#include "CkTextureDebugger/Window/SCkTextureDebugger_SceneAuditTable.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_TextureHealthTable.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiTable.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Input/Events.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/SWindow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_texture_debugger_ux_tests
{
    auto Make_ComponentRow(
        UStaticMeshComponent* InComponent,
        UTexture2D* InTexture,
        FString InTextureName) -> FCkTextureDebugger_ComponentRow
    {
        auto TextureRow = FCkTextureDebugger_TextureRow{};
        TextureRow.NavigationTarget = InTexture;
        TextureRow.Health.DisplayName = MoveTemp(InTextureName);
        TextureRow.Health.AssetPath = FSoftObjectPath{InTexture};
        TextureRow.Health.CookedWidth = 1024;
        TextureRow.Health.CookedHeight = 1024;

        auto Slot = FCkTextureDebugger_MaterialSlotRow{};
        Slot.SlotIndex = 0;
        Slot.DisplayName = TEXT("SharedMaterial");
        Slot.Textures.Add(MoveTemp(TextureRow));

        auto Component = FCkTextureDebugger_ComponentRow{};
        Component.NavigationTarget = InComponent;
        Component.ActorPath = FSoftObjectPath{TEXT("/Game/Test.SharedActor")};
        Component.ActorDisplayName = TEXT("SharedActor");
        Component.ComponentDisplayName = TEXT("SharedComponent");
        Component.ComponentClassName = TEXT("StaticMeshComponent");
        Component.Kind = ECkTextureDebugger_ComponentKind::StaticMesh;
        Component.SupportsCheckerOverride = true;
        Component.MaterialSlots.Add(MoveTemp(Slot));
        return Component;
    }

    auto Find_WidgetsByType(const TSharedRef<SWidget>& InWidget, const FString& InType, TArray<TSharedPtr<SWidget>>& OutWidgets) -> void
    {
        if ((InWidget->GetTypeAsString() == InType || InWidget->GetTag() == FName{*InType})) { OutWidgets.Add(InWidget); }
        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        { Find_WidgetsByType(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType, OutWidgets); }
    }

    using FAuthoredHealthList = SListView<SCkUiTable::FRecord>;

    auto Find_AuthoredHealthList(const TSharedRef<SCkTextureDebugger_TextureHealthTable>& InTable) -> TSharedPtr<FAuthoredHealthList>
    {
        const TSharedPtr<SCkUiTable> AuthoredTable = InTable->Get_AuthoredTable();
        return AuthoredTable.IsValid() ? AuthoredTable->GetList() : TSharedPtr<FAuthoredHealthList>{};
    }

    auto Get_TextField(const SCkUiTable::FRecord& InRecord, const FString& InField) -> FString
    {
        const FCkUiFieldValue* Field = InRecord.IsValid() ? InRecord->FindField(InField) : nullptr;
        return Field != nullptr ? Field->Text.ToString() : FString{};
    }

    auto Get_ColorField(const SCkUiTable::FRecord& InRecord, const FString& InField) -> FLinearColor
    {
        const FCkUiFieldValue* Field = InRecord.IsValid() ? InRecord->FindField(InField) : nullptr;
        return Field != nullptr ? Field->Color : FLinearColor::Transparent;
    }

    auto Contains_Text(const TSharedRef<SWidget>& InWidget, const FString& InText) -> bool
    {
        if (InWidget->GetTypeAsString() == TEXT("SCkFlexText")
            && StaticCastSharedRef<SCkFlexText>(InWidget)->GetText().ToString() == InText)
        { return true; }
        if (InWidget->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InWidget)->GetText().ToString() == InText)
        { return true; }

        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        {
            if (Contains_Text(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; }
        }
        return false;
    }

    struct FMountedWindowScope final
    {
        explicit FMountedWindowScope(FSlateApplication& InSlate) : Slate(InSlate), OriginalCursor(InSlate.GetCursorPos()) {}
        ~FMountedWindowScope()
        {
            if (ContextTable.IsValid()) { ContextTable->ReleaseContextMenu(); }
            if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); }
            Slate.SetCursorPos(OriginalCursor);
        }

        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
        TSharedPtr<SCkUiTable> ContextTable;
        FVector2D OriginalCursor;
    };

    struct FClipboardTextScope final
    {
        FClipboardTextScope() { FPlatformApplicationMisc::ClipboardPaste(OriginalText); }
        ~FClipboardTextScope() { FPlatformApplicationMisc::ClipboardCopy(*OriginalText); }

        FString OriginalText;
    };

    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto FindContextMenu(FSlateApplication& InSlate, const FString& InLabel) -> TSharedPtr<SWindow>
    {
        // Popup construction and native window arrangement can finish on different ticks.
        // Inspect the window containing our command instead of whichever popup is topmost.
        for (int32 Attempt = 0; Attempt < 100; ++Attempt)
        {
            TArray<TSharedRef<SWindow>> Windows;
            InSlate.GetAllVisibleWindowsOrdered(Windows);
            for (const auto& Window : Windows)
            {
                if (!Contains_Text(Window, InLabel)) { continue; }
                TArray<TSharedPtr<SWidget>> Buttons;
                Find_WidgetsByType(Window, TEXT("SMenuEntryButton"), Buttons);
                for (const auto& Button : Buttons)
                {
                    FWidgetPath Path;
                    if (Contains_Text(Button.ToSharedRef(), InLabel)
                        && Button->GetCachedGeometry().GetLocalSize().X > 0.0f
                        && Button->GetCachedGeometry().GetLocalSize().Y > 0.0f
                        && InSlate.GeneratePathToWidgetUnchecked(Button.ToSharedRef(), Path)) { return Window; }
                }
            }
            FPlatformProcess::Sleep(0.01f);
            TickSlate(InSlate);
        }
        return {};
    }

    auto RouteMouseClick(FSlateApplication& InSlate, const FVector2D& InPosition, const FKey& InButton) -> void
    {
        InSlate.SetCursorPos(InPosition);
        const FPointerEvent Move{0, InPosition, InPosition, TSet<FKey>{}, EKeys::Invalid, 0.0f, FModifierKeysState{}};
        InSlate.ProcessMouseMoveEvent(Move, true);
        const FPointerEvent Down{0, InPosition, InPosition, TSet<FKey>{InButton}, InButton, 0.0f, FModifierKeysState{}};
        InSlate.ProcessMouseButtonDownEvent(nullptr, Down);
        const FPointerEvent Up{0, InPosition, InPosition, TSet<FKey>{}, InButton, 0.0f, FModifierKeysState{}};
        InSlate.ProcessMouseButtonUpEvent(Up);
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Ux_EditorWorldCatalog,
    "Ck.TextureDebugger.Ux.EditorWorldCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Ux_EditorWorldCatalog::RunTest(const FString& Parameters)
{
    auto Model = FCkDebuggerModel_WorldSelector{};
    Model.Set_IncludeEditorWorld(true);

    auto* EditorWorld = static_cast<UWorld*>(nullptr);
    if (GEngine != nullptr)
    {
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Editor && Context.World() != nullptr)
            {
                EditorWorld = Context.World();
                break;
            }
        }
    }

    TestNotNull(TEXT("Editor-context automation has an Editor world"), EditorWorld);
    if (EditorWorld == nullptr)
    { return false; }

    const auto Available = Model.Get_AvailableWorlds();
    TestTrue(TEXT("Editor-capable selector exposes the Editor world"), Available.Contains(EditorWorld));

    Model.Ensure_AutoSelect();
    TestNotNull(TEXT("Editor-capable selector auto-selects an inspectable world"), Model.Get_SelectedWorld());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Ux_StableTables,
    "Ck.TextureDebugger.Ux.StableTables",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Ux_StableTables::RunTest(const FString& Parameters)
{
    auto ComponentA = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    auto ComponentB = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    auto Texture = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};

    auto Snapshot = FCkTextureDebugger_LoadedWorldSnapshot{};
    Snapshot.Components.Add(ck_texture_debugger_ux_tests::Make_ComponentRow(
        ComponentA.Get(), Texture.Get(), TEXT("SharedTexture")));
    Snapshot.Components.Add(ck_texture_debugger_ux_tests::Make_ComponentRow(
        ComponentB.Get(), Texture.Get(), TEXT("SharedTexture")));

    auto AuditTable = SNew(SCkTextureDebugger_SceneAuditTable);
    AuditTable->SetSnapshot(Snapshot);
    TestEqual(TEXT("Same-named components remain distinct audit rows"), AuditTable->Get_TotalRowCount(), 2);
    TestEqual(TEXT("Both audit rows are visible without a filter"), AuditTable->Get_VisibleRowCount(), 2);

    AuditTable->SetSelectedComponent(ComponentA.Get());
    TestEqual(TEXT("Programmatic audit selection is singular"), AuditTable->Get_SelectedRowCount(), 1);
    AuditTable->SetSelectedComponent(ComponentB.Get());
    TestEqual(TEXT("Changing audit selection replaces the previous row"), AuditTable->Get_SelectedRowCount(), 1);
    AuditTable->SetSelectedComponent({});
    TestEqual(TEXT("Audit selection can be cleared"), AuditTable->Get_SelectedRowCount(), 0);

    auto HealthTable = SNew(SCkTextureDebugger_TextureHealthTable);
    HealthTable->Set_Snapshot(Snapshot);
    TestEqual(TEXT("Health keeps contextual references from distinct components"), HealthTable->Get_TotalRowCount(), 2);
    TestEqual(TEXT("Health initially shows the full loaded-world inventory"), HealthTable->Get_VisibleRowCount(), 2);

    HealthTable->Set_Snapshot(Snapshot, ComponentA.Get());
    TestEqual(TEXT("Component context highlights without filtering Health"), HealthTable->Get_VisibleRowCount(), 2);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Ux_NativeTableEvents,
    "Ck.TextureDebugger.Ux.NativeTableEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Ux_NativeTableEvents::RunTest(const FString& Parameters)
{
    using namespace ck_texture_debugger_ux_tests;

    auto Components = TArray<TStrongObjectPtr<UStaticMeshComponent>>{};
    auto Textures = TArray<TStrongObjectPtr<UTexture2D>>{};
    auto Snapshot = FCkTextureDebugger_LoadedWorldSnapshot{};
    for (auto Index = 0; Index < 12; ++Index)
    {
        auto Component = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
        auto Texture = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};
        auto Row = Make_ComponentRow(Component.Get(), Texture.Get(), FString::Printf(TEXT("NativeEvent_Texture_%02d"), Index));
        if (Index == 0)
        {
            Row.MaterialSlots[0].DisplayName = TEXT("LongSelection_ResolvedMaterialSlot_With_Inspectable_Runtime_Facts");
            auto& Health = Row.MaterialSlots[0].Textures[0].Health;
            Health.DisplayName = TEXT("LongSelection_Texture_With_A_Long_Runtime_Display_Name_For_Detail_Wrapping");
            Health.ClassName = TEXT("Texture2D_Runtime_Inspection_Class");
            Health.FormatName = TEXT("DXT5_Long_Runtime_Format_Descriptor");
            Health.LodGroupName = TEXT("World_NormalMap_Long_Streaming_Group");
            Health.MipCount = 12;
            Health.ResidentBytes = 1048576;
            Health.DedicatedVideoBytes = 2097152;
            Health.HasStreamingMetrics = true;
            Health.ResidentMipCount = 7;
            Health.RequestedMipCount = 10;
            Health.MaxMipCount = 12;
            Health.StreamingAvailability = ECkTextureDebugger_StreamingAvailability::Available;
        }
        Snapshot.Components.Add(MoveTemp(Row));
        Components.Add(MoveTemp(Component));
        Textures.Add(MoveTemp(Texture));
    }

    const auto Table = SNew(SCkTextureDebugger_TextureHealthTable);
    Table->Set_Snapshot(Snapshot);

    if (Table->Get_LayoutRevision() == 0)
    {
        AddError(TEXT("Texture Health layout failed to load: ") + Table->Get_LayoutError().ToString());
        return false;
    }

    const auto List = Find_AuthoredHealthList(Table);
    TestTrue(TEXT("Texture Health Table exposes its authored SCkUiTable SListView"), List.IsValid());
    if (NOT List.IsValid()) { return false; }
    TestEqual(TEXT("Native list observes the complete real snapshot"), List->GetNumItemsBeingObserved(), 12);

    const auto InitialItems = List->GetItems();
    const auto* LongItemFound = InitialItems.FindByPredicate([](const SCkUiTable::FRecord& Row)
    { return Get_TextField(Row, TEXT("texture")).Contains(TEXT("LongSelection")); });
    TestNotNull(TEXT("Long detail fixture is a real list item"), LongItemFound);
    if (LongItemFound == nullptr) { return false; }
    const auto LongItem = *LongItemFound;

    // Exercise STableRow's actual left-button-down selection path.  Do not use SetItemSelection here:
    // its ESelectInfo argument bypasses the deferred-vs-instantaneous row signaling contract.
    List->SetSelection(List->GetItems()[0], ESelectInfo::OnKeyPress);
    const FGeometry ListGeometry = FGeometry::MakeRoot(FVector2D{960.0f, 640.0f}, FSlateLayoutTransform{});
    Table->SlatePrepass(1.0f);
    List->Tick(ListGeometry, FPlatformTime::Seconds(), 0.0f);
    const TSet<FKey> PressedButtons{EKeys::LeftMouseButton};
    const FPointerEvent MouseDown{0, FVector2D{10.0f, 10.0f}, FVector2D{10.0f, 10.0f}, PressedButtons,
        EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
    const auto ExerciseHeldRowClick = [this, &Snapshot, &Table, &List, &ListGeometry, &MouseDown](const int32 ItemIndex)
    {
        const auto ClickedItem = List->GetItems()[ItemIndex];
        const TSharedPtr<ITableRow> GeneratedRow = List->WidgetFromItem(ClickedItem);
        if (!TestTrue(TEXT("Visible list item has a generated native STableRow"), GeneratedRow.IsValid())) { return false; }
        GeneratedRow->AsWidget()->OnMouseButtonDown(ListGeometry, MouseDown);
        const TOptional<FCkTextureDebugger_TextureHealthSelection> Immediate = Table->Get_Selection();
        if (!TestTrue(TEXT("Generated row mouse-down immediately updates public selection"), Immediate.IsSet())) { return false; }
        TestEqual(TEXT("Generated row mouse-down selects its own texture"), Immediate->DisplayName, Get_TextField(ClickedItem, TEXT("texture")));

        // Deliberately omit mouse-up: reconciliation must preserve the just-signaled selection while the button is held.
        Table->Set_Snapshot(Snapshot);
        const TOptional<FCkTextureDebugger_TextureHealthSelection> AfterRefresh = Table->Get_Selection();
        if (!TestTrue(TEXT("Held mouse selection survives snapshot reconciliation"), AfterRefresh.IsSet())) { return false; }
        TestEqual(TEXT("Snapshot reconciliation retains the held row selection"), AfterRefresh->DisplayName, Get_TextField(ClickedItem, TEXT("texture")));
        TestTrue(TEXT("Native list retains the held row selection"), List->GetSelectedItems().Num() == 1 && List->GetSelectedItems()[0] == ClickedItem);
        return true;
    };
    if (!ExerciseHeldRowClick(1) || !ExerciseHeldRowClick(2)) { return false; }

    List->SetSelection(LongItem, ESelectInfo::OnMouseClick);
    const auto InitialSelection = Table->Get_Selection();
    TestTrue(TEXT("Real list selection enters the table's public selection state"), InitialSelection.IsSet());
    if (NOT InitialSelection.IsSet()) { return false; }
    TestEqual(TEXT("Selected detail carries the selected real row"), InitialSelection->DisplayName, Get_TextField(LongItem, TEXT("texture")));
    TestTrue(TEXT("Selected detail carries the long material facts"), InitialSelection->Details.Contains(TEXT("LongSelection_ResolvedMaterialSlot")));
    TestTrue(TEXT("Selected detail carries the long format fact"), InitialSelection->Details.Contains(TEXT("DXT5_Long_Runtime_Format_Descriptor")));

    List->SetScrollOffset(5.0f);
    const auto ScrollBeforeRefresh = List->GetScrollOffset();
    Table->Set_Snapshot(Snapshot);
    Snapshot.Components[0].MaterialSlots[0].Textures[0].Health.ResidentMipCount = 9;
    Table->Set_Snapshot(Snapshot);
    const auto RefreshedItems = List->GetItems();
    TestTrue(TEXT("Identical snapshot preserves the selected row's stable pointer"), RefreshedItems.Contains(LongItem));
    const auto RefreshedSelection = Table->Get_Selection();
    TestTrue(TEXT("Identical snapshot preserves the selected row"), RefreshedSelection.IsSet());
    TestTrue(TEXT("Identical snapshot preserves the list scroll offset"), FMath::IsNearlyEqual(List->GetScrollOffset(), ScrollBeforeRefresh));

    auto SearchWidgets = TArray<TSharedPtr<SWidget>>{};
    Find_WidgetsByType(Table, TEXT("SSearchBox"), SearchWidgets);
    TestEqual(TEXT("Texture Health authored layout exposes its two real editable search boxes"), SearchWidgets.Num(), 2);
    if (SearchWidgets.Num() != 2) { return false; }
    const auto FilterBox = StaticCastSharedPtr<SSearchBox>(SearchWidgets[0]);
    const auto HighlightBox = StaticCastSharedPtr<SSearchBox>(SearchWidgets[1]);
    TestTrue(TEXT("Both editable search children have their exact native type"), FilterBox.IsValid() && HighlightBox.IsValid());
    if (NOT FilterBox.IsValid() || NOT HighlightBox.IsValid()) { return false; }

    FilterBox->SetText(FText::FromString(TEXT("LongSelection")));
    TestEqual(TEXT("Real Filter box narrows the table through its bound production callback"), List->GetNumItemsBeingObserved(), 1);
    TestTrue(TEXT("Filter retains the matching stable list pointer"), List->GetItems()[0] == LongItem);
    TestTrue(TEXT("Filter retains selection of its matching row"), Table->Get_Selection().IsSet());

    FilterBox->SetText(FText::GetEmpty());
    TestEqual(TEXT("Clearing the real Filter box restores the table inventory"), List->GetNumItemsBeingObserved(), 12);
    HighlightBox->SetText(FText::FromString(TEXT("LongSelection")));
    TestEqual(TEXT("Real Highlight box keeps all rows visible"), List->GetNumItemsBeingObserved(), 12);
    TestTrue(TEXT("Highlight marks the matching real row"), Get_ColorField(LongItem, TEXT("text-color")).Equals(CkStyle::Text()));
    const auto OtherRow = List->GetItems().FindByPredicate([&LongItem](const auto& InRow) { return InRow != LongItem; });
    TestTrue(TEXT("Highlight dims a non-matching real row"), OtherRow != nullptr
        && Get_ColorField(*OtherRow, TEXT("text-color")).Equals(CkStyle::TextMute()));
    TestTrue(TEXT("Highlight retains selected row state"), Table->Get_Selection().IsSet());

    // Exercise the same external authoring transaction as the live file timer, with native state populated.
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid()) { AddError(TEXT("Texture layout fixture plugin is unavailable")); return false; }
    FString Markup;
    FString Stylesheet;
    if (!FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/TextureHealth.ui.html")))
        || !FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/TextureHealth.ui.css"))))
    { AddError(TEXT("Texture authoring source files are required by the production fixture")); return false; }
    TestTrue(TEXT("The production external layout loaded on construction"), Table->Get_LayoutRevision() > 0);
    TestTrue(TEXT("The production external layout has no diagnostics"), Table->Get_LayoutError().IsEmpty());
    const auto Splitter = Table->Get_AuthoredSplitter();
    TestTrue(TEXT("One persistent authored splitter"), Splitter.IsValid());
    FilterBox->SetText(FText::FromString(TEXT("Selection")));
    List->SetScrollOffset(0.25f);
    const auto ScrollBeforeReload = List->GetScrollOffset();
    const auto VisibleBeforeReload = Table->Get_VisibleRowCount();
    const auto RevisionBeforeReload = Table->Get_LayoutRevision();
    const FString ReloadMarkup = Markup.Replace(TEXT("Selected texture"), TEXT("Texture inspection"))
        .Replace(TEXT("<text id=\"texture-cell\""), TEXT("<text id=\"texture-cell-marker\">Reload marker</text><text id=\"texture-cell\""));
    const auto Reload = Table->TryReload_Layout(ReloadMarkup,
        Stylesheet + TEXT("\n.search-row { gap: 16px; }"));
    TestTrue(TEXT("A fully bound authored candidate applies"), Reload.Succeeded);
    for (const auto& Error : Reload.Errors) { AddError(Error); }
    TestEqual(TEXT("Successful reload advances revision once"), Table->Get_LayoutRevision(), RevisionBeforeReload + 1);
    TestTrue(TEXT("Reload retains the persistent authored table list object"), Find_AuthoredHealthList(Table) == List);
    TestEqual(TEXT("Reload retains filtered inventory"), Table->Get_VisibleRowCount(), VisibleBeforeReload);
    TestTrue(TEXT("Reload retains selected texture and preview root"), Table->Get_Selection().IsSet()
        && Table->Get_Selection()->Texture == InitialSelection->Texture);
    TestTrue(TEXT("Reload retains the selected stable row pointer"), List->GetItems().Contains(LongItem));
    TestTrue(TEXT("Reload retains native scroll offset"), FMath::IsNearlyEqual(List->GetScrollOffset(), ScrollBeforeReload));
    List->RequestListRefresh();
    Table->SlatePrepass(1.0f);
    List->Tick(ListGeometry, FPlatformTime::Seconds(), 0.0f);
    TestTrue(TEXT("Reloaded table exposes its authored heading"), Contains_Text(Table, TEXT("Texture inspection")));
    TestTrue(TEXT("Reloaded generated texture cell exposes its authored marker"), Contains_Text(Table, TEXT("Reload marker")));
    TestTrue(TEXT("Retired rows do not create cell diagnostics during reload"), Table->Get_AuthoredTable()->GetLastCellError().IsEmpty());
    auto SearchAfterReload = TArray<TSharedPtr<SWidget>>{};
    Find_WidgetsByType(Table, TEXT("SSearchBox"), SearchAfterReload);
    TestTrue(TEXT("Reload retains both authored search control identities"), SearchAfterReload == SearchWidgets);
    TestEqual(TEXT("Reload retains filter text"), FilterBox->GetText().ToString(), FString{TEXT("Selection")});
    TestEqual(TEXT("Reload retains highlight text"), HighlightBox->GetText().ToString(), FString{TEXT("LongSelection")});
    TestTrue(TEXT("Reload preserves the authored splitter and its state owner"), Splitter == Table->Get_AuthoredSplitter());

    const auto AcceptedRevision = Table->Get_LayoutRevision();
    const auto Rejected = Table->TryReload_Layout(Markup.Replace(TEXT("bind=\"inventory\""), TEXT("bind=\"missing\"")), Stylesheet);
    TestFalse(TEXT("Unknown binding rejects the entire candidate"), Rejected.Succeeded);
    TestEqual(TEXT("Rejected reload retains revision"), Table->Get_LayoutRevision(), AcceptedRevision);
    TestFalse(TEXT("Rejected reload exposes a designer diagnostic"), Table->Get_LayoutError().IsEmpty());
    TestTrue(TEXT("Rejected reload retains list identity and selection"), Find_AuthoredHealthList(Table) == List && Table->Get_Selection().IsSet());
    TestTrue(TEXT("A corrected document recovers without reopening"), Table->TryReload_Layout(Markup, Stylesheet).Succeeded);
    TestTrue(TEXT("Correction clears the diagnostic"), Table->Get_LayoutError().IsEmpty());

    auto ButtonWidgets = TArray<TSharedPtr<SWidget>>{};
    Find_WidgetsByType(Table, TEXT("SButton"), ButtonWidgets);
    const auto* ClearButtonFound = ButtonWidgets.FindByPredicate([](const TSharedPtr<SWidget>& Button)
    { return Button.IsValid() && Contains_Text(Button.ToSharedRef(), TEXT("Clear")); });
    TestNotNull(TEXT("Texture Health Table exposes its real Clear button"), ClearButtonFound);
    if (ClearButtonFound == nullptr) { return false; }
    const auto ClearButton = StaticCastSharedPtr<SButton>(*ClearButtonFound);
    TestTrue(TEXT("Clear control has its exact native SButton type"), ClearButton.IsValid());
    if (NOT ClearButton.IsValid()) { return false; }
    ClearButton->SimulateClick();
    TestFalse(TEXT("Real Clear button removes the public selection"), Table->Get_Selection().IsSet());
    TestEqual(TEXT("Real Clear button clears the native list selection"), List->GetSelectedItems().Num(), 0);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Ux_TextureHealthContextMenuTarget,
    "Ck.TextureDebugger.Ux.TextureHealthContextMenuTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Ux_TextureHealthContextMenuTarget::RunTest(const FString& Parameters)
{
    using namespace ck_texture_debugger_ux_tests;

    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Texture Health context-menu test requires initialized Slate."));
        return false;
    }

    const auto ComponentA = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    const auto ComponentB = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    const auto TextureA = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};
    const auto TextureB = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};
    auto Snapshot = FCkTextureDebugger_LoadedWorldSnapshot{};
    Snapshot.Components.Add(Make_ComponentRow(ComponentA.Get(), TextureA.Get(), TEXT("ContextTarget_A")));
    Snapshot.Components.Add(Make_ComponentRow(ComponentB.Get(), TextureB.Get(), TEXT("ContextTarget_B")));
    Snapshot.Components[1].MaterialSlots[0].DisplayName = TEXT("ContextTarget_B_Material");
    Snapshot.Components[1].MaterialSlots[0].Textures[0].Health.FormatName = TEXT("ContextTarget_B_Format");

    const TSharedRef<SCkTextureDebugger_TextureHealthTable> Table = SNew(SCkTextureDebugger_TextureHealthTable);
    Table->Set_Snapshot(Snapshot);
    if (!TestTrue(TEXT("Installed Texture Health markup loaded"), Table->Get_LayoutRevision() > 0)
        || !TestTrue(TEXT("Installed Texture Health markup has no diagnostics"), Table->Get_LayoutError().IsEmpty())) { return false; }

    const TSharedPtr<SCkUiTable> AuthoredTable = Table->Get_AuthoredTable();
    const TSharedPtr<FAuthoredHealthList> List = Find_AuthoredHealthList(Table);
    if (!TestTrue(TEXT("Installed Texture Health markup exposes its authored table"), AuthoredTable.IsValid() && List.IsValid())) { return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    FMountedWindowScope WindowScope{Slate};
    WindowScope.ContextTable = AuthoredTable;
    WindowScope.Window = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{960.0f, 640.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Table];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    TickSlate(Slate);

    const SCkUiTable::FRecord* ItemA = List->GetItems().FindByPredicate([](const SCkUiTable::FRecord& Item)
    { return Get_TextField(Item, TEXT("texture")) == TEXT("ContextTarget_A"); });
    const SCkUiTable::FRecord* ItemB = List->GetItems().FindByPredicate([](const SCkUiTable::FRecord& Item)
    { return Get_TextField(Item, TEXT("texture")) == TEXT("ContextTarget_B"); });
    if (!TestNotNull(TEXT("Installed table projects row A"), ItemA) || !TestNotNull(TEXT("Installed table projects row B"), ItemB)) { return false; }

    const TSharedPtr<ITableRow> NativeRowB = List->WidgetFromItem(*ItemB);
    if (!TestTrue(TEXT("Projected row B has a generated native row"), NativeRowB.IsValid())) { return false; }

    const TSharedRef<SWidget> NativeRowBWidget = NativeRowB->AsWidget();
    const FGeometry RowBGeometry = NativeRowBWidget->GetCachedGeometry();
    const FVector2D MenuPosition = RowBGeometry.GetAbsolutePosition() + RowBGeometry.GetAbsoluteSize() * 0.5f;
    FWidgetPath RowBPath;
    if (!TestTrue(TEXT("Generated row B is mounted in Slate's hit-test path"), Slate.GeneratePathToWidgetUnchecked(NativeRowBWidget, RowBPath))) { return false; }
    RouteMouseClick(Slate, MenuPosition, EKeys::RightMouseButton);
    TickSlate(Slate);

    const TSharedPtr<SWindow> MenuWindow = FindContextMenu(Slate, TEXT("Copy texture details"));
    if (!TestTrue(TEXT("Right-clicking native row B opens an authored popup menu"), MenuWindow.IsValid())) { return false; }
    TestTrue(TEXT("Native popup contains the installed copy-details entry"), Contains_Text(MenuWindow.ToSharedRef(), TEXT("Copy texture details")));

    List->SetSelection(*ItemA, ESelectInfo::OnMouseClick);
    TestTrue(TEXT("Native selection can move to A while B's context menu remains open"), Table->Get_Selection().IsSet()
        && Table->Get_Selection()->DisplayName == TEXT("ContextTarget_A"));

    auto MenuButtons = TArray<TSharedPtr<SWidget>>{};
    // Slate's native FMenuBuilder wraps entries in its private SMenuEntryButton subclass.
    Find_WidgetsByType(MenuWindow.ToSharedRef(), TEXT("SMenuEntryButton"), MenuButtons);
    const TSharedPtr<SWidget>* CopyButtonWidget = MenuButtons.FindByPredicate([](const TSharedPtr<SWidget>& Button)
    { return Button.IsValid() && Contains_Text(Button.ToSharedRef(), TEXT("Copy texture details")); });
    if (!TestNotNull(TEXT("Native copy-details entry has a clickable Slate button"), CopyButtonWidget)) { return false; }
    const TSharedPtr<SButton> CopyButton = StaticCastSharedPtr<SButton>(*CopyButtonWidget);
    if (!TestTrue(TEXT("Native copy-details entry exposes SButton behavior"), CopyButton.IsValid())) { return false; }
    const FGeometry CopyButtonGeometry = CopyButton->GetCachedGeometry();
    const FVector2D CopyButtonPosition = CopyButtonGeometry.GetAbsolutePosition() + CopyButtonGeometry.GetAbsoluteSize() * 0.5f;
    FWidgetPath CopyButtonPath;
    if (!TestTrue(TEXT("Native copy-details entry is mounted in Slate's hit-test path"), Slate.GeneratePathToWidgetUnchecked(CopyButton.ToSharedRef(), CopyButtonPath))
        || !TestTrue(TEXT("Native copy-details entry has nonzero arranged geometry"), CopyButtonGeometry.GetAbsoluteSize().X > 0.0f && CopyButtonGeometry.GetAbsoluteSize().Y > 0.0f)) { return false; }

    FClipboardTextScope Clipboard;
    RouteMouseClick(Slate, CopyButtonPosition, EKeys::LeftMouseButton);
    FString CopiedText;
    FPlatformApplicationMisc::ClipboardPaste(CopiedText);
    TestTrue(TEXT("Context action retains its original row B target after selection changes to A"), CopiedText.Contains(TEXT("ContextTarget_B"))
        && CopiedText.Contains(TEXT("ContextTarget_B_Material")) && CopiedText.Contains(TEXT("ContextTarget_B_Format")));
    TestFalse(TEXT("Context action does not retarget the copied details to A"), CopiedText.Contains(TEXT("ContextTarget_A")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTextureDebugger_Ux_PreviewLifetime,
    "Ck.TextureDebugger.Ux.PreviewLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_Ux_PreviewLifetime::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_ux_tests;
    const auto Component = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    auto Texture = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};
    const auto WeakTexture = TWeakObjectPtr<UTexture2D>{Texture.Get()};
    auto Snapshot = FCkTextureDebugger_LoadedWorldSnapshot{};
    Snapshot.Components.Add(Make_ComponentRow(Component.Get(), Texture.Get(), TEXT("Lifetime")));
    auto Table = TSharedPtr<SCkTextureDebugger_TextureHealthTable>{SNew(SCkTextureDebugger_TextureHealthTable)};
    Table->Set_Snapshot(Snapshot);
    auto List = Find_AuthoredHealthList(Table.ToSharedRef());
    if (!List.IsValid() || List->GetItems().Num() != 1) { AddError(TEXT("Lifetime fixture did not publish one native row")); return false; }
    List->SetItemSelection(List->GetItems()[0], true, ESelectInfo::OnMouseClick);
    Texture.Reset();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    TestTrue(TEXT("Active preview retains its texture during GC"), WeakTexture.IsValid());
    Table->Clear_Selection();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    TestFalse(TEXT("Clear releases preview ownership; copied snapshot rows do not root textures"), WeakTexture.IsValid());
    const auto WeakTable = TWeakPtr<SCkTextureDebugger_TextureHealthTable>{Table};
    List.Reset();
    Table.Reset();
    TestFalse(TEXT("Closing the table leaves no Slate metadata ownership cycle"), WeakTable.IsValid());
    const auto Reopened = SNew(SCkTextureDebugger_TextureHealthTable);
    Reopened->Set_Snapshot({});
    TestEqual(TEXT("Reopened table starts usable and empty"), Reopened->Get_TotalRowCount(), 0);
    return true;
}

#endif
