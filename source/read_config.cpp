#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>

#include "config.hpp"

namespace caches
{
    Config ReadConfig (const std::string& filepath)
    {
        std:: ifstream file(filepath);
        if (!file) throw std::runtime_error("Cannot open config file\n");
    
        int level = 0;
        if (!(file >> level) || level <= 0) 
            throw std::runtime_error("Invalid number of cache levels\n");
    
    
        std:: string cache_name;
        Config config;
        config.policies.resize(level);
    
    
        for (int i = 0; i < level; i++)
        {
            if (!(file >> cache_name))
                throw std::runtime_error("Unexpected end of config file\n");
    
            Policy value = caches:: ParsePolicy(cache_name);
            config.policies[i] = value;
        }
        return config;
    }
}
