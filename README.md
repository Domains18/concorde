# Concorde

**Secure P2P file sharing with automatic device discovery**

A local network file sharing system that combines the ease of AirDrop with the security of SSH.

## Features

✅ **Zero Configuration** - No IPs to remember, devices auto-discover  
✅ **Secure by Default** - Ed25519 public key authentication  
✅ **Trust On First Use** - SSH-style pairing (approve once, trust forever)  
✅ **Cross-Platform** - Works on macOS, Linux, and Android (Termux)  
✅ **Offline** - No internet or external servers required  

## Architecture

### 3-Layer Design

1. **Discovery Layer (UDP Broadcast)**
   - Devices broadcast presence on LAN (`255.255.255.255:9000`)
   - Includes: device name, service port, shared folders, public key fingerprint
   - Auto-pruning of offline devices

2. **Security Layer (Ed25519 PKI)**
   - Each device has unique keypair (generated on first launch)
   - Challenge-response authentication for all file transfers
   - Thread-safe trust store in `~/.concorde/trusted_peers`

3. **Transport Layer (HTTP over TCP)**
   - File streaming via HTTP server
   - Endpoints: `/catalog`, `/download/{folder}/{file}`, `/upload`
   - Only serves to trusted devices

## Quick Start

### Prerequisites

```bash
# macOS
brew install libsodium cmake

# Ubuntu/Debian
sudo apt install libsodium-dev cmake build-essential

# Termux (Android)
pkg install libsodium cmake clang
```

### Build

```bash
git clone https://github.com/Domains18/concorde.git
cd concorde
mkdir build && cd build
cmake ..
make

# Test crypto layer
./test_crypto

# Run daemon
./concorde
```

### First Run

On first launch, Concorde will:
1. Generate an Ed25519 keypair (`~/.concorde/device.key`, `device.pub`)
2. Start broadcasting device presence
3. Listen for other devices
4. Show pairing prompts when new devices are found

## Usage

### Pairing Devices

When a new device is discovered, you'll see:

```
╔════════════════════════════════════════╗
║  🔐 New Device Wants to Connect       ║
╚════════════════════════════════════════╝

  Device:      MacBook-Pro
  Fingerprint: SHA256:a3f2c9e8b1d4

Trust this device? [y/n]:
```

Type `y` to approve. Future connections from this device will auto-authenticate.

### Sharing Folders

Create `~/.concorde/config.json`:

```json
{
  "shares": {
    "work": "/home/user/Documents/work",
    "media": "/mnt/storage/movies"
  },
  "port": 8080
}
```

Restart Concorde. Other trusted devices will see "work" and "media" in their file browser.

## Security Model

**Trust On First Use (TOFU):**
- Like SSH, you manually approve devices the first time
- Public key fingerprints prevent impersonation
- All subsequent connections auto-authenticate

**Challenge-Response:**
```
Client → Server: "I want /file.mp4"
Server → Client: random_nonce
Client → Server: signature(nonce + pubkey)
Server: Verifies signature → Serves file
```

**File Structure:**
```
~/.concorde/
├── device.key         (private key, chmod 600)
├── device.pub         (public key, chmod 644)
├── trusted_peers      (list of approved devices)
└── config.json        (shared folders config)
```

## Development Status

- [x] Discovery layer (UDP broadcast)
- [x] Crypto layer (Ed25519 signing)
- [x] Trust management
- [x] Test suite
- [ ] HTTP server integration
- [ ] Config file parsing
- [ ] File transfer endpoints
- [ ] Web UI (frontend)
- [ ] Mobile app (optional)

## Documentation

- [CRYPTO.md](CRYPTO.md) - Cryptography implementation details
- [Architecture Overview](#architecture) - System design

## Contributing

Contributions welcome! This is a personal project for secure LAN file sharing.

## License

MIT

---

## Technical Details

### Discovery Packet Format (JSON)

```json
{
  "device_name": "MacBook",
  "ip": "192.168.1.100",
  "port": 8080,
  "fingerprint": "SHA256:a3f2c9...",
  "shares": ["work", "media"],
  "timestamp": 1234567890,
  "signature": "..."
}
```

### Why Ed25519?

- **Fast** - Faster than RSA for signing/verification
- **Secure** - 128-bit security level (equivalent to 3072-bit RSA)
- **Small** - 32-byte public keys, 64-byte signatures
- **Modern** - Resistant to timing attacks
- **Widely supported** - libsodium, OpenSSL 1.1+

### Dependencies

- **libsodium** - Crypto primitives (Ed25519, SHA256, random)
- **CMake** - Build system
- **C++17** - Modern C++ features (std::filesystem, etc.)

### Platforms Tested

- ✅ macOS (M1/Intel)
- ✅ Ubuntu 22.04 LTS
- 🔄 Termux on Android (pending)

## Inspiration

- **AirDrop** - Zero-config device discovery
- **SSH** - Public key authentication
- **Syncthing** - Decentralized file sync
- **Magic Wormhole** - Secure file transfer
