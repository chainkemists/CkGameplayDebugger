#include "CkDebuggerPage_Overview.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

auto FCkDebuggerPage_Overview::Get_PageName() const -> FText
{
    return FText::FromString(TEXT("Overview"));
}

auto FCkDebuggerPage_Overview::Get_PageIcon() const -> const FSlateBrush*
{
    return nullptr;
}

auto FCkDebuggerPage_Overview::Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Overview Page - Coming Soon")))
        ];
}

auto FCkDebuggerPage_Overview::Tick(float InDeltaTime) -> void
{
    // Update logic for overview page
}

auto FCkDebuggerPage_Overview::IsActive() const -> bool
{
    return IsActivePage;
}

auto FCkDebuggerPage_Overview::Set_IsActive(bool InIsActive) -> void
{
    IsActivePage = InIsActive;
}