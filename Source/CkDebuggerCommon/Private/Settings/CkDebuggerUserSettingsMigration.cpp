#include "CkDebuggerCommon/Settings/CkDebuggerUserSettingsMigration.h"

#include "Misc/ConfigCacheIni.h"
#include "UObject/Object.h"

#include "CkCore/Validation/CkIsValid.h"

namespace ck::debugger_settings
{
    auto
        Migrate_EditorUserSettingsIfNeeded(
            UObject* InSettings)
        -> void
    {
#if WITH_EDITOR
        if (ck::Is_NOT_Valid(InSettings, ck::IsValid_Policy_NullptrOnly{}) || GConfig == nullptr)
        { return; }

        auto* SettingsClass = InSettings->GetClass();
        if (ck::Is_NOT_Valid(SettingsClass, ck::IsValid_Policy_NullptrOnly{}))
        { return; }

        static const auto LegacyConfigName = FString{TEXT("EditorPerProjectUserSettings")};
        static const auto RuntimeConfigName = FString{TEXT("GameUserSettings")};
        const auto SectionName = SettingsClass->GetPathName();

        // A runtime value, even a partial one, is an explicit post-migration user choice.
        if (GConfig->DoesSectionExist(*SectionName, RuntimeConfigName)
            || NOT GConfig->DoesSectionExist(*SectionName, LegacyConfigName))
        { return; }

        InSettings->LoadConfig(SettingsClass, *LegacyConfigName);
        InSettings->SaveConfig(CPF_Config, *RuntimeConfigName);
#else
        static_cast<void>(InSettings);
#endif
    }
}
