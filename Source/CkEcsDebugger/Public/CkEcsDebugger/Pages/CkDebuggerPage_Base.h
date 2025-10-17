#pragma once

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;

struct FCkDebuggerPageContext
{
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
};

class ICkDebuggerPage_Base
{
public:
    virtual ~ICkDebuggerPage_Base() = default;

    virtual auto Get_PageName() const -> FText = 0;
    virtual auto Get_PageIcon() const -> const FSlateBrush* = 0;
    virtual auto Draw(const FCkDebuggerPageContext& InContext) -> void = 0;
    virtual auto Tick(float InDeltaTime) -> void = 0;
    virtual auto IsActive() const -> bool = 0;
    virtual auto SetActive(bool bInActive) -> void = 0;
};