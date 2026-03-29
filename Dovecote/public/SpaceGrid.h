// Source file name: SpaceGrid.h
// Author: Igor Matiushin
// Brief description: Declares the relay-side spatial relevance grid used to filter peer replication.

#ifndef LAMEPIGEON_SPACEGRID_H
#define LAMEPIGEON_SPACEGRID_H
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Squab.h"

struct CellKey
{
    int32_t X;
    int32_t Y;

    bool operator==(const CellKey& Other) const
    {
        return X == Other.X && Y == Other.Y;
    }
};

struct CellKeyHash
{
    size_t operator()(const CellKey& Key) const
    {
        return std::hash<int64_t>()((static_cast<int64_t>(Key.X) << 32) | static_cast<uint32_t>(Key.Y));
    }
};

class SpaceGrid {
public:
    explicit SpaceGrid(const float CellSize = 15000.f)
    : GridCellSize(CellSize) {}

    void Insert(uint32_t PeerId, const Vec3& Position);
    void Remove(uint32_t PeerId);
    bool Update(uint32_t PeerId, const Vec3& NewPosition);
    std::vector<uint32_t> GetRelevantPeers(const Vec3& Position, uint32_t ExcludePeerId) const;

private:
    float GridCellSize = 15000.f;
    std::unordered_map<CellKey, std::unordered_set<uint32_t>, CellKeyHash> CellsWithPeers;
    std::unordered_map<uint32_t, CellKey> PeerCells;

    CellKey WorldToCell(const Vec3& Position) const;
};


#endif