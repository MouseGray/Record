#pragma once

#include <ostream>

template<typename Ty>
void Serialize(std::ostream& out, const Ty& value)
{
    out << value;
}

inline void Serialize(std::ostream& out, const std::string& a)
{
    out << "\"" << a << "\"";
}

inline void Serialize(std::ostream& out, std::nullptr_t)
{
    out << "null";
}
