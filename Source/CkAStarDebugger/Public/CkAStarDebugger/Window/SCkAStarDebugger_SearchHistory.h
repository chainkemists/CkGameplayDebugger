#pragma once

#include "CkAStarDebugger/Data/CkAStarDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkAStarDebugger_ViewModel;
class SCkDebug_RailContainer;
class FCkUiCollection;
class FCkUiView;
class SBox;

// --------------------------------------------------------------------------------------------------------------------
// Search history — retained authored rows with a native startup fallback.
// --------------------------------------------------------------------------------------------------------------------

class SCkAStarDebugger_SearchHistory : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkAStarDebugger_SearchHistory) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkAStarDebugger_ViewModel> InViewModel) -> void;
    ~SCkAStarDebugger_SearchHistory() override;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    /** Re-emit the rail after a Layer-B style revision. Data is untouched. */
    auto Rebuild_ForStyleChange() -> void;
    auto RefreshFromViewModel() -> void;
    auto Release_AuthoredView() -> void;
    auto Get_AuthoredView() const -> TSharedPtr<FCkUiView> { return _AuthoredView; }
    auto Get_AuthoredCollection() const -> TSharedPtr<const FCkUiCollection> { return _AuthoredCollection; }
    auto Get_AuthoredLoadFailure() const -> const FString& { return _AuthoredLoadFailure; }

private:
    auto TryActivateAuthoredView() -> void;
    auto OnSelectedEntityChanged(FCk_Handle InEntity) -> void;
    auto CopyAuthoredEntry(const FString& InKey) -> void;
    auto RebuildList() -> void;
    auto BuildHistoryEntry(const FCkAStarDebugger_HistoryEntry& InEntry) -> TSharedRef<SWidget>;

    TSharedPtr<FCkAStarDebugger_ViewModel> _ViewModel;
    TSharedPtr<SCkDebug_RailContainer> _Rail;
    TSharedPtr<SBox> _ContentHost;
    TSharedPtr<FCkUiCollection> _AuthoredCollection;
    TSharedPtr<FCkUiView> _AuthoredView;
    FString _AuthoredLoadFailure;
    FString _LastProjectionSignature;
    FText _CountText = FText::FromString(TEXT("0"));
    FText _EmptyText;
    FDelegateHandle _SelectionChangedHandle;
    uint64 _SelectionRevision = 0;
    double _NextAuthoredPollSeconds = 0.0;
    bool _AuthoredMounted = false;
    bool _ProjectionCurrent = false;
    bool _Released = false;
};

// --------------------------------------------------------------------------------------------------------------------
