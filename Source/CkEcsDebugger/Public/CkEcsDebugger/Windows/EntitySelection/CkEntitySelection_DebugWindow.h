#pragma once

#include "CogWindowConfig.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

#include "CkEntitySelection_DebugWindow.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class UCk_DebugWindowConfig_EntitySelection;struct ImGuiTextFilter;

using FCogWindowEntityContextMenuFunction = TFunction<void(const FCk_Handle& InEntity)>;

//--------------------------------------------------------------------------------------------------------------------------

UENUM()
enum class ECkDebugger_EntitiesListDisplayPolicy : uint8
{
    OnlyRootEntities,
    EntityList,
    EntityHierarchy,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECkDebugger_EntitiesListDisplayPolicy);

//--------------------------------------------------------------------------------------------------------------------------

UENUM()
enum class ECkDebugger_EntitiesListSortingPolicy : uint8
{
    ID,
    EnitityNumber,
    Alphabetical
};

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

private:
    auto
    EntitiesList() -> bool;

    auto
    EntitiesListWithFilters() -> bool;

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
    ECkDebugger_EntitiesListDisplayPolicy EntitiesListDisplayPolicy = ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy;
    ECkDebugger_EntitiesListSortingPolicy EntitiesListSortingPolicy = ECkDebugger_EntitiesListSortingPolicy::Alphabetical;

    virtual void Reset() override
    {
        Super::Reset();

        EntitiesListDisplayPolicy = ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy;
        EntitiesListSortingPolicy = ECkDebugger_EntitiesListSortingPolicy::Alphabetical;
    }
};
//---------------------------------------------------------------------------------------------------------------------