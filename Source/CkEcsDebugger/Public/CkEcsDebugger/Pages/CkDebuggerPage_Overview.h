#pragma once

#include "CkEcsDebugger/Pages/CkDebuggerPage_Base.h"

class FCkDebuggerPage_Overview : public ICkDebuggerPage_Base
{
public:
    auto Get_PageName() const -> FText override;
    auto Get_PageIcon() const -> const FSlateBrush* override;
    auto Draw(const FCkDebuggerPageContext& InContext) -> void override;
    auto Tick(float InDeltaTime) -> void override;
    auto IsActive() const -> bool override { return bIsActive; }
    auto SetActive(bool bInActive) -> void override { bIsActive = bInActive; }

private:
    bool bIsActive = false;
    FCkDebuggerPageContext Context;
};