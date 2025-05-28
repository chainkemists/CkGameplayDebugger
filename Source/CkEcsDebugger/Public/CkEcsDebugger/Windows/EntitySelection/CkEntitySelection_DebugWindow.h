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

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECkDebugger_EntitiesListSortingPolicy);

//--------------------------------------------------------------------------------------------------------------------------

UENUM()
enum class ECkDebugger_EntitiesListUpdatePolicy : uint8
{
    PerFrame,
    OnButton,
    PerSecond,
    PerTenSeconds
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECkDebugger_EntitiesListUpdatePolicy);

//--------------------------------------------------------------------------------------------------------------------------

UENUM(Flags)
enum class ECkDebugger_EntitiesListFragmentFilteringTypes : uint32
{
    None,
    Attribute = 1<<0,
    Ability = 1<<1,
    AbilityOwner = 1<<2,
    Replicated = 1<<3,
    EntityScript = 1<<4,
    EntityCollection = 1<<5,
    Timer = 1<<6,
};

ENUM_CLASS_FLAGS(ECkDebugger_EntitiesListFragmentFilteringTypes)
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECkDebugger_EntitiesListFragmentFilteringTypes);

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
    Get_CurrentTime() const -> FCk_Time;

    auto
    Get_EntitiesForList(bool InRequiresUpdate) const -> TArray<FCk_Handle>;

    auto
    DisplayEntitiesList(bool InRequiresUpdate) -> bool;

    auto
    DisplayEntitiesListWithFilters(bool InRequiresUpdate) -> bool;

private:
    TObjectPtr<UCk_DebugWindowConfig_EntitySelection> Config;

    ImGuiTextFilter Filter;

    mutable FCk_Time LastUpdateTime;
    mutable TArray<FCk_Handle> CachedSelectedEntities;
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config = Cog)
class UCk_DebugWindowConfig_EntitySelection : public UCogWindowConfig
{
    GENERATED_BODY()

public:
    UPROPERTY(Config)
    ECkDebugger_EntitiesListDisplayPolicy EntitiesListDisplayPolicy = ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy;

    UPROPERTY(Config)
    ECkDebugger_EntitiesListSortingPolicy EntitiesListSortingPolicy = ECkDebugger_EntitiesListSortingPolicy::Alphabetical;

    UPROPERTY(Config)
    ECkDebugger_EntitiesListUpdatePolicy EntitiesListUpdatePolicy = ECkDebugger_EntitiesListUpdatePolicy::PerFrame;

    UPROPERTY(Config)
    ECkDebugger_EntitiesListFragmentFilteringTypes EntitiesListFragmentFilteringTypes = ECkDebugger_EntitiesListFragmentFilteringTypes::None;

    virtual void Reset() override
    {
        Super::Reset();

        EntitiesListDisplayPolicy = ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy;
        EntitiesListSortingPolicy = ECkDebugger_EntitiesListSortingPolicy::Alphabetical;
        EntitiesListUpdatePolicy = ECkDebugger_EntitiesListUpdatePolicy::PerFrame;
        EntitiesListFragmentFilteringTypes = ECkDebugger_EntitiesListFragmentFilteringTypes::None;
    }
};
//---------------------------------------------------------------------------------------------------------------------