// Source file name: ENetAmalgam.cpp
// Author: Igor Matiushin
// Brief description: Builds the plugin-local ENet sources into the Unreal module as one translation unit.

extern "C" {
#include "callbacks.c"
#include "compress.c"
#include "host.c"
#include "list.c"
#include "packet.c"
#include "peer.c"
#include "protocol.c"
#include "win32.c"
}
