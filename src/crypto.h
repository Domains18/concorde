#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>
#include <vector>
#include <cstdint>

namespace concorde {

/**
 * Ed25519 keypair for device identity
 */
struct Ed25519KeyPair {
    std::vector<uint8_t> public_key;   // 32 bytes
    std::vector<uint8_t> private_key;  // 64 bytes (seed + pubkey)
};

/**
 * Device identity management - handles keypair generation, storage, and signing
 */
class DeviceIdentity {
public:
    /**
     * Load existing identity or create new one
     * Keys stored in ~/.concorde/device.key and ~/.concorde/device.pub
     */
    static DeviceIdentity load_or_create();
    
    /**
     * Get SHA256 fingerprint of public key (for display/verification)
     * Format: "SHA256:a3f2c9..." (truncated to 12 chars for UX)
     */
    std::string get_fingerprint() const;
    
    /**
     * Get full public key (hex-encoded)
     */
    std::string get_public_key_hex() const;
    
    /**
     * Sign data with private key
     * Returns hex-encoded signature
     */
    std::string sign(const std::string& data) const;
    
    /**
     * Get device name (hostname)
     */
    std::string get_device_name() const;

private:
    DeviceIdentity(Ed25519KeyPair keypair, const std::string& device_name);
    
    /**
     * Generate new Ed25519 keypair
     */
    static Ed25519KeyPair generate_keypair();
    
    /**
     * Load keypair from files
     */
    static Ed25519KeyPair load_keypair();
    
    /**
     * Save keypair to files
     */
    static void save_keypair(const Ed25519KeyPair& keypair);
    
    /**
     * Get concorde config directory (~/.concorde)
     */
    static std::string get_config_dir();
    
    Ed25519KeyPair keypair_;
    std::string device_name_;
};

/**
 * Crypto utility functions
 */
class CryptoUtils {
public:
    /**
     * Verify Ed25519 signature
     * @param pubkey_hex Hex-encoded public key
     * @param data Data that was signed
     * @param signature_hex Hex-encoded signature
     */
    static bool verify_signature(const std::string& pubkey_hex, 
                                 const std::string& data,
                                 const std::string& signature_hex);
    
    /**
     * Generate random nonce for challenge-response
     */
    static std::string generate_nonce();
    
    /**
     * SHA256 hash of data (hex-encoded)
     */
    static std::string sha256(const std::string& data);
    
    /**
     * Hex encode binary data
     */
    static std::string to_hex(const std::vector<uint8_t>& data);
    
    /**
     * Hex decode to binary data
     */
    static std::vector<uint8_t> from_hex(const std::string& hex);
};

} // namespace concorde

#endif // CRYPTO_H
