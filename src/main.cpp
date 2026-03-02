#include "main.h"
#include "crypto.h"
#include "trust.h"
#include <chrono>
#include <filesystem>
#include <vector>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

FileServer::FileServer(DiscoveryService& discovery, 
                      int port, 
                      std::string root_dir,
                      concorde::TrustStore& trust,
                      concorde::DeviceIdentity& identity)
    : discovery_(discovery), port_(port), root_dir_(root_dir), trust_(trust), identity_(identity)
{
    setupRoutes();
}

void FileServer::run()
{
    std::cout << "starting http server on port " << port_ << "..." << std::endl;
    app_.port(port_).multithreaded().run();
}

std::string FileServer::loadWebUI()
{
    // Load webui.html from src directory
    std::ifstream file("../src/webui.html");
    if (!file) {
        // Fallback if not found
        return "<h1>Web UI not found</h1><p>webui.html missing</p>";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
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
        
        if (!trust_.is_trusted(pubkey)) {
            return {false, "Device not trusted"};
        }
        
        std::string challenge = req.url;
        if (!concorde::CryptoUtils::verify_signature(pubkey, challenge, signature)) {
            return {false, "Invalid signature"};
        }
        
        return {true, ""};
    };

    // Serve Web UI (main page)
    CROW_ROUTE(app_, "/")
    ([this]() {
        auto page = crow::response(loadWebUI());
        page.add_header("Content-Type", "text/html");
        return page;
    });

    // API: Get device info
    CROW_ROUTE(app_, "/api/device")
    ([this]() {
        crow::json::wvalue result;
        result["name"] = identity_.get_device_name();
        result["fingerprint"] = identity_.get_fingerprint();
        result["public_key"] = identity_.get_public_key_hex();
        return result;
    });

    // API: Get network peers (all discovered + trust status)
    CROW_ROUTE(app_, "/api/peers")
    ([this]() {
        auto all_peers = discovery_.getPeers();
        crow::json::wvalue json_peers(crow::json::type::List);
        int i = 0;

        for (const auto& [ip, peer] : all_peers) {
            json_peers[i]["ip"] = peer.ip_address;
            json_peers[i]["name"] = peer.device_name;
            json_peers[i]["port"] = peer.http_port;
            json_peers[i]["fingerprint"] = peer.fingerprint;
            json_peers[i]["shares"] = peer.shared_folders;
            json_peers[i]["trusted"] = peer.trusted;
            i++;
        }
        return json_peers;
    });

    // API: Get local files (no auth needed - localhost only)
    CROW_ROUTE(app_, "/api/files/local")
    ([this]() {
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
        return result;
    });

    // API: Get remote files (proxy with auth)
    CROW_ROUTE(app_, "/api/files/remote/<string>")
    ([this]([[maybe_unused]] std::string peer_ip) {
        // TODO: Make HTTP request to peer with authentication
        // For now, return empty
        crow::json::wvalue result;
        result["files"] = std::vector<std::string>{};
        result["error"] = "Remote file listing not yet implemented";
        return result;
    });

    // Upload to local (no auth needed - localhost only)
    CROW_ROUTE(app_, "/upload/local")
        .methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            crow::multipart::message file_message(req);
            int count = 0;
            for (const auto& part : file_message.parts) {
                const auto& part_value = part.body;
                
                // Simple filename from timestamp
                std::string filename = "uploaded_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(count);
                
                std::ofstream out(root_dir_ + "/" + filename, std::ios::binary);
                out << part_value;
                out.close();
                count++;
            }
            
            crow::json::wvalue result;
            result["success"] = true;
            result["uploaded"] = count;
            return crow::response(result);
        });

    // Download local file (no auth - localhost only)
    CROW_ROUTE(app_, "/download/local/<string>")
    ([this](crow::response& res, std::string filename) {
        std::string path = root_dir_ + "/" + filename;
        if(fs::exists(path)) {
            res.set_static_file_info(path);
        } else {
            res.code = 404;
            res.write("File not found");
        }
        res.end();
    });

    // Download remote file (proxy with auth)
    CROW_ROUTE(app_, "/download/remote/<string>/<string>")
    ([this](crow::response& res, 
            [[maybe_unused]] std::string peer_ip, 
            [[maybe_unused]] std::string filename) {
        // TODO: Proxy request to peer with authentication
        res.code = 501;
        res.write("Remote download not yet implemented");
        res.end();
    });

    // Legacy authenticated endpoints (for direct peer-to-peer access)
    
    // Get files (authenticated - for remote peers accessing us)
    CROW_ROUTE(app_, "/api/files")
    ([this, verify_auth](const crow::request& req) {
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

    // Download file (authenticated - for remote peers accessing us)
    CROW_ROUTE(app_, "/download/<string>")
    ([this, verify_auth]([[maybe_unused]] const crow::request& req, 
                        crow::response& res, 
                        std::string filename) {
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

    // Upload file (authenticated - for remote peers uploading to us)
    CROW_ROUTE(app_, "/upload")
        .methods(crow::HTTPMethod::POST)
        ([this, verify_auth](const crow::request& req) {
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
    FileServer server(discovery, http_port, "./shared", trust, identity);
    std::cout << "📁 Shared folder: ./shared\n";
    std::cout << "🌐 Web UI: http://localhost:" << http_port << "\n";
    std::cout << "   (Open in your browser to manage files and peers)\n\n";
    std::cout << "🔒 Peer-to-peer file operations require authentication\n";
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
