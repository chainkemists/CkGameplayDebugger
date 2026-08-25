#include "CkAiDebugger/Model/CkAiDebugger_EvidenceModel.h"

#include "Algo/Sort.h"

namespace ck_ai_debugger_evidence
{
    struct FSectionRef
    {
        const FCk_DebugOverlay_Section* Section = nullptr;
        int32 OriginalIndex = 0;
    };

    auto Get_Leaf(const FString& InTag) -> FString
    {
        FString Left;
        FString Right;
        return InTag.Split(TEXT("."), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ? Right : InTag;
    }

    auto Get_Category(const FString& InProvider) -> FString
    {
        if (InProvider.Contains(TEXT(".Goap"))) { return TEXT("GOAP"); }
        if (InProvider.Contains(TEXT(".StateMachine"))) { return TEXT("STATE"); }
        if (InProvider.Contains(TEXT(".PathNetwork")) || InProvider.Contains(TEXT(".AStar"))) { return TEXT("NAV"); }
        if (InProvider.Contains(TEXT(".Crowd"))) { return TEXT("CROWD"); }
        return TEXT("AI");
    }

    auto Get_CategoryPriority(const FString& InCategory) -> int32
    {
        if (InCategory == TEXT("GOAP")) { return 0; }
        if (InCategory == TEXT("STATE")) { return 1; }
        if (InCategory == TEXT("NAV")) { return 2; }
        if (InCategory == TEXT("CROWD")) { return 3; }
        return 4;
    }

    auto To_Tone(ECk_DebugOverlay_Severity InSeverity) -> ECk_Tone
    {
        switch (InSeverity)
        {
            case ECk_DebugOverlay_Severity::Good: return ECk_Tone::Ok;
            case ECk_DebugOverlay_Severity::Warn: return ECk_Tone::Warn;
            case ECk_DebugOverlay_Severity::Bad: return ECk_Tone::Err;
            default: return ECk_Tone::Neutral;
        }
    }

    auto Get_Headline(const FString& InCategory, const FString& InFieldTag) -> FString
    {
        const auto Field = Get_Leaf(InFieldTag);
        if (InCategory == TEXT("GOAP")) { return Field == TEXT("Plan") ? TEXT("GOAP plan") : FString::Printf(TEXT("GOAP %s"), *Field); }
        if (InCategory == TEXT("STATE")) { return Field == TEXT("State") ? TEXT("State machine state") : FString::Printf(TEXT("State machine %s"), *Field); }
        if (InCategory == TEXT("NAV")) { return Field == TEXT("Path") ? TEXT("Navigation path") : FString::Printf(TEXT("Navigation %s"), *Field); }
        if (InCategory == TEXT("CROWD")) { return FString::Printf(TEXT("Crowd %s"), *Field); }
        return Field;
    }

    auto Get_SourceDetail(const FCk_DebugOverlay_Section& InSection) -> FString
    {
        auto Topology = FString::Printf(TEXT("depth %d"), InSection.SourceDepth);
        if (InSection.ParentSourceEntityId != 0)
        { Topology += FString::Printf(TEXT(" · parent #%u"), InSection.ParentSourceEntityId); }
        if (NOT InSection.SourceName.IsEmpty())
        {
            const auto Source = InSection.SourceEntityId != 0
                ? FString::Printf(TEXT("%s #%u"), *InSection.SourceName.ToString(), InSection.SourceEntityId)
                : InSection.SourceName.ToString();
            return Source + TEXT(" · ") + Topology;
        }
        const auto Source = InSection.SourceEntityId != 0 ? FString::Printf(TEXT("Entity #%u"), InSection.SourceEntityId) : TEXT("Selected entity");
        return Source + TEXT(" · ") + Topology;
    }

    auto Compose_Detail(const FCk_DebugOverlay_Section& InSection, const FCk_DebugOverlay_Row& InRow) -> FString
    {
        auto Parts = TArray<FString>{Get_SourceDetail(InSection), Get_Leaf(InRow.FieldTag.ToString()), InRow.Value.ToString()};
        if (InRow.MergedCount > 1) { Parts.Add(FString::Printf(TEXT("%d merged"), InRow.MergedCount)); }
        if (NOT InRow.ExplicitHistory.IsEmpty())
        {
            auto History = TArray<FString>{};
            for (const auto& Entry : InRow.ExplicitHistory) { History.Add(Entry.ToString()); }
            Parts.Add(FString::Printf(TEXT("history: %s"), *FString::Join(History, TEXT(" → "))));
        }
        return FString::Join(Parts, TEXT(" · "));
    }

    auto Make_EventMessage(const TCHAR* InVerb, const FCkAiDebugger_EvidenceFact& InFact) -> FString
    {
        return FString::Printf(TEXT("%s · %s · %s: %s"),
            InVerb, *InFact.SourceLabel, *InFact.Headline, *InFact.DisplayValue);
    }

    auto Get_TopologyState(ECk_Tone InTone) -> FString
    {
        if (InTone == ECk_Tone::Err) { return TEXT("faulted"); }
        if (InTone == ECk_Tone::Warn) { return TEXT("attention"); }
        if (InTone == ECk_Tone::Ok) { return TEXT("running"); }
        return TEXT("observed");
    }

    // Request_CreateEntity stamps every entity with this sentinel until something renames it.
    // Sub-state-machine and sub-planner entities are never renamed, so the sentinel reaches the
    // hierarchy as a row title that identifies nothing.
    auto Is_UnnamedSource(const FString& InSourceName) -> bool
    {
        return InSourceName.IsEmpty() || InSourceName.StartsWith(TEXT("NO NAME"));
    }

    auto Get_InstanceName(
        const FCk_DebugOverlay_Section& InSection,
        const TCHAR*                    InUnnamedKind) -> FString
    {
        const auto SourceName = InSection.SourceName.ToString();
        if (NOT Is_UnnamedSource(SourceName))
        { return SourceName; }

        // The entity id is the only thing that distinguishes two unnamed siblings, so it is
        // promoted into the visible title rather than left in the copy text.
        return FString::Printf(TEXT("%s #%u"), InUnnamedKind, InSection.SourceEntityId);
    }

    auto Find_Row(
        const FCk_DebugOverlay_Section& InSection,
        const TCHAR*                    InFieldLeaf) -> const FCk_DebugOverlay_Row*
    {
        return InSection.Rows.FindByPredicate([InFieldLeaf](const FCk_DebugOverlay_Row& InRow)
        { return Get_Leaf(InRow.FieldTag.ToString()) == InFieldLeaf; });
    }

    auto Get_RowValue(
        const FCk_DebugOverlay_Section& InSection,
        const TCHAR*                    InFieldLeaf) -> FString
    {
        const auto* Row = Find_Row(InSection, InFieldLeaf);
        return Row != nullptr ? Row->Value.ToString() : FString{};
    }

    // A chain level is emitted by the State Machine provider as a "> "-prefixed State row. The
    // prefix count IS the nesting level, so it is consumed here rather than rendered.
    auto Split_ChainPrefix(const FString& InValue, int32& OutLevel) -> FString
    {
        auto Remaining = InValue;
        OutLevel = 0;
        while (Remaining.StartsWith(TEXT("> ")))
        {
            ++OutLevel;
            Remaining = Remaining.RightChop(2);
        }
        return Remaining;
    }

    auto Get_ChainFromRow(const FCk_DebugOverlay_Row* InRow) -> TArray<FString>
    {
        auto Chain = TArray<FString>{};
        if (InRow == nullptr)
        { return Chain; }

        // ExplicitHistory is the untruncated form when the provider supplies one; the value
        // string is a display summary that may have been capped.
        if (NOT InRow->ExplicitHistory.IsEmpty())
        {
            for (const auto& Entry : InRow->ExplicitHistory) { Chain.Add(Entry.ToString()); }
            return Chain;
        }

        const auto Value = InRow->Value.ToString();
        if (Value.IsEmpty()) { return Chain; }
        Value.ParseIntoArray(Chain, TEXT(" > "), true);
        return Chain;
    }

    auto Compose_TopologyDetail(const FCk_DebugOverlay_Section& InSection) -> FString
    {
        auto Rows = TArray<FString>{};
        Rows.Reserve(InSection.Rows.Num());
        for (const auto& Row : InSection.Rows)
        {
            auto Detail = FString::Printf(TEXT("%s: %s"), *Get_Leaf(Row.FieldTag.ToString()), *Row.Value.ToString());
            if (Row.MergedCount > 1) { Detail += FString::Printf(TEXT(" (%d merged)"), Row.MergedCount); }
            if (NOT Row.ExplicitHistory.IsEmpty())
            {
                auto History = TArray<FString>{};
                for (const auto& Entry : Row.ExplicitHistory) { History.Add(Entry.ToString()); }
                Detail += FString::Printf(TEXT(" [history: %s]"), *FString::Join(History, TEXT(" → ")));
            }
            Rows.Add(MoveTemp(Detail));
        }
        return Rows.IsEmpty() ? TEXT("No current rows") : FString::Join(Rows, TEXT(" · "));
    }
}

auto FCkAiDebugger_EvidenceKey::ToString() const -> FString
{
    return FString::Printf(TEXT("%s|%u|%s|%d"), *ProviderTag, SourceEntityId, *FieldTag, Occurrence);
}

bool FCkAiDebugger_EvidenceKey::operator==(const FCkAiDebugger_EvidenceKey& InOther) const
{
    return ProviderTag == InOther.ProviderTag
        && SourceEntityId == InOther.SourceEntityId
        && FieldTag == InOther.FieldTag
        && Occurrence == InOther.Occurrence;
}

auto ck::ai_debugger::evidence::Normalize(const FCk_DebugOverlay_EntityModel& InModel) -> TArray<FCkAiDebugger_EvidenceFact>
{
    auto Sections = TArray<ck_ai_debugger_evidence::FSectionRef>{};
    Sections.Reserve(InModel.Sections.Num());
    for (int32 Index = 0; Index < InModel.Sections.Num(); ++Index)
    { Sections.Add({&InModel.Sections[Index], Index}); }

    Sections.Sort([](const auto& InLeft, const auto& InRight)
    {
        const auto LeftCategory = ck_ai_debugger_evidence::Get_Category(InLeft.Section->ProviderTag.ToString());
        const auto RightCategory = ck_ai_debugger_evidence::Get_Category(InRight.Section->ProviderTag.ToString());
        const auto LeftPriority = ck_ai_debugger_evidence::Get_CategoryPriority(LeftCategory);
        const auto RightPriority = ck_ai_debugger_evidence::Get_CategoryPriority(RightCategory);
        if (LeftPriority != RightPriority) { return LeftPriority < RightPriority; }
        if (InLeft.Section->SourceOrder != InRight.Section->SourceOrder) { return InLeft.Section->SourceOrder < InRight.Section->SourceOrder; }
        if (InLeft.Section->ProviderTag != InRight.Section->ProviderTag) { return InLeft.Section->ProviderTag.ToString() < InRight.Section->ProviderTag.ToString(); }
        return InLeft.OriginalIndex < InRight.OriginalIndex;
    });

    auto Occurrences = TMap<FString, int32>{};
    auto Facts = TArray<FCkAiDebugger_EvidenceFact>{};
    for (const auto& SectionRef : Sections)
    {
        const auto& Section = *SectionRef.Section;
        const auto Provider = Section.ProviderTag.ToString();
        const auto Category = ck_ai_debugger_evidence::Get_Category(Provider);
        for (const auto& Row : Section.Rows)
        {
            const auto Field = Row.FieldTag.ToString();
            const auto OccurrenceBase = FString::Printf(TEXT("%s|%u|%s"), *Provider, Section.SourceEntityId, *Field);
            const auto Occurrence = Occurrences.FindOrAdd(OccurrenceBase)++;
            auto Fact = FCkAiDebugger_EvidenceFact{};
            Fact.Key = {Provider, Section.SourceEntityId, Field, Occurrence};
            Fact.StableKey = Fact.Key.ToString();
            Fact.Tone = ck_ai_debugger_evidence::To_Tone(Row.Severity);
            Fact.Category = Category;
            Fact.SourceLabel = Section.SourceName.IsEmpty()
                ? (Section.SourceEntityId == 0
                    ? TEXT("Selected entity")
                    : FString::Printf(TEXT("Entity #%u"), Section.SourceEntityId))
                : (Section.SourceEntityId == 0
                    ? Section.SourceName.ToString()
                    : FString::Printf(TEXT("%s #%u"), *Section.SourceName.ToString(), Section.SourceEntityId));
            Fact.Headline = ck_ai_debugger_evidence::Get_Headline(Category, Field);
            Fact.DisplayValue = Row.Value.ToString();
            Fact.Detail = ck_ai_debugger_evidence::Compose_Detail(Section, Row);
            Fact.CopyText = FString::Printf(TEXT("[%s] %s\n%s\nkey: %s"), *Fact.Category, *Fact.Headline, *Fact.Detail, *Fact.StableKey);
            Fact.SourceOrder = Section.SourceOrder;
            Fact.ValueState = FString::Printf(TEXT("%d|%s|%d|%s"), static_cast<int32>(Row.Severity), *Row.Value.ToString(), Row.MergedCount, *Fact.Detail);
            Facts.Add(MoveTemp(Fact));
        }
    }

    Facts.Sort([](const auto& InLeft, const auto& InRight)
    {
        const auto LeftSeverity = static_cast<int32>(InLeft.Tone);
        const auto RightSeverity = static_cast<int32>(InRight.Tone);
        if (LeftSeverity != RightSeverity) { return LeftSeverity > RightSeverity; }
        const auto LeftPriority = ck_ai_debugger_evidence::Get_CategoryPriority(InLeft.Category);
        const auto RightPriority = ck_ai_debugger_evidence::Get_CategoryPriority(InRight.Category);
        if (LeftPriority != RightPriority) { return LeftPriority < RightPriority; }
        if (InLeft.SourceOrder != InRight.SourceOrder) { return InLeft.SourceOrder < InRight.SourceOrder; }
        if (InLeft.Key.FieldTag != InRight.Key.FieldTag) { return InLeft.Key.FieldTag < InRight.Key.FieldTag; }
        if (InLeft.Key.Occurrence != InRight.Key.Occurrence) { return InLeft.Key.Occurrence < InRight.Key.Occurrence; }
        return InLeft.StableKey < InRight.StableKey;
    });
    return Facts;
}

auto ck::ai_debugger::evidence::NormalizeTopology(const FCk_DebugOverlay_EntityModel& InModel) -> FCkAiDebugger_Topology
{
    using namespace ck_ai_debugger_evidence;

    auto Result = FCkAiDebugger_Topology{};

    const auto Make_BaseNode = [](const FCk_DebugOverlay_Section& InSection) -> FCkAiDebugger_TopologyNode
    {
        auto Node = FCkAiDebugger_TopologyNode{};
        Node.SourceEntityId = InSection.SourceEntityId;
        Node.ParentSourceEntityId = InSection.ParentSourceEntityId;
        Node.Depth = InSection.SourceDepth;
        Node.SourceOrder = InSection.SourceOrder;
        Node.Detail = Compose_TopologyDetail(InSection);
        Node.Tone = To_Tone(ck_debugoverlay::Get_MaxSeverity(InSection.Rows));
        return Node;
    };

    for (const auto& Section : InModel.Sections)
    {
        const auto Category = Get_Category(Section.ProviderTag.ToString());
        if (Category != TEXT("GOAP") && Category != TEXT("STATE")) { continue; }

        if (Category == TEXT("GOAP"))
        {
            auto Node = Make_BaseNode(Section);
            Node.Name = Get_InstanceName(Section, TEXT("Planner"));

            // Active is what the planner is DOING. The status word alone reads the same for a
            // planner mid-plan and one that has nothing to run, so the action is the headline.
            const auto Active = Get_RowValue(Section, TEXT("Active"));
            Node.Headline = Active == TEXT("(none)") ? FString{} : Active;

            const auto Status = Get_RowValue(Section, TEXT("Status"));
            const auto Cost = Get_RowValue(Section, TEXT("Cost"));
            Node.Status = Status;
            if (NOT Node.Status.IsEmpty() && NOT Cost.IsEmpty())
            { Node.Status += FString::Printf(TEXT(" · cost %s"), *Cost); }

            const auto* PlanRow = Find_Row(Section, TEXT("Plan"));
            auto Plan = Get_ChainFromRow(PlanRow);
            if (Plan.Num() == 1 && Plan[0] == TEXT("(no plan)")) { Plan.Reset(); }
            if (NOT Plan.IsEmpty())
            {
                Node.Chain = MoveTemp(Plan);
                Node.ChainLabel = TEXT("Plan");
            }

            Node.StableKey = FString::Printf(TEXT("%s|%u|%u|%d"),
                *Section.ProviderTag.ToString(), Node.SourceEntityId, Node.ParentSourceEntityId, Node.Depth);
            Result.Goaps.Add(MoveTemp(Node));
            continue;
        }

        // ---- State Machine: one node per nested level of this source's chain ----
        // The provider walks into every live sub-state-machine and emits a "> "-prefixed State row
        // per level. Collapsing that into one node is what hid the nesting; each level becomes its
        // own row, indented by the level it was found at.
        const auto InstanceName = Get_InstanceName(Section, TEXT("State machine"));
        const auto* TrailRow = Find_Row(Section, TEXT("History"));
        const auto Trail = Get_ChainFromRow(TrailRow);

        auto EmittedLevels = 0;
        for (const auto& Row : Section.Rows)
        {
            if (Get_Leaf(Row.FieldTag.ToString()) != TEXT("State")) { continue; }

            auto ChainLevel = 0;
            const auto StateName = Split_ChainPrefix(Row.Value.ToString(), ChainLevel);

            auto Node = Make_BaseNode(Section);
            Node.ChainLevel = ChainLevel;
            Node.Depth = Section.SourceDepth + ChainLevel;
            Node.Name = ChainLevel == 0
                ? InstanceName
                : FString::Printf(TEXT("%s ▸ sub"), *InstanceName);
            Node.Headline = StateName;
            Node.Tone = To_Tone(Row.Severity);
            Node.Status = Get_TopologyState(Node.Tone);

            // The transition trail belongs to the source entity as a whole, so it hangs off the
            // level that IS that entity rather than repeating down every nested level.
            if (ChainLevel == 0 && NOT Trail.IsEmpty())
            {
                Node.Chain = Trail;
                Node.ChainLabel = TEXT("Trail");
            }

            Node.StableKey = FString::Printf(TEXT("%s|%u|%u|%d|%d"),
                *Section.ProviderTag.ToString(), Node.SourceEntityId, Node.ParentSourceEntityId,
                Node.Depth, EmittedLevels);
            Result.StateMachines.Add(MoveTemp(Node));
            ++EmittedLevels;
        }

        // A source that collected no State row is still a live instance; retaining it is the
        // contract that keeps every nested runtime instance visible.
        if (EmittedLevels == 0)
        {
            auto Node = Make_BaseNode(Section);
            Node.Name = InstanceName;
            Node.Status = Get_TopologyState(Node.Tone);
            Node.StableKey = FString::Printf(TEXT("%s|%u|%u|%d|0"),
                *Section.ProviderTag.ToString(), Node.SourceEntityId, Node.ParentSourceEntityId, Node.Depth);
            Result.StateMachines.Add(MoveTemp(Node));
        }
    }

    const auto SortNodes = [](TArray<FCkAiDebugger_TopologyNode>& InNodes)
    {
        InNodes.Sort([](const auto& InLeft, const auto& InRight)
        {
            if (InLeft.SourceOrder != InRight.SourceOrder) { return InLeft.SourceOrder < InRight.SourceOrder; }
            if (InLeft.ChainLevel != InRight.ChainLevel) { return InLeft.ChainLevel < InRight.ChainLevel; }
            return InLeft.StableKey < InRight.StableKey;
        });
    };
    SortNodes(Result.Goaps);
    SortNodes(Result.StateMachines);
    return Result;
}

auto FCkAiDebugger_EvidenceDeltaTracker::Observe(const TArray<FCkAiDebugger_EvidenceFact>& InCurrent, double InTimeSeconds) -> TArray<FCkAiDebugger_EvidenceEvent>
{
    auto Current = TMap<FString, FCkAiDebugger_EvidenceFact>{};
    for (const auto& Fact : InCurrent) { Current.Add(Fact.StableKey, Fact); }

    if (NOT _HasSeeded)
    {
        _Previous = MoveTemp(Current);
        _HasSeeded = true;
        return {};
    }

    auto Events = TArray<FCkAiDebugger_EvidenceEvent>{};
    for (const auto& [Key, Fact] : Current)
    {
        const auto* Previous = _Previous.Find(Key);
        if (Previous == nullptr)
        { Events.Add({Key, Fact.Tone, Fact.Category, ck_ai_debugger_evidence::Make_EventMessage(TEXT("Added"), Fact), InTimeSeconds}); }
        else if (Previous->ValueState != Fact.ValueState)
        { Events.Add({Key, Fact.Tone, Fact.Category, ck_ai_debugger_evidence::Make_EventMessage(TEXT("Changed"), Fact), InTimeSeconds}); }
    }
    for (const auto& [Key, Fact] : _Previous)
    {
        if (NOT Current.Contains(Key))
        { Events.Add({Key, ECk_Tone::Ok, Fact.Category, ck_ai_debugger_evidence::Make_EventMessage(TEXT("Resolved"), Fact), InTimeSeconds}); }
    }
    Events.Sort([](const auto& InLeft, const auto& InRight) { return InLeft.StableKey < InRight.StableKey; });
    _Previous = MoveTemp(Current);
    return Events;
}

auto FCkAiDebugger_EvidenceDeltaTracker::Reset() -> void
{
    _Previous.Reset();
    _HasSeeded = false;
}
