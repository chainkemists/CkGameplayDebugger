#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SHorizontalBox;
class STextBlock;

// ====================================================================================================================
// SCkGoapDebugger_Breadcrumb — top-of-center widget that visualises the
// currently-selected ActionSet's active chain as a row of clickable segments.
//
// Layout:
//   "Chain:"  [Root] ▸ [Mid] ▸ [Leaf]   (right-aligned) Catalog: N · Active: M · Reset chain
//
// Click semantics:
//   - Segment click → ViewModel::SetSelectedAction.
//   - Reset link    → logs a warning (D3 stub — wiring to
//                     utils_goap_action_set::Request_ResetActiveChain is a
//                     follow-up).
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_Breadcrumb : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_Breadcrumb) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    ~SCkGoapDebugger_Breadcrumb();

    // Called externally by the parent on ViewModel::OnChanged.
    auto RefreshFromViewModel() -> void;

private:
    auto RebuildSegments() -> void;
    auto OnResetChainClicked() -> FReply;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    TSharedPtr<SHorizontalBox> _SegmentsBox;
    TSharedPtr<STextBlock>     _MetaText;

    // Structural hash of the chain (set of handles + selected handle). Rebuild
    // only when this changes.
    uint32 _LastSegmentsHash = 0;
    bool   _HasMaterialized  = false;
};

// ====================================================================================================================
