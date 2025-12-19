#include "discovery.h"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>

// simple serialization: "deviceName|Port|Folder1,Folder2"
std::string serialize_packet(const std::string &name, int port)
{
    return name + "|" + std::to_string(port) + "|public,docs";
};

DiscoveryService::DiscoveryService(int b_port, int h_port, const std::string& name)
    : broadcast_port_(b_port), my_http_port_(h_port), my_device_name_(name), running_(false) {}

DiscoveryService::~DiscoveryService() { stop(); }

void DiscoveryService::start()
{
    if (running_)
        return;
    running_ = true;
    broadcast_thread_ = std::thread(&DiscoveryService::broadCastLoop, this);
    listener_thread_ = std::thread(&DiscoveryService::listenLoop, this);
};

void DiscoveryService::stop()
{
    if (!running_)
        return;
    running_ = false;
    if (broadcast_thread_.joinable())
        broadcast_thread_.join();
    if (listener_thread_.joinable())
        listener_thread_.join();
}

void DiscoveryService::broadCastLoop()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(broadcast_port_);
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    while (running_)
    {
        std::string msg = serialize_packet(my_device_name_, my_http_port_);
        sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    close(sock);
}


void DiscoveryService::listenLoop(){
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(broadcast_port_);
    recv_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr *)&recv_addr, sizeof(recv_addr));

    char buffer[1025];
    while(running_){
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);
        int len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&sender_addr, &sender_len);

        if(len > 0) {
            buffer[len] = '\0';
            std::string data(buffer);
            std::string sender_ip = inet_ntoa(sender_addr.sin_addr);

            // don't discover self(I will be using UUID)
            std::lock_guard<std::mutex> lock(peers_mutex_);
            peers_[sender_ip] = Peer{
                .ip_address = sender_ip,
                .device_name = data.substr(0, data.find("|")),
                .http_port = std::stoi(data.substr(
                    data.find("|") + 1,
                    data.find("|", data.find("|") + 1) - (data.find("|") + 1))),
                .shared_folders = {"public", "docs"},
                .last_seen = std::chrono::steady_clock::now()};
        }
    }
    close(sock);
}


std::map<std::string, Peer> DiscoveryService::getPeers(){
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return peers_;
}