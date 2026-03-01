#include "crypto.h"
#include <sodium.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <cstring>

namespace concorde {

// ============================================================================
// DeviceIdentity Implementation
// ============================================================================

DeviceIdentity::DeviceIdentity(Ed25519KeyPair keypair, const std::string& device_name)
    : keypair_(std::move(keypair)), device_name_(device_name) {
}

DeviceIdentity DeviceIdentity::load_or_create() {
    // Initialize libsodium
    if (sodium_init() < 0) {
        throw std::runtime_error("Failed to initialize libsodium");
    }
    
    std::string config_dir = get_config_dir();
    std::string privkey_path = config_dir + "/device.key";
    std::string pubkey_path = config_dir + "/device.pub";
    
    Ed25519KeyPair keypair;
    
    // Check if keys exist
    struct stat buffer;
    bool keys_exist = (stat(privkey_path.c_str(), &buffer) == 0) &&
                      (stat(pubkey_path.c_str(), &buffer) == 0);
    
    if (keys_exist) {
        keypair = load_keypair();
    } else {
        // Create config directory if it doesn't exist
        mkdir(config_dir.c_str(), 0700);
        
        // Generate new keypair
        keypair = generate_keypair();
        save_keypair(keypair);
    }
    
    // Get hostname
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    
    return DeviceIdentity(keypair, std::string(hostname));
}

std::string DeviceIdentity::get_fingerprint() const {
    std::string hash = CryptoUtils::sha256(
        std::string(keypair_.public_key.begin(), keypair_.public_key.end())
    );
    // Return SHA256: prefix + first 12 chars
    return "SHA256:" + hash.substr(0, 12);
}

std::string DeviceIdentity::get_public_key_hex() const {
    return CryptoUtils::to_hex(keypair_.public_key);
}

std::string DeviceIdentity::sign(const std::string& data) const {
    std::vector<uint8_t> signature(crypto_sign_BYTES);
    
    crypto_sign_detached(
        signature.data(),
        nullptr,
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size(),
        keypair_.private_key.data()
    );
    
    return CryptoUtils::to_hex(signature);
}

std::string DeviceIdentity::get_device_name() const {
    return device_name_;
}

Ed25519KeyPair DeviceIdentity::generate_keypair() {
    Ed25519KeyPair keypair;
    keypair.public_key.resize(crypto_sign_PUBLICKEYBYTES);
    keypair.private_key.resize(crypto_sign_SECRETKEYBYTES);
    
    crypto_sign_keypair(
        keypair.public_key.data(),
        keypair.private_key.data()
    );
    
    return keypair;
}

Ed25519KeyPair DeviceIdentity::load_keypair() {
    std::string config_dir = get_config_dir();
    
    Ed25519KeyPair keypair;
    keypair.public_key.resize(crypto_sign_PUBLICKEYBYTES);
    keypair.private_key.resize(crypto_sign_SECRETKEYBYTES);
    
    // Load private key
    std::ifstream privkey_file(config_dir + "/device.key", std::ios::binary);
    if (!privkey_file) {
        throw std::runtime_error("Failed to open private key file");
    }
    privkey_file.read(reinterpret_cast<char*>(keypair.private_key.data()), 
                      crypto_sign_SECRETKEYBYTES);
    
    // Load public key
    std::ifstream pubkey_file(config_dir + "/device.pub", std::ios::binary);
    if (!pubkey_file) {
        throw std::runtime_error("Failed to open public key file");
    }
    pubkey_file.read(reinterpret_cast<char*>(keypair.public_key.data()), 
                     crypto_sign_PUBLICKEYBYTES);
    
    return keypair;
}

void DeviceIdentity::save_keypair(const Ed25519KeyPair& keypair) {
    std::string config_dir = get_config_dir();
    
    // Save private key (chmod 600)
    std::string privkey_path = config_dir + "/device.key";
    std::ofstream privkey_file(privkey_path, std::ios::binary);
    if (!privkey_file) {
        throw std::runtime_error("Failed to create private key file");
    }
    privkey_file.write(reinterpret_cast<const char*>(keypair.private_key.data()),
                       crypto_sign_SECRETKEYBYTES);
    privkey_file.close();
    chmod(privkey_path.c_str(), 0600);
    
    // Save public key (chmod 644)
    std::string pubkey_path = config_dir + "/device.pub";
    std::ofstream pubkey_file(pubkey_path, std::ios::binary);
    if (!pubkey_file) {
        throw std::runtime_error("Failed to create public key file");
    }
    pubkey_file.write(reinterpret_cast<const char*>(keypair.public_key.data()),
                      crypto_sign_PUBLICKEYBYTES);
    pubkey_file.close();
    chmod(pubkey_path.c_str(), 0644);
}

std::string DeviceIdentity::get_config_dir() {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw->pw_dir;
    }
    return std::string(home) + "/.concorde";
}

// ============================================================================
// CryptoUtils Implementation
// ============================================================================

bool CryptoUtils::verify_signature(const std::string& pubkey_hex,
                                   const std::string& data,
                                   const std::string& signature_hex) {
    auto pubkey = from_hex(pubkey_hex);
    auto signature = from_hex(signature_hex);
    
    if (pubkey.size() != crypto_sign_PUBLICKEYBYTES) {
        return false;
    }
    
    if (signature.size() != crypto_sign_BYTES) {
        return false;
    }
    
    int result = crypto_sign_verify_detached(
        signature.data(),
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size(),
        pubkey.data()
    );
    
    return result == 0;
}

std::string CryptoUtils::generate_nonce() {
    std::vector<uint8_t> nonce(32);
    randombytes_buf(nonce.data(), nonce.size());
    return to_hex(nonce);
}

std::string CryptoUtils::sha256(const std::string& data) {
    std::vector<uint8_t> hash(crypto_hash_sha256_BYTES);
    
    crypto_hash_sha256(
        hash.data(),
        reinterpret_cast<const uint8_t*>(data.data()),
        data.size()
    );
    
    return to_hex(hash);
}

std::string CryptoUtils::to_hex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> CryptoUtils::from_hex(const std::string& hex) {
    std::vector<uint8_t> data;
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
        data.push_back(byte);
    }
    
    return data;
}

} // namespace concorde
