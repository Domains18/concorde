#include "discovery.h"
#include "main.h"
#include "iostream"
#include <filesystem>
#include "hostname.h"



const int UDP_PORT = 45454;
const int HTTP_PORT = 8080;
const std::string DEVICE_NAME = get_hostname();
const std::string SHARED_DIR = "./shared_files";