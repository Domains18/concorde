#include "discovery.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

DiscoveryService::DiscoveryService(int b_port, 
                                  int h_port, 
                                  const std::string& name,
                                  concorde::DeviceIdentity& identity,
                                  concorde::TrustStore& trust)
    : broadcast_port_(b_port), 
      my_http_port_(h_port), 
      my_device_name_(name),
      identity_(identity),
      trust_(trust),
      pairing_manager_(trust),
      running_(false)
{
}

DiscoveryService::~DiscoveryService()
{
    stop();
}

void DiscoveryService::start()
{
    if (running_)
        return;
    running_ = true;
    broadcast_thread_ = std::thread(&DiscoveryService::broadCastLoop, this);
    listener_thread_ = std::thread(&DiscoveryService::listenLoop, this);
}

void DiscoveryService::stop()
{
    if (!running_)
        return;
    running_ = false;
    if (broadcast_thread_.joinable())
        broadcast_thread_.join();
    if (listener_thread_.joinable())
        listener_thread_.join();
}

void DiscoveryService::broadCastLoop()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(broadcast_port_);
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    while (running_)
    {
        // Create signed beacon
        json beacon;
        beacon["device_name"] = my_device_name_;
        beacon["port"] = my_http_port_;
        beacon["public_key"] = identity_.get_public_key_hex();
        beacon["fingerprint"] = identity_.get_fingerprint();
        beacon["shares"] = json::array({"public", "docs"});
        beacon["timestamp"] = std::time(nullptr);
        
        // Sign the beacon
        std::string beacon_str = beacon.dump();
        beacon["signature"] = identity_.sign(beacon_str);
        
        std::string msg = beacon.dump();
        sendto(sock, msg.c_str(), msg.length(), 0, 
               (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    close(sock);
}

void DiscoveryService::listenLoop()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(broadcast_port_);
    recv_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&recv_addr, sizeof(recv_addr));

    char buffer[4096];
    while (running_)
    {
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);
        int len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, 
                          (struct sockaddr*)&sender_addr, &sender_len);

        if (len > 0)
        {
            buffer[len] = '\0';
            std::string sender_ip = inet_ntoa(sender_addr.sin_addr);
            
            try {
                json beacon = json::parse(buffer);
                
                // Extract beacon fields
                std::string device_name = beacon["device_name"];
                int port = beacon["port"];
                time_t timestamp = beacon["timestamp"];
                std::string pubkey = beacon["public_key"];
                std::string fingerprint = beacon["fingerprint"];
                std::string signature = beacon["signature"];
                
                // Don't discover ourselves
                if (pubkey == identity_.get_public_key_hex()) {
                    continue;
                }
                
                // Verify signature
                json beacon_without_sig = beacon;
                beacon_without_sig.erase("signature");
                std::string beacon_str = beacon_without_sig.dump();
                
                if (!concorde::CryptoUtils::verify_signature(pubkey, beacon_str, signature)) {
                    std::cerr << "⚠️  Invalid signature from " << sender_ip 
                             << " (" << device_name << ")" << std::endl;
                    continue;
                }
                
                // Check timestamp to prevent replay attacks
                const auto now = std::time(nullptr);
                if (std::abs(now - timestamp) > 15) // 15s tolerance
                {
                    std::cerr << "⚠️  Stale beacon from " << sender_ip << " - rejecting." << std::endl;
                    continue;
                }

                // Check if trusted
                bool is_trusted = trust_.is_trusted(pubkey);
                
                // If not trusted, handle pairing
                if (!is_trusted) {
                    handleNewDevice(sender_ip, device_name, pubkey, fingerprint);
                    is_trusted = trust_.is_trusted(pubkey); // Check again
                }
                
                // Update peer info
                std::lock_guard<std::mutex> lock(peers_mutex_);
                peers_[sender_ip] = Peer{
                    .ip_address = sender_ip,
                    .device_name = device_name,
                    .http_port = port,
                    .shared_folders = beacon["shares"].get<std::vector<std::string>>(),
                    .public_key = pubkey,
                    .fingerprint = fingerprint,
                    .trusted = is_trusted,
                    .last_seen = std::chrono::steady_clock::now()
                };
                
                if (is_trusted) {
                    trust_.update_last_seen(pubkey);
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Error parsing beacon: " << e.what() << std::endl;
            }
        }
    }
    close(sock);
}

void DiscoveryService::handleNewDevice(const std::string& ip,
                                      const std::string& name,
                                      const std::string& pubkey,
                                      const std::string& fingerprint)
{
    // This runs in listener thread, so pairing prompt will block discovery
    // In production, you'd want to queue this and handle in separate thread
    std::cout << "\n🔍 New device discovered: " << name << " (" << ip << ")\n";
    pairing_manager_.request_approval(name, pubkey, fingerprint);
}

void DiscoveryService::prunePeers()
{
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = peers_.begin(); it != peers_.end();)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_seen).count();
        
        if (elapsed > 15) {
            std::cout << "⏱️  Peer timeout: " << it->second.device_name 
                     << " (" << it->first << ")" << std::endl;
            it = peers_.erase(it);
        } else {
            ++it;
        }
    }
}

std::map<std::string, Peer> DiscoveryService::getPeers()
{
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return peers_;
}

std::map<std::string, Peer> DiscoveryService::getTrustedPeers()
{
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::map<std::string, Peer> trusted;
    
    for (const auto& [ip, peer] : peers_) {
        if (peer.trusted) {
            trusted[ip] = peer;
        }
    }
    
    return trusted;
}
