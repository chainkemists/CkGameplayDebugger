#include "CkGoapDebugger/Window/SCkGoapDebugger_InspectorGateway.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_goap_debugger_gateway_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkGoapDebugger_InspectorGatewayAuthored,
    "Ck.UiAuthoring.GoapDebugger.InspectorGateway.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkGoapDebugger_InspectorGatewayAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_goap_debugger_gateway_authored_tests;
    if (!FSlateApplication::IsInitialized())
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
    TestTrue(TEXT("Invalid selection renders through the authored empty-state port"),
        Gateway->_EmptyHost.IsValid()
        && Gateway->_EmptyHost->GetChildren()->Num() == 1
        && Gateway->_EmptyHost->GetChildren()->GetChildAt(0) != SNullWidget::NullWidget);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Stylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"))
        : FString{};
    if (!TestTrue(TEXT("Installed GOAP gateway resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("GoapInspectorGateway.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("GoapInspectorGateway.ui.css")))))
    { return false; }

    const int64 AcceptedRevision = View->GetRevision();
    TestTrue(TEXT("Compatible GOAP gateway reload is accepted"), View->TryReload(
        Markup, Stylesheet, TEXT("GoapInspectorGateway compatible test candidate")).Succeeded);
    TestTrue(TEXT("Compatible reload retains the authored gateway view"),
        Gateway->_AuthoredView == View && View->GetRevision() > AcceptedRevision);

    const TSharedRef<SWidget> MainBeforeRejected = View->GetRegion(TEXT("main"));
    const int64 RevisionBeforeRejected = View->GetRevision();
    TestFalse(TEXT("Missing GOAP gateway port is rejected"), View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-port\" /></region></ui>"),
        TEXT(""), TEXT("GoapInspectorGateway rejected test candidate")).Succeeded);
    TestTrue(TEXT("Rejected gateway reload retains the admitted tree and revision"),
        View->GetRegion(TEXT("main")) == MainBeforeRejected && View->GetRevision() == RevisionBeforeRejected);

    Slate.DestroyWindowImmediately(Host.ToSharedRef());
    Host.Reset();
    Tick(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    View.Reset();
    Gateway.Reset();
    TestFalse(TEXT("Gateway teardown releases its authored view and native ports"), ReleasedView.IsValid());
    return true;
}

#endif
