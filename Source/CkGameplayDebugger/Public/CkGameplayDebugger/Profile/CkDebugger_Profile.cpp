#include "CkDebugger_Profile.h"

#include "CkCore/EditorOnly/CkEditorOnly_Utils.h"
#include "CkGameplayDebugger/Action/CkDebugger_Action.h"
#include "CkGameplayDebugger/Filter/CkDebugger_Filter.h"
#include "CkGameplayDebugger/Submenu/CkDebugger_Submenu.h"

#include <UObject/ObjectSaveContext.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_profile
{
    auto
        PushAssetErrorNotification(
            const FString& InMsg,
            const UObject* InTargetObject = nullptr)
        -> void
    {
        UCk_Utils_EditorOnly_UE::Request_PushNewEditorMessage
        (
            FCk_Utils_EditorOnly_PushNewEditorMessage_Params
            {
                TEXT("CkGameplayDebugger"),
                FCk_MessageSegments
                {
                    {
                        FCk_TokenizedMessage{InMsg}.Set_TargetObject(const_cast<UObject*>(InTargetObject))
                    }
                }
            }
            .Set_MessageSeverity(ECk_EditorMessage_Severity::Error)
            .Set_MessageLogDisplayPolicy(ECk_EditorMessage_MessageLog_DisplayPolicy::DoNotFocus)
            .Set_ToastNotificationDisplayPolicy(ECk_EditorMessage_ToastNotification_DisplayPolicy::Display)
        );
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameplayDebugger_DebugProfile_PDA::
    PreSave(
        FObjectPreSaveContext InObjectSaveContext) -> void
{
    if (NOT IsTemplate())
    {
        ValidateAssetData();
    }

    Super::PreSave(InObjectSaveContext);
}

auto
    UCk_GameplayDebugger_DebugProfile_PDA::
    ValidateAssetData() const
    -> void
{
    int32 NumOfInvalidSubmenus = 0;
    int32 NumOfInvalidSubmenuNames = 0;
    int32 NumOfInvalidSubmenuActivationKeys = 0;

    int32 NumOfInvalidFilters = 0;
    int32 NumOfInvalidFilterNames = 0;

    int32 NumOfInvalidGlobalActions = 0;
    int32 NumOfInvalidGlobalActionNames = 0;
    int32 NumOfInvalidGlobalActionActivationKeys = 0;

    // TODO: Validate Debug Nav Controls Keys

    for (const auto& DebugSubmenu : _Submenus)
    {
        if (ck::Is_NOT_Valid(DebugSubmenu))
        {
            ++NumOfInvalidSubmenus;
            continue;
        }

        if (NOT DebugSubmenu->Get_HasValidSubmenuName())
        {
            ++NumOfInvalidSubmenuNames;
        }

        if (NOT DebugSubmenu->Get_HasValidSubmenuActivationKey())
        {
            ++NumOfInvalidSubmenuActivationKeys;
        }
    }

    for (const auto& DebugFilter : _Filters)
    {
        if (ck::Is_NOT_Valid(DebugFilter))
        {
            ++NumOfInvalidFilters;
            continue;
        }

        if (NOT DebugFilter->Get_HasValidFilterName())
        {
            ++NumOfInvalidFilterNames;
        }

        // TODO: Validate Filter Actions
    }

    for (const auto& GlobalDebugAction : _GlobalActions)
    {
        if (ck::Is_NOT_Valid(GlobalDebugAction))
        {
            ++NumOfInvalidGlobalActions;
            continue;
        }

        if (NOT GlobalDebugAction->Get_HasValidActionName())
        {
            ++NumOfInvalidGlobalActionNames;
        }

        if (NOT GlobalDebugAction->Get_HasValidActionActivationKey())
        {
            ++NumOfInvalidGlobalActionActivationKeys;
        }
    }

    // Submenus
    {
        if (NumOfInvalidSubmenus > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Submenu set to 'None'"), NumOfInvalidSubmenus), this);
        }

        if (NumOfInvalidSubmenuNames > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Submenu with an INVALID Name"), NumOfInvalidSubmenuNames), this);
        }

        if (NumOfInvalidSubmenuActivationKeys > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Submenu with an INVALID Activation Key"), NumOfInvalidSubmenuActivationKeys), this);
        }
    }

    // Filters
    {
        if (NumOfInvalidFilters > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Filter set to 'None'"), NumOfInvalidFilters), this);
        }

        if (NumOfInvalidFilterNames > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Filter with an INVALID Name"), NumOfInvalidFilterNames), this);
        }
    }

    // Global Actions
    {
        if (NumOfInvalidGlobalActions > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Global Action set to 'None'"), NumOfInvalidGlobalActions), this);
        }

        if (NumOfInvalidGlobalActionNames > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Global Action with an INVALID Name"), NumOfInvalidGlobalActionNames), this);
        }

        if (NumOfInvalidGlobalActionActivationKeys > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Global Action with an INVALID Activation Key"), NumOfInvalidGlobalActionActivationKeys), this);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
