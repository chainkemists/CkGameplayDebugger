#include "CkGoapDebugger/Window/SCkGoapDebugger_InspectorGateway.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_goap_debugger_gateway_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    template<typename T_Handle>
    auto MakeHandle(const uint32 InEntityNumber) -> T_Handle
    {
        const auto EntityId = static_cast<FCk_Entity::IdType>(InEntityNumber);
        return ck::StaticCast<T_Handle>(FCk_Handle{FCk_Entity{EntityId}, FCk_RegistryHandle{}});
    }

    auto GetTextField(const TSharedPtr<const FCkUiRecord>& InRecord, const FString& InField) -> FString
    {
        const auto* Field = InRecord.IsValid() ? InRecord->FindField(InField) : nullptr;
        return Field != nullptr && Field->Kind == ECkUiFieldKind::Text
            ? Field->Text.ToString()
            : FString{};
    }

    auto GetColorField(const TSharedPtr<const FCkUiRecord>& InRecord, const FString& InField) -> FLinearColor
    {
        const auto* Field = InRecord.IsValid() ? InRecord->FindField(InField) : nullptr;
        return Field != nullptr && Field->Kind == ECkUiFieldKind::Color
            ? Field->Color
            : FLinearColor::Transparent;
    }

    auto MakeProjectionFixture() -> FCkGoapDebugger_EntitySnapshot
    {
        constexpr auto ErrorIfNotFound = false;
        const auto AttackTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Combat.Attack")), ErrorIfNotFound);
        const auto DefenseTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Combat.Defense")), ErrorIfNotFound);
        const auto CueTag = FGameplayTag::RequestGameplayTag(FName(TEXT("CueGym.Concurrency.Multiple")), ErrorIfNotFound);

        auto DisabledPlanner = FCkGoapDebugger_ActionSetInfo{};
        DisabledPlanner.DebugName = TEXT("DisabledPlanner");
        DisabledPlanner.ActionSetTag = DefenseTag;
        DisabledPlanner.EnableToggle = ECk_EnableDisable::Disable;

        const auto Strategic = MakeHandle<FCk_Handle_Goap_Action>(101);
        const auto OperateShop = MakeHandle<FCk_Handle_Goap_Action>(102);
        const auto ServeCustomer = MakeHandle<FCk_Handle_Goap_Action>(103);

        auto SelectedPlanner = FCkGoapDebugger_ActionSetInfo{};
        SelectedPlanner.DebugName = TEXT("SelectedPlanner");
        SelectedPlanner.ActionSetTag = AttackTag;
        SelectedPlanner.ActiveChainHandles = {Strategic, OperateShop, ServeCustomer};

        auto StrategicAction = FCkGoapDebugger_ActionInfo{};
        StrategicAction.Handle = Strategic;
        StrategicAction.ClassName = TEXT("Strategic");
        StrategicAction.ActionTag = AttackTag;

        auto OperateShopAction = FCkGoapDebugger_ActionInfo{};
        OperateShopAction.Handle = OperateShop;
        OperateShopAction.ClassName = TEXT("OperateShop");
        OperateShopAction.ActionTag = DefenseTag;
        OperateShopAction.PlanStatus = ECk_GoapPlanStatus::Planning;

        auto ServeCustomerAction = FCkGoapDebugger_ActionInfo{};
        ServeCustomerAction.Handle = ServeCustomer;
        ServeCustomerAction.ClassName = TEXT("ServeCustomer");
        ServeCustomerAction.ActionTag = CueTag;
        ServeCustomerAction.PlanStatus = ECk_GoapPlanStatus::PlanFound;
        ServeCustomerAction.PlanCost = 42.0f;
        ServeCustomerAction.PlanClassNames = {TEXT("FindCustomer"), TEXT("WalkToCustomer"),
            TEXT("ServeCustomer"), TEXT("ConfirmService")};

        SelectedPlanner.Catalog = {StrategicAction, OperateShopAction, ServeCustomerAction};

        auto Snapshot = FCkGoapDebugger_EntitySnapshot{};
        Snapshot.ActionSets = {DisabledPlanner, SelectedPlanner};
        return Snapshot;
    }

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkGoapDebugger_InspectorGatewayAuthored,
    "Ck.UiAuthoring.GoapDebugger.InspectorGateway.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkGoapDebugger_InspectorGatewayAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_goap_debugger_gateway_authored_tests;
    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("GOAP Inspector Gateway authored test requires Slate."));
        return false;
    }

    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> Host;
    ON_SCOPE_EXIT
    {
        if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); }
    };

    TSharedPtr<SCkGoapDebugger_InspectorGateway> Gateway =
        SNew(SCkGoapDebugger_InspectorGateway).Entity(FCk_Handle{});
    Host = SNew(SWindow)
        .ClientSize(FVector2D{420.0f, 640.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Gateway.ToSharedRef()];
    Slate.AddWindow(Host.ToSharedRef(), true);
    Tick(Slate);

    TSharedPtr<FCkUiView> View = Gateway->_AuthoredView;
    if (!TestTrue(TEXT("Production GOAP Inspector Gateway admits authored section composition"),
        Gateway->_AuthoredMounted && View.IsValid() && View->GetLastResult().Succeeded))
    {
        AddError(Gateway->_AuthoredLoadError);
        return false;
    }
    TestTrue(TEXT("Authored gateway owns its vertical scroll"),
        View->GetScroll(TEXT("goap-gateway-scroll")).IsValid());
    TestTrue(TEXT("Gateway owns all three authored record presenters"),
        View->GetRepeat(TEXT("goap-gateway-planner-records")).IsValid()
        && View->GetRepeat(TEXT("goap-gateway-chain-records")).IsValid()
        && View->GetRepeat(TEXT("goap-gateway-preview-records")).IsValid());
    TestTrue(TEXT("Invalid selection renders through the authored empty-state binding"),
        Gateway->_AuthoredPlannerRecords.IsValid()
        && Gateway->_AuthoredChainRecords.IsValid()
        && Gateway->_AuthoredPreviewRecords.IsValid()
        && NOT Gateway->_AuthoredHasContent);

    constexpr auto ErrorIfNotFound = false;
    TestTrue(TEXT("Projection fixture tags are registered"),
        FGameplayTag::RequestGameplayTag(FName(TEXT("Combat.Attack")), ErrorIfNotFound).IsValid()
        && FGameplayTag::RequestGameplayTag(FName(TEXT("Combat.Defense")), ErrorIfNotFound).IsValid()
        && FGameplayTag::RequestGameplayTag(FName(TEXT("CueGym.Concurrency.Multiple")), ErrorIfNotFound).IsValid());
    const auto Fixture = MakeProjectionFixture();
    if (NOT TestTrue(TEXT("Handcrafted GOAP snapshot publishes atomically"), Gateway->Publish_AuthoredSnapshot(&Fixture)))
    { return false; }

    const auto& PlannerRecords = Gateway->_AuthoredPlannerRecords->GetRecords();
    const auto& ChainRecords = Gateway->_AuthoredChainRecords->GetRecords();
    const auto& PreviewRecords = Gateway->_AuthoredPreviewRecords->GetRecords();
    if (NOT TestEqual(TEXT("Both planners are projected"), PlannerRecords.Num(), 2)) { return false; }
    TestEqual(TEXT("Disabled planner preserves its state"), GetTextField(PlannerRecords[0], TEXT("status")), TEXT("Disabled"));
    TestEqual(TEXT("PlanFound planner wins display priority"), Gateway->_AuthoredChainHeading, TEXT("ACTIVE CHAIN - SelectedPlanner"));
    TestEqual(TEXT("Selected planner reports active depth"), GetTextField(PlannerRecords[1], TEXT("status")), TEXT("PlanFound - 3 active"));
    if (NOT TestEqual(TEXT("All active-chain entries are projected"), ChainRecords.Num(), 3)) { return false; }
    TestEqual(TEXT("Active-chain root name is retained"), GetTextField(ChainRecords[0], TEXT("name")), TEXT("Strategic"));
    TestEqual(TEXT("Active-chain leaf name is retained"), GetTextField(ChainRecords[2], TEXT("name")), TEXT("ServeCustomer"));
    TestTrue(TEXT("Active-chain leaf keeps warning emphasis"),
        GetColorField(ChainRecords[2], TEXT("foreground")).Equals(CkStyle::Warn()));
    TestEqual(TEXT("Action tag chain preserves order"), Gateway->_AuthoredChainTags,
        TEXT("Action tag chain: Combat.Attack > Combat.Defense > CueGym.Concurrency.Multiple"));
    TestEqual(TEXT("Leaf status is projected"), Gateway->_AuthoredLeafStatus, TEXT("PlanFound"));
    TestEqual(TEXT("Leaf cost is projected"), Gateway->_AuthoredLeafCost, TEXT("42"));
    TestEqual(TEXT("Leaf plan length is projected"), Gateway->_AuthoredLeafLength, TEXT("4 actions"));
    if (NOT TestEqual(TEXT("Preview is capped to three records"), PreviewRecords.Num(), 3)) { return false; }
    TestEqual(TEXT("Preview preserves third action"), GetTextField(PreviewRecords[2], TEXT("name")), TEXT("ServeCustomer"));
    TestTrue(TEXT("Preview overflow is explicit"), Gateway->_AuthoredPreviewOverflowVisible
        && Gateway->_AuthoredPreviewOverflow == TEXT("+ 1 more - open the Window to see all"));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Stylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"))
        : FString{};
    if (NOT TestTrue(TEXT("Installed GOAP gateway resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("GoapInspectorGateway.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("GoapInspectorGateway.ui.css")))))
    { return false; }

    const int64 AcceptedRevision = View->GetRevision();
    TestTrue(TEXT("Compatible GOAP gateway reload is accepted"), View->TryReload(
        Markup, Stylesheet, TEXT("GoapInspectorGateway compatible test candidate")).Succeeded);
    TestTrue(TEXT("Compatible reload retains the authored gateway view"),
        Gateway->_AuthoredView == View && View->GetRevision() > AcceptedRevision);

    TSharedPtr<SWidget> MainBeforeRejected = View->GetRegion(TEXT("main"));
    const int64 RevisionBeforeRejected = View->GetRevision();
    auto AcceptedPlannerRecords = Gateway->_AuthoredPlannerRecords;
    auto AcceptedChainRecords = Gateway->_AuthoredChainRecords;
    auto AcceptedPreviewRecords = Gateway->_AuthoredPreviewRecords;
    TestFalse(TEXT("Missing GOAP gateway binding is rejected"), View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><text id=\"missing\" bind=\"missing-binding\" /></region></ui>"),
        TEXT(""), TEXT("GoapInspectorGateway rejected test candidate")).Succeeded);
    TestTrue(TEXT("Rejected gateway reload retains the admitted tree and revision"),
        View->GetRegion(TEXT("main")) == MainBeforeRejected && View->GetRevision() == RevisionBeforeRejected
        && Gateway->_AuthoredPlannerRecords == AcceptedPlannerRecords
        && Gateway->_AuthoredChainRecords == AcceptedChainRecords
        && Gateway->_AuthoredPreviewRecords == AcceptedPreviewRecords);

    Slate.DestroyWindowImmediately(Host.ToSharedRef());
    Host.Reset();
    Tick(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    const TWeakPtr<FCkUiCollection> ReleasedPlannerRecords = AcceptedPlannerRecords;
    const TWeakPtr<FCkUiCollection> ReleasedChainRecords = AcceptedChainRecords;
    const TWeakPtr<FCkUiCollection> ReleasedPreviewRecords = AcceptedPreviewRecords;
    View.Reset();
    AcceptedPlannerRecords.Reset();
    AcceptedChainRecords.Reset();
    AcceptedPreviewRecords.Reset();
    MainBeforeRejected.Reset();
    Gateway.Reset();
    TestFalse(TEXT("Gateway teardown releases its authored view"), ReleasedView.IsValid());
    TestFalse(TEXT("Gateway teardown releases authored planner records"), ReleasedPlannerRecords.IsValid());
    TestFalse(TEXT("Gateway teardown releases authored chain records"), ReleasedChainRecords.IsValid());
    TestFalse(TEXT("Gateway teardown releases authored preview records"), ReleasedPreviewRecords.IsValid());
    return true;
}

#endif
