#include "CkSlateDebugger_Module.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkSlateDebugger/CkSlateDebuggerStyle.h"
#include "CkSlateDebugger/CkSlateDebuggerWindow.h"

#define LOCTEXT_NAMESPACE "FCkSlateDebuggerModule"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSlateDebuggerModule::
    StartupModule()
    -> void
{
    FCkSlateDebuggerStyle::Initialize();
    RegisterConsoleCommands();
}

auto
    FCkSlateDebuggerModule::
    ShutdownModule()
    -> void
{
    CloseDebugger();
    UnregisterConsoleCommands();
    FCkSlateDebuggerStyle::Shutdown();
}

auto FCkSlateDebuggerModule::OpenDebugger() -> void
{
    if (IsDebuggerOpen())
    {
        SlateWindow->BringToFront();
        return;
    }

    DebuggerWindow = SNew(SCkSlateDebuggerWindow);

    SlateWindow = SNew(SWindow)
        .Title(LOCTEXT("CkDebuggerTitle", "CkDebugger - ECS Inspector"))
        .ClientSize(FVector2D(1400, 900))
        .SupportsMaximize(true)
        .SupportsMinimize(true)
        .SizingRule(ESizingRule::UserSized)
        .AutoCenter(EAutoCenter::PreferredWorkArea);

    SlateWindow->SetContent(DebuggerWindow.ToSharedRef());

    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().AddWindow(SlateWindow.ToSharedRef());
    }

    SlateWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>&)
    {
        DebuggerWindow.Reset();
        SlateWindow.Reset();
    }));
}

auto FCkSlateDebuggerModule::CloseDebugger() -> void
{
    if (NOT IsDebuggerOpen())
    { return; }

    if (SlateWindow.IsValid())
    {
        SlateWindow->RequestDestroyWindow();
    }

    DebuggerWindow.Reset();
    SlateWindow.Reset();
}

auto FCkSlateDebuggerModule::IsDebuggerOpen() const -> bool
{
    return SlateWindow.IsValid() && DebuggerWindow.IsValid();
}

auto FCkSlateDebuggerModule::RegisterConsoleCommands() -> void
{
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("ck.debugger.open"),
        TEXT("Open the CkDebugger window"),
        FConsoleCommandDelegate::CreateLambda([this]()
        {
            OpenDebugger();
        }),
        ECVF_Default
    ));

    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("ck.debugger.close"),
        TEXT("Close the CkDebugger window"),
        FConsoleCommandDelegate::CreateLambda([this]()
        {
            CloseDebugger();
        }),
        ECVF_Default
    ));

    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("ck.debugger.toggle"),
        TEXT("Toggle the CkDebugger window"),
        FConsoleCommandDelegate::CreateLambda([this]()
        {
            if (IsDebuggerOpen())
            {
                CloseDebugger();
            }
            else
            {
                OpenDebugger();
            }
        }),
        ECVF_Default
    ));
}

auto FCkSlateDebuggerModule::UnregisterConsoleCommands() -> void
{
    for (auto* Command : ConsoleCommands)
    {
        IConsoleManager::Get().UnregisterConsoleObject(Command);
    }
    ConsoleCommands.Empty();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkSlateDebuggerModule, CkSlateDebugger)
