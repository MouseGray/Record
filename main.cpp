#include <iostream>

#include <unordered_map>

#include "record.h"

int main()
{
    using namespace std::string_literals;

    Record rec;

    std::unordered_map<int, int> h;
    h.rehash(6);

    rec.emplace("integer", 10);
    rec.emplace("double", 12.3);
    rec.emplace("string", "This is string"s);
    rec.emplace("self", rec);

    std::cout << "double: " << rec["double"].As<double>() << std::endl;

    Serialize(std::cout, rec);
}
