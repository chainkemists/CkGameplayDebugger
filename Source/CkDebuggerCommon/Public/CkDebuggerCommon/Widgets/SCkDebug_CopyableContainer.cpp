#include "SCkDebug_CopyableContainer.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_CopyableContainer::Construct(const FArguments& InArgs) -> void
{
    const auto CopyText = InArgs._CopyText;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("NoBorder"))
        .Padding(FMargin(0.0f))
        .OnMouseButtonDown_Lambda([WeakThis = TWeakPtr<SWidget>(AsShared()), CopyText]
            (const FGeometry&, const FPointerEvent& Evt) -> FReply
        {
            const auto Self = WeakThis.Pin();
            if (NOT Self.IsValid())
            { return FReply::Unhandled(); }
            return ck::DebugCopyMenu::Handle_RightClickToCopy(Self.ToSharedRef(), Evt, CopyText);
        })
        [
            InArgs._Content.Widget
        ]
    ];
}
