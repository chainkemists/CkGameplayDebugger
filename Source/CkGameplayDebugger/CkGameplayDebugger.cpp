#include "CkGameplayDebugger.h"

#include "CkGameplayDebugger/Category/CkDebugger_Category.h"
#include "CkGameplayDebugger/Profile/CkDebugger_Profile.h"
#include "CkGameplayDebugger/Subsystem/CkDebugger_Subsystem.h"

#define LOCTEXT_NAMESPACE "FCkGameplayDebuggerModule"

#if WITH_GAMEPLAY_DEBUGGER
#include <GameplayDebugger.h>
#include "Engine/Console.h"
#include "Engine/AssetManager.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugger
{
    namespace cmd
    {
        static FAutoConsoleCommandWithWorldAndArgs Cmd_LoadDebugProfile
        (
            TEXT("LoadDebugProfile"),
            TEXT("Load a specific Gameplay Debugger Profile"),
            FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& InArgs, UWorld* InWorld)
            {
                if (ck::Is_NOT_Valid(InWorld))
                { return; }

                if (InArgs.IsEmpty())
                { return; }

                const auto& DebugProfileAssetPath = *InArgs[0];

                /*const auto FoundDebugProfileAsset = Cast<UCk_GameplayDebugger_DebugProfile_PDA>
                (
                    StaticLoadObject(UCk_GameplayDebugger_DebugProfile_PDA::StaticClass(),
                    nullptr,
                    DebugProfileAssetPath
                ));

                InWorld->GetSubsystem<UCk_GameplayDebugger_Subsystem_UE>()->Request_LoadDebugProfile(FoundDebugProfileAsset);*/
            })
        );
    }
}

// --------------------------------------------------------------------------------------------------------------------

void FCkGameplayDebuggerModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
    //If the gameplay debugger is available, register the category and notify the editor about the changes
    IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();

    GameplayDebuggerModule.RegisterCategory
    (
        "CkGameplayDebugger",
        IGameplayDebugger::FOnGetCategory::CreateStatic(&FCk_GameplayDebugger_Category::MakeInstance),
        EGameplayDebuggerCategoryState::EnabledInGameAndSimulate
    );

    GameplayDebuggerModule.NotifyCategoriesChanged();

#if ALLOW_CONSOLE
    _ConsoleAutoCompleteDelegateHandle = UConsole::RegisterConsoleAutoCompleteEntries.AddStatic(&FCkGameplayDebuggerModule::PopulateAutoCompleteEntries);
#endif

#endif
}

auto
    FCkGameplayDebuggerModule::
    ShutdownModule()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    //If the gameplay debugger is available, unregister the category
    if (IGameplayDebugger::IsAvailable())
    {
        IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
        GameplayDebuggerModule.UnregisterCategory("CkGameplayDebugger");
    }

#if ALLOW_CONSOLE
    UConsole::RegisterConsoleAutoCompleteEntries.Remove(_ConsoleAutoCompleteDelegateHandle);
#endif

#endif
}

#if ALLOW_CONSOLE
auto
    FCkGameplayDebuggerModule::
    PopulateAutoCompleteEntries(
        TArray<FAutoCompleteCommand>& AutoCompleteList)
    -> void
{
    TArray<FAssetData> OutItemAssets;
    UAssetManager::Get().GetPrimaryAssetDataList(FPrimaryAssetType{UCk_GameplayDebugger_DebugProfile_PDA::StaticClass()->GetFName()}, OutItemAssets);

    const auto* ConsoleSettings = GetDefault<UConsoleSettings>();

    for (const FAssetData& ItemAsset : OutItemAssets)
    {
        FAutoCompleteCommand AutoCompleteCommand;

        AutoCompleteCommand.Command = ck::Format_UE(TEXT("LoadDebugProfile {}"), ItemAsset.GetObjectPathString());
        AutoCompleteCommand.Desc = FString{TEXT("Load a specific Gameplay Debugger Profile")};
        AutoCompleteCommand.Color = ConsoleSettings->AutoCompleteCommandColor;

        AutoCompleteList.Add(MoveTemp(AutoCompleteCommand));
    }
}
#endif

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkGameplayDebuggerModule, CkGameplayDebugger)
