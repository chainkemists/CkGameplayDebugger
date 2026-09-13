#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_SceneNode.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Processor.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiStyledButton.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_inspector_scene_node_authored_test
{
    auto ContainsText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock"))
        {
            const TSharedRef<STextBlock> Text = StaticCastSharedRef<STextBlock>(InRoot);
            if (Text->GetText().ToString().Contains(InText)) { return true; }
        }
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText"))
        {
            const TSharedRef<SCkFlexText> Text = StaticCastSharedRef<SCkFlexText>(InRoot);
            if (Text->GetText().ToString().Contains(InText)) { return true; }
        }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; }
        }
        return false;
    }

    auto FindTaggedButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        const FString Type = InRoot->GetTypeAsString();
        if ((Type == TEXT("SButton") || Type == TEXT("SCkUiStyledButton")) && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindTaggedButton(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            {
                return Found;
            }
        }
        return nullptr;
    }

    auto FindEditor(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SEditableTextBox"))
        {
            return StaticCastSharedRef<SEditableTextBox>(InRoot);
        }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableTextBox> Found = FindEditor(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            {
                return Found;
            }
        }
        return nullptr;
    }

    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto SetAndCommit(
        FSlateApplication& InSlate,
        const TSharedRef<SEditableTextBox>& InInput,
        const FString& InText) -> bool
    {
        InSlate.SetUserFocus(0, InInput, EFocusCause::SetDirectly);
        TickSlate(InSlate);
        InInput->SetText(FText::FromString(InText));
        if (NOT InSlate.ProcessKeyDownEvent(
            FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0}))
        {
            return false;
        }
        TickSlate(InSlate);
        return true;
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope()
        {
            if (Window.IsValid())
            {
                Slate.DestroyWindowImmediately(Window.ToSharedRef());
            }
        }

        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };

    auto FindTaggedButtonForText(
        const TSharedRef<SWidget>& InRoot,
        const FString& InText,
        const FName InTag) -> TSharedPtr<SButton>
    {
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindTaggedButtonForText(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText, InTag); Found.IsValid())
            { return Found; }
        }
        return ContainsText(InRoot, InText) ? FindTaggedButton(InRoot, InTag) : nullptr;
    }

    auto FindRecordKeyByText(const FCkUiCollection& InCollection, const FString& InText) -> FString
    {
        for (const TSharedPtr<const FCkUiRecord>& Record : InCollection.GetRecords())
        {
            const FCkUiFieldValue* Field = Record.IsValid() ? Record->FindField(TEXT("sibling")) : nullptr;
            if (Field != nullptr && Field->Kind == ECkUiFieldKind::Text && Field->Text.ToString() == InText)
            { return Record->GetKey(); }
        }
        return {};
    }

    auto CreateTransform(const FCk_Handle& InOwner, const FTransform& InTransform) -> FCk_Handle_Transform
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
        if (ck::Is_NOT_Valid(Entity)) { return {}; }
        return UCk_Utils_Transform_UE::Add(Entity, InTransform, ECk_Replication::DoesNotReplicate);
    }

    auto CreateSceneNode(
        const FCk_Handle& InOwner,
        FCk_Handle_Transform& InParent,
        const FTransform& InLocalTransform) -> FCk_Handle_SceneNode
    {
        auto Child = CreateTransform(InOwner, InLocalTransform *
            UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(InParent));
        return ck::IsValid(Child)
            ? UCk_Utils_SceneNode_UE::Add(Child, InParent, InLocalTransform)
            : FCk_Handle_SceneNode{};
    }

    auto TickAuthored(const TSharedRef<SCkInspector_SceneNodeAuthored>& InAuthored) -> void
    {
        InAuthored->Tick(
            FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorSceneNodeAuthored,
    "Ck.UiAuthoring.EcsDebugger.SceneNodeInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorSceneNodeAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_scene_node_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = GetMutableDefault<UCkDebuggerStyleSettings>();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), StyleSettings)) { return false; }
    const ECkDebugAxis_EditControlStyle PreviousStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    if (NOT TestTrue(TEXT("fixture has transient ownership and an automation world"),
        ck::IsValid(LifetimeOwner) && GWorld != nullptr)) { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(GWorld);

    auto ParentA = CreateTransform(LifetimeOwner, FTransform{FRotator{0.0f, 20.0f, 0.0f}, FVector{100.0f, 0.0f, 0.0f}});
    auto ParentB = CreateTransform(LifetimeOwner, FTransform{FRotator::ZeroRotator, FVector{-100.0f, 50.0f, 0.0f}});
    const FTransform LocalA{FRotator{10.0f, 20.0f, 30.0f}, FVector{1.0f, 2.0f, 3.0f}, FVector{2.0f, 3.0f, 4.0f}};
    auto NodeA = CreateSceneNode(LifetimeOwner, ParentA, LocalA);
    auto OldSibling = CreateSceneNode(LifetimeOwner, ParentA, FTransform{FRotator::ZeroRotator, FVector{4.0f, 5.0f, 6.0f}});
    auto NodeB = CreateSceneNode(LifetimeOwner, ParentB, FTransform{FRotator::ZeroRotator, FVector{-7.0f, 8.0f, 9.0f}});
    if (NOT TestTrue(TEXT("fixture creates two parents, two siblings, and an independent node"),
        ck::IsValid(ParentA) && ck::IsValid(ParentB) && ck::IsValid(NodeA)
            && ck::IsValid(OldSibling) && ck::IsValid(NodeB))) { return false; }

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    const TSharedPtr<FCkDebuggerModel_EntitySelection> Selection = MakeShared<FCkDebuggerModel_EntitySelection>();
    auto Inspector = FCkInspector_SceneNode{};
    Inspector.Set_EditGuard(EditGuard);
    Inspector.Set_SelectionModel(Selection);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(NodeA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(NodeB);
    if (NOT TestEqual(TEXT("A mounts SceneNode's authored shell"), RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_SceneNodeAuthored")})
        || NOT TestEqual(TEXT("B mounts an independent SceneNode authored shell"), RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_SceneNodeAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_SceneNodeAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_SceneNodeAuthored>(RenderedA);
    const TSharedRef<SCkInspector_SceneNodeAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_SceneNodeAuthored>(RenderedB);
    const TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    const TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    const TSharedPtr<FCkUiCollection> SiblingsA = AuthoredA->Get_SiblingsCollection();
    TestTrue(TEXT("independent views project live parent, relative, resolved, and sibling state"),
        ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB && SiblingsA.IsValid()
            && AuthoredA->Get_Parent() == FCk_Handle{ParentA}
            && AuthoredB->Get_Parent() == FCk_Handle{ParentB}
            && AuthoredA->Get_RelativeTransform().Equals(LocalA)
            && SiblingsA->GetRecords().Num() == 2);

    ViewA->GetRegion(TEXT("main"))->SlatePrepass();
    const TSharedPtr<SButton> HeldDetach = FindTaggedButton(RenderedA, TEXT("scene-node-detach"));
    const FString OldSiblingText = FCk_Handle{OldSibling}.ToString();
    const TSharedPtr<SButton> HeldOldSibling = FindTaggedButtonForText(
        RenderedA, OldSiblingText, TEXT("scene-node-sibling"));
    if (NOT TestTrue(TEXT("authored physical detach and sibling actions are mounted"),
        HeldDetach.IsValid() && HeldDetach->IsEnabled() && HeldOldSibling.IsValid())) { return false; }

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(NodeA); RowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(NodeB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native fallback remains the complete capture and diff authority"),
        RowsA.Contains(TEXT("Parent:")) && RowsA.Contains(TEXT("Layer:"))
            && RowsA.Contains(TEXT("Dirty This Frame:")) && RowsA.Contains(TEXT("Location:"))
            && RowsA.Contains(TEXT("Rotation (R,P,Y):")) && RowsA.Contains(TEXT("Scale:"))
            && RowsA.Contains(TEXT("Set Location:")) && RowsA.Contains(TEXT("Set Rotation (R,P,Y):"))
            && RowsA.Contains(TEXT("Set Scale:")) && RowsA.Contains(TEXT("Attachment:"))
            && RowsA.Contains(TEXT("Nodes:")) && Differing.Contains(TEXT("Location:")));
    TSharedPtr<SCkInspector_SceneNodeAuthored> DiffAuthored;
    {
        const FCkInspector_DiffMarkScope DiffScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_SceneNodeAuthored>(Inspector.Build_Inspector(NodeA));
    }
    TestTrue(TEXT("authored labels retain native comparison verdicts"),
        DiffAuthored.IsValid() && DiffAuthored->Is_DiffMarked(TEXT("Location:")));

    const FVector EditedLocation{11.0f, 12.0f, 13.0f};
    const FRotator EditedRotation{15.0f, 25.0f, 35.0f};
    const FVector EditedScale{5.0f, 6.0f, 7.0f};
    AuthoredA->Commit_Location(EditedLocation);
    AuthoredA->Commit_Rotation(EditedRotation);
    AuthoredA->Commit_Scale(EditedScale);
    TestTrue(TEXT("back-to-back edits expose one composed pending transform before the request pump"),
        NodeA.Has<ck::FFragment_SceneNode_Requests>()
            && AuthoredA->Get_RelativeTransform().GetLocation().Equals(EditedLocation)
            && AuthoredA->Get_RelativeTransform().GetRotation().Rotator().Equals(EditedRotation)
            && AuthoredA->Get_RelativeTransform().GetScale3D().Equals(EditedScale));
    ck::FProcessor_SceneNode_HandleRequests{World.Get_Registry()}.Pump();
    const FTransform Updated = UCk_Utils_SceneNode_UE::Get_Offset(NodeA);
    TestTrue(TEXT("one request pump applies every back-to-back authored edit without clobbering siblings"),
        Updated.GetLocation().Equals(EditedLocation)
            && Updated.GetRotation().Rotator().Equals(EditedRotation)
            && Updated.GetScale3D().Equals(EditedScale));

    const FTransform ExternalQueuedOffset{
        FRotator{45.0f, 55.0f, 65.0f}, FVector{21.0f, 22.0f, 23.0f}, FVector{8.0f, 9.0f, 10.0f}};
    const FVector InterleavedLocation{31.0f, 32.0f, 33.0f};
    UCk_Utils_SceneNode_UE::Request_UpdateOffset(NodeA,
        FCk_Request_SceneNode_UpdateRelativeTransform{ExternalQueuedOffset}, {});
    AuthoredA->Commit_Location(InterleavedLocation);
    ck::FProcessor_SceneNode_HandleRequests{World.Get_Registry()}.Pump();
    const FTransform Interleaved = UCk_Utils_SceneNode_UE::Get_Offset(NodeA);
    TestTrue(TEXT("authored edits compose over a newer external queued request without requiring a widget tick"),
        Interleaved.GetLocation().Equals(InterleavedLocation)
            && Interleaved.GetRotation().Equals(ExternalQueuedOffset.GetRotation())
            && Interleaved.GetScale3D().Equals(ExternalQueuedOffset.GetScale3D()));
    TickAuthored(AuthoredA);
    TestTrue(TEXT("the authored projection reads the committed SceneNode offset after the request pump"),
        AuthoredA->Get_RelativeTransform().Equals(Interleaved));

    HeldOldSibling->SimulateClick();
    TestTrue(TEXT("physical sibling action selects its exact live handle"),
        Selection->Get_PrimarySelection() == FCk_Handle{OldSibling});
    const FString OldKey = FindRecordKeyByText(*SiblingsA, OldSiblingText);
    UCk_Utils_SceneNode_UE::Request_Detach(OldSibling, {});
    auto NewSibling = CreateSceneNode(LifetimeOwner, ParentA,
        FTransform{FRotator::ZeroRotator, FVector{40.0f, 50.0f, 60.0f}});
    TickAuthored(AuthoredA);
    const FString NewSiblingText = FCk_Handle{NewSibling}.ToString();
    const FString NewKey = FindRecordKeyByText(*SiblingsA, NewSiblingText);
    TestTrue(TEXT("equal-count sibling replacement reconciles by full handle identity"),
        ck::IsValid(NewSibling) && SiblingsA->GetRecords().Num() == 2
            && NOT OldKey.IsEmpty() && NOT NewKey.IsEmpty() && OldKey != NewKey
            && NOT SiblingsA->FindRecord(OldKey).IsValid() && SiblingsA->FindRecord(NewKey).IsValid());
    Selection->Set_SelectedEntities({FCk_Handle{NodeA}});
    HeldOldSibling->SimulateClick();
    TestTrue(TEXT("held stale sibling control cannot navigate after equal-count replacement"),
        Selection->Get_PrimarySelection() == FCk_Handle{NodeA});
    AuthoredA->Navigate_Sibling(NewKey);
    TestTrue(TEXT("replacement stable key navigates only to the replacement"),
        Selection->Get_PrimarySelection() == FCk_Handle{NewSibling});

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString Root = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("SceneNode authored resources are installed"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Root, TEXT("EcsInspectorSceneNode.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Root, TEXT("EcsInspectorSceneNode.ui.css")))))
    { return false; }
    TestTrue(TEXT("HTML owns complete read, edit, detach, resolved, and sibling placement"),
        Markup.Contains(TEXT(">Relative Transform</text>"))
            && Markup.Contains(TEXT(">Edit Relative Transform</text>"))
            && Markup.Contains(TEXT("action=\"scene-node-detach\""))
            && Markup.Contains(TEXT("bind=\"scene-node-resolved-rotation-2\""))
            && Markup.Contains(TEXT("bind=\"scene-node-resolved-scale-2\""))
            && Markup.Contains(TEXT("item-action=\"scene-node-navigate-sibling\""))
            && NOT Markup.Contains(TEXT("scene-node-native-body")));
    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload preserves A's retained view"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("SceneNode A compatible candidate")).Succeeded
            && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore
            && FindTaggedButton(RenderedA, TEXT("scene-node-detach")) == HeldDetach);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    TestFalse(TEXT("missing native editor port is rejected atomically"), ViewB->TryReload(
        Markup.Replace(TEXT("bind=\"scene-node-scale-port\""), TEXT("bind=\"missing-scene-node-port\"")),
        Stylesheet, TEXT("SceneNode B missing port")).Succeeded);
    TestTrue(TEXT("rejected reload preserves B's tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBefore);

    TestTrue(TEXT("partial SceneNode composition can be removed while the entity remains live"),
        NodeB.Try_Remove<ck::FFragment_SceneNode_Current>() && ck::IsValid(NodeB));
    AuthoredB->Commit_Location(FVector{999.0f});
    AuthoredB->Request_Detach();
    TestTrue(TEXT("held controls fail closed after SceneNode composition loss"),
        NOT AuthoredB->Get_IsAvailable() && NOT AuthoredB->Get_CanRequest()
            && NOT NodeB.Has<ck::FFragment_SceneNode_Requests>());

    const FTransform WorldBeforeDetach = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(NodeA);
    HeldDetach->SimulateClick();
    TestTrue(TEXT("physical detach is immediate, preserves world transform, and inerts the mounted surface"),
        ck::IsValid(NodeA) && UCk_Utils_Transform_UE::Has(NodeA) && NOT UCk_Utils_SceneNode_UE::Has(NodeA)
            && UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(NodeA).Equals(WorldBeforeDetach)
            && NOT AuthoredA->Get_IsAvailable() && NOT AuthoredA->Get_CanRequest());
    HeldDetach->SimulateClick();
    TestFalse(TEXT("held detach control cannot recreate SceneNode request state"),
        NodeA.Has<ck::FFragment_SceneNode_Requests>());

    auto TeardownNode = CreateSceneNode(LifetimeOwner, ParentB,
        FTransform{FRotator{3.0f, 4.0f, 5.0f}, FVector{6.0f, 7.0f, 8.0f}});
    if (NOT TestTrue(TEXT("teardown fixture creates a live SceneNode"), ck::IsValid(TeardownNode)))
    {
        return false;
    }
    const FTransform TeardownOffset = UCk_Utils_SceneNode_UE::Get_Offset(TeardownNode);
    TSharedPtr<SWidget> HeldRotationInput;
    TSharedPtr<SEditableTextBox> HeldRotationEditor;
    {
        auto TeardownInspector = FCkInspector_SceneNode{};
        TeardownInspector.Set_EditGuard(EditGuard);
        const TSharedRef<SWidget> TeardownRendered = TeardownInspector.Build_Inspector(TeardownNode);
        TeardownRendered->SlatePrepass();
        HeldRotationInput = FindTaggedWidget(TeardownRendered, TEXT("scene-node-rotation-input-0"));
        HeldRotationEditor = HeldRotationInput.IsValid()
            ? FindEditor(HeldRotationInput.ToSharedRef())
            : TSharedPtr<SEditableTextBox>{};
        if (NOT TestTrue(TEXT("teardown fixture retains the physical rotation numeric leaf"),
            HeldRotationInput.IsValid() && HeldRotationEditor.IsValid()))
        {
            return false;
        }
        TeardownInspector.OnDeactivated();
    }

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{160.0f, 80.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [HeldRotationEditor.ToSharedRef()];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    TickSlate(Slate);
    const FString ReleasedDisplay = HeldRotationEditor->GetText().ToString();
    const bool bReleasedCommitHandled = SetAndCommit(Slate, HeldRotationEditor.ToSharedRef(), TEXT("73"));
    TestEqual(TEXT("retained rotation getter evaluates to the safe inert default"),
        ReleasedDisplay, FString{TEXT("0.00")});
    TestTrue(TEXT("retained rotation editor still receives the physical Enter route"),
        bReleasedCommitHandled);
    TestTrue(TEXT("retained rotation commit stays inert after owner release and destruction"),
        UCk_Utils_SceneNode_UE::Get_Offset(TeardownNode).Equals(TeardownOffset)
            && NOT TeardownNode.Has<ck::FFragment_SceneNode_Requests>());
    TestFalse(TEXT("retained rotation edit callbacks cannot reactivate the released guard"),
        EditGuard->Get_HasActiveEdit());

    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases every retained view, collection, and edit scope"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted()
            && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredA->Get_SiblingsCollection().IsValid()
            && NOT EditGuard->Get_HasActiveEdit());
    AuthoredA->Navigate_Sibling(NewKey);
    TestTrue(TEXT("held sibling controls remain inert after release"),
        Selection->Get_PrimarySelection() == FCk_Handle{NewSibling});
    return true;
}

#endif
