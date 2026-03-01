# Concorde Development Roadmap

## ✅ Completed

### Phase 1: Security Foundation
- [x] Ed25519 keypair generation and storage
- [x] SHA256 fingerprints for device verification
- [x] Message signing and verification
- [x] Trust store with persistent storage
- [x] Interactive pairing system (SSH-style TOFU)
- [x] Challenge-response authentication
- [x] Thread-safe peer management
- [x] Crypto test suite

**Files:** `crypto.h/cpp`, `trust.h/cpp`, `test_crypto.cpp`, `CRYPTO.md`

### Phase 2: Discovery Layer (Basic)
- [x] UDP broadcast infrastructure
- [x] Peer discovery mechanism
- [x] Peer map management
- [x] Auto-pruning of offline devices

**Files:** `discovery.h/cpp`

### Phase 3: HTTP Server (Basic)
- [x] Basic Crow HTTP server
- [x] File listing endpoint (`/api/files`)
- [x] Peer listing endpoint (`/api/peers`)
- [x] Upload endpoint (`/upload`)
- [x] Download endpoint (`/download/<file>`)
- [x] Simple web UI

**Files:** `server.cpp`, `main.cpp`

## 🚧 In Progress

### Phase 4: Security Integration
**Priority: HIGH**

Integrate crypto layer with existing components:

- [ ] **UDP Beacon Signing**
  - Add `signature` field to discovery packets
  - Sign beacon with device private key
  - Verify signatures on received beacons
  - Reject unsigned or invalid beacons

- [ ] **HTTP Authentication**
  - Add `X-Pubkey` and `X-Signature` headers
  - Implement challenge-response for file downloads
  - Verify client public key against trust store
  - Reject requests from untrusted devices

- [ ] **Interactive Pairing**
  - Show pairing prompt when unknown device discovered
  - Display fingerprint for verification
  - Add to trust store on approval
  - Persist trust decisions

**Estimated effort:** 2-3 hours

### Phase 5: Configuration System
**Priority: MEDIUM**

- [ ] **Config File (`~/.concorde/config.json`)**
  ```json
  {
    "shares": {
      "work": "/home/user/Documents/work",
      "media": "/mnt/storage/movies"
    },
    "port": 8080,
    "discovery_port": 9000,
    "broadcast_interval": 5
  }
  ```

- [ ] **Config Parser**
  - Load config on startup
  - Validate paths exist
  - Create default config if missing
  - Support multiple share aliases

- [ ] **Share Announcements**
  - Include share list in UDP beacons
  - Update discovery packet format
  - Display shared folders in web UI

**Estimated effort:** 2 hours

## 📋 Remaining Features

### Phase 6: Enhanced File Transfer
**Priority: MEDIUM**

- [ ] **Progress Tracking**
  - Show upload/download progress
  - Estimate time remaining
  - Handle large files (>1GB)

- [ ] **Resumable Downloads**
  - Support HTTP Range requests
  - Resume interrupted transfers
  - Checksum verification

- [ ] **Directory Browsing**
  - List subdirectories
  - Navigate folder structure
  - Download entire folders (as zip)

**Estimated effort:** 3-4 hours

### Phase 7: Web UI Improvements
**Priority: LOW**

- [ ] **Modern Frontend**
  - Replace basic HTML with React/Vue
  - Drag-and-drop uploads
  - Real-time peer updates (WebSocket)
  - File preview (images, videos)

- [ ] **Mobile-Friendly**
  - Responsive design
  - Touch-optimized controls
  - QR code pairing

**Estimated effort:** 8-10 hours

### Phase 8: Advanced Security
**Priority: LOW**

- [ ] **Trust Expiry**
  - Re-verify devices after 30 days
  - Prompt for re-approval
  - Revocation broadcast

- [ ] **SSH Key Import**
  - Reuse existing `~/.ssh/id_ed25519`
  - No new keypair needed
  - Leverage existing trust

- [ ] **Encrypted Transfers**
  - Optional TLS for file transfer
  - Ephemeral keys for forward secrecy
  - End-to-end encryption

**Estimated effort:** 6-8 hours

### Phase 9: Cross-Platform Testing
**Priority: MEDIUM**

- [ ] **macOS**
  - Build and test on M1/Intel
  - Homebrew installation
  - LaunchAgent daemon

- [ ] **Android (Termux)**
  - Build with Termux environment
  - Background service
  - Notification support

- [ ] **Windows (Optional)**
  - MinGW build
  - Windows Service
  - MSI installer

**Estimated effort:** 4-6 hours per platform

### Phase 10: Deployment & Distribution
**Priority: LOW**

- [ ] **Packaging**
  - Homebrew formula (macOS)
  - .deb package (Ubuntu/Debian)
  - AppImage (Linux)
  - APK (Android via Termux)

- [ ] **Documentation**
  - User guide
  - Setup instructions
  - Troubleshooting FAQ
  - API documentation

- [ ] **CI/CD**
  - GitHub Actions build
  - Automated testing
  - Release automation

**Estimated effort:** 4-5 hours

## 🎯 Next Immediate Steps

### 1. Security Integration (TODAY)
The most critical missing piece is connecting the crypto layer to the network layer.

**What to do:**
1. Modify `discovery.cpp` to sign UDP beacons
2. Modify `discovery.cpp` to verify incoming beacons
3. Add pairing prompt when unknown device discovered
4. Modify `main.cpp` HTTP endpoints to require authentication
5. Test pairing between two devices

**Files to modify:**
- `src/discovery.cpp` - Add signing/verification
- `src/main.cpp` - Add HTTP auth headers
- `src/discovery.h` - Add signature to PeerInfo struct

### 2. Configuration System (TOMORROW)
Allow users to configure shared folders.

**What to do:**
1. Create config parser
2. Load config in `main()`
3. Pass shared folders to FileServer
4. Update discovery beacon with shares

### 3. Testing & Polish (NEXT WEEK)
- Test on multiple devices
- Fix bugs
- Improve error messages
- Add logging

## 📊 Progress Summary

| Phase | Status | Completion |
|-------|--------|------------|
| Security Foundation | ✅ Complete | 100% |
| Discovery Layer | ✅ Complete | 100% |
| HTTP Server | ✅ Complete | 100% |
| Security Integration | 🚧 In Progress | 0% |
| Configuration | 📋 Planned | 0% |
| Enhanced File Transfer | 📋 Planned | 0% |
| Web UI | 📋 Planned | 0% |
| Advanced Security | 📋 Planned | 0% |
| Cross-Platform | 📋 Planned | 0% |
| Deployment | 📋 Planned | 0% |

**Overall: ~35% complete**

## 🎉 What Works Right Now

You can run Concorde and it will:
- ✅ Generate Ed25519 keypair
- ✅ Broadcast device presence on LAN
- ✅ Discover other devices
- ✅ Serve files via HTTP
- ✅ List local files
- ✅ Upload/download files

**What's missing:**
- ❌ Authentication (anyone can access files)
- ❌ Pairing (no trust verification)
- ❌ Configuration (hardcoded paths)

## 🚀 How to Test Now

```bash
cd ~/Documents/github/tools/concorde/build

# Create shared folder
mkdir -p shared
echo "Hello from device 1!" > shared/test.txt

# Run Concorde
./concorde

# In browser: http://localhost:8080
# You'll see the web UI and can browse files
```

**From another device on the same network:**
```bash
# Discover peers
curl http://<ip>:8080/api/peers

# Download file
curl http://<ip>:8080/download/test.txt
```

**⚠️ Warning:** Currently NO authentication - anyone on your network can access files!
That's why Phase 4 (Security Integration) is the next priority.
