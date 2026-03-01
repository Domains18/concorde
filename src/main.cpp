#include "main.h"
#include "crypto.h"
#include "trust.h"
#include <chrono>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

FileServer::FileServer(DiscoveryService& discovery, 
                      int port, 
                      std::string root_dir,
                      concorde::TrustStore& trust)
    : discovery_(discovery), port_(port), root_dir_(root_dir), trust_(trust)
{
    setupRoutes();
}

void FileServer::run()
{
    std::cout << "starting http server on port " << port_ << "..." << std::endl;
    app_.port(port_).multithreaded().run();
}

void FileServer::setupRoutes()
{
    // Helper function to verify request authentication with nonce-based challenge-response
    auto verify_auth = [this](const crow::request& req) -> std::pair<bool, std::string> {
        std::string pubkey = req.get_header_value("X-Pubkey");
        std::string signature = req.get_header_value("X-Signature");
        std::string nonce = req.get_header_value("X-Nonce");

        if (pubkey.empty() || signature.empty() || nonce.empty())
        {
            return {false, "Missing authentication headers (X-Pubkey, X-Signature, X-Nonce)"};
        }

        // Check if trusted
        if (!trust_.is_trusted(pubkey)) {
            return {false, "Device not trusted"};
        }

        // Verify and consume nonce (prevents replay attacks)
        if (!consume_nonce(nonce, pubkey))
        {
            return {false, "Invalid or expired nonce"};
        }

        // Verify signature over nonce + URL
        std::string challenge = nonce + req.url;
        if (!concorde::CryptoUtils::verify_signature(pubkey, challenge, signature)) {
            return {false, "Invalid signature"};
        }
        
        return {true, ""};
    };

    // Challenge endpoint - issue nonce for authentication
    CROW_ROUTE(app_, "/api/challenge")
    (
        [this](const crow::request& req)
        {
            cleanup_expired_nonces();

            std::string pubkey = req.get_header_value("X-Pubkey");
            if (pubkey.empty())
            {
                return crow::response(400, "Missing X-Pubkey header");
            }

            // Check if device is trusted
            if (!trust_.is_trusted(pubkey))
            {
                return crow::response(403, "Device not trusted");
            }

            std::string nonce = generate_challenge(pubkey);
            crow::json::wvalue result;
            result["nonce"] = nonce;
            result["expires_in"] = NONCE_LIFETIME.count();
            return crow::response(result);
        });

    // Public endpoint - list local files (authenticated)
    CROW_ROUTE(app_, "/api/files")
    (
        [this, verify_auth](const crow::request& req)
        {
            auto [authenticated, error] = verify_auth(req);
            if (!authenticated) {
                return crow::response(403, "Unauthorized: " + error);
            }
            
            std::vector<std::string> files;
            if (fs::exists(root_dir_) && fs::is_directory(root_dir_))
            {
                for (const auto& entry : fs::directory_iterator(root_dir_))
                {
                    if (fs::is_regular_file(entry.status()))
                    {
                        files.push_back(entry.path().filename().string());
                    }
                }
            }
            crow::json::wvalue result;
            result["files"] = files;
            return crow::response(result);
        });

    // Get network peers (only trusted ones)
    CROW_ROUTE(app_, "/api/peers")
    (
        [this]()
        {
            auto peers = discovery_.getTrustedPeers();
            crow::json::wvalue json_peers;
            int i = 0;

            for (const auto& [ip, peer] : peers)
            {
                json_peers[i]["ip"] = peer.ip_address;
                json_peers[i]["name"] = peer.device_name;
                json_peers[i]["port"] = peer.http_port;
                json_peers[i]["fingerprint"] = peer.fingerprint;
                json_peers[i]["shares"] = peer.shared_folders;
                i++;
            }
            return crow::response(json_peers);
        });

    // Upload handling (authenticated)
    CROW_ROUTE(app_, "/upload")
        .methods(crow::HTTPMethod::POST)(
            [this, verify_auth](const crow::request& req)
            {
                auto [authenticated, error] = verify_auth(req);
                if (!authenticated) {
                    return crow::response(403, "Unauthorized: " + error);
                }
                
                crow::multipart::message file_message(req);
                for (const auto& part : file_message.parts)
                {
                    const auto& part_value = part.body;
                    std::string filename = "uploaded_" + std::to_string(std::time(nullptr));
                    std::ofstream out(root_dir_ + "/" + filename, std::ios::binary);
                    out << part_value;
                    out.close();
                }
                return crow::response(200, "File uploaded successfully");
            });

    // Download handling (authenticated)
    CROW_ROUTE(app_, "/download/<string>")
    ([this, verify_auth]([[maybe_unused]] const crow::request& req, 
                        crow::response& res, 
                        std::string filename){
        auto [authenticated, error] = verify_auth(req);
        if (!authenticated) {
            res.code = 403;
            res.write("Unauthorized: " + error);
            res.end();
            return;
        }
        
        std::string path = root_dir_ + "/" + filename;
        if(fs::exists(path)){
            res.set_static_file_info(path);
        } else {
            res.code = 404;
            res.write("File not found");
        }
        res.end();
    });

    // Public web UI (no auth - just info page)
    CROW_ROUTE(app_, "/")
    (
        []()
        {
            return "<h1>🏰 Concorde - Secure P2P File Sharing</h1>"
                   "<h2>Authenticated Endpoints:</h2>"
                   "<p>🔒 GET /api/files - List local files (requires auth)</p>"
                   "<p>🔒 GET /download/&lt;filename&gt; - Download file (requires auth)</p>"
                   "<p>🔒 POST /upload - Upload file (requires auth)</p>"
                   "<h2>Public Endpoints:</h2>"
                   "<p>GET /api/peers - List trusted network peers</p>"
                   "<p>GET /api/challenge - Get nonce for authentication (requires X-Pubkey "
                   "header)</p>"
                   "<hr>"
                   "<p><b>Authentication:</b> Challenge-response with Ed25519 signatures and "
                   "time-limited nonces.</p>"
                   "<p><b>Flow:</b> 1) Request nonce from /api/challenge, 2) Sign nonce+URL, 3) "
                   "Send request with X-Nonce, X-Pubkey, X-Signature headers.</p>"
                   "<p>Pair devices using the Concorde CLI to enable file sharing.</p>";
        });
}

// Main entry point
int main() {
    std::cout << "🏰 Concorde - Secure P2P File Sharing\n";
    std::cout << "=====================================\n\n";
    
    // Load device identity
    concorde::DeviceIdentity identity = concorde::DeviceIdentity::load_or_create();
    std::cout << "✅ Device: " << identity.get_device_name() << "\n";
    std::cout << "🔑 Fingerprint: " << identity.get_fingerprint() << "\n\n";
    
    // Initialize trust store
    concorde::TrustStore trust;
    std::cout << "👥 Trusted peers: " << trust.count() << "\n\n";
    
    // Start discovery service
    int broadcast_port = 9000;
    int http_port = 8080;
    DiscoveryService discovery(broadcast_port, http_port, 
                               identity.get_device_name(), identity, trust);
    discovery.start();
    std::cout << "📡 Discovery: Broadcasting on port " << broadcast_port << "\n";
    
    // Start file server
    FileServer server(discovery, http_port, "./shared", trust);
    std::cout << "📁 Shared folder: ./shared\n";
    std::cout << "🌐 HTTP server: http://0.0.0.0:" << http_port << "\n\n";
    std::cout << "🔒 All file operations require authentication\n";
    std::cout << "   Using challenge-response with time-limited nonces\n";
    std::cout << "   New devices will prompt for pairing approval\n\n";
    
    server.run();
    
    return 0;
}

// Nonce management implementation
std::string FileServer::generate_challenge(const std::string& pubkey)
{
    std::lock_guard<std::mutex> lock(nonce_mutex_);

    std::string nonce = concorde::CryptoUtils::generate_nonce();
    auto expires_at = std::chrono::steady_clock::now() + NONCE_LIFETIME;

    active_nonces_[nonce] = {expires_at, pubkey};
    return nonce;
}

bool FileServer::consume_nonce(const std::string& nonce, const std::string& pubkey)
{
    std::lock_guard<std::mutex> lock(nonce_mutex_);

    auto it = active_nonces_.find(nonce);
    if (it == active_nonces_.end())
    {
        return false; // Nonce not found
    }

    // Check if nonce is expired
    if (std::chrono::steady_clock::now() > it->second.expires_at)
    {
        active_nonces_.erase(it);
        return false;
    }

    // Check if nonce was issued for this pubkey
    if (it->second.pubkey != pubkey)
    {
        return false;
    }

    // Consume nonce (single use)
    active_nonces_.erase(it);
    return true;
}

void FileServer::cleanup_expired_nonces()
{
    std::lock_guard<std::mutex> lock(nonce_mutex_);

    auto now = std::chrono::steady_clock::now();
    for (auto it = active_nonces_.begin(); it != active_nonces_.end();)
    {
        if (now > it->second.expires_at)
        {
            it = active_nonces_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
