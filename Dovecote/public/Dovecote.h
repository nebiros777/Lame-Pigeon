// Source file name: Dovecote.h
// Author: Igor Matiushin
// Brief description: Declares the standalone relay server interface and owned runtime state.

#ifndef LAMEPIGEON_DOVECOTE_H
#define LAMEPIGEON_DOVECOTE_H

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "Birdhouse.h"
#include "DovecoteConfig.h"
#include "LamePigeonProtocol.h"
#include "Squab.h"
#include <enet/enet.h>

class Dovecote {

public:
    Dovecote()
    {
        address = {ENET_HOST_ANY, DovecoteConfig::Port};
    }
    ~Dovecote() = default;

    bool Initialize();
    void Deinitialize();

    void RunServer();

private:
    struct PendingInteractionEvent
    {
        uint32_t    SenderPeer = 0;
        uint32_t    OtherPeer  = 0;
        uint32_t    RoomId     = 0;
        uint32_t    EventId    = 0;
        double      ServerRecvSeconds = 0.0;
        float       DeltaToContact    = 0.f;
        std::string MyTag;
        std::string TheirTag;
        bool        Matched = false;
    };

    void HandleInteractionPredict(uint32_t senderId, LamePigeon::ReadBuffer& buf);
    void TickInteractionMatcher(double nowSeconds);
    void RemoveInteractionsInvolvingPeer(uint32_t peerId);
    void SendInteractionReject(uint32_t peerId, uint32_t yourEventId, uint8_t reason);

    ENetAddress address{};
    ENetHost*   server  = NULL;
    bool        running = true;

    std::unordered_map<uint32_t, Squab> Peers;
    uint32_t                            NextPeerId = 1;

    std::unordered_map<uint32_t, Birdhouse> Rooms;

    std::vector<PendingInteractionEvent> PendingInteractions;
};

#endif
