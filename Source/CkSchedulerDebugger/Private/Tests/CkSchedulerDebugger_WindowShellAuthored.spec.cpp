#include "CkSchedulerDebugger/Window/SCkSchedulerDebuggerWindow.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_scheduler_debugger_window_shell_authored_tests
{
	auto Tick(FSlateApplication& InSlate) -> void
	{
		InSlate.PumpMessages();
		InSlate.Tick();
		InSlate.Tick();
	}

	auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
	{
		if (InRoot->GetTag() == InTag) { return InRoot; }

		const FChildren* Children = InRoot->GetChildren();
		for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
		{
			if (const TSharedPtr<SWidget> Found = FindTaggedWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag))
			{ return Found; }
		}
		return {};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCkSchedulerDebuggerWindow_AuthoredShell,
	"Ck.UiAuthoring.SchedulerDebugger.Window.AuthoredStableShell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkSchedulerDebuggerWindow_AuthoredShell::RunTest(const FString&) -> bool
{
	using namespace ck_scheduler_debugger_window_shell_authored_tests;

	if (NOT FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Scheduler authored-shell test requires Slate."));
		return false;
	}

	FSlateApplication& Slate = FSlateApplication::Get();
	TSharedPtr<SWindow> Host;
	ON_SCOPE_EXIT
	{
		if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); }
	};

	TSharedPtr<SCkSchedulerDebuggerWindow> Window = SNew(SCkSchedulerDebuggerWindow);
	Host = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.ClientSize(FVector2D{1120.0f, 700.0f})
		.CreateTitleBar(false)
		.HasCloseButton(false)
		[Window.ToSharedRef()];
	Slate.AddWindow(Host.ToSharedRef(), true);
	Tick(Slate);

	TSharedPtr<FCkUiView> View = Window->Get_AuthoredShellView();
	if (NOT TestTrue(TEXT("Production Scheduler window admits the authored stable shell"),
		View.IsValid() && View->GetLastResult().Succeeded))
	{
		AddError(Window->Get_AuthoredShellLoadFailure());
		return false;
	}

	const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
	const TArray<FName> RequiredIds{
		TEXT("scheduler-shell-root"), TEXT("scheduler-shell-layout"),
		TEXT("scheduler-shell-stats"), TEXT("scheduler-shell-frame-strip"),
		TEXT("scheduler-shell-tabs"), TEXT("scheduler-shell-page")};
	for (const FName Id : RequiredIds)
	{
		TestTrue(*FString::Printf(TEXT("Authored Scheduler shell owns '%s'"), *Id.ToString()),
			FindTaggedWidget(Main, Id).IsValid());
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
	FString Markup;
	FString Css;
	const FString Directory = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
	if (NOT TestTrue(TEXT("Installed Scheduler shell resources are readable"), Plugin.IsValid()
		&& FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Directory, TEXT("SchedulerDebuggerShell.ui.html")))
		&& FFileHelper::LoadFileToString(Css, *FPaths::Combine(Directory, TEXT("SchedulerDebuggerShell.ui.css")))))
	{ return false; }

	const int64 RevisionBeforeAcceptedReload = View->GetRevision();
	TestTrue(TEXT("Compatible Scheduler shell reload succeeds"),
		View->TryReload(Markup, Css, TEXT("Scheduler shell compatible candidate")).Succeeded);
	Tick(Slate);
	TestTrue(TEXT("Compatible reload retains the production view and advances its revision"),
		Window->Get_AuthoredShellView() == View && View->GetRevision() > RevisionBeforeAcceptedReload);

	const TSharedRef<SWidget> MainBeforeRejectedReload = View->GetRegion(TEXT("main"));
	const int64 RevisionBeforeRejectedReload = View->GetRevision();
	TestFalse(TEXT("Invalid Scheduler shell candidate is rejected atomically"), View->TryReload(
		TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-port\" /></region></ui>"),
		TEXT(""), TEXT("Scheduler shell rejected candidate")).Succeeded);
	TestTrue(TEXT("Rejected Scheduler shell candidate retains the accepted tree and revision"),
		View->GetRegion(TEXT("main")) == MainBeforeRejectedReload
			&& View->GetRevision() == RevisionBeforeRejectedReload);

	const TWeakPtr<SCkSchedulerDebuggerWindow> ReleasedWindow = Window;
	const TWeakPtr<FCkUiView> ReleasedView = View;
	Slate.DestroyWindowImmediately(Host.ToSharedRef());
	Host.Reset();
	Tick(Slate);
	View.Reset();
	Window.Reset();
	TestFalse(TEXT("Scheduler window teardown releases the production window"), ReleasedWindow.IsValid());
	TestFalse(TEXT("Scheduler window teardown releases its authored shell view"), ReleasedView.IsValid());
	return true;
}

#endif
