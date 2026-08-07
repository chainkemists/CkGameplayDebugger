#include "SCkDebug_ToggleSurface.h"

#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"

#include "Widgets/Input/SCheckBox.h"

// ====================================================================================================================

auto
    SCkDebug_ToggleSurface::
    Construct(const FArguments& InArgs)
    -> void
{
    ChildSlot
    [
        SNew(SCheckBox)
        .Style(&FCkDebuggerCommonStyle::Get_IconToggleStyle())
        .IsChecked_Lambda([IsOn = InArgs._IsOn]()
        {
            return IsOn.Get(false) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged_Lambda([OnStateChanged = InArgs._OnStateChanged](ECheckBoxState InState)
        {
            if (OnStateChanged.IsBound())
            { OnStateChanged.Execute(InState == ECheckBoxState::Checked); }
        })
        .IsEnabled(InArgs._IsEnabled)
        .AccessibleText(InArgs._AccessibleText)
        .ToolTipText(InArgs._ToolTipText)
        [
            InArgs._Content.Widget
        ]
    ];
}

// ====================================================================================================================
