#include <iostream>
#include "config.hpp"


int main(int argc, char* argv[])
{
    if (argc != 2) 
    {
        std:: cerr << "Wrong number of arguments\n";
        return 1;
    }   
        
    caches:: Config config = caches:: ReadConfig(argv[1]);

    for (int i = 0; i < (int)config.policies.size(); i++)
    {
        std:: cout << static_cast<int>(config.policies[i]) << "\n";
    }
    return 0;
}