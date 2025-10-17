#pragma once

#include "CkDebuggerPage_Base.h"

class FCkDebuggerPage_Overview : public ICkDebuggerPage_Base
{
public:
    auto Get_PageName() const -> FText override;
    auto Get_PageIcon() const -> const FSlateBrush* override;
    auto Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget> override;
    auto Tick(float InDeltaTime) -> void override;
    auto IsActive() const -> bool override;
    auto Set_IsActive(bool InIsActive) -> void override;

private:
    bool IsActivePage = false;
};