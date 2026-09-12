#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_EntityInfo.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"

namespace ck_inspector_entity_info_authored_test
{
    auto CountType(const TSharedRef<SWidget>& InRoot, const FString& InType) -> int32
    {
        int32 Count = InRoot->GetTypeAsString() == InType ? 1 : 0;
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { Count += CountType(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType); }
        return Count;
    }

    auto FindInput(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTag() == TEXT("entity-info-name-input")
            && InRoot->GetTypeAsString() == TEXT("SCkUiTextInputBox"))
        { return StaticCastSharedRef<SEditableTextBox>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableTextBox> Found = FindInput(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto ReplaceAndCommit(FSlateApplication& InSlate, const TSharedRef<SEditableTextBox>& InInput, const FString& InText) -> bool
    {
        InSlate.SetUserFocus(0, InInput, EFocusCause::SetDirectly);
        Tick(InSlate);
        const FModifierKeysState Control{false, false, true, false, false, false, false, false, false};
        if (NOT InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::A, Control, 0, false, 0, 0})) { return false; }
        for (const TCHAR Character : InText)
        {
            if (NOT InSlate.ProcessKeyCharEvent(FCharacterEvent{Character, FModifierKeysState{}, 0, false})) { return false; }
        }
        if (NOT InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0})) { return false; }
        Tick(InSlate);
        return true;
    }

    auto BeginDraftWithoutTick(FSlateApplication& InSlate, const TSharedRef<SEditableTextBox>& InInput, const TCHAR InCharacter) -> bool
    {
        InSlate.SetUserFocus(0, InInput, EFocusCause::SetDirectly);
        Tick(InSlate);
        const FModifierKeysState Control{false, false, true, false, false, false, false, false, false};
        return InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::A, Control, 0, false, 0, 0})
            && InSlate.ProcessKeyCharEvent(FCharacterEvent{InCharacter, FModifierKeysState{}, 0, false});
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope() { if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); } }
        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorEntityInfoAuthored,
    "Ck.UiAuthoring.EcsDebugger.EntityInfoInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorEntityInfoAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_entity_info_authored_test;
    if (NOT FSlateApplication::IsInitialized()) { AddError(TEXT("Entity Info authored test requires Slate.")); return false; }

    auto World = ck::FEcsWorld{};
    auto EntityA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto EntityB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    TestTrue(TEXT("fixture creates distinct entities"), ck::IsValid(EntityA) && ck::IsValid(EntityB) && EntityA != EntityB);
    if (NOT ck::IsValid(EntityA) || NOT ck::IsValid(EntityB)) { return false; }
    UCk_Utils_Handle_UE::Set_DebugName(EntityA, TEXT("Entity A"), ECk_Override::Override);
    UCk_Utils_Handle_UE::Set_DebugName(EntityB, TEXT("Entity B"), ECk_Override::Override);

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    auto Inspector = FCkInspector_EntityInfo{};
    Inspector.Set_EditGuard(EditGuard);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("first build returns authored Entity Info"), RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_EntityInfoAuthored")})
        || NOT TestEqual(TEXT("second build returns authored Entity Info"), RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_EntityInfoAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }

    const TSharedRef<SCkInspector_EntityInfoAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_EntityInfoAuthored>(RenderedA);
    const TSharedRef<SCkInspector_EntityInfoAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_EntityInfoAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    if (NOT TestTrue(TEXT("each build owns an independent accepted view"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && ViewA != ViewB && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    { AddError(AuthoredA->Get_LoadError()); AddError(AuthoredB->Get_LoadError()); return false; }
    TestTrue(TEXT("each authored instance mounts one native EntityRef port"),
        CountType(RenderedA, TEXT("SCkDebug_EntityRef")) == 1 && CountType(RenderedB, TEXT("SCkDebug_EntityRef")) == 1);
    TestTrue(TEXT("live values preserve name and absent actor semantics"),
        AuthoredA->Get_NameText() == TEXT("Entity A") && AuthoredA->Get_ActorText() == TEXT("None"));

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{520.0f, 220.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [RenderedA];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    Tick(Slate);
    const TSharedPtr<SEditableTextBox> Input = FindInput(RenderedA);
    if (NOT TestTrue(TEXT("HTML text-input exposes the retained native editor"), Input.IsValid())) { return false; }
    TestTrue(TEXT("native keystroke begins an authored draft without an intervening tick"),
        BeginDraftWithoutTick(Slate, Input.ToSharedRef(), TEXT('D')));
    TestTrue(TEXT("the changed callback claims the panel edit guard synchronously"), EditGuard->Get_HasActiveEdit());
    TestTrue(TEXT("Escape cancels the retained draft"),
        Slate.ProcessKeyDownEvent(FKeyEvent{EKeys::Escape, FModifierKeysState{}, 0, false, 0, 0}));
    Tick(Slate);
    TestFalse(TEXT("cancelled draft releases the panel edit guard"), EditGuard->Get_HasActiveEdit());
    if (NOT TestTrue(TEXT("keyboard commit reaches the production rename action"),
        ReplaceAndCommit(Slate, Input.ToSharedRef(), TEXT("  Renamed A  ")))) { return false; }
    TestTrue(TEXT("rename trims whitespace and overrides the existing debug name"),
        UCk_Utils_Handle_UE::Get_DebugName(EntityA) == TEXT("Renamed A") && AuthoredA->Get_NameText() == TEXT("Renamed A"));
    if (NOT TestTrue(TEXT("whitespace-only keyboard commit is routed"),
        ReplaceAndCommit(Slate, Input.ToSharedRef(), TEXT("   ")))) { return false; }
    TestEqual(TEXT("whitespace-only rename is rejected"), UCk_Utils_Handle_UE::Get_DebugName(EntityA), FName{TEXT("Renamed A")});

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native replay preserves live name and identity values for multi-select diff"),
        RowsA.FindRef(TEXT("Name:")) != RowsB.FindRef(TEXT("Name:"))
            && RowsA.FindRef(TEXT("ID:")) != RowsB.FindRef(TEXT("ID:"))
            && Differing.Contains(TEXT("Name:")) && Differing.Contains(TEXT("Set Name:")) && Differing.Contains(TEXT("ID:")));

    TSharedPtr<SCkInspector_EntityInfoAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_EntityInfoAuthored>(Inspector.Build_Inspector(EntityA));
    }
    TestTrue(TEXT("authored labels receive matching diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_NameDiffMarked() && DiffAuthored->Is_SetNameDiffMarked() && DiffAuthored->Is_IdDiffMarked());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Entity Info resource is readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityInfo.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityInfo.ui.css"))))) { return false; }
    TestTrue(TEXT("resource owns stable row labels, input action, value bindings, and native identity port"),
        Markup.Contains(TEXT(">Name:</text>")) && Markup.Contains(TEXT(">Set Name:</text>"))
            && Markup.Contains(TEXT(">ID:</text>")) && Markup.Contains(TEXT(">Actor:</text>"))
            && Markup.Contains(TEXT("<text-input")) && Markup.Contains(TEXT("entity-info-name-changed"))
            && Markup.Contains(TEXT("entity-info-name-committed"))
            && Markup.Contains(TEXT("entity-info-id-port")) && Markup.Contains(TEXT("entity-info-actor-color")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    const TSharedPtr<SEditableTextBox> InputBeforeReload = Input;
    TestTrue(TEXT("compatible reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Entity Info A compatible candidate")).Succeeded);
    TestTrue(TEXT("A reload retains its input and does not mutate B"),
        FindInput(RenderedA) == InputBeforeReload && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing native identity port is rejected atomically by B"), ViewB->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><text id=\"name\" bind=\"entity-info-name-value\" /></region></ui>"),
        TEXT(""), TEXT("Entity Info B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected B reload retains its tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    const FName NameBeforeRelease = UCk_Utils_Handle_UE::Get_DebugName(EntityA);
    const TWeakPtr<FCkUiView> ReleasedViewA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedViewB = ViewB;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes every retained result inert and releases the edit guard"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted()
            && NOT EditGuard->Get_HasActiveEdit());
    ViewA.Reset(); ViewB.Reset();
    TestFalse(TEXT("deactivation releases all per-build views"), ReleasedViewA.IsValid() || ReleasedViewB.IsValid());
    ReplaceAndCommit(Slate, Input.ToSharedRef(), TEXT("Stale Rename"));
    TestEqual(TEXT("held stale input cannot mutate its released entity"), UCk_Utils_Handle_UE::Get_DebugName(EntityA), NameBeforeRelease);
    return true;
}

#endif
