Lame Pigeon is a multiplayer framework for character replication, room-based
visibility, and client-side interaction prediction. It was made first of all
for students and for the wider game development community. Right now there is
no free product in this area that gives the same combination of simplicity,
open code, and freedom to inspect and change the networking logic. The closest
relatives are commercial products such as Strix and Photon Fusion, but they do
not give the same freedom for debugging and direct control over the data flow.

## Support the project
If you find this useful, you can support development via the ❤️ Sponsor button on GitHub.

The project has three main parts. Dovecote is the relay server. Carrier is the
client networking layer and protocol implementation. The Unreal plugin embeds
Carrier and exposes it to the engine. The Unreal demo project is the main usage
example of the system. Carrier itself can also be used in pure C++ clients if a
project does not use Unreal.

Dovecote is a relay, not a full gameplay simulation server. It accepts peers,
assigns peer identifiers, tracks room membership, stores the latest transforms,
and forwards only the data that should be visible to nearby players. This keeps
the server small and focused. The world is divided into rooms, and inside a
room the server uses spatial relevance so that clients only receive proxies that
matter to them at the moment.

Carrier handles connection, room join and leave, movement packets, replicated
variables, RPC traffic, ping measurement, and remote proxy interpolation. When
movement updates arrive, Carrier stores snapshots and renders remote peers with
a small delay, which makes movement look smooth instead of snapping from packet
to packet. If updates stop for a short moment, Carrier can extrapolate from the
last known velocity.

The project also includes client-side interaction prediction for close
player-to-player events such as a dash hit. Each client predicts the contact
locally and sends a prediction packet to the relay. Dovecote checks whether the
two peers reported a matching interaction in the same room and within the
allowed time window. If the reports match, the interaction is accepted
silently. If not, the sender receives a reject packet. This keeps the action
responsive on the client while still giving the relay a final check.

The Unreal plugin turns Carrier data into engine-facing systems. It pumps the
network layer every frame, spawns and despawns proxy characters, applies
interpolated movement, forwards replicated values and RPC calls, and exposes the
features to Blueprints. The demo project builds actual gameplay on top of this
layer. It shows character movement, jump replication, dash, knockback, health,
movement context sharing, and predicted player contact.

Flock is the stress client for the project. It creates many Carrier-based peers
and moves them through the same world to test crowd replication, relevance
changes, spawning, despawning, and general server load.

Lame Pigeon is distributed under the MIT License. The code can be used,
modified, redistributed, and included in commercial work as long as the
copyright notice and license text stay with the software.
