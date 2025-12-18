#pragma once
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

struct Peer
{
    std::string ip_address;
    std::string device_name;
    int http_port;
    std::vector<std::string> shared_folders;
    std::chrono::steady_clock::time_point last_seen; // prunning offline peers
};


class DiscoveryService {
    public:
        DiscoveryService(int broadcast_port, int http_port, std::string device_name);
        ~DiscoveryService();

        void start(); //listener threads
        void stop();


        //thread-safe access to get current active peers
        std::map<std::string, Peer> getPeers();


    private:
    void broadCastLoop();
    void listenLoop();
    void prunePeers(); // remove devices we haven't heard from in X seconds


    int broadcast_port_;
    int my_http_port_;
    std::map<std::string, Peer> getPeers();
};