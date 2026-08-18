#include "CkDebug_CopyMenu_Utils.h"
#include "CkEditorTools/Style/CkIconStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"

#if WITH_EDITOR
    #include "ToolMenu.h"
    #include "ToolMenuSection.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck::DebugCopyMenu
{
    namespace
    {
        auto Get_CopyIcon() -> FSlateIcon
        {
            return FSlateIcon(FCkIconStyle::GetStyleSetName(), FCkIconStyle::Get_StyleKey(ECk_Icon::Copy, ECk_Icon_BrushSize::Size_16x16));
        }

        auto Make_CopyAction(FString InTextToCopy) -> FUIAction
        {
            return FUIAction(FExecuteAction::CreateLambda([Text = MoveTemp(InTextToCopy)]()
            {
                FPlatformApplicationMisc::ClipboardCopy(*Text);
            }));
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto Push_CopyTextMenu(
        const TSharedRef<SWidget>& InSourceWidget,
        const FVector2D& InScreenSpacePosition,
        const FString& InTextToCopy) -> FReply
    {
        auto MenuBuilder = FMenuBuilder(true, nullptr);

        AddCopyEntry(
            MenuBuilder,
            FText::FromString(TEXT("Copy Text")),
            FText::FromString(TEXT("Copy this text to the clipboard")),
            InTextToCopy);

        FSlateApplication::Get().PushMenu(
            InSourceWidget,
            FWidgetPath(),
            MenuBuilder.MakeWidget(),
            InScreenSpacePosition,
            FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

        return FReply::Handled();
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto Handle_RightClickToCopy(
        const TSharedRef<SWidget>& InSourceWidget,
        const FPointerEvent& InMouseEvent,
        const FString& InTextToCopy) -> FReply
    {
        if (InMouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
        { return FReply::Unhandled(); }

        return Push_CopyTextMenu(InSourceWidget, InMouseEvent.GetScreenSpacePosition(), InTextToCopy);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto AddCopyEntry(
        FMenuBuilder& InMenuBuilder,
        const FText& InLabel,
        const FText& InTooltip,
        const FString& InTextToCopy) -> void
    {
        InMenuBuilder.AddMenuEntry(
            InLabel,
            InTooltip,
            Get_CopyIcon(),
            Make_CopyAction(InTextToCopy));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto AddCopyEntryToToolMenu(
        UToolMenu* InMenu,
        FName InEntryName,
        const FText& InLabel,
        const FText& InTooltip,
        const FString& InTextToCopy) -> void
    {
#if WITH_EDITOR
        if (InMenu == nullptr)
        { return; }

        auto& Section = InMenu->FindOrAddSection(
            TEXT("CkDebugCopy"),
            FText::FromString(TEXT("Copy")));

        Section.AddMenuEntry(
            InEntryName,
            InLabel,
            InTooltip,
            Get_CopyIcon(),
            Make_CopyAction(InTextToCopy));
#endif
    }
}
