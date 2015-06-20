// Utilities for unit testing

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "../../src/cuckoohash_map.hh"


using IntIntTable = cuckoohash_map<
    int,
    int,
    std::hash<int>,
    std::equal_to<int>,
    std::allocator<std::pair<const int, int>>,
    4>;


using StringIntTable = cuckoohash_map<
    std::string,
    int,
    std::hash<std::string>,
    std::equal_to<std::string>,
    std::allocator<std::pair<const std::string, int>>,
    4>;
