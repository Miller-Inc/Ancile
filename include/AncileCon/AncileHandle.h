//
// Created by James Miller on 9/28/2026.
//

#pragma once
#include <vector>
#include <string>
#include "Actor.h"
#include "SharedMemory.h"
#include "Sock.h"
#include "NetParser.h"

namespace Ancile {

    class AncileHandle {
        public:
        AncileHandle() = default;
        ~AncileHandle();

        std::vector<Actor> Handles;

        bool InitConnection(const ConParams& params);

        void UpdateFromConnection();
        void UpdateToConnection();

        uint8_t ConnectionType = NoConnection;

    private:
        ShmHandle mShmHandle{};
        SocketHandle mSocketHandle{};

        void CleanupShm();
        void CleanupSock();

        bool bReady = false;
    };
}