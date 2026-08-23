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
    auto Result = FCkAiDebugger_Topology{};
    for (const auto& Section : InModel.Sections)
    {
        const auto Category = ck_ai_debugger_evidence::Get_Category(Section.ProviderTag.ToString());
        if (Category != TEXT("GOAP") && Category != TEXT("STATE")) { continue; }

        auto Node = FCkAiDebugger_TopologyNode{};
        Node.SourceEntityId = Section.SourceEntityId;
        Node.ParentSourceEntityId = Section.ParentSourceEntityId;
        Node.Depth = Section.SourceDepth;
        Node.SourceOrder = Section.SourceOrder;
        Node.Name = Section.SourceName.IsEmpty() ? ck_ai_debugger_evidence::Get_Leaf(Section.ProviderTag.ToString()) : Section.SourceName.ToString();
        Node.Detail = ck_ai_debugger_evidence::Compose_TopologyDetail(Section);
        Node.Tone = ck_ai_debugger_evidence::To_Tone(ck_debugoverlay::Get_MaxSeverity(Section.Rows));
        Node.State = ck_ai_debugger_evidence::Get_TopologyState(Node.Tone);
        Node.StableKey = FString::Printf(TEXT("%s|%u|%u|%d"), *Section.ProviderTag.ToString(), Node.SourceEntityId, Node.ParentSourceEntityId, Node.Depth);
        if (Category == TEXT("GOAP")) { Result.Goaps.Add(MoveTemp(Node)); }
        else { Result.StateMachines.Add(MoveTemp(Node)); }
    }

    const auto SortNodes = [](TArray<FCkAiDebugger_TopologyNode>& InNodes)
    {
        InNodes.Sort([](const auto& InLeft, const auto& InRight)
        {
            if (InLeft.SourceOrder != InRight.SourceOrder) { return InLeft.SourceOrder < InRight.SourceOrder; }
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
