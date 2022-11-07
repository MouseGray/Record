#pragma once

#include <cassert>
#include <numeric>
#include <optional>
#include <type_traits>
#include <utility>

#include "serializer.h"

template<typename Ty>
static constexpr auto in_place_v = std::is_trivial_v<Ty> &&
                                   alignof(Ty) <= alignof(void*) &&
                                   sizeof(Ty) <= sizeof(void*);

template<typename Ty>
using OriginalType = std::remove_cv_t<std::remove_reference_t<Ty>>;

struct ObjectServices
{
    using services_t = const ObjectServices& (*)();
    using storage_t  = std::aligned_storage_t<sizeof(void*), alignof(void*)>;

    using serializer_t = void (*)(std::ostream&, const storage_t*);
    using copier_t     = void (*)(storage_t*, const storage_t*);
    using deleter_t    = void (*)(storage_t*);

#ifndef NDEBUG
    std::size_t id;
#endif
    serializer_t serializer;
    copier_t     copier;
    deleter_t    deleter;
};

#ifndef NDEBUG
inline std::size_t Counter()
{
    static std::size_t counter{};
    return counter++;
}

template<typename Ty>
std::size_t TypeID()
{
    static std::size_t id = Counter();
    return id;
}
#endif

template<typename Ty>
void Serializer(std::ostream& out, const ObjectServices::storage_t* ptr)
{
    if(ptr == nullptr)
    {
        Serialize(out, nullptr);
        return;
    }

    if constexpr (in_place_v<Ty>)
    {
        Serialize(out, *std::launder(reinterpret_cast<const Ty*>(ptr)));
    }
    else
    {
        Serialize(out, **std::launder(reinterpret_cast<const Ty* const*>(ptr)));
    }
}

template<typename Ty>
void Creator(ObjectServices::storage_t* dest, Ty value)
{
    if constexpr (in_place_v<Ty>)
    {
        new (dest) Ty{std::forward<Ty>(value)};
    }
    else
    {
        new (dest) Ty*{new Ty{std::forward<Ty>(value)}};
    }
}

template<typename Ty>
void Copier(ObjectServices::storage_t* dest, const ObjectServices::storage_t* src)
{
    if constexpr (in_place_v<Ty>)
    {
        *dest = *src;
    }
    else
    {
        new (dest) Ty*{new Ty{**std::launder(reinterpret_cast<const Ty* const*>(src))}};
    }
}

template<typename Ty>
void Deleter(ObjectServices::storage_t* ptr)
{
    if constexpr (!in_place_v<Ty>)
    {
        delete *std::launder(reinterpret_cast<Ty**>(ptr));
    }
}

template<typename Ty>
const ObjectServices& GetObjectServices()
{
    static ObjectServices services {
        TypeID<Ty>(),
        Serializer<Ty>,
        Copier<Ty>,
        Deleter<Ty>
    };

    return services;
}

struct ValueBase
{
    ValueBase() :
        info_{nullptr}
    {

    }

    template<typename Ty>
    ValueBase(Ty value) :
        info_{GetObjectServices<OriginalType<Ty>>}
    {
        Creator<OriginalType<Ty>>(&storage_, std::move(value));
    }

    template<typename Ty>
    ValueBase(std::optional<Ty> value) :
        info_{nullptr}
    {
        if(value.has_value())
        {
            info_ = GetObjectServices<OriginalType<Ty>>;
            Creator<OriginalType<Ty>>(&storage_, *value);
        }
    }

    ValueBase(const ValueBase& value_base) :
        info_{value_base.info_}
    {
        if(value_base.info_ == nullptr)
            return;

        info_().copier(&storage_, &value_base.storage_);
    }

    ValueBase(ValueBase&& value_base) noexcept :
        info_{std::exchange(value_base.info_, nullptr)}
    {
        if(info_ == nullptr)
            return;

        storage_ = value_base.storage_;
    }

    ValueBase& operator=(const ValueBase& value_base)
    {
        ValueBase tmp{value_base};
        *this = std::move(tmp);
        return *this;
    }

    ValueBase& operator=(ValueBase&& value_base) noexcept
    {
        if(info_ != nullptr)
            info_().deleter(&storage_);

        info_ = std::exchange(value_base.info_, nullptr);

        if(info_ == nullptr)
            return *this;

        storage_ = value_base.storage_;

        return *this;
    }

    ~ValueBase()
    {
        if(info_ == nullptr)
            return;

        info_().deleter(&storage_);
    }

    ObjectServices::storage_t storage_;

    ObjectServices::services_t info_;
};

class Value : ValueBase
{
public:
    Value() :
        ValueBase{}
    {

    }

    template<typename Ty>
    Value(Ty value) :
        ValueBase{std::move(value)}
    {

    }

    template<typename Ty>
    Value(std::optional<Ty> value) :
        ValueBase{std::move(value)}
    {

    }

    template<typename Ty>
    Ty& As()
    {
        assert(info_ != nullptr);

#ifndef NDEBUG
        if(info_().id != TypeID<Ty>())
            throw std::invalid_argument{"Invalid type"};
#endif
        if constexpr(in_place_v<OriginalType<Ty>>)
        {
            return *std::launder(reinterpret_cast<OriginalType<Ty>*>(&storage_));
        }
        else
        {
            return **std::launder(reinterpret_cast<OriginalType<Ty>**>(&storage_));
        }
    }

    template<typename Ty>
    std::optional<Ty> AsOpt()
    {
        if(info_ == nullptr)
            return {};

        return {As<Ty>()};
    }
private:

    friend void Serialize(std::ostream& out, const Value& value)
    {
        if(value.info_ == nullptr)
        {
            Serializer<std::nullptr_t>(out, nullptr);
            return;
        }

        value.info_().serializer(out, &value.storage_);
    }
};
