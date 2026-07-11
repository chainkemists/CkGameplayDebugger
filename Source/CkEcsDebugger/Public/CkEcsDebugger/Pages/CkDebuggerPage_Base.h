#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class FCkDebuggerModel_InspectorFilter;

struct FCkDebuggerPageContext
{
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TSharedPtr<FCkDebuggerModel_InspectorFilter> FilterModel;

    /** Pushes a query into the entity list's Filter bar (archetype card click-through). */
    TFunction<void(const FString&)> RequestEntityFilter;
};

class ICkDebuggerPage_Base
{
public:
    virtual ~ICkDebuggerPage_Base() = default;

    virtual auto Get_PageName() const -> FText = 0;
    virtual auto Get_PageIcon() const -> const FSlateBrush* = 0;
    virtual auto Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget> = 0;
    virtual auto Tick(float InDeltaTime) -> void = 0;
    virtual auto IsActive() const -> bool = 0;
    virtual auto Set_IsActive(bool InIsActive) -> void = 0;
};