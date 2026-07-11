#pragma once

#include "CkObjectPoolingDebugger/Data/CkObjectPoolingDebugger_Snapshot.h"

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

class STextBlock;
class ITableRow;
class STableViewBase;
class FCkDebuggerModel_WorldSelector;

// ====================================================================================================================
// CK Object Pooling Debugger window. One virtualized row per (class, archetype) pool the CkCore
// ObjectPooling subsystem manages, plus the pinned-unique count. Row objects are rebuilt only when
// the pool SET changes (keys); stats update in place otherwise, so scroll position survives.
// ====================================================================================================================

class SCkObjectPoolingDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    using ItemPtr = TSharedPtr<FCkObjectPoolingDebugger_PoolRow>;

    SLATE_BEGIN_ARGS(SCkObjectPoolingDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Object Pooling Debugger")); }

private:
    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto BuildHeaderRow() -> TSharedRef<SWidget>;
    auto OnGenerateRow(ItemPtr InItem, const TSharedRef<STableViewBase>& InTable) -> TSharedRef<ITableRow>;

    static auto MakeSignature(const FCkObjectPoolingDebugger_Snapshot& InSnapshot) -> FString;

    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    TSharedPtr<STextBlock>                      _SummaryText;
    TSharedPtr<SListView<ItemPtr>>              _ListView;

    TArray<ItemPtr> _Items;
    FString         _LastSignature;
};

// ====================================================================================================================
