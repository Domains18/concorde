
#pragma once
#include "crow.h"
#include "discovery.h"
#include "trust.h"
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

class FileServer
{
  public:
    FileServer(DiscoveryService& discovery, 
              int port, 
              std::string root_dir,
              concorde::TrustStore& trust);
    void run();

  private:
    DiscoveryService& discovery_;
    int port_;
    std::string root_dir_;
    concorde::TrustStore& trust_;
    crow::SimpleApp app_;

    // Nonce management for challenge-response authentication
    struct NonceData
    {
        std::chrono::steady_clock::time_point expires_at;
        std::string pubkey;
    };
    std::unordered_map<std::string, NonceData> active_nonces_;
    std::mutex nonce_mutex_;
    const std::chrono::seconds NONCE_LIFETIME{60}; // 60 second validity

    void setupRoutes();
    std::string generate_challenge(const std::string& pubkey);
    bool consume_nonce(const std::string& nonce, const std::string& pubkey);
    void cleanup_expired_nonces();
};
