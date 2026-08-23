#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkTextureDebugger/Data/CkTextureDebugger_Types.h"
#include "CkTextureDebugger/Model/CkTextureDebugger_CheckerOverrideSession.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_TextureHealthTable.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"

class FCkDebug_ViewportComponentPicker;
class FCkDebuggerModel_WorldSelector;
class SCkTextureDebugger_MaterialInputsPage;
class SCkTextureDebugger_SceneAuditTable;
class SCkTextureDebugger_SurfaceLightingPage;
class SCkTextureDebugger_UvDensityPage;
class SVerticalBox;
class SWidgetSwitcher;
class UPrimitiveComponent;
class UTexture;
class UWorld;
#if WITH_EDITOR
class FObjectPostSaveContext;
class FObjectPreSaveContext;
#endif

class SCkTextureDebuggerWindow final : public SCkDebugger_WindowBase
{
public:
    SLATE_BEGIN_ARGS(SCkTextureDebuggerWindow) {}
    SLATE_END_ARGS()

    virtual ~SCkTextureDebuggerWindow() override;
    auto Construct(const FArguments& InArgs) -> void;

    virtual auto
        Get_WindowId() const
        -> FName override
    {
        return TEXT("CkTextureDebugger");
    }

    virtual auto
        Get_WindowDisplayName() const
        -> FText override;

    virtual auto
        Tick(
            const FGeometry& InAllottedGeometry,
            double InCurrentTime,
            float InDeltaTime)
        -> void override;

private:
    enum class EScope : uint8 { Selected, LoadedWorld };

    auto Build_CheckerPage() -> TSharedRef<SWidget>;
    auto Build_TextureHealthPage() -> TSharedRef<SWidget>;
    auto Build_SceneAuditPage() -> TSharedRef<SWidget>;
    auto Build_ContextStrip() const -> TSharedRef<SWidget>;
    auto Build_TextPage(FText InHeading, TAttribute<FText> InText) -> TSharedRef<SWidget>;
    auto Build_Section(FText InHeading, TSharedRef<SWidget> InContent) const -> TSharedRef<SWidget>;
    auto Get_StatusText() const -> FText;
    auto Get_ActivePageIndex() const -> int32;
    auto OnPageSelected(FName InPageId) -> void;
    auto OnModelWorldChanged(UWorld* InWorld) -> void;
    auto OnWorldChanged() -> void;
    auto OnWorldCleanup(UWorld* InWorld, bool InSessionEnded, bool InCleanupResources) -> void;
    auto OnSessionInvalidated() -> void;
    auto RemoveCheckerFromLiveWorld() -> FString;
#if WITH_EDITOR
    auto OnPreSaveWorld(UWorld* InWorld, FObjectPreSaveContext InContext) -> void;
    auto OnPostSaveWorld(UWorld* InWorld, FObjectPostSaveContext InContext) -> void;
#endif
    auto OnComponentPicked(const struct FCkDebug_ComponentPickResult& InPick) -> void;
    auto OnAuditComponentSelected(TWeakObjectPtr<UPrimitiveComponent> InComponent) -> void;
    auto OnTextureHealthSelectionChanged(const FCkTextureDebugger_TextureHealthSelection& InSelection) -> void;
    auto RebuildSlotControls() -> void;
    auto RefreshSnapshot() -> void;
    auto Sync_DiagnosticPages() -> void;
    auto Find_SelectedComponent() const -> const FCkTextureDebugger_ComponentRow*;
    auto Get_SelectedSlotIndices() const -> TArray<int32>;
    auto Build_Targets() const -> TArray<FCkTextureDebugger_CheckerTarget>;
    auto Get_ComponentWideTargetSignature() const -> FString;
    auto Is_ComponentWideOverrideConfirmed() const -> bool;
    auto Set_ComponentWideOverrideConfirmed(bool InConfirmed) -> void;
    auto Reset_ComponentWideOverrideConfirmation() -> void;
    auto TryLoadCheckerMaterial() const -> class UMaterialInterface*;
    auto TryLoadCheckerTexture() const -> class UTexture*;
    auto OnCheckerSelected(FName InCheckerId) -> void;
    auto OnScopeChanged(bool InLoadedWorld) -> void;
    auto OnSlotSelected(int32 InSlotIndex, bool InSelected) -> void;
    auto OnSelectNextComponent() -> FReply;
    auto OnApplyChecker() -> FReply;
    auto OnRestoreChecker() -> FReply;
    auto OnRefreshRequested() -> FReply;
    auto Get_SelectedComponentSummary() const -> FText;
    auto Get_SelectedSlotsSummary() const -> FText;
    auto Get_HealthSummary() const -> FText;
    auto Get_SelectedTextureContextSummary() const -> FText;
    auto Get_UvDensitySummary() const -> FText;
    auto Get_MaterialInputsSummary() const -> FText;
    auto Get_SurfaceLightingSummary() const -> FText;
    auto Get_AuditSummary() const -> FText;
    auto Get_CheckerLoadSummary(FName InCheckerId) const -> FText;
    auto Get_StreamingAvailabilitySummary() const -> FText;

    TSharedPtr<SWidgetSwitcher> _PageSwitcher;
    TSharedPtr<SVerticalBox> _SlotBox;
    TSharedPtr<SCkTextureDebugger_SceneAuditTable> _SceneAuditTable;
    TSharedPtr<SCkTextureDebugger_TextureHealthTable> _TextureHealthTable;
    TSharedPtr<SCkTextureDebugger_UvDensityPage> _UvDensityPage;
    TSharedPtr<SCkTextureDebugger_MaterialInputsPage> _MaterialInputsPage;
    TSharedPtr<SCkTextureDebugger_SurfaceLightingPage> _SurfaceLightingPage;
    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    TSharedPtr<FCkDebug_ViewportComponentPicker> _ComponentPicker;
    TArray<TStrongObjectPtr<UTexture>> _CheckerTextureRoots;
    TArray<FSlateBrush> _CheckerBrushes;
    FCkTextureDebugger_CheckerOverrideSession _OverrideSession;
    FCkTextureDebugger_LoadedWorldSnapshot _Snapshot;
    TWeakObjectPtr<UPrimitiveComponent> _SelectedComponent;
    TOptional<FCkTextureDebugger_TextureHealthSelection> _SelectedTexture;
    TSet<int32> _SelectedSlots;
    FName _ActivePageId = TEXT("Checker");
    FName _SelectedCheckerId = TEXT("ColorGrid2K");
    EScope _Scope = EScope::Selected;
    bool _ConfirmedComponentWideInstanceOverride = false;
    FString _ConfirmedComponentWideTargetSignature;
    FString _StatusDetail;
    FDelegateHandle _WorldChangedHandle;
    FDelegateHandle _WorldCleanupHandle;
    FDelegateHandle _SessionInvalidatedHandle;
#if WITH_EDITOR
    FDelegateHandle _PreSaveWorldHandle;
    FDelegateHandle _PostSaveWorldHandle;
#endif
};
