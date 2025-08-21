#pragma once

#include <Modules/ModuleManager.h>
#include <Framework/Docking/TabManager.h>
#include <Widgets/Docking/SDockTab.h>
#include <Framework/Commands/UIAction.h>

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGTOOLS_API FCkDebugToolsModule : public IModuleInterface
{
public:
    static constexpr auto ModuleName = "CkDebugTools";

public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    template<typename T_DebugToolWidget>
    auto RegisterDebugTool(const FName& InTabName, const FText& InDisplayName) -> void;

private:
    auto DoRegisterToolTabSpawner(const FName& InTabName, const FText& InDisplayName,
                                  TFunction<TSharedRef<SWidget>()> InWidgetFactory) -> void;
    auto DoUnregisterAllTabSpawners() -> void;
    auto DoCreateToolbarExtension() -> void;

private:
    TArray<FName> _RegisteredTabNames;
};

// --------------------------------------------------------------------------------------------------------------------

template<typename T_DebugToolWidget>
auto FCkDebugToolsModule::RegisterDebugTool(const FName& InTabName, const FText& InDisplayName) -> void
{
    static_assert(TIsDerivedFrom<T_DebugToolWidget, class SCkDebugTool_Base>::IsDerived, 
                  "T_DebugToolWidget must derive from SCkDebugTool_Base");
    
    DoRegisterToolTabSpawner(InTabName, InDisplayName, 
        []() -> TSharedRef<SWidget>
        {
            return SNew(T_DebugToolWidget);
        });
}

// --------------------------------------------------------------------------------------------------------------------