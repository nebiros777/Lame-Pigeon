// Source file name: Birdhouse.cpp
// Author: Igor Matiushin
// Brief description: Implements room membership changes and relevance-based proxy updates for relay peers.
#include "../public/LamePigeonProtocol.h"
#include "../public/Birdhouse.h"
#include <unordered_set>

void Birdhouse::AddSquab(Squab *InSquab, const std::unordered_map<uint32_t, Squab> &AllPeers)
{
    std::vector<uint32_t> Neighbors = Grid.GetRelevantPeers(InSquab->LastTransform.Position, InSquab->Id);

    SquabIds.push_back(InSquab->Id);
    Grid.Insert(InSquab->Id, InSquab->LastTransform.Position);

    for (uint32_t NeighborId : Neighbors)
    {
        auto It = AllPeers.find(NeighborId);
        if (It == AllPeers.end()) continue;
        const Squab& Neighbor = It->second;

        {
            LamePigeon::WriteBuffer Out;
            Out.WriteHeader(LamePigeon::PacketType::SPAWN_PROXY);
            Out.WriteU32(Neighbor.Id);
            Out.WriteF32(Neighbor.LastTransform.Position.X);
            Out.WriteF32(Neighbor.LastTransform.Position.Y);
            Out.WriteF32(Neighbor.LastTransform.Position.Z);
            Out.WriteF32(Neighbor.LastTransform.Yaw);
            Out.WriteF32(Neighbor.LastTransform.Velocity.X);
            Out.WriteF32(Neighbor.LastTransform.Velocity.Y);
            Out.WriteF32(Neighbor.LastTransform.Velocity.Z);
            Out.WriteU8(Neighbor.LastTransform.Flags);
            ENetPacket* Packet = enet_packet_create(Out.Data.data(), Out.Data.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(InSquab->EnetPeer, CHANNEL_RELIABLE, Packet);
        }

        {
            LamePigeon::WriteBuffer Out;
            Out.WriteHeader(LamePigeon::PacketType::SPAWN_PROXY);
            Out.WriteU32(InSquab->Id);
            Out.WriteF32(InSquab->LastTransform.Position.X);
            Out.WriteF32(InSquab->LastTransform.Position.Y);
            Out.WriteF32(InSquab->LastTransform.Position.Z);
            Out.WriteF32(InSquab->LastTransform.Yaw);
            Out.WriteF32(InSquab->LastTransform.Velocity.X);
            Out.WriteF32(InSquab->LastTransform.Velocity.Y);
            Out.WriteF32(InSquab->LastTransform.Velocity.Z);
            Out.WriteU8(InSquab->LastTransform.Flags);
            ENetPacket* Packet = enet_packet_create(Out.Data.data(), Out.Data.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(Neighbor.EnetPeer, CHANNEL_RELIABLE, Packet);
        }
    }
}

void Birdhouse::RemoveSquab(uint32_t SquabId, const std::unordered_map<uint32_t, Squab> &AllPeers)
{
    auto SquabIt = AllPeers.find(SquabId);
    if (SquabIt != AllPeers.end())
    {
        std::vector<uint32_t> Neighbors = Grid.GetRelevantPeers(
             SquabIt->second.LastTransform.Position, SquabId);

        LamePigeon::WriteBuffer Out;
        Out.WriteHeader(LamePigeon::PacketType::DESPAWN_PROXY);
        Out.WriteU32(SquabId);

        for (uint32_t NeighborId : Neighbors)
        {
            auto It = AllPeers.find(NeighborId);
            if (It == AllPeers.end()) continue;

            ENetPacket* Pkt = enet_packet_create(Out.Data.data(), Out.Data.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(It->second.EnetPeer, CHANNEL_RELIABLE, Pkt);

            if (SquabIt->second.EnetPeer)
            {
                LamePigeon::WriteBuffer OutNeighbor;
                OutNeighbor.WriteHeader(LamePigeon::PacketType::DESPAWN_PROXY);
                OutNeighbor.WriteU32(NeighborId);
                ENetPacket* PktN =
                    enet_packet_create(OutNeighbor.Data.data(), OutNeighbor.Data.size(), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(SquabIt->second.EnetPeer, CHANNEL_RELIABLE, PktN);
            }
        }
    }

    Grid.Remove(SquabId);
    SquabIds.erase(
        std::remove(SquabIds.begin(), SquabIds.end(), SquabId),
        SquabIds.end()
    );
}

void Birdhouse::OnPositionUpdate(Squab *Sender, const Vec3 &NewPos, float Yaw, float Pitch,
                                 const Vec3 &Velocity, uint8_t Flags,
                                 const std::unordered_map<uint32_t, Squab> &AllPeers)
{
    Vec3 OldPos = Sender->LastTransform.Position;
    std::vector<uint32_t> OldRelevant = Grid.GetRelevantPeers(OldPos, Sender->Id);

    Sender->LastTransform = { NewPos, Yaw, Pitch, Velocity, Flags };
    Grid.Update(Sender->Id, NewPos);

    std::vector<uint32_t> NewRelevant = Grid.GetRelevantPeers(NewPos, Sender->Id);
    std::unordered_set<uint32_t> NewSet(NewRelevant.begin(), NewRelevant.end());
    std::unordered_set<uint32_t> OldSet(OldRelevant.begin(), OldRelevant.end());

    for (uint32_t NeighborId : OldRelevant)
    {
        if (NewSet.count(NeighborId)) continue;
        auto It = AllPeers.find(NeighborId);
        if (It == AllPeers.end()) continue;
        LamePigeon::WriteBuffer Out;
        Out.WriteHeader(LamePigeon::PacketType::DESPAWN_PROXY);
        Out.WriteU32(Sender->Id);
        ENetPacket* Pkt = enet_packet_create(Out.Data.data(), Out.Data.size(), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(It->second.EnetPeer, CHANNEL_RELIABLE, Pkt);

        if (Sender->EnetPeer)
        {
            LamePigeon::WriteBuffer OutReverse;
            OutReverse.WriteHeader(LamePigeon::PacketType::DESPAWN_PROXY);
            OutReverse.WriteU32(NeighborId);
            ENetPacket* PktRev =
                enet_packet_create(OutReverse.Data.data(), OutReverse.Data.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(Sender->EnetPeer, CHANNEL_RELIABLE, PktRev);
        }
    }

    for (uint32_t NeighborId : NewRelevant)
    {
        if (OldSet.count(NeighborId)) continue;
        auto It = AllPeers.find(NeighborId);
        if (It == AllPeers.end()) continue;
        const Squab& Neighbor = It->second;

        {
            LamePigeon::WriteBuffer Out;
            Out.WriteHeader(LamePigeon::PacketType::SPAWN_PROXY);
            Out.WriteU32(Neighbor.Id);
            Out.WriteF32(Neighbor.LastTransform.Position.X);
            Out.WriteF32(Neighbor.LastTransform.Position.Y);
            Out.WriteF32(Neighbor.LastTransform.Position.Z);
            Out.WriteF32(Neighbor.LastTransform.Yaw);
            Out.WriteF32(Neighbor.LastTransform.Velocity.X);
            Out.WriteF32(Neighbor.LastTransform.Velocity.Y);
            Out.WriteF32(Neighbor.LastTransform.Velocity.Z);
            Out.WriteU8(Neighbor.LastTransform.Flags);
            ENetPacket* Packet = enet_packet_create(Out.Data.data(), Out.Data.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(Sender->EnetPeer, CHANNEL_RELIABLE, Packet);
        }

        {
            LamePigeon::WriteBuffer Out;
            Out.WriteHeader(LamePigeon::PacketType::SPAWN_PROXY);
            Out.WriteU32(Sender->Id);
            Out.WriteF32(NewPos.X);
            Out.WriteF32(NewPos.Y);
            Out.WriteF32(NewPos.Z);
            Out.WriteF32(Yaw);
            Out.WriteF32(Velocity.X);
            Out.WriteF32(Velocity.Y);
            Out.WriteF32(Velocity.Z);
            Out.WriteU8(Flags);
            ENetPacket* Packet = enet_packet_create(Out.Data.data(), Out.Data.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(Neighbor.EnetPeer, CHANNEL_RELIABLE, Packet);
        }
    }

    LamePigeon::WriteBuffer Out;
    Out.WriteHeader(LamePigeon::PacketType::MOVE_PROXY);
    Out.WriteU32(Sender->Id);
    Out.WriteF32(NewPos.X);
    Out.WriteF32(NewPos.Y);
    Out.WriteF32(NewPos.Z);
    Out.WriteF32(Yaw);
    Out.WriteF32(Pitch);
    Out.WriteF32(Velocity.X);
    Out.WriteF32(Velocity.Y);
    Out.WriteF32(Velocity.Z);
    Out.WriteU8(Flags);

    for (uint32_t NeighborId : NewRelevant)
    {
        auto it = AllPeers.find(NeighborId);
        if (it == AllPeers.end()) continue;
        ENetPacket* Packet = enet_packet_create(Out.Data.data(), Out.Data.size(), 0);
        enet_peer_send(it->second.EnetPeer, CHANNEL_UNRELIABLE, Packet);
    }
}
