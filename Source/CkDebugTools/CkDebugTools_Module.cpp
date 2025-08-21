#include "CkDebugTools_Module.h"
#include "CkDebugTools/CkDebugTools_Style.h"
#include "CkDebugTools/CkDebugTools_Commands.h"
#include "CkDebugTools/CkDebugTools_Base_Widget.h"
#include "CkDebugTools/CkEntity_Debugtool.h"

#include <Framework/Docking/LayoutExtender.h>
#include <WorkspaceMenuStructure.h>
#include <WorkspaceMenuStructureModule.h>
#include <ToolMenus.h>
#include <LevelEditor.h>

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebugToolsModule::StartupModule() -> void
{
    FCkDebugToolsStyle::Initialize();
    FCkDebugToolsCommands::Register();

    DoCreateToolbarExtension();

    // Register debug tools
    RegisterDebugTool<SCkEntityDebugTool>(TEXT("CkEntityDebugTool"),
                                          FText::FromString(TEXT("Entity Inspector")));
}

auto FCkDebugToolsModule::ShutdownModule() -> void
{
    DoUnregisterAllTabSpawners();
    FCkDebugToolsCommands::Unregister();
    FCkDebugToolsStyle::Shutdown();
}

auto FCkDebugToolsModule::DoRegisterToolTabSpawner(const FName& InTabName, const FText& InDisplayName,
                                                   TFunction<TSharedRef<SWidget>()> InWidgetFactory) -> void
{
    auto& GlobalTabManager = FGlobalTabmanager::Get();

    GlobalTabManager->RegisterNomadTabSpawner(
        InTabName,
        FOnSpawnTab::CreateLambda([InDisplayName, InWidgetFactory](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
        {
            return SNew(SDockTab)
                .TabRole(ETabRole::NomadTab)
                .Label(InDisplayName)
                .ToolTipText(FText::Format(FText::FromString(TEXT("{0} - ECS Debug Tool")), InDisplayName))
                [
                    InWidgetFactory()
                ];
        })
    )
    .SetDisplayName(InDisplayName)
    .SetTooltipText(FText::Format(FText::FromString(TEXT("Open {0} for ECS debugging")), InDisplayName))
    .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory())
    .SetIcon(FSlateIcon("CkDebugToolsStyle", "CkDebugTools.Icon"));

    _RegisteredTabNames.Add(InTabName);
}

auto FCkDebugToolsModule::DoUnregisterAllTabSpawners() -> void
{
    if (FSlateApplication::IsInitialized())
    {
        auto& GlobalTabManager = FGlobalTabmanager::Get();
        for (const auto& TabName : _RegisteredTabNames)
        {
            GlobalTabManager->UnregisterNomadTabSpawner(TabName);
        }
    }
    _RegisteredTabNames.Empty();
}

auto FCkDebugToolsModule::DoCreateToolbarExtension() -> void
{
    auto& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    auto ToolbarExtender = MakeShared<FExtender>();

    ToolbarExtender->AddToolBarExtension(
        "Settings",
        EExtensionHook::After,
        nullptr,
        FToolBarExtensionDelegate::CreateLambda([](FToolBarBuilder& Builder)
        {
            Builder.BeginSection("CkDebugTools");
            {
                Builder.AddComboButton(
                    FUIAction(),
                    FOnGetContent::CreateLambda([]() -> TSharedRef<SWidget>
                    {
                        FMenuBuilder MenuBuilder(true, nullptr);

                        MenuBuilder.AddMenuEntry(
                            FText::FromString(TEXT("Entity Inspector")),
                            FText::FromString(TEXT("Open Entity Inspector debug tool")),
                            FSlateIcon("CkDebugToolsStyle", "CkDebugTools.Entity.Icon"),
                            FUIAction(FExecuteAction::CreateLambda([]()
                            {
                                FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("CkEntityDebugTool")));
                            }))
                        );

                        return MenuBuilder.MakeWidget();
                    }),
                    FText::FromString(TEXT("Debug Tools")),
                    FText::FromString(TEXT("Open ECS Debug Tools")),
                    FSlateIcon("CkDebugToolsStyle", "CkDebugTools.Icon")
                );
            }
            Builder.EndSection();
        })
    );

    LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkDebugToolsModule, CkDebugTools)
