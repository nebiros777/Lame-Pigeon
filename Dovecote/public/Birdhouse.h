// Source file name: Birdhouse.h
// Author: Igor Matiushin
// Brief description: Declares room-level peer management and relevance fanout for the relay server.

#ifndef LAMEPIGEON_BIRDHOUSE_H
#define LAMEPIGEON_BIRDHOUSE_H
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "SpaceGrid.h"


struct Vec3;
struct Squab;
class SpaceGrid;

class Birdhouse {
public:
    explicit Birdhouse(uint32_t inId, float cellSize = 2500.f) : RoomId(inId), Grid(cellSize) {}

    void AddSquab(Squab* InSquab, const std::unordered_map<uint32_t, Squab>& AllPeers);
    void RemoveSquab(uint32_t SquabId, const std::unordered_map<uint32_t, Squab>& AllPeers);

    void OnPositionUpdate(Squab* Sender, const Vec3& NewPos, float Yaw, float Pitch,
                          const Vec3& Velocity, uint8_t Flags,
                          const std::unordered_map<uint32_t, Squab>& AllPeers);

    const uint32_t GetId() const
    {
        return RoomId;
    }

    bool IsEmpty() const
    {
        return SquabIds.empty();
    }

    const std::vector<uint32_t>& GetSquabIds() const { return SquabIds; }

private:
    uint32_t RoomId;
    SpaceGrid Grid;
    std::vector<uint32_t> SquabIds;
};

#endif
