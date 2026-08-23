#include "CkTextureDebugger_Module.h"

#include "CkTextureDebugger/Window/SCkTextureDebuggerWindow.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
    #include "UObject/ICookInfo.h"
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkTextureDebuggerModule"

DEFINE_LOG_CATEGORY(LogCkTextureDebugger);

const FName FCkTextureDebuggerModule::_DebuggerTabName = FName{TEXT("CkTextureDebugger")};

// --------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR
namespace ck_texture_debugger_module
{
    auto Get_CookPackageNames() -> const TArray<FName>&
    {
        static const auto PackageNames = TArray<FName>{
            TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_ColorGrid_2K"),
            TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_ColorGrid_4K"),
            TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_GoldGray_4K"),
            TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_RoundedSpectrum_4K"),
            TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_DirectionalMono_4K"),
            TEXT("/CkDebugger/TextureDebugger/Materials/M_CkTextureChecker"),
        };
        return PackageNames;
    }

    auto Add_CheckerAssetCookRules(
        UE::Cook::ICookInfo&,
        TArray<UE::Cook::FPackageCookRule>& InOutCookRules) -> void
    {
        for (const auto& PackageName : Get_CookPackageNames())
        {
            if (InOutCookRules.ContainsByPredicate([PackageName](const UE::Cook::FPackageCookRule& InRule)
                {
                    return InRule.PackageName == PackageName &&
                        InRule.CookRule == UE::Cook::EPackageCookRule::AddToCook;
                }))
            {
                continue;
            }

            auto& Rule = InOutCookRules.AddDefaulted_GetRef();
            Rule.PackageName = PackageName;
            Rule.InstigatorName = FName{TEXT("CkTextureDebugger")};
            Rule.CookRule = UE::Cook::EPackageCookRule::AddToCook;
        }
    }
}

auto
    FCkTextureDebuggerModule::
    Get_CookPackageNames()
    -> const TArray<FName>&
{
    return ck_texture_debugger_module::Get_CookPackageNames();
}
#endif

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdTextureDebugger(
    TEXT("ck.TextureDebugger"),
    TEXT("Opens (1) or closes (0) the CK Texture & Surface Debugger. Usage: ck.TextureDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkTextureDebuggerModule::Get();

        if (InArgs.IsEmpty())
        {
            Module.ToggleDebugger();
            return;
        }

        const auto Value = FCString::Atoi(*InArgs[0]);
        if (Value == 1) { Module.OpenDebugger(); }
        else if (Value == 0) { Module.CloseDebugger(); }
        else { Module.ToggleDebugger(); }
    }));

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkTextureDebuggerModule::
    StartupModule()
    -> void
{
#if WITH_EDITOR
    _ModifyCookHandle = UE::Cook::FDelegates::ModifyCook.AddStatic(
        &ck_texture_debugger_module::Add_CheckerAssetCookRules);
#endif

    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkTextureDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(LOCTEXT("TextureDebuggerTabTitle", "CK Texture & Surface Debugger"))
        .SetTooltipText(LOCTEXT("TextureDebuggerTabTooltip", "Inspect checker application, texture health, and surface context"));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkTextureDebugger"),
        _DebuggerTabName,
        LOCTEXT("TextureDebuggerDisplayName", "[CK] Texture & Surface Debugger"),
        LOCTEXT("TextureDebuggerTooltip", "Apply checker textures and inspect texture, material, UV, and surface health"),
        ECk_Icon::TextureAsset,
        ECkDebuggerToolCategory::Tools,
        50}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));
}

auto
    FCkTextureDebuggerModule::
    ShutdownModule()
    -> void
{
    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    CloseDebugger();

#if WITH_EDITOR
    if (_ModifyCookHandle.IsValid())
    {
        UE::Cook::FDelegates::ModifyCook.Remove(_ModifyCookHandle);
        _ModifyCookHandle.Reset();
    }
#endif

    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName);
    }
}

auto
    FCkTextureDebuggerModule::
    Get()
    -> FCkTextureDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkTextureDebuggerModule>(TEXT("CkTextureDebugger"));
}

auto
    FCkTextureDebuggerModule::
    OpenDebugger()
    -> void
{
    UE_LOG(LogCkTextureDebugger, Display, TEXT("Opening Texture & Surface Debugger tab."));
    ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName);
}

auto
    FCkTextureDebuggerModule::
    CloseDebugger()
    -> void
{
    if (_DebuggerTab.IsValid())
    {
        if (NOT IsEngineExitRequested())
        { _DebuggerTab->RequestCloseTab(); }

        _DebuggerTab.Reset();
    }

    _DebuggerWindow.Reset();
}

auto
    FCkTextureDebuggerModule::
    ToggleDebugger()
    -> void
{
    if (IsDebuggerOpen()) { CloseDebugger(); }
    else { OpenDebugger(); }
}

auto
    FCkTextureDebuggerModule::
    IsDebuggerOpen() const
    -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto
    FCkTextureDebuggerModule::
    OnSpawnDebuggerTab(
        const FSpawnTabArgs&)
    -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkTextureDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("TextureDebuggerTabLabel", "CK Texture & Surface Debugger"))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            _DebuggerWindow.Reset();
            _DebuggerTab.Reset();
        })
        [
            _DebuggerWindow.ToSharedRef()
        ];

    _DebuggerWindow->Set_OwningTab(_DebuggerTab);
    UE_LOG(LogCkTextureDebugger, Display, TEXT("Texture & Surface Debugger tab and window constructed."));
    return _DebuggerTab.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkTextureDebuggerModule, CkTextureDebugger)
