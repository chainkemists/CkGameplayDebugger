#pragma once

#include "CkProceduralAnimationDebugger/Data/CkProceduralAnimationDebugger_DataCollector.h"
#include "CkProceduralAnimationDebugger/Data/CkProceduralAnimationDebugger_History.h"

class CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_Model
    : public TSharedFromThis<FCkProceduralAnimationDebugger_Model>
{
public:
    FCkProceduralAnimationDebugger_Model();
    ~FCkProceduralAnimationDebugger_Model();
    auto Set_World(UWorld* InWorld) -> void;
    auto Get_World() const -> UWorld*;
    auto Refresh() -> void;
    auto Select(const FCk_Handle& InEntity, bool InBroadcast = false) -> bool;
    auto Reset() -> void;
    auto ReleaseSession() -> void;
    auto Get_IsReleased() const -> bool { return _Released; }
    auto Get_Rows() const -> const TArray<FCkProceduralAnimationDebugger_Row>& { return _Rows; }
    auto Get_History() const -> const FCkProceduralAnimationDebugger_History& { return _History; }
    auto Get_SelectedHandle() const -> FCk_Handle;
    auto Get_SelectedGone() const -> bool { return _SelectedGone; }
    auto Get_SelectedId() const -> const FString& { return _SelectedId; }
    auto Get_LiveStatus() const -> const FCk_ProceduralAnimation_DebugSnapshot*;
    auto Request_Hold() -> void;
    auto Request_GoLive() -> void;
    auto Request_Scrub(int32 InIndex) -> bool;
    auto Get_OnChanged() -> FSimpleMulticastDelegate& { return _OnChanged; }
    static auto Get_SelectionSource() -> FName { return TEXT("CkProceduralAnimationDebugger"); }

private:
    auto HandleSelection(const FCk_Handle& InEntity, FName InSource) -> void;
    auto HandleWorldCleanup(UWorld* InWorld, bool InSessionEnded, bool InCleanupResources) -> void;
    TWeakObjectPtr<UWorld> _World;
    TArray<FCkProceduralAnimationDebugger_Row> _Rows;
    FCkProceduralAnimationDebugger_History _History;
    FString _SelectedId;
    bool _SelectedGone = false;
    bool _Released = false;
    FSimpleMulticastDelegate _OnChanged;
    FDelegateHandle _SessionHandle;
    FDelegateHandle _SelectionHandle;
    FDelegateHandle _WorldCleanupHandle;
};
