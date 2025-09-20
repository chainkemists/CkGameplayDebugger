#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "CkEcs/Handle/CkHandle.h"

class SCkSlateDebuggerWindow;
class UWorld;

class SCkDebuggerViewBase : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerViewBase)
    {
        _DebuggerWindow = nullptr;
    }
        SLATE_ARGUMENT(TWeakPtr<SCkSlateDebuggerWindow>, DebuggerWindow)
    SLATE_END_ARGS()

    virtual void Construct(const FArguments& InArgs);

    virtual auto GetViewName() const -> FName = 0;
    virtual auto GetViewDisplayName() const -> FText = 0;
    virtual auto GetViewIcon() const -> const FSlateBrush* { return nullptr; }

    virtual auto UpdateView() -> void {}
    virtual auto RefreshView() -> void {}

    virtual auto OnWorldChanged(UWorld* NewWorld) -> void;
    virtual auto OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void;

    auto GetSelectedWorld() const -> UWorld*;
    auto GetSelectedEntities() const -> TArray<FCk_Handle>;
    auto GetPrimarySelectedEntity() const -> FCk_Handle;

protected:
    auto GetDebuggerWindow() const -> TSharedPtr<SCkSlateDebuggerWindow>;

    TWeakPtr<SCkSlateDebuggerWindow> DebuggerWindow;
    TWeakObjectPtr<UWorld> CachedWorld;
    TArray<FCk_Handle> CachedSelection;
};