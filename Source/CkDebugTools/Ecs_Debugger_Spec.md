# ECS Debugger Framework - Technical Implementation Spec

## Architecture Overview
Replacing ImGui/Cog-based debugging tools with professional Slate-based debug windows. Each tool is an independent nomad tab that can float, dock, and operate separately. Framework based on CueToolbox pattern but generalized for ECS debugging.

## Core Framework Components

### Module Structure
```cpp
class FCkEcsDebuggerModule : public IModuleInterface
{
    // Registers individual tool tab spawners
    // Handles toolbar integration
    // Manages style initialization
    auto DoRegisterToolTabSpawner(const FName& TabName, const FText& DisplayName, 
                                  TFunction<TSharedRef<SWidget>()> WidgetFactory) -> void;
};
```

### Base Debug Tool Widget
```cpp
class SCkEcsDebugTool_Base : public SCompoundWidget
{
    // Common panels all tools need
    auto DoCreateEntitySelectionPanel() -> TSharedRef<SWidget>;     // Entity picker/display
    auto DoCreateValidationPanel() -> TSharedRef<SWidget>;          // Validation status
    auto DoCreateSearchPanel() -> TSharedRef<SWidget>;              // Search/filter UI
    auto DoCreateActionPanel() -> TSharedRef<SWidget>;              // Common actions
    
    // Integration with existing ECS subsystem
    auto DoUpdateFromEntitySelection() -> void;                     // React to entity changes
    auto DoRefreshData() -> void;                                   // Refresh tool data
    
    // Virtual methods for derived tools to implement
    virtual auto DoCreateContentPanel() -> TSharedRef<SWidget> = 0; // Tool-specific content
    virtual auto DoGetToolName() -> FText = 0;                     // Tool display name
    virtual auto DoGetToolIcon() -> FSlateIcon = 0;                // Tool icon
};
```

### Data Discovery Base
```cpp
class FCkEcsDebuggerDiscovery_Base
{
    // Common data patterns
    auto Request_RefreshFromEntities(const TArray<FCk_Handle>& Entities) -> void;
    auto Request_ApplySearchFilter(const FString& SearchText) -> void;
    auto Request_ValidateData() -> void;
    auto Get_ValidationStats() -> TTuple<int32, int32, int32>; // total, errors, warnings
    
    // Virtual methods for specific data handling
    virtual auto DoDiscoverData() -> void = 0;
    virtual auto DoValidateData() -> void = 0;
};
```

### Style System
```cpp
class FCkEcsDebuggerStyle
{
    // Copy of FCkCueToolboxStyle with ECS debugger branding
    // Colors: AccentBlue, WarningOrange, DebugPurple, etc.
    // Fonts: Regular, Bold, Header
    // Button styles: Primary, Secondary, Small
    // Table styles: Normal rows, headers
};
```

## Implementation Requirements

### Single Class Override Rule
Each new debug tool should only require implementing ONE class:
```cpp
class SCkEntityDebugTool : public SCkEcsDebugTool_Base
{
    // ONLY these methods need implementation:
    auto DoCreateContentPanel() -> TSharedRef<SWidget> override;
    auto DoGetToolName() -> FText override { return FText::FromString(TEXT("Entity Inspector")); }
    auto DoGetToolIcon() -> FSlateIcon override { return FSlateIcon("EcsDebugger", "Entity.Icon"); }
    
    // Optional data discovery if needed:
    TSharedPtr<FCkEntityDiscovery> _EntityDiscovery;
};
```

### Integration Points
- Must integrate with existing `UCk_EcsDebugger_Subsystem_UE` for entity selection
- Must work alongside existing ImGui/Cog tools during transition
- Each tool registers as independent nomad tab spawner
- Tools can be opened/closed/docked independently

### Entity Debug Tool (First Implementation)
Replaces `FCk_EntityBasics_DebugWindow` functionality:
- Display basic entity information (handle, actor, etc.)
- Transform section with debug visualization
- Network information (NetMode, NetRole)
- Relationships (team, context owner, lifetime owner)
- Color-coded value display (entity=blue, vector=green, enum=yellow, etc.)

## Required Files to Implement

### Core Framework Files (need to create):
- `CkEcsDebuggerModule.h/.cpp` - Main module
- `CkEcsDebuggerStyle.h/.cpp` - Style system (copy from CkCueToolboxStyle)
- `CkEcsDebuggerCommands.h/.cpp` - Command system
- `SCkEcsDebugTool_Base.h/.cpp` - Base widget class
- `FCkEcsDebuggerDiscovery_Base.h/.cpp` - Base data discovery

### Entity Tool Files (first concrete implementation):
- `SCkEntityDebugTool.h/.cpp` - Entity inspector tool
- `FCkEntityDiscovery.h/.cpp` - Entity-specific data handling (if needed)

### Required Reference Files (need in chat):
- `Source/CkEcsDebugger/Public/CkEcsDebugger/Windows/Entity/CkEntity_DebugWindow.cpp` - Original entity debugger logic
- `Source/CkEcsDebugger/Public/CkEcsDebugger/Windows/Entity/CkEntity_DebugWindow.h` - Original entity debugger interface
- `CkCueToolbox.h/.cpp` - Reference architecture
- `CkCueToolboxStyle.h/.cpp` - Style reference to copy
- `CkCueToolboxModule.h/.cpp` - Module pattern reference
- `CkCueToolboxCommands.h/.cpp` - Commands pattern reference

### Integration Files (need access to):
- ECS Debugger Subsystem files for entity selection integration
- Any common ECS utility headers for entity operations

## Validation Criteria
1. Can create new debug tool with single class override
2. Entity debug tool shows all original functionality with professional UI
3. Independent tab spawning works for each tool
4. Integration with existing ECS debugger subsystem preserved
5. Professional styling consistent with CueToolbox
6. Framework can be extended to other tools (Abilities, Attributes, etc.)

## Color Coding System (from original)
- **Entity references**: Blue (#82b1ff)
- **Vectors/positions**: Green (#c3e88d)  
- **Numbers**: Pink (#f8bbd9)
- **Enums**: Yellow (#ffcc02)
- **Unknown values**: Red (#ff5722)
- **None/null values**: Gray (#666666)

## Next Steps
1. Create base framework files
2. Copy and adapt CueToolboxStyle as FCkEcsDebuggerStyle
3. Implement Entity debug tool using base framework
4. Test independent tab registration
5. Validate single-class override requirement
6. Prepare framework for additional tools (Abilities, Attributes, etc.)