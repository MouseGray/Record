#pragma once

#include <cassert>
#include <numeric>
#include <optional>
#include <type_traits>
#include <utility>

#include "storage.h"

struct ObjectServices
{
    using services_t = const ObjectServices& (*)();

#ifndef NDEBUG
    std::size_t id;
#endif
    storage::serializer_t serializer;
    storage::copier_t     copier;
    storage::mover_t      mover;
    storage::deleter_t    destructor;
    bool                  is_meta;
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
const ObjectServices& GetObjectServices()
{
    static ObjectServices services {
#ifndef NDEBUG
        TypeID<Ty>(),
#endif
        storage::serialize<Ty>,
        storage::copy<Ty>,
        storage::move<Ty>,
        storage::destruct<Ty>,
        false
    };

    return services;
}

template<typename Ty>
const ObjectServices& GetMetaObjectServices()
{
    static ObjectServices services {
#ifndef NDEBUG
        TypeID<Ty>(),
#endif
        storage::serialize<Ty>,
        storage::copy<Ty>,
        storage::move<Ty>,
        storage::destruct<Ty>,
        true
    };

    return services;
}

struct ValueBase
{
    using MetaType = char;

    static_assert(storage::is_inplace<MetaType>, "Invalid MetaType size");

    ValueBase() noexcept
    {
        info_ = nullptr;
    }

    template<typename Ty>
    ValueBase(Ty value)
    {
        using original_type = storage::to_original<Ty>;

        info_ = GetObjectServices<original_type>;
        storage::construct(storage_, std::move(value));
    }

    template<typename Ty>
    ValueBase(std::optional<Ty> value)
    {
        using original_type = storage::to_original<Ty>;

        if(value.has_value())
        {
            info_ = GetObjectServices<original_type>;
            storage::construct(storage_, std::move(*value));
        }
        else
        {
            info_ = nullptr;
        }
    }

    ValueBase(const ValueBase& value_base)
    {
        if(value_base.info_ != nullptr)
        {
            info_ = value_base.info_;
            info_().copier(storage_, value_base.storage_);
        }
        else
        {
            info_ = nullptr;
        }
    }

    ValueBase(ValueBase&& value_base) noexcept
    {
        info_ = value_base.info_;

        if(info_ != nullptr)
        {
            info_().mover(storage_, value_base.storage_);

            value_base.info_ = nullptr;
        }
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
            info_().destructor(storage_);

        info_ = value_base.info_;

        if(info_ != nullptr)
        {
            info_().mover(storage_, value_base.storage_);

            value_base.info_ = nullptr;
        }

        return *this;
    }

    ~ValueBase()
    {
        if(info_ == nullptr)
            return;

        info_().destructor(storage_);
    }

    alignas(storage::storage_align) storage::storage_t storage_;

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
        using original_type = storage::to_original<Ty>;

        assert(HasValue());

        assert(info_().id == TypeID<Ty>());

        return storage::get<original_type>(storage_);
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

    bool IsMeta() const noexcept
    {
        if(info_ != nullptr)
            return info_().is_meta;

        return false;
    }

    void SetMeta(MetaType value) noexcept
    {
        if(info_ != nullptr)
            info_().destructor(storage_);

        info_ = GetMetaObjectServices<MetaType>;
        storage::construct(storage_, value);
    }

    MetaType GetMeta() const noexcept
    {
        assert(IsMeta());

        return storage::get<MetaType>(storage_);
    }
private:

    friend void Serialize(std::ostream& out, const Value& value)
    {
        if(value.info_ == nullptr)
        {
            storage::serialize<std::nullptr_t>(out, nullptr);
            return;
        }

        value.info_().serializer(out, value.storage_);
    }
};
