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
    if constexpr (std::is_same_v<std::nullptr_t, Ty>)
    {
        Serialize(out, nullptr);
    }
    else if (in_place_v<Ty>)
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
#ifndef NDEBUG
        TypeID<Ty>(),
#endif
        Serializer<Ty>,
        Copier<Ty>,
        Deleter<Ty>
    };

    return services;
}

struct ValueBase
{
    using MetaType = char;

    static_assert(in_place_v<MetaType>, "Invalid MetoType size");

    ValueBase() noexcept
    {
        info_ = nullptr;
        Creator<MetaType>(&storage_, 0);
    }

    template<typename Ty>
    ValueBase(Ty value) noexcept(in_place_v<Ty>)
    {
        info_ = GetObjectServices<OriginalType<Ty>>;
        Creator<OriginalType<Ty>>(&storage_, std::move(value));
    }

    template<typename Ty>
    ValueBase(std::optional<Ty> value) noexcept(in_place_v<Ty>)
    {
        if(value.has_value())
        {
            info_ = GetObjectServices<OriginalType<Ty>>;
            Creator<OriginalType<Ty>>(&storage_, std::move(*value));
        }
        else
        {
            info_ = nullptr;
            Creator<MetaType>(&storage_, 0);
        }
    }

    ValueBase(const ValueBase& value_base)
    {
        if(value_base.info_ != nullptr)
        {
            info_ = value_base.info_;
            info_().copier(&storage_, &value_base.storage_);
        }
        else
        {
            info_ = nullptr;
            storage_ = value_base.storage_;
        }
    }

    ValueBase(ValueBase&& value_base) noexcept
    {
        info_ = value_base.info_;
        storage_ = value_base.storage_;

        value_base.info_ = nullptr;
        Creator<MetaType>(&value_base.storage_, 0);
    }

    ValueBase& operator=(const ValueBase& value_base)
    {
        if(this == std::addressof(value_base))
            return *this;

        ValueBase tmp{value_base};
        *this = std::move(tmp);
        return *this;
    }

    ValueBase& operator=(ValueBase&& value_base) noexcept
    {
        if(this == std::addressof(value_base))
            return *this;

        if(info_ != nullptr)
            info_().deleter(&storage_);

        info_ = value_base.info_;
        storage_ = value_base.storage_;

        value_base.info_ = nullptr;
        Creator<MetaType>(&value_base.storage_, 0);

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
    using ValueBase::ValueBase;

    bool HasValue() const noexcept
    {
        return info_ != nullptr;
    }

    template<typename Ty>
    const Ty& As() const noexcept
    {
        assert(HasValue());

        assert(info_().id == TypeID<Ty>());

        if constexpr(in_place_v<OriginalType<Ty>>)
        {
            return *std::launder(reinterpret_cast<const OriginalType<Ty>*>(&storage_));
        }
        else
        {
            return **std::launder(reinterpret_cast<const OriginalType<Ty>**>(&storage_));
        }
    }

    template<typename Ty>
    Ty& As() noexcept
    {
        return const_cast<Ty&>(static_cast<const Value*>(this)->As<Ty>());
    }

    template<typename Ty>
    std::optional<Ty> AsOpt() const
    {
        if(HasValue())
            return {As<Ty>()};

        return {};
    }

    void SetMeta(MetaType value) noexcept
    {
        if(info_ != nullptr)
            info_().deleter(&storage_);

        info_ = nullptr;
        Creator<MetaType>(&storage_, value);
    }

    MetaType GetMeta() const noexcept
    {
        assert(info_ == nullptr);

        return *std::launder(reinterpret_cast<const MetaType*>(&storage_));
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
