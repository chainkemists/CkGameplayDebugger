#include "CkDebug_PickerOverlayCards.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_picker_overlay_cards
{
    static ck::DebugPickerCards::FFactoryFn GFactory;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::DebugPickerCards
{
    auto
        Register_Factory(
            FFactoryFn InFactory)
        -> void
    {
        ck_debug_picker_overlay_cards::GFactory = MoveTemp(InFactory);
    }

    auto
        Create()
        -> TSharedPtr<ICkDebug_PickerOverlayCards>
    {
        if (NOT ck_debug_picker_overlay_cards::GFactory)
        { return nullptr; }

        return ck_debug_picker_overlay_cards::GFactory();
    }
}

// --------------------------------------------------------------------------------------------------------------------
