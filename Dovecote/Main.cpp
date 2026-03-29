// Source file name: Main.cpp
// Author: Igor Matiushin
// Brief description: Starts the standalone Dovecote relay server process.

#include <cstdio>

#include "public/Dovecote.h"
#include "public/LamePigeonProtocol.h"
#include "public/Squab.h"

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);

    Dovecote DovecoteInstance;
    if (DovecoteInstance.Initialize())
    {
        DovecoteInstance.RunServer();
    }
    DovecoteInstance.Deinitialize();
    return 0;
}