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
#if WITH_EDITOR
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
                },
                ECk_EditorMessage_Severity::Error,
                ECk_EditorMessage_DisplayPolicy::ToastNotification
            }
        );
#endif // WITH_EDITOR
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
    int32 numOfInvalidSubmenus = 0;
    int32 numOfInvalidSubmenuNames = 0;
    int32 numOfInvalidSubmenuActivationKeys = 0;

    int32 numOfInvalidFilters = 0;
    int32 numOfInvalidFilterNames = 0;

    int32 numOfInvalidGlobalActions = 0;
    int32 numOfInvalidGlobalActionNames = 0;
    int32 numOfInvalidGlobalActionActivationKeys = 0;

    // TODO: Validate Debug Nav Controls Keys

    for (const auto& debugSubmenu : _Submenus)
    {
        if (ck::Is_NOT_Valid(debugSubmenu))
        {
            ++numOfInvalidSubmenus;
            continue;
        }

        if (NOT debugSubmenu->Get_HasValidSubmenuName())
        {
            ++numOfInvalidSubmenuNames;
        }

        if (NOT debugSubmenu->Get_HasValidSubmenuActivationKey())
        {
            ++numOfInvalidSubmenuActivationKeys;
        }
    }

    for (const auto& debugFilter : _Filters)
    {
        if (ck::Is_NOT_Valid(debugFilter))
        {
            ++numOfInvalidFilters;
            continue;
        }

        if (NOT debugFilter->Get_HasValidFilterName())
        {
            ++numOfInvalidFilterNames;
        }

        // TODO: Validate Filter Actions
    }

    for (const auto& globalDebugAction : _GlobalActions)
    {
        if (ck::Is_NOT_Valid(globalDebugAction))
        {
            ++numOfInvalidGlobalActions;
            continue;
        }

        if (NOT globalDebugAction->Get_HasValidActionName())
        {
            ++numOfInvalidGlobalActionNames;
        }

        if (NOT globalDebugAction->Get_HasValidActionActivationKey())
        {
            ++numOfInvalidGlobalActionActivationKeys;
        }
    }

    // Submenus
    {
        if (numOfInvalidSubmenus > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Submenu set to 'None'"), numOfInvalidSubmenus), this);
        }

        if (numOfInvalidSubmenuNames > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Submenu with an INVALID Name"), numOfInvalidSubmenuNames), this);
        }

        if (numOfInvalidSubmenuActivationKeys > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Submenu with an INVALID Activation Key"), numOfInvalidSubmenuActivationKeys), this);
        }
    }

    // Filters
    {
        if (numOfInvalidFilters > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Filter set to 'None'"), numOfInvalidFilters), this);
        }

        if (numOfInvalidFilterNames > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Filter with an INVALID Name"), numOfInvalidFilterNames), this);
        }
    }

    // Global Actions
    {
        if (numOfInvalidGlobalActions > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Global Action set to 'None'"), numOfInvalidGlobalActions), this);
        }

        if (numOfInvalidGlobalActionNames > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Global Action with an INVALID Name"), numOfInvalidGlobalActionNames), this);
        }

        if (numOfInvalidGlobalActionActivationKeys > 0)
        {
            ck_debug_profile::PushAssetErrorNotification(ck::Format_UE(TEXT("Debug Profile has [{}] Global Action with an INVALID Activation Key"), numOfInvalidGlobalActionActivationKeys), this);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
