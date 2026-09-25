#pragma once

#include "CkProceduralAnimationDebugger/Data/CkProceduralAnimationDebugger_DataCollector.h"
#include "CkProceduralAnimationDebugger/Data/CkProceduralAnimationDebugger_History.h"

#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

enum class ECkProceduralAnimationDebugger_SelectionSync : uint8
{
    Silent,
    Broadcast
};

// --------------------------------------------------------------------------------------------------------------------

class CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_Model
    : public TSharedFromThis<FCkProceduralAnimationDebugger_Model>
{
public:
    FCkProceduralAnimationDebugger_Model();
    ~FCkProceduralAnimationDebugger_Model();

public:
    auto Set_World(UWorld* InWorld) -> void;
    auto Get_World() const -> UWorld*;
    auto Refresh() -> void;
    // A picked leg selects its body and becomes the focused leg; switching to another body clears the focused leg.
    auto Select(
        const FCk_Handle& InEntity,
        ECkProceduralAnimationDebugger_SelectionSync InSync = ECkProceduralAnimationDebugger_SelectionSync::Silent) -> bool;
    auto Request_SelectLeg(const FString& InLegEntityId) -> void;
    auto Reset() -> void;
    auto ReleaseSession() -> void;
    auto Get_IsReleased() const -> bool { return _Released; }
    auto Get_Rows() const -> const TArray<FCkProceduralAnimationDebugger_Row>& { return _Rows; }
    auto Get_History() const -> const FCkProceduralAnimationDebugger_History& { return _History; }
    auto Get_HistoryRevision() const -> uint64 { return _HistoryRevision; }
    auto Get_SelectedHandle() const -> FCk_Handle;
    auto Get_SelectedGone() const -> bool { return _SelectedGone; }
    auto Get_SelectedId() const -> const FString& { return _SelectedId; }
    auto Get_SelectedLegId() const -> const FString& { return _SelectedLegId; }
    // The live leg behind the selected leg id; invalid once that leg is detached or its body is gone.
    auto Get_SelectedLeg() const -> FCk_Handle_ProceduralLeg;
    // The only gameplay mutations the debugger makes. Both go through the leg feature's public requests,
    // exactly as gameplay code would, and are rejected without a live selected leg.
    auto Request_EnableDisableSelectedLeg(ECk_EnableDisable InEnableDisable) -> bool;
    auto Request_DetachSelectedLeg(ECk_ProceduralLeg_ReleasedPartsOwnership InPartsOwnership) -> bool;
    auto Get_LiveStatus() const -> const FCk_ProceduralAnimation_DebugSnapshot*;
    auto Request_Hold() -> void;
    auto Request_GoLive() -> void;
    auto Request_Scrub(int32 InIndex) -> bool;
    auto Request_ScrubBySequence(uint64 InSequence) -> bool;
    auto Get_OnChanged() -> FSimpleMulticastDelegate& { return _OnChanged; }
    static auto Get_SelectionSource() -> FName { return TEXT("CkProceduralAnimationDebugger"); }

private:
    auto DoClear() -> void;
    auto DoCapture_Selected() -> void;
    auto HandleSelection(const FCk_Handle& InEntity, FName InSource) -> void;
    auto HandleWorldCleanup(UWorld* InWorld, bool InSessionEnded, bool InCleanupResources) -> void;

private:
    TWeakObjectPtr<UWorld> _World;
    TArray<FCkProceduralAnimationDebugger_Row> _Rows;
    TOptional<FCk_ProceduralAnimation_DebugSnapshot> _LiveSnapshot;
    FCkProceduralAnimationDebugger_History _History;
    FString _SelectedId;
    FString _SelectedLegId;
    uint64 _HistoryRevision = 0;
    bool _SelectedGone = false;
    bool _Released = false;
    FSimpleMulticastDelegate _OnChanged;
    FDelegateHandle _SessionHandle;
    FDelegateHandle _SelectionHandle;
    FDelegateHandle _WorldCleanupHandle;
};

// --------------------------------------------------------------------------------------------------------------------
