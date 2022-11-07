#pragma once

#include <ostream>

template<typename Ty>
void Serialize(std::ostream& out, Ty value)
{
    out << value;
}

inline void Serialize(std::ostream& out, std::string a)
{
    out << "\"" << a << "\"";
}

inline void Serialize(std::ostream& out, std::nullptr_t)
{
    out << "null";
}
