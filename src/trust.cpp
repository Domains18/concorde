#include "trust.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

namespace concorde {

// ============================================================================
// TrustStore Implementation
// ============================================================================

TrustStore::TrustStore() {
    load();
}

void TrustStore::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string path = get_trust_file_path();
    std::ifstream file(path);
    
    if (!file) {
        // File doesn't exist yet - empty trust store
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Format: pubkey|device_name|fingerprint|first_seen|last_seen
        std::istringstream iss(line);
        std::string pubkey, device_name, fingerprint;
        time_t first_seen, last_seen;
        
        std::getline(iss, pubkey, '|');
        std::getline(iss, device_name, '|');
        std::getline(iss, fingerprint, '|');
        iss >> first_seen;
        iss.ignore(1); // Skip '|'
        iss >> last_seen;
        
        PeerInfo info{pubkey, device_name, fingerprint, first_seen, last_seen};
        trusted_peers_[pubkey] = info;
    }
}

void TrustStore::save() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string path = get_trust_file_path();
    std::ofstream file(path);
    
    if (!file) {
        std::cerr << "Failed to save trust store to " << path << std::endl;
        return;
    }
    
    // Write header
    file << "# Concorde Trusted Peers\n";
    file << "# Format: pubkey|device_name|fingerprint|first_seen|last_seen\n";
    file << "#\n";
    
    // Write peers
    for (const auto& [pubkey, info] : trusted_peers_) {
        file << pubkey << "|"
             << info.device_name << "|"
             << info.fingerprint << "|"
             << info.first_seen << "|"
             << info.last_seen << "\n";
    }
    
    file.close();
    chmod(path.c_str(), 0600);
}

void TrustStore::add_trusted_peer(const std::string& pubkey_hex,
                                  const std::string& device_name,
                                  const std::string& fingerprint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    time_t now = time(nullptr);
    
    PeerInfo info{
        pubkey_hex,
        device_name,
        fingerprint,
        now,    // first_seen
        now     // last_seen
    };
    
    trusted_peers_[pubkey_hex] = info;
    
    // Save immediately
    mutex_.unlock();
    save();
    mutex_.lock();
}

bool TrustStore::is_trusted(const std::string& pubkey_hex) {
    std::lock_guard<std::mutex> lock(mutex_);
    return trusted_peers_.find(pubkey_hex) != trusted_peers_.end();
}

void TrustStore::update_last_seen(const std::string& pubkey_hex) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = trusted_peers_.find(pubkey_hex);
    if (it != trusted_peers_.end()) {
        it->second.last_seen = time(nullptr);
    }
}

void TrustStore::revoke(const std::string& pubkey_hex) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    trusted_peers_.erase(pubkey_hex);
    
    // Save immediately
    mutex_.unlock();
    save();
    mutex_.lock();
}

const PeerInfo* TrustStore::get_peer_info(const std::string& pubkey_hex) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = trusted_peers_.find(pubkey_hex);
    if (it != trusted_peers_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::map<std::string, PeerInfo> TrustStore::get_all_peers() {
    std::lock_guard<std::mutex> lock(mutex_);
    return trusted_peers_;
}

size_t TrustStore::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return trusted_peers_.size();
}

std::string TrustStore::get_trust_file_path() {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw->pw_dir;
    }
    
    std::string config_dir = std::string(home) + "/.concorde";
    
    // Create directory if it doesn't exist
    mkdir(config_dir.c_str(), 0700);
    
    return config_dir + "/trusted_peers";
}

// ============================================================================
// PairingManager Implementation
// ============================================================================

PairingManager::PairingManager(TrustStore& trust_store)
    : trust_store_(trust_store) {
}

bool PairingManager::request_approval(const std::string& device_name,
                                      const std::string& pubkey_hex,
                                      const std::string& fingerprint) {
    std::cout << "\n╔════════════════════════════════════════╗\n";
    std::cout << "║  🔐 New Device Wants to Connect       ║\n";
    std::cout << "╚════════════════════════════════════════╝\n\n";
    
    std::cout << "  Device:      " << device_name << "\n";
    std::cout << "  Fingerprint: " << fingerprint << "\n\n";
    
    std::cout << "Trust this device? [y/n]: ";
    std::cout.flush();
    
    std::string response;
    std::getline(std::cin, response);
    
    if (response == "y" || response == "Y" || response == "yes") {
        trust_store_.add_trusted_peer(pubkey_hex, device_name, fingerprint);
        std::cout << "\n✅ Device trusted! Future connections will auto-authenticate.\n\n";
        return true;
    } else {
        std::cout << "\n❌ Device rejected.\n\n";
        return false;
    }
}

bool PairingManager::auto_approve_if_trusted(const std::string& pubkey_hex) {
    return trust_store_.is_trusted(pubkey_hex);
}

} // namespace concorde
