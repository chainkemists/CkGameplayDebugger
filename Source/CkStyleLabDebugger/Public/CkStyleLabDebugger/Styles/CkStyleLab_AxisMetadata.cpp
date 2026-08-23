#include "CkStyleLabDebugger/Styles/CkStyleLab_AxisMetadata.h"

namespace ck::style_lab
{
    namespace
    {
        auto Get_Groups() -> const TArray<FCkStyleLab_GroupMetadata>&
        {
            static const auto Groups = TArray<FCkStyleLab_GroupMetadata>{
                {ECkStyleLab_Group::WorkbenchSurfaces, NSLOCTEXT("CkStyleLabGroups", "WorkbenchSurfaces.Name", "Layout, panes, and density"), NSLOCTEXT("CkStyleLabGroups", "WorkbenchSurfaces.Desc", "The overall debugger workspace: pane treatment, corners, spacing, separators, row bands, and text scale."), true},
                {ECkStyleLab_Group::TokensLegend, NSLOCTEXT("CkStyleLabGroups", "TokensLegend.Name", "Tokens, chips, and legend"), NSLOCTEXT("CkStyleLabGroups", "TokensLegend.Desc", "Compact semantic labels: chips, badges, fold markers, provider labels, legends, and merged counts."), true},
                {ECkStyleLab_Group::EntityValues, NSLOCTEXT("CkStyleLabGroups", "EntityValues.Name", "Entity and value reading"), NSLOCTEXT("CkStyleLabGroups", "EntityValues.Desc", "How entity references are named and drawn, and how key/value columns align."), true},
                {ECkStyleLab_Group::HierarchyEditing, NSLOCTEXT("CkStyleLabGroups", "HierarchyEditing.Name", "Hierarchy and edit behavior"), NSLOCTEXT("CkStyleLabGroups", "HierarchyEditing.Desc", "Tree complexity, section hierarchy, edit affordances, and value-change feedback."), true},
                {ECkStyleLab_Group::Icons, NSLOCTEXT("CkStyleLabGroups", "Icons.Name", "Icons and glyph treatment"), NSLOCTEXT("CkStyleLabGroups", "Icons.Desc", "Debugger glyph size and the backdrop used to separate icons from dense data."), true},
                {ECkStyleLab_Group::GraphTelemetry, NSLOCTEXT("CkStyleLabGroups", "GraphTelemetry.Name", "Graph telemetry"), NSLOCTEXT("CkStyleLabGroups", "GraphTelemetry.Desc", "Node density and the timing and strength of live graph feedback."), true},
                {ECkStyleLab_Group::InputHud, NSLOCTEXT("CkStyleLabGroups", "InputHud.Name", "Input HUD — Signal Strip"), NSLOCTEXT("CkStyleLabGroups", "InputHud.Desc", "Feature-specific Input HUD presentation. These settings are separate from the generic debugger axes above."), false},
            };
            return Groups;
        }

        // Keep this table in FCkDebuggerStyleSelection declaration order. Reflection remains the
        // source of row order; the table is solely the packaged-safe presentation contract.
        auto Get_AxisMetadata() -> const TArray<FCkStyleLab_AxisMetadata>&
        {
            // FName registration must happen after the module/runtime has initialized, rather than
            // during static initialization of this translation unit.
            static const auto AxisMetadata = TArray<FCkStyleLab_AxisMetadata>{
                {TEXT("ChipStyle"), NSLOCTEXT("CkStyleLabAxisMetadata", "ChipStyle.DisplayName", "Chip Style"), NSLOCTEXT("CkStyleLabAxisMetadata", "ChipStyle.ToolTip", "Treatment for inspector badge boxes, feature chips, and overlay field chips."), ECkStyleLab_Group::TokensLegend},
                {TEXT("RowDensity"), NSLOCTEXT("CkStyleLabAxisMetadata", "RowDensity.DisplayName", "Row Density"), NSLOCTEXT("CkStyleLabAxisMetadata", "RowDensity.ToolTip", "Row padding for tree rows, inspector rows, and overlay card rows."), ECkStyleLab_Group::WorkbenchSurfaces},
                {TEXT("EntityIdStyle"), NSLOCTEXT("CkStyleLabAxisMetadata", "EntityIdStyle.DisplayName", "Entity Id Style"), NSLOCTEXT("CkStyleLabAxisMetadata", "EntityIdStyle.ToolTip", "How an entity reference composes its display TEXT on every SCkDebug_EntityRef site. How it is drawn stays with Entity Ref Style."), ECkStyleLab_Group::EntityValues},
                {TEXT("BadgeStyle"), NSLOCTEXT("CkStyleLabAxisMetadata", "BadgeStyle.DisplayName", "Badge Style"), NSLOCTEXT("CkStyleLabAxisMetadata", "BadgeStyle.ToolTip", "Treatment for count badges and fold badges."), ECkStyleLab_Group::TokensLegend},
                {TEXT("SeparatorWeight"), NSLOCTEXT("CkStyleLabAxisMetadata", "SeparatorWeight.DisplayName", "Separator Weight"), NSLOCTEXT("CkStyleLabAxisMetadata", "SeparatorWeight.ToolTip", "Thickness of section separators in the inspector and the overlay card."), ECkStyleLab_Group::WorkbenchSurfaces},
                {TEXT("FoldChipStyle"), NSLOCTEXT("CkStyleLabAxisMetadata", "FoldChipStyle.DisplayName", "Fold Chip Style"), NSLOCTEXT("CkStyleLabAxisMetadata", "FoldChipStyle.ToolTip", "Treatment for tree fold / group chips."), ECkStyleLab_Group::TokensLegend},
                {TEXT("ValueAlignment"), NSLOCTEXT("CkStyleLabAxisMetadata", "ValueAlignment.DisplayName", "Value Alignment"), NSLOCTEXT("CkStyleLabAxisMetadata", "ValueAlignment.ToolTip", "Alignment of the inspector value column."), ECkStyleLab_Group::EntityValues},
                {TEXT("ProviderChipStyle"), NSLOCTEXT("CkStyleLabAxisMetadata", "ProviderChipStyle.DisplayName", "Provider Chip Style"), NSLOCTEXT("CkStyleLabAxisMetadata", "ProviderChipStyle.ToolTip", "Treatment for overlay section provider chips."), ECkStyleLab_Group::TokensLegend},
                {TEXT("LegendMode"), NSLOCTEXT("CkStyleLabAxisMetadata", "LegendMode.DisplayName", "Legend Mode"), NSLOCTEXT("CkStyleLabAxisMetadata", "LegendMode.ToolTip", "Overlay legend strip: deduped across sections, off, or repeated per section."), ECkStyleLab_Group::TokensLegend},
                {TEXT("MergeCountDisplay"), NSLOCTEXT("CkStyleLabAxisMetadata", "MergeCountDisplay.DisplayName", "Merge Count Display"), NSLOCTEXT("CkStyleLabAxisMetadata", "MergeCountDisplay.ToolTip", "How a merged row's xN count renders. WHETHER merging happens stays a project setting."), ECkStyleLab_Group::TokensLegend},
                {TEXT("FlashOnChange"), NSLOCTEXT("CkStyleLabAxisMetadata", "FlashOnChange.DisplayName", "Flash On Change"), NSLOCTEXT("CkStyleLabAxisMetadata", "FlashOnChange.ToolTip", "Inspector value-change flash scope."), ECkStyleLab_Group::HierarchyEditing},
                {TEXT("SectionHeaderStyle"), NSLOCTEXT("CkStyleLabAxisMetadata", "SectionHeaderStyle.DisplayName", "Section Header Style"), NSLOCTEXT("CkStyleLabAxisMetadata", "SectionHeaderStyle.ToolTip", "Treatment for inspector and overlay section headers."), ECkStyleLab_Group::HierarchyEditing},
                {TEXT("IconSize"), NSLOCTEXT("CkStyleLabAxisMetadata", "IconSize.DisplayName", "Icon Size"), NSLOCTEXT("CkStyleLabAxisMetadata", "IconSize.ToolTip", "Glyph size for every SCkDebug_Icon site: Small 12, Medium 16, Large 20."), ECkStyleLab_Group::Icons},
                {TEXT("EditControlStyle"), NSLOCTEXT("CkStyleLabAxisMetadata", "EditControlStyle.DisplayName", "Edit Control Style"), NSLOCTEXT("CkStyleLabAxisMetadata", "EditControlStyle.ToolTip", "Inspector edit affordances: always inline, revealed on row hover, or hidden (fully read-only inspectors)."), ECkStyleLab_Group::HierarchyEditing},
                {TEXT("TreeComplexity"), NSLOCTEXT("CkStyleLabAxisMetadata", "TreeComplexity.DisplayName", "Tree Complexity"), NSLOCTEXT("CkStyleLabAxisMetadata", "TreeComplexity.ToolTip", "ECS entity tree declutter dial. Minimal folds technical rows harder and tightens the sibling-group threshold; Full turns folding and grouping off. Modulates the project's own tree settings — it never replaces them."), ECkStyleLab_Group::HierarchyEditing},
                {TEXT("IconTreatment"), NSLOCTEXT("CkStyleLabAxisMetadata", "IconTreatment.DisplayName", "Icon Treatment"), NSLOCTEXT("CkStyleLabAxisMetadata", "IconTreatment.ToolTip", "Backdrop behind every SCkDebug_Icon glyph: bare, a tinted well, or a thin ring."), ECkStyleLab_Group::Icons},
                {TEXT("TextScale"), NSLOCTEXT("CkStyleLabAxisMetadata", "TextScale.DisplayName", "Text Scale"), NSLOCTEXT("CkStyleLabAxisMetadata", "TextScale.ToolTip", "Type-size multiplier applied on top of the palette's font-size roles: Normal x1.0, Small x0.875, Large x1.125."), ECkStyleLab_Group::WorkbenchSurfaces},
                {TEXT("EntityRefStyle"), NSLOCTEXT("CkStyleLabAxisMetadata", "EntityRefStyle.DisplayName", "Entity Ref Style"), NSLOCTEXT("CkStyleLabAxisMetadata", "EntityRefStyle.ToolTip", "Visual treatment of the entity reference pill. What TEXT it composes stays with Entity Id Style."), ECkStyleLab_Group::EntityValues},
                {TEXT("CornerStyle"), NSLOCTEXT("CkStyleLabAxisMetadata", "CornerStyle.DisplayName", "Corner Style"), NSLOCTEXT("CkStyleLabAxisMetadata", "CornerStyle.ToolTip", "Corner shape for chips, badges, and cards."), ECkStyleLab_Group::WorkbenchSurfaces},
                {TEXT("SurfaceElevation"), NSLOCTEXT("CkStyleLabAxisMetadata", "SurfaceElevation.DisplayName", "Pane Treatment"), NSLOCTEXT("CkStyleLabAxisMetadata", "SurfaceElevation.ToolTip", "Cards use separated layered panes; Workbench uses contiguous flat panes."), ECkStyleLab_Group::WorkbenchSurfaces},
                {TEXT("GraphNodeStyle"), NSLOCTEXT("CkStyleLabAxisMetadata", "GraphNodeStyle.DisplayName", "Graph Node Style"), NSLOCTEXT("CkStyleLabAxisMetadata", "GraphNodeStyle.ToolTip", "Border weight and inactive fade for debug graph nodes (state machine, GOAP)."), ECkStyleLab_Group::GraphTelemetry},
                {TEXT("GraphMotion"), NSLOCTEXT("CkStyleLabAxisMetadata", "GraphMotion.DisplayName", "Graph Motion"), NSLOCTEXT("CkStyleLabAxisMetadata", "GraphMotion.ToolTip", "Pacing for state-machine graph state and transition feedback: quick, measured, or deliberate."), ECkStyleLab_Group::GraphTelemetry},
                {TEXT("GraphEventEmphasis"), NSLOCTEXT("CkStyleLabAxisMetadata", "GraphEventEmphasis.DisplayName", "Graph Event Emphasis"), NSLOCTEXT("CkStyleLabAxisMetadata", "GraphEventEmphasis.ToolTip", "Outline and edge-flash strength for state-machine graph events: subtle, clear, or bold."), ECkStyleLab_Group::GraphTelemetry},
                {TEXT("RowBanding"), NSLOCTEXT("CkStyleLabAxisMetadata", "RowBanding.DisplayName", "Row Banding"), NSLOCTEXT("CkStyleLabAxisMetadata", "RowBanding.ToolTip", "Row-to-row separation in list and tree surfaces: none, alternating fills, or a per-row rule."), ECkStyleLab_Group::WorkbenchSurfaces},
            };
            return AxisMetadata;
        }

        #define CK_STYLE_LAB_AXIS_OPTION(Property, Value, Label) \
            {TEXT(#Property), Value, NSLOCTEXT("CkStyleLabAxisMetadata", #Property "." #Value, Label)}

        auto Get_AxisOptionMetadata() -> const TArray<FCkStyleLab_AxisOptionMetadata>&
        {
            static const auto AxisOptionMetadata = TArray<FCkStyleLab_AxisOptionMetadata>{
                CK_STYLE_LAB_AXIS_OPTION(ChipStyle, 0, "Tint"), CK_STYLE_LAB_AXIS_OPTION(ChipStyle, 1, "Solid"), CK_STYLE_LAB_AXIS_OPTION(ChipStyle, 2, "Outline"), CK_STYLE_LAB_AXIS_OPTION(ChipStyle, 3, "Text Only"),
                CK_STYLE_LAB_AXIS_OPTION(RowDensity, 0, "Comfortable"), CK_STYLE_LAB_AXIS_OPTION(RowDensity, 1, "Compact"), CK_STYLE_LAB_AXIS_OPTION(RowDensity, 2, "Airy"),
                CK_STYLE_LAB_AXIS_OPTION(EntityIdStyle, 0, "Name And Id"), CK_STYLE_LAB_AXIS_OPTION(EntityIdStyle, 1, "Compact Id"), CK_STYLE_LAB_AXIS_OPTION(EntityIdStyle, 2, "Name Only"),
                CK_STYLE_LAB_AXIS_OPTION(BadgeStyle, 0, "Solid"), CK_STYLE_LAB_AXIS_OPTION(BadgeStyle, 1, "Hollow"), CK_STYLE_LAB_AXIS_OPTION(BadgeStyle, 2, "Count Only"),
                CK_STYLE_LAB_AXIS_OPTION(SeparatorWeight, 0, "Hairline"), CK_STYLE_LAB_AXIS_OPTION(SeparatorWeight, 1, "None"), CK_STYLE_LAB_AXIS_OPTION(SeparatorWeight, 2, "Standard"), CK_STYLE_LAB_AXIS_OPTION(SeparatorWeight, 3, "Heavy"),
                CK_STYLE_LAB_AXIS_OPTION(FoldChipStyle, 0, "Chip"), CK_STYLE_LAB_AXIS_OPTION(FoldChipStyle, 1, "Text"), CK_STYLE_LAB_AXIS_OPTION(FoldChipStyle, 2, "Minimal"),
                CK_STYLE_LAB_AXIS_OPTION(ValueAlignment, 0, "Left"), CK_STYLE_LAB_AXIS_OPTION(ValueAlignment, 1, "Right"), CK_STYLE_LAB_AXIS_OPTION(ValueAlignment, 2, "Aligned Columns"),
                CK_STYLE_LAB_AXIS_OPTION(ProviderChipStyle, 0, "Tint"), CK_STYLE_LAB_AXIS_OPTION(ProviderChipStyle, 1, "Solid"), CK_STYLE_LAB_AXIS_OPTION(ProviderChipStyle, 2, "Abbrev Only"),
                CK_STYLE_LAB_AXIS_OPTION(LegendMode, 0, "Deduped"), CK_STYLE_LAB_AXIS_OPTION(LegendMode, 1, "Off"), CK_STYLE_LAB_AXIS_OPTION(LegendMode, 2, "Per Section"),
                CK_STYLE_LAB_AXIS_OPTION(MergeCountDisplay, 0, "Suffix Text"), CK_STYLE_LAB_AXIS_OPTION(MergeCountDisplay, 1, "Count Badge"), CK_STYLE_LAB_AXIS_OPTION(MergeCountDisplay, 2, "Hidden"),
                CK_STYLE_LAB_AXIS_OPTION(FlashOnChange, 0, "Off"), CK_STYLE_LAB_AXIS_OPTION(FlashOnChange, 1, "Value"), CK_STYLE_LAB_AXIS_OPTION(FlashOnChange, 2, "Row"),
                CK_STYLE_LAB_AXIS_OPTION(SectionHeaderStyle, 0, "Uppercase"), CK_STYLE_LAB_AXIS_OPTION(SectionHeaderStyle, 1, "Mixed"), CK_STYLE_LAB_AXIS_OPTION(SectionHeaderStyle, 2, "Minimal"),
                CK_STYLE_LAB_AXIS_OPTION(IconSize, 0, "Medium"), CK_STYLE_LAB_AXIS_OPTION(IconSize, 1, "Small"), CK_STYLE_LAB_AXIS_OPTION(IconSize, 2, "Large"),
                CK_STYLE_LAB_AXIS_OPTION(EditControlStyle, 0, "Inline"), CK_STYLE_LAB_AXIS_OPTION(EditControlStyle, 1, "On Hover"), CK_STYLE_LAB_AXIS_OPTION(EditControlStyle, 2, "Hidden"),
                CK_STYLE_LAB_AXIS_OPTION(TreeComplexity, 0, "Normal"), CK_STYLE_LAB_AXIS_OPTION(TreeComplexity, 1, "Minimal"), CK_STYLE_LAB_AXIS_OPTION(TreeComplexity, 2, "Full"),
                CK_STYLE_LAB_AXIS_OPTION(IconTreatment, 0, "Plain"), CK_STYLE_LAB_AXIS_OPTION(IconTreatment, 1, "Well"), CK_STYLE_LAB_AXIS_OPTION(IconTreatment, 2, "Ring"),
                CK_STYLE_LAB_AXIS_OPTION(TextScale, 0, "Normal"), CK_STYLE_LAB_AXIS_OPTION(TextScale, 1, "Small"), CK_STYLE_LAB_AXIS_OPTION(TextScale, 2, "Large"),
                CK_STYLE_LAB_AXIS_OPTION(EntityRefStyle, 0, "Flat"), CK_STYLE_LAB_AXIS_OPTION(EntityRefStyle, 1, "Pill"), CK_STYLE_LAB_AXIS_OPTION(EntityRefStyle, 2, "Outline Pill"), CK_STYLE_LAB_AXIS_OPTION(EntityRefStyle, 3, "Monochrome"),
                CK_STYLE_LAB_AXIS_OPTION(CornerStyle, 0, "Rounded"), CK_STYLE_LAB_AXIS_OPTION(CornerStyle, 1, "Sharp"), CK_STYLE_LAB_AXIS_OPTION(CornerStyle, 2, "Pill"),
                CK_STYLE_LAB_AXIS_OPTION(SurfaceElevation, 0, "Cards"), CK_STYLE_LAB_AXIS_OPTION(SurfaceElevation, 1, "Workbench"),
                CK_STYLE_LAB_AXIS_OPTION(GraphNodeStyle, 0, "Card"), CK_STYLE_LAB_AXIS_OPTION(GraphNodeStyle, 1, "Minimal"), CK_STYLE_LAB_AXIS_OPTION(GraphNodeStyle, 2, "Dense"),
                CK_STYLE_LAB_AXIS_OPTION(GraphMotion, 0, "Quick"), CK_STYLE_LAB_AXIS_OPTION(GraphMotion, 1, "Measured"), CK_STYLE_LAB_AXIS_OPTION(GraphMotion, 2, "Deliberate"),
                CK_STYLE_LAB_AXIS_OPTION(GraphEventEmphasis, 0, "Subtle"), CK_STYLE_LAB_AXIS_OPTION(GraphEventEmphasis, 1, "Clear"), CK_STYLE_LAB_AXIS_OPTION(GraphEventEmphasis, 2, "Bold"),
                CK_STYLE_LAB_AXIS_OPTION(RowBanding, 0, "Off"), CK_STYLE_LAB_AXIS_OPTION(RowBanding, 1, "Zebra"), CK_STYLE_LAB_AXIS_OPTION(RowBanding, 2, "Hairline"),
            };
            return AxisOptionMetadata;
        }

        #undef CK_STYLE_LAB_AXIS_OPTION
    }

    auto Get_GroupMetadata() -> const TArray<FCkStyleLab_GroupMetadata>&
    {
        return Get_Groups();
    }

    auto Find_AxisMetadata(const FName InPropertyName) -> const FCkStyleLab_AxisMetadata*
    {
        return Get_AxisMetadata().FindByPredicate([InPropertyName](const FCkStyleLab_AxisMetadata& InMetadata)
        {
            return InMetadata.PropertyName == InPropertyName;
        });
    }

    auto Find_AxisOptionLabel(const FName InPropertyName, const int64 InValue) -> const FText*
    {
        if (const auto* Metadata = Get_AxisOptionMetadata().FindByPredicate(
                [InPropertyName, InValue](const FCkStyleLab_AxisOptionMetadata& InMetadata)
                {
                    return InMetadata.PropertyName == InPropertyName && InMetadata.Value == InValue;
                }))
        {
            return &Metadata->DisplayName;
        }
        return nullptr;
    }
} // namespace ck::style_lab
