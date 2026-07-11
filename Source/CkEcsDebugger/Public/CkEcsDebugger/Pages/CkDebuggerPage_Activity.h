#pragma once

#include "CkDebuggerPage_Base.h"
#include "CkEcs/Handle/CkHandle.h"

#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------
// Activity feed (redesign spec §3.6): spawn/destroy rows derived debugger-side by the
// world model's cache diff — no CkEcs hooks. Rows click-to-select (spawns only; a
// destroyed entity has nothing to select). Subscription is always-on (cheap: one array
// append per diff); the LIST view only refreshes while the page is active.
//
// v1 deviations (recorded in PROGRESS.md): no unseen-count tab badge (the page host has
// no badge surface); SM-transition rows not included (stretch item).
// --------------------------------------------------------------------------------------------------------------------

class FCkDebuggerPage_Activity : public ICkDebuggerPage_Base
{
public:
    ~FCkDebuggerPage_Activity();

    auto Get_PageName() const -> FText override;
    auto Get_PageIcon() const -> const FSlateBrush* override;
    auto Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget> override;
    auto Tick(float InDeltaTime) -> void override;
    auto IsActive() const -> bool override;
    auto Set_IsActive(bool InIsActive) -> void override;

private:
    struct FActivityEvent
    {
        bool IsSpawn = true;
        FCk_Handle Entity;      // valid for spawns while the entity lives
        FString DisplayName;
        FDateTime Timestamp;
    };

    auto HandleCacheDiff(const TArray<FCk_Handle>& InAdded, const TArray<FCk_Handle>& InRemoved) -> void;
    auto HandleWorldChanged(UWorld* InWorld) -> void;
    auto OnGenerateRow(TSharedPtr<FActivityEvent> InEvent, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;

    bool IsActivePage = false;
    bool FeedDirty = false;

    TArray<TSharedPtr<FActivityEvent>> Events;   // newest first, capped
    TSharedPtr<SListView<TSharedPtr<FActivityEvent>>> ListView;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    FDelegateHandle CacheDiffHandle;
    FDelegateHandle WorldChangedHandle;
};
