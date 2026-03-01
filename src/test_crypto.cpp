#include "crypto.h"
#include "trust.h"
#include <iostream>

using namespace concorde;

int main() {
    std::cout << "🔐 Concorde Crypto Layer Test\n\n";
    
    // Test 1: Device Identity
    std::cout << "1️⃣  Testing Device Identity...\n";
    auto identity = DeviceIdentity::load_or_create();
    std::cout << "   Device: " << identity.get_device_name() << "\n";
    std::cout << "   Fingerprint: " << identity.get_fingerprint() << "\n";
    std::cout << "   Public Key: " << identity.get_public_key_hex().substr(0, 16) << "...\n";
    
    // Test 2: Signing and Verification
    std::cout << "\n2️⃣  Testing Signing...\n";
    std::string message = "Hello from Concorde!";
    std::string signature = identity.sign(message);
    std::cout << "   Message: " << message << "\n";
    std::cout << "   Signature: " << signature.substr(0, 32) << "...\n";
    
    bool valid = CryptoUtils::verify_signature(
        identity.get_public_key_hex(),
        message,
        signature
    );
    std::cout << "   Verification: " << (valid ? "✅ Valid" : "❌ Invalid") << "\n";
    
    // Test 3: Nonce Generation
    std::cout << "\n3️⃣  Testing Nonce Generation...\n";
    std::string nonce1 = CryptoUtils::generate_nonce();
    std::string nonce2 = CryptoUtils::generate_nonce();
    std::cout << "   Nonce 1: " << nonce1.substr(0, 16) << "...\n";
    std::cout << "   Nonce 2: " << nonce2.substr(0, 16) << "...\n";
    std::cout << "   Different: " << (nonce1 != nonce2 ? "✅ Yes" : "❌ No") << "\n";
    
    // Test 4: Trust Store
    std::cout << "\n4️⃣  Testing Trust Store...\n";
    TrustStore trust;
    std::cout << "   Trusted peers: " << trust.count() << "\n";
    
    // Add a test peer
    std::string test_pubkey = "abcdef1234567890abcdef1234567890";
    std::string test_fingerprint = "SHA256:test123";
    trust.add_trusted_peer(test_pubkey, "Test-Device", test_fingerprint);
    std::cout << "   After adding test peer: " << trust.count() << "\n";
    
    bool is_trusted = trust.is_trusted(test_pubkey);
    std::cout << "   Test peer trusted: " << (is_trusted ? "✅ Yes" : "❌ No") << "\n";
    
    // Test 5: SHA256
    std::cout << "\n5️⃣  Testing SHA256...\n";
    std::string hash = CryptoUtils::sha256("Concorde");
    std::cout << "   SHA256('Concorde'): " << hash.substr(0, 16) << "...\n";
    
    std::cout << "\n✅ All tests completed!\n";
    std::cout << "\nConfiguration stored in: ~/.concorde/\n";
    std::cout << "  - device.key (private key)\n";
    std::cout << "  - device.pub (public key)\n";
    std::cout << "  - trusted_peers (trust store)\n";
    
    return 0;
}
