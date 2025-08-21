#pragma once

#include <Framework/Commands/Commands.h>

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGTOOLS_API FCkDebugToolsCommands : public TCommands<FCkDebugToolsCommands>
{
public:
    FCkDebugToolsCommands();

    auto RegisterCommands() -> void override;

public:
    TSharedPtr<FUICommandInfo> OpenEntityTool;
    TSharedPtr<FUICommandInfo> RefreshData;
};

// --------------------------------------------------------------------------------------------------------------------