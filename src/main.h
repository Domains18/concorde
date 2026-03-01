#pragma once
#include "crow.h"
#include "discovery.h"
#include "trust.h"
#include "crypto.h"
#include <string>

class FileServer
{
  public:
    FileServer(DiscoveryService& discovery, 
              int port, 
              std::string root_dir,
              concorde::TrustStore& trust,
              concorde::DeviceIdentity& identity);
    void run();

  private:
    DiscoveryService& discovery_;
    int port_;
    std::string root_dir_;
    concorde::TrustStore& trust_;
    concorde::DeviceIdentity& identity_;
    crow::SimpleApp app_;

    void setupRoutes();
    std::string loadWebUI();
};
