#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;
class SCkDebuggerGallery_Window;
class UWorld;

class FCkDebuggerCommonModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static auto Get() -> FCkDebuggerCommonModule&;

	auto OpenGallery() -> void;
	auto CloseGallery() -> void;
	auto ToggleGallery() -> void;
	auto IsGalleryOpen() const -> bool;

	static const FName GalleryTabName;

private:
	auto OnSpawnGalleryTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
	auto HandlePieSessionBoundary(bool InIsSimulating) -> void;
	auto HandleWorldBeginTearDown(UWorld* InWorld) -> void;

	TSharedPtr<SCkDebuggerGallery_Window> _GalleryWindow;
	TSharedPtr<SDockTab> _GalleryTab;
	FDelegateHandle _BeginPieHandle;
	FDelegateHandle _EndPieHandle;
	FDelegateHandle _WorldBeginTearDownHandle;
};
