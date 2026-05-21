#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class STextBlock;
class SBox;
class SVerticalBox;

// ====================================================================================================================
// SCkGoapDebugger_WorldStateRail — fixed-width right rail mirroring the
// mockup's `.ws-rail` panel. Shows the resolved WorldState for the currently
// selected Action (or the selected ActionSet's WS as a fallback when no
// specific Action is selected).
//
// Layout (top-to-bottom):
//   - Header  : "WorldState · {WS_Label}"   { keys count }   (pane-head style)
//   - Body    : scrollable list of key/value rows; each row optionally tinted
//               with the "recently changed" amber background and prefixed by
//               a star glyph.
//   - Footer  : "★ = recently changed · {recent} of {total} keys"
//
// Rebuild strategy:
//   - Cheap: each refresh clears the body VBox and re-populates.
//     The WS list is small (~20 keys typical).
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_WorldStateRail : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_WorldStateRail) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    ~SCkGoapDebugger_WorldStateRail();

    // Called by the parent window on ViewModel::OnChanged.
    auto RefreshFromViewModel() -> void;

private:
    // -----------------------------------------------------------------------------------------------------------------
    // Build helpers
    // -----------------------------------------------------------------------------------------------------------------

    auto BuildEmptyState() -> TSharedRef<SWidget>;
    auto BuildKeyRow(const FCkGoapDebugger_WorldStateEntry& InEntry) -> TSharedRef<SWidget>;

    // Resolve which WS list to show + the label/source. Returns false when no
    // entity / ActionSet is selected.
    auto Resolve_DisplayedWorldState(
        const TArray<FCkGoapDebugger_WorldStateEntry>*& OutEntries,
        FString& OutLabel) const -> bool;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    // Host slots
    TSharedPtr<SBox>         _HeaderHost;
    TSharedPtr<SVerticalBox> _Body;
    TSharedPtr<SBox>         _FooterHost;
};

// ====================================================================================================================
