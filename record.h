#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>

#include "serializer.h"
#include "value.h"

using Record = std::unordered_map<std::string, Value>;

void Serialize(std::ostream& out, const Record& rec)
{
    auto serialize_first_value = [&out](auto&& p)
    {
        out << "\"" << p.first << "\":";
        Serialize(out, p.second);
    };

    auto serialize_value = [&out](auto&& p)
    {
        out << ",\"" << p.first << "\":";
        Serialize(out, p.second);
    };

    out << "{";
    serialize_first_value(*rec.begin());
    for(auto first = std::next(rec.begin()), end = rec.end(); first != end; ++first)
    {
        serialize_value(*first);
    }
    out << "}" << std::endl;
}
