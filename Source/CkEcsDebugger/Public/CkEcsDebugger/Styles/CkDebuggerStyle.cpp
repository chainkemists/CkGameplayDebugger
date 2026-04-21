#include "CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkObjective/Objective/CkObjective_Fragment_Data.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FCkDebuggerStyle::StyleInstance = nullptr;

// -----------------------------------------------------------------------------------------------------------------
auto FCkDebuggerStyle::Get_ObjectiveStatusColor(ECk_ObjectiveStatus InStatus) -> FLinearColor
{
    switch (InStatus)
    {
        case ECk_ObjectiveStatus::NotStarted: return CkDebugStyle::Status_NotStarted();
        case ECk_ObjectiveStatus::Active:     return CkDebugStyle::Status_Active();
        case ECk_ObjectiveStatus::Completed:  return CkDebugStyle::Status_Completed();
        case ECk_ObjectiveStatus::Failed:     return CkDebugStyle::Status_Failed();
        default:                              return CkDebugStyle::Text();
    }
}

// -----------------------------------------------------------------------------------------------------------------
auto FCkDebuggerStyle::Initialize() -> void
{
    if (NOT StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

auto FCkDebuggerStyle::Shutdown() -> void
{
    if (StyleInstance.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
        StyleInstance.Reset();
    }
}

auto FCkDebuggerStyle::Get() -> const ISlateStyle&
{
    return *StyleInstance;
}

auto FCkDebuggerStyle::GetStyleSetName() -> FName
{
    static const FName StyleSetName(TEXT("CkDebuggerStyle"));
    return StyleSetName;
}

auto FCkDebuggerStyle::Create() -> TSharedRef<FSlateStyleSet>
{
    auto Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

    CreateBrushes(Style);
    CreateColors(Style);
    CreateTextStyles(Style);

    return Style;
}

auto FCkDebuggerStyle::CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    InStyle->Set("CkDebugger.Background.Dark", new FSlateColorBrush(CkDebugStyle::BgRoot()));
    InStyle->Set("CkDebugger.Background.Medium", new FSlateColorBrush(CkDebugStyle::Bg1()));
    InStyle->Set("CkDebugger.Background.Light", new FSlateColorBrush(CkDebugStyle::Bg2()));
    InStyle->Set("CkDebugger.Border", new FSlateColorBrush(CkDebugStyle::Border()));
    InStyle->Set("CkDebugger.Selection", new FSlateColorBrush(CkDebugStyle::Selection()));
    InStyle->Set("CkDebugger.Hover", new FSlateColorBrush(CkDebugStyle::Hover()));

    InStyle->Set("CkDebugger.Panel.Background", new FSlateColorBrush(CkDebugStyle::Bg1()));
    InStyle->Set("CkDebugger.Panel.Border", new FSlateRoundedBoxBrush(
        CkDebugStyle::Border(),
        2.0f,
        CkDebugStyle::Bg1(),
        1.0f
    ));

    InStyle->Set("CkDebugger.Row.Even", new FSlateColorBrush(CkDebugStyle::Bg1()));
    InStyle->Set("CkDebugger.Row.Odd", new FSlateColorBrush(CkDebugStyle::Bg2()));

    InStyle->Set("CkDebugger.Separator", new FSlateColorBrush(CkDebugStyle::Border()));

    InStyle->Set("CkDebugger.Graph.Background", new FSlateColorBrush(CkDebugStyle::Graph_Background()));
    InStyle->Set("CkDebugger.Graph.NodeBackground", new FSlateRoundedBoxBrush(
        CkDebugStyle::Graph_Node_Default(), GraphNode_CornerRadius));
    InStyle->Set("CkDebugger.Graph.NodeBackground.Hover", new FSlateRoundedBoxBrush(
        CkDebugStyle::Graph_Node_Center(), GraphNode_CornerRadius));
    InStyle->Set("CkDebugger.Graph.NodeBackground.Center", new FSlateRoundedBoxBrush(
        CkDebugStyle::Graph_Node_Center(), GraphNode_CornerRadius));

    InStyle->Set("CkDebugger.Badge.Rounded", new FSlateRoundedBoxBrush(
        FLinearColor::White, 3.0f));
}

auto FCkDebuggerStyle::CreateColors(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    InStyle->Set("CkDebugger.Color.Text.Primary", CkDebugStyle::Text());
    InStyle->Set("CkDebugger.Color.Text.Secondary", CkDebugStyle::TextDim());
    InStyle->Set("CkDebugger.Color.Text.Muted", CkDebugStyle::TextMute());
    InStyle->Set("CkDebugger.Color.Text.Highlight", CkDebugStyle::TextStrong());

    InStyle->Set("CkDebugger.Color.EntityID", CkDebugStyle::EntityId());
    InStyle->Set("CkDebugger.Color.Transform", CkDebugStyle::Transform());
    InStyle->Set("CkDebugger.Color.Network", CkDebugStyle::Network());
    InStyle->Set("CkDebugger.Color.Relationship", CkDebugStyle::Relationship());
    InStyle->Set("CkDebugger.Color.Attribute", CkDebugStyle::Attribute());
    InStyle->Set("CkDebugger.Color.Reference", CkDebugStyle::Reference());
    InStyle->Set("CkDebugger.Color.None", CkDebugStyle::None());

    InStyle->Set("CkDebugger.Color.Error", CkDebugStyle::Err());
    InStyle->Set("CkDebugger.Color.Warning", CkDebugStyle::Warn());
    InStyle->Set("CkDebugger.Color.Success", CkDebugStyle::Ok());
}

auto FCkDebuggerStyle::CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    const auto DefaultFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
    const auto BoldFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
    const auto MonospaceFont = FCoreStyle::GetDefaultFontStyle("Mono", 9);
    const auto HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
    const auto LargeHeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);

    InStyle->Set("CkDebugger.Text.Normal", FTextBlockStyle()
        .SetFont(DefaultFont)
        .SetColorAndOpacity(CkDebugStyle::Text()));

    InStyle->Set("CkDebugger.Text.Bold", FTextBlockStyle()
        .SetFont(BoldFont)
        .SetColorAndOpacity(CkDebugStyle::Text()));

    InStyle->Set("CkDebugger.Text.Monospace", FTextBlockStyle()
        .SetFont(MonospaceFont)
        .SetColorAndOpacity(CkDebugStyle::TextDim()));

    InStyle->Set("CkDebugger.Text.Header", FTextBlockStyle()
        .SetFont(HeaderFont)
        .SetColorAndOpacity(CkDebugStyle::TextStrong()));

    InStyle->Set("CkDebugger.Text.LargeHeader", FTextBlockStyle()
        .SetFont(LargeHeaderFont)
        .SetColorAndOpacity(CkDebugStyle::TextStrong()));

    InStyle->Set("CkDebugger.Text.Muted", FTextBlockStyle()
        .SetFont(DefaultFont)
        .SetColorAndOpacity(CkDebugStyle::TextMute()));
}
