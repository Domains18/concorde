#pragma once
#include "crypto.h"
#include "trust.h"
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct Peer
{
    std::string ip_address;
    std::string device_name;
    int http_port;
    std::vector<std::string> shared_folders;
    std::string public_key;      // Hex-encoded Ed25519 public key
    std::string fingerprint;     // SHA256:... for display
    bool trusted;                // Is this device trusted?
    std::chrono::steady_clock::time_point last_seen;
};

class DiscoveryService
{
  public:
    DiscoveryService(int broadcast_port, 
                    int http_port, 
                    const std::string& device_name,
                    concorde::DeviceIdentity& identity,
                    concorde::TrustStore& trust);
    DiscoveryService(const DiscoveryService&) = delete;
    DiscoveryService& operator=(const DiscoveryService&) = delete;
    ~DiscoveryService();

    void start();
    void stop();

    // Thread-safe access to get current active peers
    std::map<std::string, Peer> getPeers();
    
    // Get only trusted peers
    std::map<std::string, Peer> getTrustedPeers();

  private:
    void broadCastLoop();
    void listenLoop();
    void prunePeers();
    
    // Handle new device discovery
    void handleNewDevice(const std::string& ip, 
                        const std::string& name,
                        const std::string& pubkey,
                        const std::string& fingerprint);

    int broadcast_port_;
    int my_http_port_;
    std::string my_device_name_;
    
    concorde::DeviceIdentity& identity_;
    concorde::TrustStore& trust_;
    concorde::PairingManager pairing_manager_;

    std::atomic<bool> running_;
    std::thread broadcast_thread_;
    std::thread listener_thread_;

    std::mutex peers_mutex_;
    std::map<std::string, Peer> peers_;
};
