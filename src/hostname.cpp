#include "hostname.h"
#include <unistd.h>
#include <random>
#include <sstream>



static std::string generate_random_name(){
    static const char* animals[] = {
        "Lion", "Tiger", "Bear", "Wolf", "Eagle", "Shark", "Panther", "Leopard", "Cheetah", "Falcon"
    };


    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> a(0,4);
    std::uniform_int_distribution<> b(100,999);

    std::ostringstream oss;
    oss <<animals[a(gen)] << b(gen);
    return oss.str();
}



std::string get_hostname()
{
    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        return std::string(hostname);
    }
    else
    {
        return generate_random_name();
    }
}