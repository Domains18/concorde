#pragma once
#include "crow.h"
#include "discovery.h"
#include <string>

class FileServer
{
public:
    FileServer(DiscoveryService &discovery, int port, std::string root_dir);
    void run();

private:
    DiscoveryService &discovery_;
    int port_;
    std::string root_dir_;
    crow::SimpleApp app_;


    void setupRoutes();
};
