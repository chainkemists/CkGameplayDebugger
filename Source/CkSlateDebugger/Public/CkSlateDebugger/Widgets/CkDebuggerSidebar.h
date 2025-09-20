#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnViewSelected, const FName&);

struct FCkDebuggerNavItem
{
    FName Name;
    FText DisplayName;
    FName Category;
    TSharedPtr<FSlateBrush> Icon;
};

class SCkDebuggerSidebar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerSidebar) {}
        SLATE_EVENT(FOnViewSelected, OnViewSelected)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    auto SetActiveView(const FName& ViewName) -> void;

private:
    auto BuildNavigation() -> void;
    auto CreateNavCategory(const FText& CategoryName, const TArray<FCkDebuggerNavItem>& Items) -> TSharedRef<SWidget>;
    auto CreateNavItem(const FCkDebuggerNavItem& Item) -> TSharedRef<SWidget>;

    auto IsViewActive(const FName& ViewName) const -> bool;

private:
    FOnViewSelected OnViewSelected;
    FName ActiveViewName;

    TSharedPtr<SVerticalBox> NavigationBox;
    TArray<FCkDebuggerNavItem> NavigationItems;
};