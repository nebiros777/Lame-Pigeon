// Source file name: Dovecote.cpp
// Author: Igor Matiushin
// Brief description: Implements relay server startup, room traffic, RPC forwarding, and interaction matching.

#include "../public/Dovecote.h"
#include "../public/Birdhouse.h"

#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

double SteadySecondsSinceEpoch()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

bool TagsMutuallyConsistent(const std::string& myA, const std::string& theirA, const std::string& myB,
                            const std::string& theirB)
{
    if (myA.empty() || theirA.empty() || myB.empty() || theirB.empty())
        return false;
    return myA == theirB && theirA == myB;
}

}

bool Dovecote::Initialize()
{
    if (enet_initialize() != 0)
        return false;
    server = enet_host_create(&address, DovecoteConfig::MaxPeers, DovecoteConfig::ChannelCount, 0, 0);
    if (!server)
        return false;
    return true;
}

void Dovecote::Deinitialize()
{
    enet_host_destroy(server);
    enet_deinitialize();
}

void Dovecote::RunServer()
{
    while (running)
    {
        ENetEvent event;
        int       firstWaitMilliseconds = DovecoteConfig::ServiceTimeoutMs;
        while (enet_host_service(server, &event, firstWaitMilliseconds) > 0)
        {
            firstWaitMilliseconds = 0;
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT: {
                Squab newSquab = Squab(NextPeerId++, event.peer);
                Peers[newSquab.Id] = newSquab;

                event.peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(newSquab.Id));
                std::printf("Peer connected: %u\n", newSquab.Id);

                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                uint32_t               SenderId = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer->data));
                LamePigeon::ReadBuffer Buf(event.packet->data, event.packet->dataLength);

                if (Buf.CanRead(1))
                {
                    LamePigeon::PacketType Type = Buf.ReadType();

                    if (Type == LamePigeon::PacketType::JOIN_ROOM)
                    {
                        uint32_t RoomId = Buf.ReadU32();
                        Peers[SenderId].RoomId = RoomId;

                        if (Rooms.find(RoomId) == Rooms.end())
                        {
                            Rooms.emplace(RoomId, Birdhouse(RoomId, DovecoteConfig::GridCellSize));
                        }

                        Rooms.at(RoomId).AddSquab(&Peers[SenderId], Peers);
                    }
                    else if (Type == LamePigeon::PacketType::LEAVE_ROOM)
                    {
                        uint32_t RoomId = Peers[SenderId].RoomId;
                        auto     RoomIt = Rooms.find(RoomId);
                        if (RoomIt != Rooms.end())
                        {
                            RoomIt->second.RemoveSquab(SenderId, Peers);
                            if (RoomIt->second.IsEmpty())
                                Rooms.erase(RoomIt);
                        }
                        Peers[SenderId].RoomId = 0;
                        RemoveInteractionsInvolvingPeer(SenderId);
                    }
                    else if (Type == LamePigeon::PacketType::POSITION_UPDATE)
                    {
                        Vec3 NewPos;
                        NewPos.X    = Buf.ReadF32();
                        NewPos.Y    = Buf.ReadF32();
                        NewPos.Z    = Buf.ReadF32();
                        float Yaw   = Buf.ReadF32();
                        float Pitch = Buf.ReadF32();
                        Vec3  Velocity;
                        Velocity.X  = Buf.ReadF32();
                        Velocity.Y  = Buf.ReadF32();
                        Velocity.Z  = Buf.ReadF32();
                        uint8_t Flags = Buf.CanRead(1) ? Buf.ReadU8() : 0;
                        if (Buf.CanRead(4))
                            (void)Buf.ReadU32();

                        Squab&   Sender = Peers[SenderId];
                        uint32_t RoomId = Sender.RoomId;

                        auto RoomIt = Rooms.find(RoomId);
                        if (RoomIt != Rooms.end())
                        {
                            RoomIt->second.OnPositionUpdate(&Sender, NewPos, Yaw, Pitch, Velocity, Flags, Peers);
                        }
                        else
                        {
                            Sender.LastTransform = { NewPos, Yaw, Pitch, Velocity, Flags };
                        }
                    }
                    else if (Type == LamePigeon::PacketType::PING)
                    {
                        LamePigeon::WriteBuffer Out;
                        Out.WriteHeader(LamePigeon::PacketType::PONG);
                        ENetPacket* Pkt = enet_packet_create(Out.Data.data(), Out.Data.size(), ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, CHANNEL_RELIABLE, Pkt);
                    }
                    else if (Type == LamePigeon::PacketType::INTERACTION_PREDICT)
                    {
                        HandleInteractionPredict(SenderId, Buf);
                    }
                    else if (Type == LamePigeon::PacketType::RPC_CALL)
                    {
                        const uint8_t* packetBytes     = event.packet->data;
                        const size_t   packetByteCount = event.packet->dataLength;
                        if (packetByteCount >= 1 + 4)
                        {
                            uint32_t targetPeerIdNetworkOrder;
                            memcpy(&targetPeerIdNetworkOrder, packetBytes + 1, 4);
                            const uint32_t targetPeerId = ntohl(targetPeerIdNetworkOrder);

                            const uint32_t senderRoomId = Peers[SenderId].RoomId;
                            if (senderRoomId != 0)
                            {
                                if (targetPeerId == 0xFFFFFFFFu)
                                {
                                    for (auto& roomPeerEntry : Peers)
                                    {
                                        if (roomPeerEntry.first == SenderId || roomPeerEntry.second.RoomId != senderRoomId)
                                            continue;
                                        if (!roomPeerEntry.second.EnetPeer)
                                            continue;
                                        ENetPacket* relayCopy =
                                            enet_packet_create(packetBytes, packetByteCount, event.packet->flags);
                                        if (relayCopy->dataLength >= 5)
                                        {
                                            const uint32_t senderIdNetworkOrder = htonl(SenderId);
                                            memcpy(relayCopy->data + 1, &senderIdNetworkOrder, 4);
                                        }
                                        enet_peer_send(roomPeerEntry.second.EnetPeer, event.channelID, relayCopy);
                                    }
                                }
                                else
                                {
                                    const auto victimIterator = Peers.find(targetPeerId);
                                    if (victimIterator != Peers.end() && victimIterator->second.RoomId == senderRoomId &&
                                        victimIterator->second.EnetPeer)
                                    {
                                        std::vector<uint8_t> targetedPayload;
                                        targetedPayload.reserve(packetByteCount);
                                        targetedPayload.push_back(static_cast<uint8_t>(LamePigeon::PacketType::RPC_CALL));
                                        const uint32_t senderIdNetworkOrder = htonl(SenderId);
                                        const uint8_t* senderIdBytes =
                                            reinterpret_cast<const uint8_t*>(&senderIdNetworkOrder);
                                        targetedPayload.insert(targetedPayload.end(), senderIdBytes, senderIdBytes + 4);
                                        targetedPayload.insert(targetedPayload.end(), packetBytes + 1 + 4,
                                                               packetBytes + packetByteCount);
                                        ENetPacket* targetedPacket = enet_packet_create(
                                            targetedPayload.data(), targetedPayload.size(), ENET_PACKET_FLAG_RELIABLE);
                                        enet_peer_send(victimIterator->second.EnetPeer, CHANNEL_RELIABLE, targetedPacket);
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        uint32_t RoomId = Peers[SenderId].RoomId;
                        if (RoomId != 0)
                        {
                            const uint8_t* srcData = event.packet->data;
                            size_t         srcLen  = event.packet->dataLength;
                            for (auto& p : Peers)
                            {
                                if (p.first == SenderId || p.second.RoomId != RoomId)
                                    continue;
                                if (!p.second.EnetPeer)
                                    continue;
                                ENetPacket* copy = enet_packet_create(srcData, srcLen, event.packet->flags);
                                if (copy->dataLength >= 5)
                                {
                                    uint32_t sid = htonl(static_cast<uint32_t>(SenderId));
                                    memcpy(copy->data + 1, &sid, 4);
                                }
                                enet_peer_send(p.second.EnetPeer, event.channelID, copy);
                            }
                        }
                    }
                }

                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                uint32_t Id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer->data));
                std::printf("Peer disconnected: %u\n", Id);
                RemoveInteractionsInvolvingPeer(Id);

                uint32_t RoomId = Peers[Id].RoomId;
                auto     RoomIt = Rooms.find(RoomId);
                if (RoomIt != Rooms.end())
                {
                    RoomIt->second.RemoveSquab(Id, Peers);
                    if (RoomIt->second.IsEmpty())
                        Rooms.erase(RoomIt);
                }

                Peers.erase(Id);
                break;
            }

            default:
                break;
            }
        }
        TickInteractionMatcher(SteadySecondsSinceEpoch());
    }
}

void Dovecote::SendInteractionReject(uint32_t peerId, uint32_t yourEventId, uint8_t reason)
{
    auto        it        = Peers.find(peerId);
    if (it == Peers.end() || !it->second.EnetPeer)
        return;
    LamePigeon::WriteBuffer w;
    w.WriteHeader(LamePigeon::PacketType::INTERACTION_REJECT);
    w.WriteU32(yourEventId);
    w.WriteU8(reason);
    ENetPacket* pkt = enet_packet_create(w.Data.data(), w.Data.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(it->second.EnetPeer, CHANNEL_RELIABLE, pkt);
}

void Dovecote::HandleInteractionPredict(uint32_t senderId, LamePigeon::ReadBuffer& buf)
{
    if (!buf.CanRead(4 + 4 + 4 + 2 + 2))
        return;
    const uint32_t eventId    = buf.ReadU32();
    const uint32_t otherPeer  = buf.ReadU32();
    const float    delta      = buf.ReadF32();
    std::string    myTag      = buf.ReadString();
    std::string    theirTag   = buf.ReadString();

    if (myTag.empty() || theirTag.empty())
        return;

    const auto peerIt = Peers.find(senderId);
    if (peerIt == Peers.end() || peerIt->second.RoomId == 0)
        return;
    if (Peers.find(otherPeer) == Peers.end() || Peers[otherPeer].RoomId != peerIt->second.RoomId)
        return;

    PendingInteractionEvent ev;
    ev.SenderPeer         = senderId;
    ev.OtherPeer          = otherPeer;
    ev.RoomId             = peerIt->second.RoomId;
    ev.EventId            = eventId;
    ev.ServerRecvSeconds  = SteadySecondsSinceEpoch();
    ev.DeltaToContact     = delta;
    ev.MyTag              = std::move(myTag);
    ev.TheirTag           = std::move(theirTag);
    ev.Matched            = false;

    for (auto it = PendingInteractions.begin(); it != PendingInteractions.end();)
    {
        if (!it->Matched && it->SenderPeer == senderId && it->OtherPeer == otherPeer)
            it = PendingInteractions.erase(it);
        else
            ++it;
    }

    PendingInteractions.push_back(std::move(ev));

    const size_t n = PendingInteractions.size();
    if (n < 2)
        return;
    const size_t j = n - 1;
    for (size_t i = 0; i < j; ++i)
    {
        PendingInteractionEvent& A = PendingInteractions[i];
        PendingInteractionEvent& B = PendingInteractions[j];
        if (A.Matched || B.Matched)
            continue;
        if (A.RoomId != B.RoomId)
            continue;
        if (!(A.SenderPeer == B.OtherPeer && A.OtherPeer == B.SenderPeer))
            continue;
        if (!TagsMutuallyConsistent(A.MyTag, A.TheirTag, B.MyTag, B.TheirTag))
            continue;
        const double tA = A.ServerRecvSeconds + static_cast<double>(A.DeltaToContact);
        const double tB = B.ServerRecvSeconds + static_cast<double>(B.DeltaToContact);
        const double dT = std::fabs(tA - tB);
        if (dT > static_cast<double>(DovecoteConfig::InteractionConfirmWindowSeconds))
            continue;

        A.Matched = true;
        B.Matched = true;
        return;
    }
}

void Dovecote::TickInteractionMatcher(double nowSeconds)
{
    std::vector<PendingInteractionEvent> kept;
    kept.reserve(PendingInteractions.size());

    for (PendingInteractionEvent& e : PendingInteractions)
    {
        const double age = nowSeconds - e.ServerRecvSeconds;
        if (e.Matched)
        {
            if (age <= static_cast<double>(DovecoteConfig::InteractionEventTtlSeconds))
                kept.push_back(std::move(e));
            continue;
        }
        if (age >= static_cast<double>(DovecoteConfig::InteractionRejectAfterSeconds))
        {
            SendInteractionReject(e.SenderPeer, e.EventId, 0);
            continue;
        }
        kept.push_back(std::move(e));
    }
    PendingInteractions.swap(kept);
}

void Dovecote::RemoveInteractionsInvolvingPeer(uint32_t peerId)
{
    std::vector<PendingInteractionEvent> kept;
    kept.reserve(PendingInteractions.size());
    for (PendingInteractionEvent& e : PendingInteractions)
    {
        if (e.SenderPeer == peerId || e.OtherPeer == peerId)
        {
            if (!e.Matched)
                SendInteractionReject(e.SenderPeer, e.EventId, 1);
            continue;
        }
        kept.push_back(std::move(e));
    }
    PendingInteractions.swap(kept);
}
