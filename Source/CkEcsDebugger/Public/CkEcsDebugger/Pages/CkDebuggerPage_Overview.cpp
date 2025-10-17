#include "CkDebuggerPage_Overview.h"

#include "SlateIM/Public/SlateIM.h"

auto FCkDebuggerPage_Overview::Get_PageName() const -> FText
{
    return FText::FromString(TEXT("Overview"));
}

auto FCkDebuggerPage_Overview::Get_PageIcon() const -> const FSlateBrush*
{
    return nullptr;
}

auto FCkDebuggerPage_Overview::Draw(const FCkDebuggerPageContext& InContext) -> void
{
    Context = InContext;

    SlateIM::Text(TEXT("Overview Page - Content Coming Soon"));
}

auto FCkDebuggerPage_Overview::Tick(float InDeltaTime) -> void
{
}