# Concorde Crypto Layer

Secure device-to-device trust using **Ed25519 public key cryptography**.

## Overview

Concorde uses a **SSH-style TOFU (Trust On First Use)** model:
1. Each device generates an Ed25519 keypair on first launch
2. Devices discover each other via UDP broadcasts
3. User manually approves new devices (pairing)
4. Future connections auto-authenticate using challenge-response

## Architecture

### Device Identity (`DeviceIdentity`)
- **Ed25519 keypair** (32-byte public key, 64-byte private key)
- **Fingerprint** (SHA256 hash, displayed as `SHA256:a3f2c9...`)
- **Signing** (sign data with private key for authentication)

### Trust Store (`TrustStore`)
- **Thread-safe** map of trusted public keys
- **Persistent storage** in `~/.concorde/trusted_peers`
- **Auto-save** on trust changes

### Pairing Manager (`PairingManager`)
- **Interactive approval** (shows fingerprint, asks user to confirm)
- **Auto-approval** for already-trusted devices

## File Structure

```
~/.concorde/
├── device.key        (private key, chmod 600)
├── device.pub        (public key, chmod 644)
└── trusted_peers     (list of trusted devices)
```

## Usage Example

```cpp
#include "crypto.h"
#include "trust.h"

// Load or create device identity
auto identity = DeviceIdentity::load_or_create();
std::cout << "Device: " << identity.get_device_name() << "\n";
std::cout << "Fingerprint: " << identity.get_fingerprint() << "\n";

// Sign a message
std::string message = "Hello!";
std::string signature = identity.sign(message);

// Verify signature from another device
bool valid = CryptoUtils::verify_signature(
    peer_pubkey,    // Their public key
    message,
    signature
);

// Trust management
TrustStore trust;
trust.add_trusted_peer(peer_pubkey, "MacBook", "SHA256:abc123");

if (trust.is_trusted(peer_pubkey)) {
    // Allow file transfer
}
```

## Challenge-Response Flow

```
Client                           Server
  |                                |
  |-- "Give me /file.mp4" -------->|
  |                                |
  |<------- nonce ----------------|
  |                                |
  |-- sign(nonce + pubkey) ------->|
  |                                |
  |   [Server verifies signature]  |
  |                                |
  |<------- file stream ----------|
```

## Security Properties

✅ **Authenticity** - Ed25519 signatures prevent impersonation  
✅ **Integrity** - SHA256 fingerprints detect tampering  
✅ **Replay Protection** - Nonces prevent replay attacks  
✅ **Forward Secrecy** - (Future: ephemeral keys for file transfer)

## Dependencies

- **libsodium** (modern crypto library)
  ```bash
  # macOS
  brew install libsodium
  
  # Ubuntu/Debian
  apt install libsodium-dev
  
  # Termux (Android)
  pkg install libsodium
  ```

## Building

```bash
mkdir build && cd build
cmake ..
make

# Run crypto test
./test_crypto
```

## Testing

```bash
./test_crypto
```

Expected output:
```
🔐 Concorde Crypto Layer Test

1️⃣  Testing Device Identity...
   Device: pop-os
   Fingerprint: SHA256:a3f2c9...
   Public Key: 5f3e8b2a...

2️⃣  Testing Signing...
   Message: Hello from Concorde!
   Signature: 8a9c7e1f...
   Verification: ✅ Valid

3️⃣  Testing Nonce Generation...
   Nonce 1: 7b4d9f2a...
   Nonce 2: 1c8e5a3b...
   Different: ✅ Yes

4️⃣  Testing Trust Store...
   Trusted peers: 0
   After adding test peer: 1
   Test peer trusted: ✅ Yes

5️⃣  Testing SHA256...
   SHA256('Concorde'): 3a7f9e1b...

✅ All tests completed!
```

## Integration with Discovery Layer

The UDP beacon will include the public key fingerprint:

```json
{
  "device_name": "MacBook",
  "port": 8080,
  "fingerprint": "SHA256:a3f2c9...",
  "shares": ["work", "media"],
  "signature": "..."
}
```

When a new device is discovered:
1. Check if `fingerprint` is in trust store
2. If yes → auto-approve, allow connections
3. If no → show pairing prompt, ask user to approve

## Future Enhancements

- [ ] QR code pairing (for mobile devices)
- [ ] Import SSH keys (`~/.ssh/id_ed25519`)
- [ ] Trust expiry (re-verify after 30 days)
- [ ] Revocation broadcast (tell peers to untrust)
- [ ] Certificate pinning (pin specific device versions)
