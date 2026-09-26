#include <exception>
#include <iostream>

#include "config.hpp"



int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <config_file>\n";
        return 1;
    }

    try
    {
        const caches::Config config = caches::ReadConfig(argv[1]);

        for (const auto policy : config.policies)
        {
            std::cout << static_cast<int>(policy) << '\n';
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "Configuration error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}