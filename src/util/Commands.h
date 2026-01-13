#pragma once

#include <memory>
#include <vector>
#include <optional>
#include "../render/Model.h"

// Base command interface.
struct ICommand
{
    virtual ~ICommand() = default;
    virtual void apply(Document& doc) = 0;
    virtual void revert(Document& doc) = 0;
};

using ICommandPtr = std::unique_ptr<ICommand>;

// Undo/redo history (command stack).
struct History
{
    std::vector<ICommandPtr> undoStack, redoStack;

    void push(ICommandPtr cmd, Document& doc)
    {
        cmd->apply(doc);
        redoStack.clear();
        undoStack.push_back(std::move(cmd));
    }

    void undo(Document& doc)
    {
        if (undoStack.empty())
        {
            return;
        }
        auto cmd = std::move(undoStack.back());
        undoStack.pop_back();
        cmd->revert(doc);
        redoStack.push_back(std::move(cmd));
    }

    void redo(Document& doc)
    {
        if (redoStack.empty())
        {
            return;
        }
        auto cmd = std::move(redoStack.back());
        redoStack.pop_back();
        cmd->apply(doc);
        undoStack.push_back(std::move(cmd));
    }
};

// Create/Delete.
struct CmdCreateLine : ICommand
{
    Line line;
    size_t idx{ 0 };

    explicit CmdCreateLine(Line l)
        : line(std::move(l))
    {
    }

    void apply(Document& doc) override
    {
        idx = doc.originals.size();
        doc.originals.push_back(line);
    }

    void revert(Document& doc) override
    {
        if (idx < doc.originals.size())
        {
            doc.originals.erase(doc.originals.begin() + (ptrdiff_t)idx);
        }
    }
};

struct CmdDeleteLine : ICommand
{
    Id id{ 0 };
    Line backup;
    size_t idx{ 0 };

    explicit CmdDeleteLine(Id i) : id(i) {}

    void apply(Document& doc) override
    {
        for (size_t k = 0; k < doc.originals.size(); ++k)
        {
            if (doc.originals[k].id == id)
            {
                backup = doc.originals[k];
                idx = k;
                doc.originals.erase(doc.originals.begin() + (ptrdiff_t)k);
                break;
            }
        }
    }

    void revert(Document& doc) override
    {
        if (idx <= doc.originals.size())
        {
            doc.originals.insert(doc.originals.begin() + (ptrdiff_t)idx, backup);
        }
    }
};

// Create a full regular polygon (all edges + group) as one undo/redo step.
struct CmdCreateRegularPolygon : ICommand
{
    std::vector<Line> lines;
    RegularPolyGroup group;
    std::vector<size_t> indices;

    explicit CmdCreateRegularPolygon(std::vector<Line> ln, RegularPolyGroup g)
        : lines(std::move(ln)), group(std::move(g))
    {
    }

    void apply(Document& doc) override
    {
        indices.clear();
        indices.reserve(lines.size());

        // Insert lines.
        for (auto& l : lines)
        {
            indices.push_back(doc.originals.size());
            doc.originals.push_back(l);
        }

        // Add group if missing.
        if (!findRegPoly(doc, group.id))
        {
            doc.regPolys.push_back(group);
        }

        // Link lines to the group.
        for (auto id : group.lineIds)
        {
            if (auto* L = findLine(doc, id)) L->groupId = group.id;
        }

        // Select last edge.
        if (!group.lineIds.empty())
        {
            setSingleSelection(doc, group.lineIds.back());
        }
    }

    void revert(Document& doc) override
    {
        // Unlink lines.
        for (auto id : group.lineIds)
        {
            if (auto* L = findLine(doc, id)) if (L->groupId == group.id) L->groupId = 0;
        }

        // Remove lines.
        for (int i = (int)indices.size() - 1; i >= 0; --i)
        {
            doc.originals.erase(doc.originals.begin() + (ptrdiff_t)indices[i]);
        }

        // Remove group.
        for (size_t i = 0; i < doc.regPolys.size(); ++i)
        {
            if (doc.regPolys[i].id == group.id)
            {
                doc.regPolys.erase(doc.regPolys.begin() + (ptrdiff_t)i);

                break;
            }
        }

        clearSelection(doc);
    }
};

// Edit endpoints.
struct CmdEditEndpoints : ICommand
{
    Id id{ 0 };
    glm::vec2 a0{}, b0{}, a1{}, b1{};

    CmdEditEndpoints(Id i, glm::vec2 oldA, glm::vec2 oldB, glm::vec2 newA, glm::vec2 newB)
        : id(i), a0(oldA), b0(oldB), a1(newA), b1(newB)
    {
    }

    void apply(Document& doc) override
    {
        if (auto* l = findLine(doc, id))
        {
            l->a = a1; l->b = b1; l->dirty = true;
        }
    }

    void revert(Document& doc) override
    {
        if (auto* l = findLine(doc, id))
        {
            l->a = a0; l->b = b0; l->dirty = true;
        }
    }
};

// Move whole line.
struct CmdMoveLine : ICommand
{
    Id id{ 0 };
    glm::vec2 da{};

    CmdMoveLine(Id i, glm::vec2 delta)
        : id(i), da(delta)
    {
    }

    void apply(Document& doc) override
    {
        if (auto* l = findLine(doc, id))
        {
            l->a += da; l->b += da; l->dirty = true;
        }
    }

    void revert(Document& doc) override
    {
        if (auto* l = findLine(doc, id))
        {
            l->a -= da; l->b -= da; l->dirty = true;
        }
    }
};

// Style change.
struct CmdStyle : ICommand
{
    Id id{ 0 };
    Color fromC{}, toC{};
    float fromT{}, toT{};

    CmdStyle(Id i, Color fC, float fT, Color tC, float tT)
        : id(i), fromC(fC), toC(tC), fromT(fT), toT(tT)
    {
    }

    void apply(Document& doc) override
    {
        if (auto* l = findLine(doc, id))
        {
            l->color = toC; l->thicknessPx = toT;
        }
    }

    void revert(Document& doc) override
    {
        if (auto* l = findLine(doc, id))
        {
            l->color = fromC; l->thicknessPx = fromT;
        }
    }
};

// Transform changes.
struct CmdTransforms : ICommand
{
    Id id{ 0 };
    int k0{}, d0{}, k1{}, d1{};

    CmdTransforms(Id i, int oldK, int oldD, int newK, int newD)
        : id(i), k0(oldK), d0(oldD), k1(newK), d1(newD)
    {
    }

    void apply(Document& doc) override
    {
        if (auto* l = findLine(doc, id))
        {
            l->koch2Iters = k1; l->dragonIters = d1; l->dirty = true;
        }
    }

    void revert(Document& doc) override
    {
        if (auto* l = findLine(doc, id))
        {
            l->koch2Iters = k0; l->dragonIters = d0; l->dirty = true;
        }
    }
};

// Edit endpoints for many lines in one command.
struct CmdEditManyEndpoints : ICommand
{
    std::vector<Id> ids;
    std::vector<glm::vec2> a0, b0, a1, b1; // Old/new endpoints.

    CmdEditManyEndpoints(std::vector<Id> ids_,
        std::vector<glm::vec2> a0_, std::vector<glm::vec2> b0_,
        std::vector<glm::vec2> a1_, std::vector<glm::vec2> b1_)
        : ids(std::move(ids_)), a0(std::move(a0_)), b0(std::move(b0_)), a1(std::move(a1_)), b1(std::move(b1_))
    {
    }

    void apply(Document& doc) override
    {
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (auto* l = findLine(doc, ids[i]))
            {
                l->a = a1[i]; l->b = b1[i]; l->dirty = true;
            }
        }
    }

    void revert(Document& doc) override
    {
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (auto* l = findLine(doc, ids[i]))
            {
                l->a = a0[i]; l->b = b0[i]; l->dirty = true;
            }
        }
    }
};

// Uniform style applied to many (each line remembers its own old style).
struct CmdStyleMany : ICommand
{
    std::vector<Id> ids;
    std::vector<Color> fromC;
    std::vector<float> fromT;
    Color toC;
    float toT;

    CmdStyleMany(std::vector<Id> ids_, Color tc, float tt, const Document& doc)
        : ids(std::move(ids_)), toC(tc), toT(tt)
    {
        fromC.reserve(ids.size());
        fromT.reserve(ids.size());
        for (auto id : ids)
        {
            auto* l = findLine(const_cast<Document&>(doc), id);
            fromC.push_back(l ? l->color : Color{});
            fromT.push_back(l ? l->thicknessPx : 0.f);
        }
    }

    void apply(Document& doc) override
    {
        for (auto id : ids)
        {
            if (auto* l = findLine(doc, id))
            {
                l->color = toC; l->thicknessPx = toT;
            }
        }
    }

    void revert(Document& doc) override
    {
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (auto* l = findLine(doc, ids[i]))
            {
                l->color = fromC[i]; l->thicknessPx = fromT[i];
            }
        }
    }
};

struct CmdTransformsMany : ICommand
{
    std::vector<Id> ids;
    std::vector<int> k0, d0;
    int k1, d1;

    CmdTransformsMany(std::vector<Id> ids_, int newK, int newD, const Document& doc)
        : ids(std::move(ids_)), k1(newK), d1(newD)
    {
        k0.reserve(ids.size());
        d0.reserve(ids.size());
        for (auto id : ids)
        {
            auto* l = findLine(const_cast<Document&>(doc), id);
            k0.push_back(l ? l->koch2Iters : 0);
            d0.push_back(l ? l->dragonIters : 0);
        }
    }

    void apply(Document& doc) override
    {
        for (auto id : ids)
        {
            if (auto* l = findLine(doc, id))
            {
                l->koch2Iters = k1; l->dragonIters = d1; l->dirty = true;
            }
        }
    }

    void revert(Document& doc) override
    {
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (auto* l = findLine(doc, ids[i]))
            {
                l->koch2Iters = k0[i]; l->dragonIters = d0[i]; l->dirty = true;
            }
        }
    }
};

// Delete many (keeps indices so order is preserved).
struct CmdDeleteMany : ICommand
{
    std::vector<Id> ids;
    std::vector<Line> backups;
    std::vector<size_t> indices; // Original positions.

    explicit CmdDeleteMany(std::vector<Id> ids_)
        : ids(std::move(ids_))
    {
    }

    void apply(Document& doc) override
    {
        backups.clear();
        indices.clear();

        // Collect matches.
        for (size_t i = 0; i < doc.originals.size(); ++i)
        {
            if (std::find(ids.begin(), ids.end(), doc.originals[i].id) != ids.end())
            {
                backups.push_back(doc.originals[i]);
                indices.push_back(i);
            }
        }

        // Erase from highest index to lowest.
        for (int i = (int)indices.size() - 1; i >= 0; --i)
        {
            doc.originals.erase(doc.originals.begin() + (ptrdiff_t)indices[i]);
        }

        doc.selection.clear();
    }

    void revert(Document& doc) override
    {
        // Restore in ascending index order.
        for (size_t i = 0; i < indices.size(); ++i)
        {
            doc.originals.insert(doc.originals.begin() + (ptrdiff_t)indices[i], backups[i]);
        }
    }
};

// Create/remove a RegularPolyGroup record (lines are created via CmdCreateLine).
struct CmdCreateRegPolyGroup : ICommand
{
    RegularPolyGroup group;

    explicit CmdCreateRegPolyGroup(const RegularPolyGroup& g)
        : group(g)
    {
    }

    void apply(Document& doc) override
    {
        // Only add if missing.
        if (!findRegPoly(doc, group.id))
        {
            doc.regPolys.push_back(group);
        }

        // Re-attach line->group link in case lines were re-created.
        for (auto id : group.lineIds)
        {
            if (auto* l = findLine(doc, id)) l->groupId = group.id;
        }
    }

    void revert(Document& doc) override
    {
        // Unlink lines.
        for (auto id : group.lineIds)
        {
            if (auto* l = findLine(doc, id)) l->groupId = 0;
        }

        // Remove group.
        for (size_t i = 0; i < doc.regPolys.size(); ++i)
        {
            if (doc.regPolys[i].id == group.id)
            {
                doc.regPolys.erase(doc.regPolys.begin() + (ptrdiff_t)i);
                break;
            }
        }
    }
};

// Edit center/radius/rotation of a regular polygon as one undoable step.
struct CmdRegularPolyParams : ICommand
{
    Id groupId;
    glm::vec2 oldCenter, newCenter;
    float oldRadius, newRadius;
    float oldRotDeg, newRotDeg;

    CmdRegularPolyParams(Id gid, glm::vec2 c0, float r0, float rot0,
        glm::vec2 c1, float r1, float rot1)
        : groupId(gid),
        oldCenter(c0), newCenter(c1),
        oldRadius(r0), newRadius(r1),
        oldRotDeg(rot0), newRotDeg(rot1)
    {
    }

    static void rebuildLines(Document& doc, RegularPolyGroup& g)
    {
        int N = std::max(3, g.sides);
        float base = glm::radians(g.rotationDeg);

        for (int i = 0; i < N && i < (int)g.lineIds.size(); ++i)
        {
            float t0 = base + (i) * 6.2831853f / N;
            float t1 = base + (i + 1) * 6.2831853f / N;
            glm::vec2 p0 = g.center + g.radius * glm::vec2(std::cos(t0), std::sin(t0));
            glm::vec2 p1 = g.center + g.radius * glm::vec2(std::cos(t1), std::sin(t1));
            if (auto* l = findLine(doc, g.lineIds[i]))
            {
                l->a = p0; l->b = p1; l->dirty = true;
            }
        }
    }

    void apply(Document& doc) override
    {
        if (auto* g = findRegPoly(doc, groupId))
        {
            g->center = newCenter;
            g->radius = newRadius;
            g->rotationDeg = newRotDeg;
            rebuildLines(doc, *g);
        }
    }

    void revert(Document& doc) override
    {
        if (auto* g = findRegPoly(doc, groupId))
        {
            g->center = oldCenter;
            g->radius = oldRadius;
            g->rotationDeg = oldRotDeg;
            rebuildLines(doc, *g);
        }
    }
};

// Creates/removes an arbitrary polygon group and links/unlinks its edges.
struct CmdCreateArbPolyGroup : ICommand
{
    ArbitraryPolyGroup group;

    explicit CmdCreateArbPolyGroup(const ArbitraryPolyGroup& g)
        : group(g)
    {
    }

    void apply(Document& doc) override
    {
        // Add the group if it isn't already present.
        if (!findArbPoly(doc, group.id))
        {
            doc.arbPolys.push_back(group);
        }
        else
        {
            // If present, refresh its line list (defensive in redo paths).
            for (auto& gg : doc.arbPolys)
            {
                if (gg.id == group.id)
                {
                    gg.lineIds = group.lineIds;
                    break;
                }
            }
        }

        // Link lines to this group.
        for (Id lid : group.lineIds)
        {
            if (auto* l = findLine(doc, lid)) l->groupId = group.id;
        }
    }

    void revert(Document& doc) override
    {
        // Unlink lines.
        for (Id lid : group.lineIds)
        {
            if (auto* l = findLine(doc, lid))
            {
                if (l->groupId == group.id) l->groupId = 0;
            }
        }

        // Remove the group.
        for (size_t i = 0; i < doc.arbPolys.size(); ++i)
        {
            if (doc.arbPolys[i].id == group.id)
            {
                doc.arbPolys.erase(doc.arbPolys.begin() + (ptrdiff_t)i);
                break;
            }
        }
    }
};