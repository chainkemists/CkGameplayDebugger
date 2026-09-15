#include "CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.h"
#include "CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentDetailPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentListPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_EventLogPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_NavmeshStatusPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_StatsPanel.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

struct FCkCrowdDebuggerAuthoredShellTestAccess
{
	static auto View(const SCkCrowdDebuggerWindow& InWindow) -> TSharedPtr<FCkUiView> { return InWindow._AuthoredShellView; }
	static auto Failure(const SCkCrowdDebuggerWindow& InWindow) -> const FString& { return InWindow._AuthoredShellLoadFailure; }
	static auto Poll(SCkCrowdDebuggerWindow& InWindow) -> void
	{ InWindow.PollAuthoredShell(InWindow._NextAuthoredShellPollSeconds); }
	static auto Ports(const SCkCrowdDebuggerWindow& InWindow) -> TArray<TSharedPtr<SWidget>>
	{
		TArray<TSharedPtr<SWidget>> Result;
		Result.Add(StaticCastSharedPtr<SWidget>(InWindow._NavmeshStatusPanel));
		Result.Add(StaticCastSharedPtr<SWidget>(InWindow._AgentListPanel));
		Result.Add(StaticCastSharedPtr<SWidget>(InWindow._StatsPanel));
		Result.Add(StaticCastSharedPtr<SWidget>(InWindow._EventLogPanel));
		Result.Add(StaticCastSharedPtr<SWidget>(InWindow._ViewportPanel));
		Result.Add(StaticCastSharedPtr<SWidget>(InWindow._AgentDetailPanel));
		return Result;
	}
	static auto FallbackHosts(const SCkCrowdDebuggerWindow& InWindow) -> TArray<TSharedPtr<SBox>>
	{ return {InWindow._FallbackNavHost, InWindow._FallbackAgentsHost, InWindow._FallbackStatsHost,
		InWindow._FallbackEventsHost, InWindow._FallbackPreviewHost, InWindow._FallbackDetailHost}; }
	static auto HasFallbackOwners(const SCkCrowdDebuggerWindow& InWindow) -> bool
	{
		const TArray<TSharedPtr<SWidget>> Panels = Ports(InWindow);
		const TArray<TSharedPtr<SBox>> Hosts = FallbackHosts(InWindow);
		for (int32 Index = 0; Index < Panels.Num(); ++Index)
		{
			if (!Panels[Index].IsValid() || !Hosts.IsValidIndex(Index) || !Hosts[Index].IsValid()
				|| Panels[Index]->GetParentWidget().Get() != Hosts[Index].Get()
				|| Hosts[Index]->GetChildren()->Num() != 1
				|| &Hosts[Index]->GetChildren()->GetChildAt(0).Get() != Panels[Index].Get()) { return false; }
		}
		return true;
	}
	static auto HasAuthoredOwners(const SCkCrowdDebuggerWindow& InWindow) -> bool
	{
		const TArray<TSharedPtr<SWidget>> Panels = Ports(InWindow);
		const TArray<TSharedPtr<SBox>> Hosts = FallbackHosts(InWindow);
		for (int32 Index = 0; Index < Panels.Num(); ++Index)
		{
			if (!Panels[Index].IsValid() || !Hosts.IsValidIndex(Index) || !Hosts[Index].IsValid()
				|| !Panels[Index]->GetParentWidget().IsValid()
				|| Panels[Index]->GetParentWidget().Get() == Hosts[Index].Get()
				|| (Hosts[Index]->GetChildren()->Num() == 1
					&& &Hosts[Index]->GetChildren()->GetChildAt(0).Get() == Panels[Index].Get())) { return false; }
		}
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger_AuthoredShell,
	"Ck.CrowdDebugger.AuthoredShell.ProductionPorts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkCrowdDebugger_AuthoredShell::RunTest(const FString&) -> bool
{
	if (NOT FSlateApplication::IsInitialized()) { AddError(TEXT("Crowd authored-shell test requires Slate.")); return false; }
	auto& Slate = FSlateApplication::Get();
	const TSharedRef<SCkCrowdDebuggerWindow> Window = SNew(SCkCrowdDebuggerWindow);
	const TSharedRef<SWindow> Host = SNew(SWindow).ClientSize(FVector2D{1280.0f, 900.0f})[Window];
	Slate.AddWindow(Host, true); Slate.PumpMessages(); Slate.Tick(); Slate.Tick();
	const TSharedPtr<FCkUiView> View = FCkCrowdDebuggerAuthoredShellTestAccess::View(*Window);
	if (NOT TestTrue(TEXT("production Crowd shell admits its installed authored topology"), View.IsValid() && View->GetLastResult().Succeeded))
	{ AddError(FCkCrowdDebuggerAuthoredShellTestAccess::Failure(*Window)); Slate.DestroyWindowImmediately(Host); return false; }
	const TSharedPtr<SCkUiSplitter> Topology = View->GetSplitter(TEXT("crowd-topology"));
	TestTrue(TEXT("Crowd authored topology keeps the three stable rails"), Topology.IsValid());
	TestTrue(TEXT("installed shell makes authored topology the sole parent of all six retained panels"),
		FCkCrowdDebuggerAuthoredShellTestAccess::HasAuthoredOwners(*Window));
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
	FString Markup, Css;
	if (NOT TestTrue(TEXT("installed Crowd shell assets are readable"), Plugin.IsValid()
		&& FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/CrowdDebuggerShell.ui.html")))
		&& FFileHelper::LoadFileToString(Css, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/CrowdDebuggerShell.ui.css")))))
	{ Slate.DestroyWindowImmediately(Host); return false; }
	const int64 Revision = View->GetRevision();
	const FCkUiLoadResult Compatible = View->TryReload(Markup, Css, TEXT("Crowd compatible shell candidate"));
	const TArray<TSharedPtr<SWidget>> ProductionPorts = FCkCrowdDebuggerAuthoredShellTestAccess::Ports(*Window);
	TestTrue(TEXT("compatible reload preserves the production preview port topology"), Compatible.Succeeded && View->GetRevision() > Revision
		&& View->GetSplitter(TEXT("crowd-topology")) == Topology && FCkCrowdDebuggerAuthoredShellTestAccess::Ports(*Window) == ProductionPorts
		&& FCkCrowdDebuggerAuthoredShellTestAccess::HasAuthoredOwners(*Window));
	const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
	const int64 RejectedRevision = View->GetRevision();
	const FCkUiLoadResult Rejected = View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"bad\" bind=\"missing-crowd-port\"/></region></ui>"), TEXT(""), TEXT("Crowd missing port candidate"));
	TestTrue(TEXT("missing native port is rejected atomically"), NOT Rejected.Succeeded && View->GetRevision() == RejectedRevision
		&& View->GetRegion(TEXT("main")) == Main && FCkCrowdDebuggerAuthoredShellTestAccess::HasAuthoredOwners(*Window));

	const FString RecoveryDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/CrowdAuthoredRecovery"));
	IFileManager::Get().DeleteDirectory(*RecoveryDirectory, false, true);
	IFileManager::Get().MakeDirectory(*RecoveryDirectory, true);
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*RecoveryDirectory, false, true); };
	const FString RecoveryMarkup = FPaths::Combine(RecoveryDirectory, TEXT("CrowdDebuggerShell.ui.html"));
	const FString RecoveryCss = FPaths::Combine(RecoveryDirectory, TEXT("CrowdDebuggerShell.ui.css"));
	if (NOT TestTrue(TEXT("malformed Crowd recovery fixture writes"),
		FFileHelper::SaveStringToFile(TEXT("<ui version=\"1\"><region name=\"main\">"), *RecoveryMarkup)
		&& FFileHelper::SaveStringToFile(Css, *RecoveryCss)))
	{ Slate.DestroyWindowImmediately(Host); return false; }
	const TSharedRef<SCkCrowdDebuggerWindow> RecoveryWindow = SNew(SCkCrowdDebuggerWindow).TestResourceDirectory(RecoveryDirectory);
	const TSharedRef<SWindow> RecoveryHost = SNew(SWindow).ClientSize(FVector2D{1280.0f, 900.0f})[RecoveryWindow];
	ON_SCOPE_EXIT { Slate.DestroyWindowImmediately(RecoveryHost); };
	Slate.AddWindow(RecoveryHost, true); Slate.PumpMessages(); Slate.Tick(); Slate.Tick();
	const TArray<TSharedPtr<SWidget>> RecoveryPorts = FCkCrowdDebuggerAuthoredShellTestAccess::Ports(*RecoveryWindow);
	TestTrue(TEXT("malformed startup restores all six retained panels into native fallback"),
		FCkCrowdDebuggerAuthoredShellTestAccess::HasFallbackOwners(*RecoveryWindow));
	if (NOT TestTrue(TEXT("same Crowd resource paths are repaired"),
		FFileHelper::SaveStringToFile(Markup, *RecoveryMarkup) && FFileHelper::SaveStringToFile(Css, *RecoveryCss)))
	{ Slate.DestroyWindowImmediately(Host); return false; }
	FCkCrowdDebuggerAuthoredShellTestAccess::Poll(*RecoveryWindow);
	TestTrue(TEXT("same-path recovery adopts authored topology with identical sole-owned panels"),
		FCkCrowdDebuggerAuthoredShellTestAccess::View(*RecoveryWindow).IsValid()
		&& FCkCrowdDebuggerAuthoredShellTestAccess::View(*RecoveryWindow)->GetLastResult().Succeeded
		&& FCkCrowdDebuggerAuthoredShellTestAccess::Ports(*RecoveryWindow) == RecoveryPorts
		&& FCkCrowdDebuggerAuthoredShellTestAccess::HasAuthoredOwners(*RecoveryWindow));
	Slate.DestroyWindowImmediately(Host);
	return true;
}

#endif
