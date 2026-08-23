#pragma once

#include "CkTextureDebugger/CkTextureDebugger_Log.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkTextureDebuggerWindow;
class SDockTab;

// --------------------------------------------------------------------------------------------------------------------

class FCkTextureDebuggerModule : public IModuleInterface
{
public:
    virtual auto
        StartupModule()
        -> void override;

    virtual auto
        ShutdownModule()
        -> void override;

    static auto
        Get()
        -> FCkTextureDebuggerModule&;

    auto
        OpenDebugger()
        -> void;

    auto
        CloseDebugger()
        -> void;

    auto
        ToggleDebugger()
        -> void;

    auto
        IsDebuggerOpen() const
        -> bool;

#if WITH_EDITOR
    /** Exact package names registered with the cook delegate; exposed for focused catalog coverage. */
    static auto
        Get_CookPackageNames()
        -> const TArray<FName>&;
#endif

private:
    auto
        OnSpawnDebuggerTab(
            const class FSpawnTabArgs& InArgs)
        -> TSharedRef<SDockTab>;

    TSharedPtr<SCkTextureDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab>                  _DebuggerTab;
    uint64                                _DebuggerToolRegistrationId = 0;

    static const FName _DebuggerTabName;

#if WITH_EDITOR
    FDelegateHandle _ModifyCookHandle;
#endif
};

// --------------------------------------------------------------------------------------------------------------------
