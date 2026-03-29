// Source file name: SpaceGrid.cpp
// Author: Igor Matiushin
// Brief description: Implements the spatial relevance grid used by the relay to query nearby peers.

#include "../public/SpaceGrid.h"

#include <cmath>

void SpaceGrid::Insert(uint32_t PeerId, const Vec3 &Position)
{
    CellKey Key = WorldToCell(Position);
    CellsWithPeers[Key].insert(PeerId);
    PeerCells[PeerId] = Key;
}

void SpaceGrid::Remove(uint32_t PeerId)
{
    auto it = PeerCells.find(PeerId);
    if (it == PeerCells.end())
    {
        return;
    }
    CellsWithPeers[it->second].erase(PeerId);
    PeerCells.erase(it);
}

bool SpaceGrid::Update(uint32_t PeerId, const Vec3 &NewPosition)
{
    CellKey NewKey = WorldToCell(NewPosition);
    auto it = PeerCells.find(PeerId);

    if (it != PeerCells.end() && it->second == NewKey)
    {
        return false;
    }
    Remove(PeerId);
    Insert(PeerId, NewPosition);
    return true;
}

std::vector<uint32_t> SpaceGrid::GetRelevantPeers(const Vec3 &Position, uint32_t ExcludePeerId) const
{
    CellKey Center = WorldToCell(Position);
    std::vector<uint32_t> Result;
    for (int8_t dX = -2; dX <= 2; ++dX)
    {
        for (int8_t dY = -2; dY <= 2; ++dY)
        {
            CellKey Neighor = { Center.X + dX, Center.Y + dY };
            auto it = CellsWithPeers.find(Neighor);
            if (it == CellsWithPeers.end())
            {
                continue;
            }

            for (uint32_t iD : it->second)
            {
                if (iD != ExcludePeerId)
                {
                    Result.push_back(iD);
                }
            }
        }
    }
    return Result;
}

CellKey SpaceGrid::WorldToCell(const Vec3 &Position) const
{
    return { int32_t(floorf(Position.X / GridCellSize)),
             int32_t(floorf(Position.Y / GridCellSize)) };
}
