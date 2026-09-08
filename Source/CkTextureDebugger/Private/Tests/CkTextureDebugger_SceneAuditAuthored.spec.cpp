#include "CkSlateLayout/CkFlexText.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_SceneAuditTable.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiTable.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Input/Events.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/SWindow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_texture_debugger_scene_audit_authored_tests
{
    using FAuthoredList = SListView<SCkUiTable::FRecord>;

    auto Make_ComponentRow(UStaticMeshComponent* InComponent, UTexture2D* InTexture, const FString& InActor, const FString& InComponentName)
        -> FCkTextureDebugger_ComponentRow
    {
        auto Texture = FCkTextureDebugger_TextureRow{};
        Texture.NavigationTarget = InTexture;
        Texture.Health.DisplayName = InActor + TEXT("_Texture");
        Texture.Health.AssetPath = FSoftObjectPath{InTexture};

        auto Slot = FCkTextureDebugger_MaterialSlotRow{};
        Slot.SlotIndex = 0;
        Slot.DisplayName = InActor + TEXT("_Material");
        Slot.Textures.Add(MoveTemp(Texture));

        auto Result = FCkTextureDebugger_ComponentRow{};
        Result.NavigationTarget = InComponent;
        Result.ActorPath = FSoftObjectPath{FString{TEXT("/Game/SceneAudit.")} + InActor};
        Result.ActorDisplayName = InActor;
        Result.ComponentDisplayName = InComponentName;
        Result.ComponentClassName = TEXT("StaticMeshComponent");
        Result.Kind = ECkTextureDebugger_ComponentKind::StaticMesh;
        Result.SupportsCheckerOverride = true;
        Result.MaterialSlots.Add(MoveTemp(Slot));
        return Result;
    }

    auto Find_SearchBoxes(const TSharedRef<SWidget>& InWidget, TArray<TSharedPtr<SSearchBox>>& OutSearchBoxes) -> void
    {
        if (InWidget->GetTypeAsString() == TEXT("SSearchBox"))
        {
            OutSearchBoxes.Add(StaticCastSharedRef<SSearchBox>(InWidget));
        }

        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        {
            Find_SearchBoxes(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), OutSearchBoxes);
        }
    }

    auto Find_Buttons(const TSharedRef<SWidget>& InWidget, TArray<TSharedPtr<SButton>>& OutButtons) -> void
    {
        if (InWidget->GetTypeAsString() == TEXT("SButton"))
        {
            OutButtons.Add(StaticCastSharedRef<SButton>(InWidget));
        }

        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        {
            Find_Buttons(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), OutButtons);
        }
    }

    auto Contains_Text(const TSharedRef<SWidget>& InWidget, const FString& InText) -> bool
    {
        if (InWidget->GetTypeAsString() == TEXT("SCkFlexText")
            && StaticCastSharedRef<SCkFlexText>(InWidget)->GetText().ToString().Contains(InText)) { return true; }
        if (InWidget->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InWidget)->GetText().ToString().Contains(InText))
        {
            return true;
        }

        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        {
            if (Contains_Text(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText))
            {
                return true;
            }
        }
        return false;
    }

    auto Find_WidgetsByType(const TSharedRef<SWidget>& InWidget, const FString& InType, TArray<TSharedPtr<SWidget>>& OutWidgets) -> void
    {
        if (InWidget->GetTypeAsString() == InType || InWidget->GetTag() == FName{*InType})
        {
            OutWidgets.Add(InWidget);
        }

        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        {
            Find_WidgetsByType(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType, OutWidgets);
        }
    }

    struct FMountedWindowScope final
    {
        explicit FMountedWindowScope(FSlateApplication& InSlate) : Slate(InSlate), OriginalCursor(InSlate.GetCursorPos()) {}
        ~FMountedWindowScope()
        {
            if (ContextTable.IsValid())
            {
                ContextTable->ReleaseContextMenu();
            }
            if (Window.IsValid())
            {
                Slate.DestroyWindowImmediately(Window.ToSharedRef());
            }
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

    auto Tick_Slate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto Route_MouseClick(FSlateApplication& InSlate, const FVector2D& InPosition, const FKey& InButton) -> void
    {
        InSlate.SetCursorPos(InPosition);
        const FPointerEvent Move{0, InPosition, InPosition, TSet<FKey>{}, EKeys::Invalid, 0.0f, FModifierKeysState{}};
        InSlate.ProcessMouseMoveEvent(Move, true);
        const FPointerEvent Down{0, InPosition, InPosition, TSet<FKey>{InButton}, InButton, 0.0f, FModifierKeysState{}};
        InSlate.ProcessMouseButtonDownEvent(nullptr, Down);
        const FPointerEvent Up{0, InPosition, InPosition, TSet<FKey>{}, InButton, 0.0f, FModifierKeysState{}};
        InSlate.ProcessMouseButtonUpEvent(Up);
    }

    auto Find_ContextMenu(FSlateApplication& InSlate, const FString& InLabel) -> TSharedPtr<SWindow>
    {
        for (int32 Attempt = 0; Attempt < 100; ++Attempt)
        {
            TArray<TSharedRef<SWindow>> Windows;
            InSlate.GetAllVisibleWindowsOrdered(Windows);
            for (const TSharedRef<SWindow>& Window : Windows)
            {
                if (!Contains_Text(Window, InLabel))
                {
                    continue;
                }

                auto Buttons = TArray<TSharedPtr<SWidget>>{};
                Find_WidgetsByType(Window, TEXT("SMenuEntryButton"), Buttons);
                for (const TSharedPtr<SWidget>& Button : Buttons)
                {
                    FWidgetPath Path;
                    const FGeometry Geometry = Button->GetCachedGeometry();
                    if (Contains_Text(Button.ToSharedRef(), InLabel) && Geometry.GetLocalSize().X > 0.0f
                        && Geometry.GetLocalSize().Y > 0.0f && InSlate.GeneratePathToWidgetUnchecked(Button.ToSharedRef(), Path))
                    {
                        return Window;
                    }
                }
            }
            FPlatformProcess::Sleep(0.01f);
            Tick_Slate(InSlate);
        }
        return {};
    }

    auto Find_MenuEntry(const TSharedRef<SWindow>& InWindow, const FString& InLabel) -> TSharedPtr<SButton>
    {
        auto Buttons = TArray<TSharedPtr<SWidget>>{};
        Find_WidgetsByType(InWindow, TEXT("SMenuEntryButton"), Buttons);
        const TSharedPtr<SWidget>* Found = Buttons.FindByPredicate(
            [&InLabel](const TSharedPtr<SWidget>& Button)
            {
                return Button.IsValid() && Contains_Text(Button.ToSharedRef(), InLabel);
            });
        return Found != nullptr ? StaticCastSharedPtr<SButton>(*Found) : nullptr;
    }

    auto Get_TextField(const SCkUiTable::FRecord& InRecord, const FString& InField) -> FString
    {
        const FCkUiFieldValue* Field = InRecord.IsValid() ? InRecord->FindField(InField) : nullptr;
        return Field != nullptr ? Field->Text.ToString() : FString{};
    }
} // namespace ck_texture_debugger_scene_audit_authored_tests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTextureDebugger_SceneAudit_Authored, "Ck.TextureDebugger.SceneAudit.Authored",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_SceneAudit_Authored::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_scene_audit_authored_tests;

    auto ComponentA = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    auto ComponentB = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    auto TextureA = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};
    auto TextureB = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};
    auto Snapshot = FCkTextureDebugger_LoadedWorldSnapshot{};
    Snapshot.Components.Add(Make_ComponentRow(ComponentA.Get(), TextureA.Get(), TEXT("FilterActor"), TEXT("FilterComponent")));
    Snapshot.Components.Add(Make_ComponentRow(ComponentB.Get(), TextureB.Get(), TEXT("OtherActor"), TEXT("OtherComponent")));

    auto Selected = TWeakObjectPtr<UPrimitiveComponent>{};
    auto SelectionCallbacks = 0;
    const auto Table = SNew(SCkTextureDebugger_SceneAuditTable)
                           .OnComponentSelected(
                               [&Selected, &SelectionCallbacks](TWeakObjectPtr<UPrimitiveComponent> InComponent)
                               {
                                   Selected = InComponent;
                                   ++SelectionCallbacks;
                               });
    Table->SetSnapshot(Snapshot);

    if (!TestTrue(TEXT("Scene Audit loads its installed authored document"), Table->Get_LayoutRevision() > 0))
    {
        AddError(TEXT("Scene Audit layout failed to load: ") + Table->Get_LayoutError().ToString());
        return false;
    }
    TestTrue(TEXT("Scene Audit installed authored document has no diagnostics"), Table->Get_LayoutError().IsEmpty());
    const TSharedPtr<SCkUiTable> AuthoredTable = Table->Get_AuthoredTable();
    TestTrue(TEXT("Scene Audit exposes its authored table"), AuthoredTable.IsValid());
    if (!AuthoredTable.IsValid())
    {
        return false;
    }

    const TSharedPtr<FAuthoredList> List = AuthoredTable->GetList();
    TestTrue(TEXT("Scene Audit authored table exposes its native list"), List.IsValid());
    if (!List.IsValid())
    {
        return false;
    }
    TestEqual(TEXT("Installed Scene Audit markup receives both loaded snapshot rows"), List->GetNumItemsBeingObserved(), 2);
    if (List->GetNumItemsBeingObserved() != 2)
    {
        return false;
    }

    auto Buttons = TArray<TSharedPtr<SButton>>{};
    Find_Buttons(Table, Buttons);
    const TSharedPtr<SButton>* ClearButtonFound = Buttons.FindByPredicate(
        [](const TSharedPtr<SButton>& Button)
        {
            return Button.IsValid() && Contains_Text(Button.ToSharedRef(), TEXT("Clear selection"));
        });
    TestNotNull(TEXT("Scene Audit exposes its real Clear selection button"), ClearButtonFound);
    if (ClearButtonFound == nullptr)
    {
        return false;
    }
    const TSharedPtr<SButton> ClearButton = *ClearButtonFound;
    TestTrue(TEXT("Scene Audit Clear selection control has its native SButton type"), ClearButton.IsValid());
    if (!ClearButton.IsValid())
    {
        return false;
    }
    ClearButton->SlatePrepass(1.0f);
    TestFalse(TEXT("Scene Audit Clear selection button begins disabled"), ClearButton->IsEnabled());

    const SCkUiTable::FRecord RetainedRecord = List->GetItems()[0];
    Table->SetSnapshot(Snapshot);
    TestTrue(TEXT("Snapshot reconciliation retains authored record identity by stable key"), List->GetItems().Contains(RetainedRecord));

    List->SetSelection(RetainedRecord, ESelectInfo::OnMouseClick);
    TestTrue(TEXT("Native authored list selection reaches the scene selection callback"), Selected.IsValid());
    TestEqual(TEXT("Authored selection selects one visible Scene Audit row"), Table->Get_SelectedRowCount(), 1);
    ClearButton->SlatePrepass(1.0f);
    TestTrue(TEXT("Scene Audit Clear selection button enables for a selected row"), ClearButton->IsEnabled());
    ClearButton->SimulateClick();
    TestFalse(TEXT("Scene Audit Clear selection button removes the public selection"), Selected.IsValid());
    TestEqual(TEXT("Scene Audit Clear selection button removes the domain row selection"), Table->Get_SelectedRowCount(), 0);
    TestEqual(TEXT("Scene Audit Clear selection button clears the native list selection"), List->GetSelectedItems().Num(), 0);
    ClearButton->SlatePrepass(1.0f);
    TestFalse(TEXT("Scene Audit Clear selection button disables after clearing"), ClearButton->IsEnabled());

    List->SetSelection(RetainedRecord, ESelectInfo::OnMouseClick);
    TestTrue(TEXT("Native authored list selection restores after clearing"), Selected.IsValid());
    const auto CallbackCountAfterUserSelection = SelectionCallbacks;
    Table->SetSelectedComponent(ComponentB.Get());
    const auto ExpectedSelection = AuthoredTable->GetSelectedKey();
    TestEqual(TEXT("Externally applied Scene Audit selection remains silent"), SelectionCallbacks, CallbackCountAfterUserSelection);
    TestEqual(TEXT("Externally applied Scene Audit selection replaces the visible row"), Table->Get_SelectedRowCount(), 1);
    Table->SetSnapshot(Snapshot);
    TestTrue(TEXT("Snapshot reconciliation retains the externally selected component"),
             ExpectedSelection.IsSet() && AuthoredTable->GetSelectedKey() == ExpectedSelection);
    TestEqual(TEXT("Snapshot reconciliation remains silent for retained selection"), SelectionCallbacks, CallbackCountAfterUserSelection);

    auto SearchBoxes = TArray<TSharedPtr<SSearchBox>>{};
    Find_SearchBoxes(Table, SearchBoxes);
    TestEqual(TEXT("Installed Scene Audit layout contains the Filter and Highlight search boxes"), SearchBoxes.Num(), 2);
    if (SearchBoxes.Num() != 2)
    {
        return false;
    }

    SearchBoxes[0]->SetText(FText::FromString(TEXT("FilterActor")));
    TestEqual(TEXT("Scene Audit Filter uses the native row Search projection"), List->GetNumItemsBeingObserved(), 1);
    if (List->GetNumItemsBeingObserved() != 1)
    {
        return false;
    }
    TestEqual(TEXT("Scene Audit Filter keeps the matching actor row"), Get_TextField(List->GetItems()[0], TEXT("actor")),
              TEXT("FilterActor"));

    SearchBoxes[0]->SetText(FText::GetEmpty());
    SearchBoxes[1]->SetText(FText::FromString(TEXT("FilterActor")));
    TestEqual(TEXT("Scene Audit Highlight leaves every authored row visible"), List->GetNumItemsBeingObserved(), 2);
    const FCkUiFieldValue* MatchColor = RetainedRecord->FindField(TEXT("text-color"));
    const auto* OtherRecord =
        List->GetItems().FindByPredicate([&RetainedRecord](const SCkUiTable::FRecord& InRecord) { return InRecord != RetainedRecord; });
    TestTrue(TEXT("Scene Audit Highlight keeps the matching retained record bright"),
             MatchColor != nullptr && MatchColor->Color.Equals(CkStyle::Text()));
    const FCkUiFieldValue* OtherColor =
        OtherRecord != nullptr && OtherRecord->IsValid() ? (*OtherRecord)->FindField(TEXT("text-color")) : nullptr;
    TestTrue(TEXT("Scene Audit Highlight mutes a nonmatching retained record"),
             OtherColor != nullptr && OtherColor->Color.Equals(CkStyle::TextMute()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTextureDebugger_SceneAudit_ContextCommands,
                                 "Ck.TextureDebugger.SceneAudit.ContextCommands",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_SceneAudit_ContextCommands::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_scene_audit_authored_tests;

    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Scene Audit context-command test requires initialized Slate."));
        return false;
    }

    const auto ComponentA = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    const auto ComponentB = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    const auto TextureA = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};
    const auto TextureB = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};
    auto Snapshot = FCkTextureDebugger_LoadedWorldSnapshot{};
    Snapshot.Components.Add(Make_ComponentRow(ComponentA.Get(), TextureA.Get(), TEXT("ContextActor_A"), TEXT("ContextComponent_A")));
    Snapshot.Components.Add(Make_ComponentRow(ComponentB.Get(), TextureB.Get(), TEXT("ContextActor_B"), TEXT("ContextComponent_B")));

    const TSharedRef<SCkTextureDebugger_SceneAuditTable> Table = SNew(SCkTextureDebugger_SceneAuditTable);
    Table->SetSnapshot(Snapshot);
    if (!TestTrue(TEXT("Installed Scene Audit context markup loaded"), Table->Get_LayoutRevision() > 0)
        || !TestTrue(TEXT("Installed Scene Audit context markup has no diagnostics"), Table->Get_LayoutError().IsEmpty()))
    {
        return false;
    }

    const TSharedPtr<SCkUiTable> AuthoredTable = Table->Get_AuthoredTable();
    const TSharedPtr<FAuthoredList> List = AuthoredTable.IsValid() ? AuthoredTable->GetList() : nullptr;
    if (!TestTrue(TEXT("Installed Scene Audit context markup exposes its authored list"), AuthoredTable.IsValid() && List.IsValid()))
    {
        return false;
    }

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
    Tick_Slate(Slate);

    const SCkUiTable::FRecord* ItemA = List->GetItems().FindByPredicate(
        [](const SCkUiTable::FRecord& Item)
        {
            return Get_TextField(Item, TEXT("actor")) == TEXT("ContextActor_A");
        });
    const SCkUiTable::FRecord* ItemB = List->GetItems().FindByPredicate(
        [](const SCkUiTable::FRecord& Item)
        {
            return Get_TextField(Item, TEXT("actor")) == TEXT("ContextActor_B");
        });
    if (!TestNotNull(TEXT("Installed Scene Audit table projects row A"), ItemA)
        || !TestNotNull(TEXT("Installed Scene Audit table projects row B"), ItemB))
    {
        return false;
    }

    const auto OpenContextMenuForB = [&Slate, &List, ItemB]() -> TSharedPtr<SWindow>
    {
        const TSharedPtr<ITableRow> NativeRow = List->WidgetFromItem(*ItemB);
        if (!NativeRow.IsValid())
        {
            return {};
        }

        const TSharedRef<SWidget> NativeRowWidget = NativeRow->AsWidget();
        const FGeometry Geometry = NativeRowWidget->GetCachedGeometry();
        FWidgetPath RowPath;
        if (Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f
            || !Slate.GeneratePathToWidgetUnchecked(NativeRowWidget, RowPath))
        {
            return {};
        }

        const FVector2D MenuPosition = Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize() * 0.5f;
        Route_MouseClick(Slate, MenuPosition, EKeys::RightMouseButton);
        Tick_Slate(Slate);
        return Find_ContextMenu(Slate, TEXT("Copy component path"));
    };

    TSharedPtr<SWindow> MenuWindow = OpenContextMenuForB();
    if (!TestTrue(TEXT("Right-clicking Scene Audit row B opens its authored popup menu"), MenuWindow.IsValid()))
    {
        return false;
    }
    TestTrue(TEXT("Scene Audit popup contains Copy component path"), Contains_Text(MenuWindow.ToSharedRef(), TEXT("Copy component path")));
    TestTrue(TEXT("Scene Audit popup contains Copy audit summary"), Contains_Text(MenuWindow.ToSharedRef(), TEXT("Copy audit summary")));

    List->SetSelection(*ItemA, ESelectInfo::OnMouseClick);
    TestTrue(TEXT("Native Scene Audit selection can move to A while B's popup remains open"),
             AuthoredTable->GetSelectedKey().IsSet() && AuthoredTable->GetSelectedKey() == (*ItemA)->GetKey());

    TSharedPtr<SButton> CopyPathButton = Find_MenuEntry(MenuWindow.ToSharedRef(), TEXT("Copy component path"));
    if (!TestTrue(TEXT("Copy component path is a mounted native popup command"), CopyPathButton.IsValid()))
    {
        return false;
    }
    FWidgetPath CopyPathButtonPath;
    const FGeometry CopyPathGeometry = CopyPathButton->GetCachedGeometry();
    if (!TestTrue(TEXT("Copy component path has hit-testable arranged geometry"), CopyPathGeometry.GetLocalSize().X > 0.0f
                                                                                   && CopyPathGeometry.GetLocalSize().Y > 0.0f
                                                                                   && Slate.GeneratePathToWidgetUnchecked(CopyPathButton.ToSharedRef(), CopyPathButtonPath)))
    {
        return false;
    }

    FClipboardTextScope Clipboard;
    Route_MouseClick(Slate, CopyPathGeometry.GetAbsolutePosition() + CopyPathGeometry.GetAbsoluteSize() * 0.5f, EKeys::LeftMouseButton);
    FString CopiedText;
    FPlatformApplicationMisc::ClipboardPaste(CopiedText);
    TestEqual(TEXT("Copy component path remains targeted at the row that opened the popup"), CopiedText, ComponentB->GetPathName());
    TestFalse(TEXT("Copy component path does not retarget to selected row A"), CopiedText == ComponentA->GetPathName());

    MenuWindow = OpenContextMenuForB();
    if (!TestTrue(TEXT("Scene Audit row B can reopen its authored popup for summary copy"), MenuWindow.IsValid()))
    {
        return false;
    }
    TSharedPtr<SButton> CopySummaryButton = Find_MenuEntry(MenuWindow.ToSharedRef(), TEXT("Copy audit summary"));
    List->SetSelection(*ItemA, ESelectInfo::OnMouseClick);
    TestTrue(TEXT("Selection returns to A before invoking B's summary command"),
        AuthoredTable->GetSelectedKey().IsSet() && AuthoredTable->GetSelectedKey() == (*ItemA)->GetKey());
    if (!TestTrue(TEXT("Copy audit summary is a mounted native popup command"), CopySummaryButton.IsValid()))
    {
        return false;
    }
    FWidgetPath CopySummaryButtonPath;
    const FGeometry CopySummaryGeometry = CopySummaryButton->GetCachedGeometry();
    if (!TestTrue(TEXT("Copy audit summary has hit-testable arranged geometry"), CopySummaryGeometry.GetLocalSize().X > 0.0f
                                                                                 && CopySummaryGeometry.GetLocalSize().Y > 0.0f
                                                                                 && Slate.GeneratePathToWidgetUnchecked(CopySummaryButton.ToSharedRef(), CopySummaryButtonPath)))
    {
        return false;
    }

    Route_MouseClick(Slate, CopySummaryGeometry.GetAbsolutePosition() + CopySummaryGeometry.GetAbsoluteSize() * 0.5f, EKeys::LeftMouseButton);
    FPlatformApplicationMisc::ClipboardPaste(CopiedText);
    TestTrue(FString::Printf(TEXT("Copy audit summary remains targeted at the row that opened the popup; clipboard=[%s]"), *CopiedText),
             CopiedText.Contains(TEXT("ContextActor_B")) && CopiedText.Contains(TEXT("ContextComponent_B"))
                 && CopiedText.Contains(ComponentB->GetPathName()));
    TestFalse(TEXT("Copy audit summary does not retarget to selected row A"), CopiedText.Contains(TEXT("ContextActor_A")));
    return true;
}

#endif
