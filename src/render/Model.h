#pragma once

#include <glm.hpp>
#include <vector>
#include <optional>
#include <unordered_map>
#include <algorithm>
#include "Types.h"

// Tools available in the editor.
enum class Tool { Select, Line, Poly, RegularPoly };

// Single original line.
struct Line
{
    Id id{ 0 };
    glm::vec2 a{}, b{};
    Color color{};
    float thicknessPx{ 3.f };

    // Transform chain config per original line.
    int koch2Iters{ 0 };
    int dragonIters{ 0 };

    // Effect cache (expanded polyline).
    bool dirty{ true };
    std::vector<glm::vec2> effect;

    // Owning group (regular or arbitrary). Resolved during lookups.
    Id groupId{ 0 };
};

// Regular polygon group: shared params drive its edge lines.
struct RegularPolyGroup
{
    Id id{ 0 }; // Group ID (separate from line IDs).
    std::vector<Id> lineIds; // Edges in document order.
    glm::vec2 center{ 0,0 };
    float radius{ 0.f };
    int sides{ 0 };
    float rotationDeg{ 0.f }; // Degrees.
};

// Arbitrary polygon group: keeps edges together as a shape.
struct ArbitraryPolyGroup
{
    Id id{ 0 };
    std::vector<Id> lineIds; // Edges in order or insertion order.
};

// All document state.
struct Document
{
    std::vector<Line> originals;
    std::vector<RegularPolyGroup> regPolys;
    std::vector<ArbitraryPolyGroup> arbPolys;

    Id nextId{ 1 };
    Id nextGroupId{ 1000000 };

    std::vector<Id> selection;

    // View.
    glm::vec2 camCenter{ 0,0 };
    float camZoom{ 1.f };
};

// ----------Line Helpers----------
inline Line* findLine(Document& d, Id id)
{
    for (auto& l : d.originals) if (l.id == id) return &l;
    return nullptr;
}

inline const Line* findLine(const Document& d, Id id)
{
    for (const auto& l : d.originals) if (l.id == id) return &l;
    return nullptr;
}

// ----------Regular Poly Group Helpers----------
inline RegularPolyGroup* findRegPoly(Document& d, Id groupId)
{
    for (auto& g : d.regPolys) if (g.id == groupId) return &g;
    return nullptr;
}

inline const RegularPolyGroup* findRegPoly(const Document& d, Id groupId)
{
    for (const auto& g : d.regPolys) if (g.id == groupId) return &g;
    return nullptr;
}

inline RegularPolyGroup* findRegPolyByLine(Document& d, Id lineId)
{
    if (auto* l = findLine(d, lineId))
    {
        if (l->groupId)
        {
            for (auto& g : d.regPolys) if (g.id == l->groupId) return &g;
        }
    }

    for (auto& g : d.regPolys)
    {
        if (std::find(g.lineIds.begin(), g.lineIds.end(), lineId) != g.lineIds.end()) return &g;
    }

    return nullptr;
}

inline const RegularPolyGroup* findRegPolyByLine(const Document& d, Id lineId)
{
    if (auto* l = findLine(d, lineId))
    {
        if (l->groupId)
        {
            for (const auto& g : d.regPolys) if (g.id == l->groupId) return &g;
        }
    }

    for (const auto& g : d.regPolys)
    {
        if (std::find(g.lineIds.begin(), g.lineIds.end(), lineId) != g.lineIds.end()) return &g;
    }

    return nullptr;
}

// ----------Arbitrary Poly Group Helpers----------
inline ArbitraryPolyGroup* findArbPoly(Document& d, Id groupId)
{
    for (auto& g : d.arbPolys) if (g.id == groupId) return &g;
    return nullptr;
}

inline const ArbitraryPolyGroup* findArbPoly(const Document& d, Id groupId)
{
    for (const auto& g : d.arbPolys) if (g.id == groupId) return &g;
    return nullptr;
}

inline ArbitraryPolyGroup* findArbPolyByLine(Document& d, Id lineId)
{
    if (auto* l = findLine(d, lineId))
    {
        if (l->groupId)
        {
            for (auto& g : d.arbPolys) if (g.id == l->groupId) return &g;
        }
    }

    for (auto& g : d.arbPolys)
    {
        if (std::find(g.lineIds.begin(), g.lineIds.end(), lineId) != g.lineIds.end()) return &g;
    }

    return nullptr;
}

inline const ArbitraryPolyGroup* findArbPolyByLine(const Document& d, Id lineId)
{
    if (auto* l = findLine(d, lineId))
    {
        if (l->groupId)
        {
            for (const auto& g : d.arbPolys) if (g.id == l->groupId) return &g;
        }
    }

    for (const auto& g : d.arbPolys)
    {
        if (std::find(g.lineIds.begin(), g.lineIds.end(), lineId) != g.lineIds.end()) return &g;
    }

    return nullptr;
}

// ----------Selection Utilities----------
inline bool isSelected(const Document& d, Id id)
{
    return std::find(d.selection.begin(), d.selection.end(), id) != d.selection.end();
}

inline void clearSelection(Document& d)
{
    d.selection.clear();
}

inline void setSingleSelection(Document& d, Id id)
{
    d.selection.assign(1, id);
}

inline void toggleSelection(Document& d, Id id)
{
    auto it = std::find(d.selection.begin(), d.selection.end(), id);

    if (it == d.selection.end()) d.selection.push_back(id);
    else d.selection.erase(it);
}