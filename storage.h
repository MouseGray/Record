#pragma once

#include <cstddef>
#include <new>
#include <ostream>
#include <utility>

namespace storage
{

template<typename Ty>
using to_original = std::remove_cv_t<std::remove_reference_t<Ty>>;

using storage_base_t = void*;

static constexpr auto storage_size = sizeof(storage_base_t);

static constexpr auto storage_align = alignof(storage_base_t);

using storage_t = std::byte[storage_size];
using storage_ptr = std::byte*;
using const_storage_ptr = const std::byte*;

using serializer_t = void (*)(std::ostream&, const_storage_ptr);
using copier_t     = void (*)(storage_ptr, const_storage_ptr);
using mover_t      = void (*)(storage_ptr, storage_ptr);
using deleter_t    = void (*)(storage_ptr);

template<typename Ty>
static constexpr auto is_inplace = alignof(Ty) <= storage_align && sizeof(Ty) <= storage_size;

template<typename Ty>
const Ty& get(const_storage_ptr storage) noexcept
{
    if constexpr (is_inplace<Ty>)
    {
        return *std::launder(reinterpret_cast<const Ty*>(storage));
    }
    else
    {
        return **std::launder(reinterpret_cast<const Ty* const*>(storage));
    }
}

template<typename Ty>
Ty& get(storage_ptr storage) noexcept
{
    if constexpr (is_inplace<Ty>)
    {
        return *std::launder(reinterpret_cast<Ty*>(storage));
    }
    else
    {
        return **std::launder(reinterpret_cast<Ty**>(storage));
    }
}

template<typename Ty>
void construct(storage_ptr storage, Ty&& value)
{
    using original_type = to_original<Ty>;

    if constexpr (is_inplace<original_type>)
    {
        new (storage) original_type{std::forward<Ty>(value)};
    }
    else
    {
        new (storage) original_type*{new original_type{std::forward<Ty>(value)}};
    }
}

template<typename Ty>
void destruct(storage_ptr storage)
{
    using original_type = to_original<Ty>;

    if constexpr (is_inplace<original_type>)
    {
        auto& value = get<original_type>(storage);
        value.~Ty();
    }
    else
    {
        auto* value = get<original_type*>(storage);
        delete value;
    }
}

template<typename Ty>
void copy(storage_ptr dest, const_storage_ptr src)
{
    using original_type = to_original<Ty>;

    const auto& value = get<original_type>(src);

    construct(dest, value);
}

template<typename Ty>
void move(storage_ptr dest, storage_ptr src)
{
    using original_type = to_original<Ty>;

    if constexpr (is_inplace<original_type>)
    {
        auto& value = get<original_type>(src);
        construct(dest, std::move(value));
        value.~Ty();
    }
    else
    {
        auto* value = get<original_type*>(src);
        construct(dest, value);
        delete value;
    }
}

template<typename Ty>
void serialize_impl(std::ostream& out, const Ty& value)
{
    Serialize(out, value);
}

template<typename Ty>
void serialize(std::ostream& out, const_storage_ptr storage)
{
    using original_type = to_original<Ty>;

    if constexpr (std::is_same_v<std::nullptr_t, Ty>)
    {
        serialize_impl(out, nullptr);
    }
    else
    {
        const auto& value = get<original_type>(storage);
        serialize_impl(out, value);
    }
}

}









