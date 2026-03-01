#pragma once
#include "crow.h"
#include "discovery.h"
#include "trust.h"
#include <string>

class FileServer
{
  public:
    FileServer(DiscoveryService& discovery, 
              int port, 
              std::string root_dir,
              concorde::TrustStore& trust);
    void run();

  private:
    DiscoveryService& discovery_;
    int port_;
    std::string root_dir_;
    concorde::TrustStore& trust_;
    crow::SimpleApp app_;

    void setupRoutes();
};
