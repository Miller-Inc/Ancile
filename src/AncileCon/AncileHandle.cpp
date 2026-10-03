//
// Created by James Miller on 9/28/2026.
//

#include "AncileCon/AncileHandle.h"

namespace Ancile {
    AncileHandle::~AncileHandle() {
        if (ConnectionType == SharedMemory) {
            Shared::Close(mShmHandle);
        }
        else if (ConnectionType == LocalNetwork) {
            Shared::Close(mSocketHandle);
            Shared::ShutdownSubsystem();
        }
    }

    bool AncileHandle::InitConnection(const ConParams& params) {
        ConnectionType = params.type;
        CleanupShm();
        CleanupSock();

        switch (params.type) {
            case SharedMemory:
                if (Shared::Open(mShmHandle, params.sharedMem.name, params.sharedMem.size)) {
                    bReady = true;
                    return true;
                }
                fprintf(stderr, "Error: Could not connect to shared memory");
                break;
            case LocalNetwork:
                if (!Shared::InitializeSubsystem()) {
                    fprintf(stderr, "Error: Could not properly start subsystem for Sockets");
                    ConnectionType = NoConnection;
                    return false;
                }

                if (Shared::Connect(mSocketHandle, params.localNetwork.ipAddress, params.localNetwork.port)) {
                    bReady = true;
                    return true;
                }
                fprintf(stderr, "Error: Could not connect to local network");
                break;
            case NoConnection:
            default:
                bReady = false;
                break;
        }

        return false;
    }

    void AncileHandle::UpdateFromConnection() {

    }

    void AncileHandle::UpdateToConnection() {

    }

    void AncileHandle::CleanupShm() {
        if ((ConnectionType & SharedMemory) == SharedMemory && bReady) {
            Shared::Close(mShmHandle);
            Shared::Unlink(mShmHandle.name);
        }
    }

    void AncileHandle::CleanupSock() {
        if ((ConnectionType & LocalNetwork) == LocalNetwork && bReady) {
            Shared::Close(mSocketHandle);
            Shared::ShutdownSubsystem();
        }
    }
}
