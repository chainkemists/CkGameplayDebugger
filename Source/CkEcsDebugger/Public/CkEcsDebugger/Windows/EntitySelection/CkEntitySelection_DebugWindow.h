#pragma once

#include "CogWindowConfig.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

#include "CkEntitySelection_DebugWindow.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class UCk_DebugWindowConfig_EntitySelection;

// --------------------------------------------------------------------------------------------------------------------

class FCk_EntitySelection_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

protected:
    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;

    auto
    RenderTick(
        float InDeltaT) -> void override;

protected:
    TObjectPtr<UCk_DebugWindowConfig_EntitySelection> Config;

    ImGuiTextFilter Filter;
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config = Cog)
class UCk_DebugWindowConfig_EntitySelection : public UCogWindowConfig
{
    GENERATED_BODY()

public:
    UPROPERTY(Config)
    bool OnlyRootEntities = true;

    virtual void Reset() override
    {
        Super::Reset();

        OnlyRootEntities = true;
    }
};
//---------------------------------------------------------------------------------------------------------------------