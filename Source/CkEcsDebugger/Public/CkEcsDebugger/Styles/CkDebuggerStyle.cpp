#include "CkDebuggerStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FCkDebuggerStyle::StyleInstance = nullptr;

const FLinearColor FCkDebuggerStyle::Color_Background_Dark = FLinearColor(0.01f, 0.01f, 0.01f);
const FLinearColor FCkDebuggerStyle::Color_Background_Medium = FLinearColor(0.025f, 0.025f, 0.025f);
const FLinearColor FCkDebuggerStyle::Color_Background_Light = FLinearColor(0.04f, 0.04f, 0.04f);
const FLinearColor FCkDebuggerStyle::Color_Border = FLinearColor(0.08f, 0.08f, 0.08f);
const FLinearColor FCkDebuggerStyle::Color_Selection = FLinearColor(0.2f, 0.4f, 0.8f);
const FLinearColor FCkDebuggerStyle::Color_SelectionInactive = FLinearColor(0.15f, 0.15f, 0.2f);
const FLinearColor FCkDebuggerStyle::Color_Hover = FLinearColor(0.06f, 0.06f, 0.08f);

const FLinearColor FCkDebuggerStyle::Color_Text_Primary = FLinearColor(0.85f, 0.85f, 0.85f);
const FLinearColor FCkDebuggerStyle::Color_Text_Secondary = FLinearColor(0.6f, 0.6f, 0.6f);
const FLinearColor FCkDebuggerStyle::Color_Text_Muted = FLinearColor(0.35f, 0.35f, 0.35f);
const FLinearColor FCkDebuggerStyle::Color_Text_Highlight = FLinearColor(0.95f, 0.95f, 0.95f);

const FLinearColor FCkDebuggerStyle::Color_Entity_ID = FLinearColor(0.51f, 0.69f, 1.0f);
const FLinearColor FCkDebuggerStyle::Color_Transform = FLinearColor(0.76f, 0.91f, 0.55f);
const FLinearColor FCkDebuggerStyle::Color_Network = FLinearColor(1.0f, 0.8f, 0.01f);
const FLinearColor FCkDebuggerStyle::Color_Relationship = FLinearColor(0.97f, 0.73f, 0.85f);
const FLinearColor FCkDebuggerStyle::Color_Attribute = FLinearColor(0.55f, 0.85f, 0.95f);
const FLinearColor FCkDebuggerStyle::Color_Reference = FLinearColor(0.51f, 0.69f, 1.0f);
const FLinearColor FCkDebuggerStyle::Color_None = FLinearColor(0.4f, 0.4f, 0.4f);
const FLinearColor FCkDebuggerStyle::Color_Error = FLinearColor(1.0f, 0.34f, 0.13f);
const FLinearColor FCkDebuggerStyle::Color_Warning = FLinearColor(1.0f, 0.8f, 0.01f);
const FLinearColor FCkDebuggerStyle::Color_Success = FLinearColor(0.25f, 0.75f, 0.25f);

// ---- Value-type colors
const FLinearColor FCkDebuggerStyle::Color_Value_Bool_True = FLinearColor(0.2f, 1.0f, 0.4f);
const FLinearColor FCkDebuggerStyle::Color_Value_Bool_False = FLinearColor(1.0f, 0.4f, 0.4f);
const FLinearColor FCkDebuggerStyle::Color_Value_Numeric = FLinearColor(0.6f, 0.9f, 0.6f);
const FLinearColor FCkDebuggerStyle::Color_Value_String = FLinearColor(1.0f, 0.85f, 0.5f);
const FLinearColor FCkDebuggerStyle::Color_Value_Math = FLinearColor(0.7f, 0.7f, 1.0f);
const FLinearColor FCkDebuggerStyle::Color_Value_Tag = FLinearColor(0.8f, 0.6f, 1.0f);
const FLinearColor FCkDebuggerStyle::Color_Value_Enum = FLinearColor(0.5f, 0.9f, 0.9f);
const FLinearColor FCkDebuggerStyle::Color_Value_Object = FLinearColor(0.9f, 0.7f, 0.4f);
const FLinearColor FCkDebuggerStyle::Color_Value_Handle = FLinearColor(0.4f, 0.8f, 1.0f);

// ---- State colors
const FLinearColor FCkDebuggerStyle::Color_State_Enabled = FLinearColor(0.0f, 1.0f, 0.5f);
const FLinearColor FCkDebuggerStyle::Color_State_Disabled = FLinearColor(1.0f, 0.5f, 0.5f);
const FLinearColor FCkDebuggerStyle::Color_State_Overlapping = FLinearColor(1.0f, 0.95f, 0.0f);
const FLinearColor FCkDebuggerStyle::Color_State_Config = FLinearColor(1.0f, 0.8f, 0.01f);

// ---- Status colors
const FLinearColor FCkDebuggerStyle::Color_Status_NotStarted = FLinearColor(0.5f, 0.5f, 0.5f);
const FLinearColor FCkDebuggerStyle::Color_Status_Active = FLinearColor(0.55f, 0.78f, 0.95f);
const FLinearColor FCkDebuggerStyle::Color_Status_Completed = FLinearColor(0.6f, 0.85f, 0.55f);
const FLinearColor FCkDebuggerStyle::Color_Status_Failed = FLinearColor(0.95f, 0.35f, 0.3f);

// ---- Graph colors
const FLinearColor FCkDebuggerStyle::Color_Graph_Background = FLinearColor(FColor(0x0D, 0x0D, 0x14));
const FLinearColor FCkDebuggerStyle::Color_Graph_Edge = FLinearColor(0.4f, 0.4f, 0.45f);
const FLinearColor FCkDebuggerStyle::Color_Graph_Node_Center = FLinearColor(FColor(0x2D, 0x2D, 0x3D));
const FLinearColor FCkDebuggerStyle::Color_Graph_Node_Default = FLinearColor(FColor(0x1E, 0x1E, 0x2E));
const FLinearColor FCkDebuggerStyle::Color_Graph_Node_Border_Default = FLinearColor(FColor(0x60, 0x7D, 0x8B));
const FLinearColor FCkDebuggerStyle::Color_Graph_Node_Border_Center = FLinearColor(FColor(0x4C, 0xAF, 0x50));

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
    InStyle->Set("CkDebugger.Background.Dark", new FSlateColorBrush(Color_Background_Dark));
    InStyle->Set("CkDebugger.Background.Medium", new FSlateColorBrush(Color_Background_Medium));
    InStyle->Set("CkDebugger.Background.Light", new FSlateColorBrush(Color_Background_Light));
    InStyle->Set("CkDebugger.Border", new FSlateColorBrush(Color_Border));
    InStyle->Set("CkDebugger.Selection", new FSlateColorBrush(Color_Selection));
    InStyle->Set("CkDebugger.Hover", new FSlateColorBrush(Color_Hover));

    InStyle->Set("CkDebugger.Panel.Background", new FSlateColorBrush(Color_Background_Medium));
    InStyle->Set("CkDebugger.Panel.Border", new FSlateRoundedBoxBrush(
        Color_Border,
        2.0f,
        Color_Background_Medium,
        1.0f
    ));

    InStyle->Set("CkDebugger.Row.Even", new FSlateColorBrush(Color_Background_Medium));
    InStyle->Set("CkDebugger.Row.Odd", new FSlateColorBrush(Color_Background_Light));

    InStyle->Set("CkDebugger.Separator", new FSlateColorBrush(Color_Border));

    InStyle->Set("CkDebugger.Graph.Background", new FSlateColorBrush(Color_Graph_Background));
    InStyle->Set("CkDebugger.Graph.NodeBackground", new FSlateRoundedBoxBrush(
        Color_Graph_Node_Default, GraphNode_CornerRadius));
    InStyle->Set("CkDebugger.Graph.NodeBackground.Hover", new FSlateRoundedBoxBrush(
        FLinearColor(FColor(0x2D, 0x2D, 0x3D)), GraphNode_CornerRadius));
    InStyle->Set("CkDebugger.Graph.NodeBackground.Center", new FSlateRoundedBoxBrush(
        Color_Graph_Node_Center, GraphNode_CornerRadius));

}

auto FCkDebuggerStyle::CreateColors(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    InStyle->Set("CkDebugger.Color.Text.Primary", Color_Text_Primary);
    InStyle->Set("CkDebugger.Color.Text.Secondary", Color_Text_Secondary);
    InStyle->Set("CkDebugger.Color.Text.Muted", Color_Text_Muted);
    InStyle->Set("CkDebugger.Color.Text.Highlight", Color_Text_Highlight);

    InStyle->Set("CkDebugger.Color.EntityID", Color_Entity_ID);
    InStyle->Set("CkDebugger.Color.Transform", Color_Transform);
    InStyle->Set("CkDebugger.Color.Network", Color_Network);
    InStyle->Set("CkDebugger.Color.Relationship", Color_Relationship);
    InStyle->Set("CkDebugger.Color.Attribute", Color_Attribute);
    InStyle->Set("CkDebugger.Color.Reference", Color_Reference);
    InStyle->Set("CkDebugger.Color.None", Color_None);

    InStyle->Set("CkDebugger.Color.Error", Color_Error);
    InStyle->Set("CkDebugger.Color.Warning", Color_Warning);
    InStyle->Set("CkDebugger.Color.Success", Color_Success);

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
        .SetColorAndOpacity(Color_Text_Primary));

    InStyle->Set("CkDebugger.Text.Bold", FTextBlockStyle()
        .SetFont(BoldFont)
        .SetColorAndOpacity(Color_Text_Primary));

    InStyle->Set("CkDebugger.Text.Monospace", FTextBlockStyle()
        .SetFont(MonospaceFont)
        .SetColorAndOpacity(Color_Text_Secondary));

    InStyle->Set("CkDebugger.Text.Header", FTextBlockStyle()
        .SetFont(HeaderFont)
        .SetColorAndOpacity(Color_Text_Highlight));

    InStyle->Set("CkDebugger.Text.LargeHeader", FTextBlockStyle()
        .SetFont(LargeHeaderFont)
        .SetColorAndOpacity(Color_Text_Highlight));

    InStyle->Set("CkDebugger.Text.Muted", FTextBlockStyle()
        .SetFont(DefaultFont)
        .SetColorAndOpacity(Color_Text_Muted));
}