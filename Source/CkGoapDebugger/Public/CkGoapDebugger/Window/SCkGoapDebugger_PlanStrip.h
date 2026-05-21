#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SHorizontalBox;
class SScrollBox;

// ====================================================================================================================
// SCkGoapDebugger_PlanStrip — horizontal scrollable strip of plan cards.
//
// One card per entry in the selected Action's PlanClassNames. The plan-entry
// → catalog-Action match is by class-name. When the matched Action has
// ChildActionHandles populated, the card renders as a "delegates" (composite)
// variant.
//
// Click semantics: clicking a card sets the corresponding catalog Action as
// the ViewModel's selected Action (when a match exists).
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_PlanStrip : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_PlanStrip) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    ~SCkGoapDebugger_PlanStrip();

    auto RefreshFromViewModel() -> void;

private:
    auto RebuildCards() -> void;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    TSharedPtr<SScrollBox>     _ScrollBox;
    TSharedPtr<SHorizontalBox> _CardsBox;

    uint32 _LastStripHash = 0;
    bool   _HasMaterialized = false;
};

// ====================================================================================================================
