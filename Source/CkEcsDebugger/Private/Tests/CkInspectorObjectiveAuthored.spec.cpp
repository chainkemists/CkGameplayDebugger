#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Objective.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkObjective/Objective/CkObjective_Fragment.h"
#include "CkObjective/Objective/CkObjective_Utils.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/IToolTip.h"
#include "Widgets/Text/STextBlock.h"

#include <variant>

namespace ck_inspector_objective_authored_test
{
    auto ContainsText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock"))
        {
            const TSharedRef<STextBlock> Text = StaticCastSharedRef<STextBlock>(InRoot);
            if (Text->GetText().ToString() == InText) { return true; }
        }
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText"))
        {
            const TSharedRef<SCkFlexText> Text = StaticCastSharedRef<SCkFlexText>(InRoot);
            if (Text->GetText().ToString() == InText) { return true; }
        }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (ContainsText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; } }
        return false;
    }

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SButton> Found = FindButton(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
            if (Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto CountType(const TSharedRef<SWidget>& InRoot, const FString& InType) -> int32
    {
        int32 Count = InRoot->GetTypeAsString() == InType ? 1 : 0;
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { Count += CountType(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType); }
        return Count;
    }

    auto GetToolTipText(const TSharedRef<SWidget>& InWidget) -> FString
    {
        const TSharedPtr<IToolTip> Tooltip = InWidget->GetToolTip();
        if (NOT Tooltip.IsValid()) { return {}; }
        const TSharedRef<SWidget> Content = Tooltip->GetContentWidget();
        Content->SlatePrepass();
        return Content->GetAccessibleText().ToString();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorObjectiveAuthored,
    "Ck.UiAuthoring.EcsDebugger.ObjectiveInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorObjectiveAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_objective_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = UCkDebuggerStyleSettings::Get_Mutable();
    const ECkDebugAxis_EditControlStyle PreviousEditControlStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousEditControlStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    const FGameplayTag NameA = FGameplayTag::RequestGameplayTag(FName{TEXT("Combat.Attack")}, false);
    const FGameplayTag NameB = FGameplayTag::RequestGameplayTag(FName{TEXT("Combat.Defense")}, false);
    if (NOT TestTrue(TEXT("fixture reuses two runtime gameplay tags"), NameA.IsValid() && NameB.IsValid() && NameA != NameB))
    { return false; }

    auto World = ck::FEcsWorld{};
    auto EntityA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto EntityB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    UWorld* const TestWorld = GWorld;
    if (NOT TestNotNull(TEXT("fixture runs inside the automation editor world"), TestWorld)) { return false; }
    EntityA.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    EntityB.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    const auto AuthoritySettings = FCk_Net_ConnectionSettings{
        ECk_Replication::DoesNotReplicate, ECk_Net_NetModeType::Host, ECk_Net_EntityNetRole::Authority};
    UCk_Utils_Net_UE::Add(EntityA, AuthoritySettings);
    UCk_Utils_Net_UE::Add(EntityB, AuthoritySettings);
    auto ParamsA = FCk_Objective_ParamsData{NameA};
    ParamsA.Set_DisplayName(FText::FromString(TEXT("Attack objective")));
    ParamsA.Set_Description(FText::FromString(TEXT("Finish the attack sequence.")));
    auto ParamsB = FCk_Objective_ParamsData{NameB};
    ParamsB.Set_DisplayName(FText::FromString(TEXT("Defense objective")));
    ParamsB.Set_Description(FText::FromString(TEXT("Hold the defensive line.")));
    const FCk_Handle_Objective ObjectiveA = UCk_Utils_Objective_UE::Add(EntityA, ParamsA);
    const FCk_Handle_Objective ObjectiveB = UCk_Utils_Objective_UE::Add(EntityB, ParamsB);
    if (NOT TestTrue(TEXT("fixture creates two valid objectives"), ck::IsValid(ObjectiveA) && ck::IsValid(ObjectiveB)))
    { return false; }

    auto Inspector = FCkInspector_Objective{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("A mounts the authored Objective inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_ObjectiveAuthored")})
        || NOT TestEqual(TEXT("B mounts the authored Objective inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_ObjectiveAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_ObjectiveAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_ObjectiveAuthored>(RenderedA);
    const TSharedRef<SCkInspector_ObjectiveAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_ObjectiveAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TestTrue(TEXT("objective builds own independent authored views"), ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB);
    TestTrue(TEXT("A projects the Objective name"), ContainsText(RenderedA, NameA.ToString()));
    TestTrue(TEXT("A projects the Objective display name"), ContainsText(RenderedA, TEXT("Attack objective")));
    TestTrue(TEXT("A projects the Objective description"), ContainsText(RenderedA, TEXT("Finish the attack sequence.")));
    TestTrue(TEXT("A projects the Objective status"), ContainsText(RenderedA, TEXT("Not Started")));
    TestEqual(TEXT("A projects three Objective controls"), CountType(RenderedA, TEXT("SButton")), 3);

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native capture remains the Objective comparison authority"),
        RowsA.FindRef(TEXT("Name:")) == NameA.ToString() && RowsB.FindRef(TEXT("Name:")) == NameB.ToString()
            && Differing.Contains(TEXT("Name:")) && Differing.Contains(TEXT("Display:"))
            && Differing.Contains(TEXT("Description:")));
    TSharedPtr<SCkInspector_ObjectiveAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_ObjectiveAuthored>(Inspector.Build_Inspector(EntityA));
    }
    TestTrue(TEXT("authored labels receive exact native Objective diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_NameDiffMarked() && DiffAuthored->Is_DisplayDiffMarked()
        && DiffAuthored->Is_DescriptionDiffMarked());

    const TSharedPtr<SButton> HeldStart = FindButton(RenderedA, TEXT("objective-start"));
    if (NOT TestTrue(TEXT("authored Objective mounts the physical Start action"), HeldStart.IsValid() && HeldStart->IsEnabled()))
    { return false; }
    HeldStart->SimulateClick();
    if (NOT TestTrue(TEXT("physical Start routes one public Objective request"),
        EntityA.Has<ck::FFragment_Objective_Requests>()
            && EntityA.Get<ck::FFragment_Objective_Requests>().Get_Requests().Num() == 1)) { return false; }
    TestTrue(TEXT("routed Objective request is Start"),
        std::holds_alternative<FCk_Request_Objective_Start>(
            EntityA.Get<ck::FFragment_Objective_Requests>().Get_Requests()[0]));
    EntityA.Try_Remove<ck::FFragment_Objective_Requests>();

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Objective resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorObjective.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorObjective.ui.css")))))
    { return false; }
    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible Objective reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Objective A compatible candidate")).Succeeded);
    TestTrue(TEXT("Objective A reload retains identity without mutating B"),
        ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore
            && FindButton(RenderedA, TEXT("objective-start")) == HeldStart);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing Objective action binding is rejected atomically by B"), ViewB->TryReload(
        Markup.Replace(TEXT("action=\"objective-start\""), TEXT("action=\"objective-missing\"")),
        Stylesheet, TEXT("Objective B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected Objective reload retains B tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    TestTrue(TEXT("fixture removes Objective current state while entity remains live"),
        EntityA.Try_Remove<ck::FFragment_Objective_Current>() && ck::IsValid(EntityA) && NOT Inspector.CanInspect(EntityA));
    HeldStart->SlatePrepass();
    TestEqual(TEXT("mounted Objective status fails closed after fragment removal"),
        AuthoredA->Get_StatusText(), FString{TEXT("--")});
    TestFalse(TEXT("stale Objective action is disabled"), HeldStart->IsEnabled());
    TestTrue(TEXT("stale Objective action explains its disabled state"),
        GetToolTipText(HeldStart.ToSharedRef()).Contains(TEXT("Objective is unavailable")));
    HeldStart->SimulateClick();
    TestFalse(TEXT("held Objective action cannot enqueue after fragment loss"),
        EntityA.Has<ck::FFragment_Objective_Requests>());

    TSharedPtr<SCkInspector_ObjectiveAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_Objective>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_ObjectiveAuthored>(DestructorInspector->Build_Inspector(EntityB));
    }
    TestTrue(TEXT("Objective inspector destruction makes its authored build inert"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());
    Inspector.OnDeactivated();
    TestTrue(TEXT("Objective deactivation releases every retained authored build"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredB->Get_View().IsValid());
    return true;
}

#endif
