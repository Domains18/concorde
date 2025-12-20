#include "main.h"
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

FileServer::FileServer(DiscoveryService& discovery, int port, std::string root_dir)
    : discovery_(discovery), port_(port), root_dir_(root_dir)
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
    // get local files
    CROW_ROUTE(app_, "/api/files")
    (
        [this]()
        {
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

    // get network peers
    CROW_ROUTE(app_, "/api/peers")
    (
        [this]()
        {
            auto peers = discovery_.getPeers();
            crow::json::wvalue json_peers;
            // loop
            int i = 0;

            for (const auto& [ip, peer] : peers)
            {
                json_peers[i]["ip"] = peer.ip_address;
                json_peers[i]["name"] = peer.device_name;
                json_peers[i]["port"] = peer.http_port;
                i++;
            }
            return json_peers;
        });

    // upload handling
    CROW_ROUTE(app_, "/upload")
        .methods(crow::HTTPMethod::POST)(
            [this](const crow::request& req)
            {
                crow::multipart::message file_message(req);
                for (const auto& part : file_message.parts)
                {
                    const auto& part_value = part.body;
                    // got a little complex, saving with timestamp for simplicity
                    // hmm parse 'Content-Disposition' for filename?
                    std::string filename = "uploaded_" + std::to_string(std::time(nullptr));
                    std::ofstream out(root_dir_ + "/" + filename, std::ios::binary);
                    out << part_value;
                    out.close();
                }
                return crow::response(200);
            });


    //download handling
    CROW_ROUTE(app_, "/download/<string>")
    ([this](const crow::request& req,crow::response& res, std::string filename){
        std::string path = root_dir_ + "/" + filename;
        if(fs::exists(path)){
            res.set_static_file_info(path);
        } else {
            res.code = 404;
            res.write("File not found");
        }
        res.end();
    });


    // serve the UI (embedded for single binary simplicity and portable use)
    CROW_ROUTE(app_, "/")
    (
        []()
        {
            return "<h1>Welcome to the P2P File Sharing Service</h1>"
                   "<p>Use /api/files to list local files.</p>"
                   "<p>Use /api/peers to list discovered peers.</p>"
                   "<p>Use /upload to upload files.</p>"
                   "<p>Use /download/&lt;filename&gt; to download files.</p>";
        });
}