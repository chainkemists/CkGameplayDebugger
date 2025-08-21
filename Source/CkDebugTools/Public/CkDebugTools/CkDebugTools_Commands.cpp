#include "CkDebugTools/CkDebugTools_Commands.h"

#include <Framework/Application/SlateApplication.h>

// --------------------------------------------------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "FCkDebugToolsCommands"

// --------------------------------------------------------------------------------------------------------------------

FCkDebugToolsCommands::FCkDebugToolsCommands()
    : TCommands<FCkDebugToolsCommands>(
        TEXT("CkDebugTools"),
        NSLOCTEXT("Contexts", "CkDebugTools", "Ck Debug Tools"),
        NAME_None,
        "CkDebugToolsStyle"
    )
{
}

auto FCkDebugToolsCommands::RegisterCommands() -> void
{
    UI_COMMAND(OpenEntityTool, "Entity Inspector", "Open the Entity Inspector debug tool", EUserInterfaceActionType::Button, FInputChord());
    UI_COMMAND(RefreshData, "Refresh Data", "Refresh the debug tool data", EUserInterfaceActionType::Button, FInputChord(EKeys::F5));
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE