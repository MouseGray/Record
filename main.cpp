#include <iostream>

#include "record.h"

int main()
{
    using namespace std::string_literals;

    Record rec;

    rec.emplace("integer", 10);
    rec.emplace("double", 12.3);
    rec.emplace("string", "This is string"s);
    rec.emplace("self", rec);

    std::cout << "double: " << rec["double"].As<double>() << std::endl;

    Serialize(std::cout, rec);
}
