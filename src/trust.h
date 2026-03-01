#ifndef TRUST_H
#define TRUST_H

#include <string>
#include <map>
#include <mutex>
#include <ctime>

namespace concorde {

/**
 * Information about a trusted peer
 */
struct PeerInfo {
    std::string public_key;      // Hex-encoded Ed25519 public key
    std::string device_name;     // Display name
    std::string fingerprint;     // SHA256:... for display
    time_t first_seen;           // When first trusted
    time_t last_seen;            // Last successful authentication
};

/**
 * Trust store - manages list of trusted peer devices
 * Thread-safe for concurrent access from UDP discovery and HTTP server
 */
class TrustStore {
public:
    TrustStore();
    
    /**
     * Load trusted peers from ~/.concorde/trusted_peers
     */
    void load();
    
    /**
     * Save trusted peers to ~/.concorde/trusted_peers
     */
    void save();
    
    /**
     * Add a new trusted peer
     * @param pubkey_hex Hex-encoded public key
     * @param device_name Device display name
     * @param fingerprint SHA256:... fingerprint
     */
    void add_trusted_peer(const std::string& pubkey_hex,
                         const std::string& device_name,
                         const std::string& fingerprint);
    
    /**
     * Check if a public key is trusted
     */
    bool is_trusted(const std::string& pubkey_hex);
    
    /**
     * Update last_seen timestamp for a peer
     */
    void update_last_seen(const std::string& pubkey_hex);
    
    /**
     * Remove a trusted peer (revoke trust)
     */
    void revoke(const std::string& pubkey_hex);
    
    /**
     * Get information about a trusted peer
     * Returns nullptr if not found
     */
    const PeerInfo* get_peer_info(const std::string& pubkey_hex);
    
    /**
     * Get all trusted peers
     */
    std::map<std::string, PeerInfo> get_all_peers();
    
    /**
     * Get number of trusted peers
     */
    size_t count() const;

private:
    std::map<std::string, PeerInfo> trusted_peers_;
    mutable std::mutex mutex_;
    
    static std::string get_trust_file_path();
};

/**
 * Pairing manager - handles interactive device pairing
 */
class PairingManager {
public:
    PairingManager(TrustStore& trust_store);
    
    /**
     * Request user approval for a new device
     * Shows fingerprint and asks for confirmation
     * @return true if user approved, false if rejected
     */
    bool request_approval(const std::string& device_name,
                         const std::string& pubkey_hex,
                         const std::string& fingerprint);
    
    /**
     * Auto-approve if public key is already trusted
     * @return true if already trusted
     */
    bool auto_approve_if_trusted(const std::string& pubkey_hex);

private:
    TrustStore& trust_store_;
};

} // namespace concorde

#endif // TRUST_H
