#pragma once

#include "Any.hpp"
#include "Array.hpp"
#include "GlobalInstance.hpp"

#include <julia_gcext.h>

namespace jl
{

namespace impl
{

class common_value
{

public:
    common_value(jl_value_t* boxed_value_) noexcept : _boxed_value{boxed_value_}
    {
        global_instance.root_value(_boxed_value);
    }

    common_value() noexcept : common_value{nullptr} {}

    ~common_value() { global_instance.release_value(_boxed_value); }

    template<typename TargT,
             std::enable_if_t<std::is_fundamental<TargT>{}
                              || impl::is_array<TargT>{}>* = nullptr>
    TargT get()
    {
        if constexpr (std::is_integral_v<TargT>)
            return static_cast<TargT>(impl::unbox<long>(_boxed_value));
        else if constexpr (std::is_floating_point_v<TargT>)
            return static_cast<TargT>(impl::unbox<double>(_boxed_value));
        else
            return *this;
    }

    template<typename TargT,
             std::enable_if_t<!std::is_fundamental<TargT>{}>* = nullptr>
    TargT get() noexcept
    {
        if constexpr (std::is_same_v<TargT, jl_value_t*>)
            return static_cast<jl_value_t*>(*this);
        else if constexpr (std::is_pointer_v<TargT>)
            return reinterpret_cast<TargT>(_boxed_value);
        else
            return *reinterpret_cast<std::decay_t<TargT>*>(_boxed_value);
    }

    template<
        typename TargT,
        typename = std::enable_if_t<std::is_fundamental<TargT>{}
                                    && !std::is_same_v<TargT, jl_value_t*>>>
    operator TargT()
    {
        return impl::unbox<TargT>(_boxed_value);
    }

    template<typename TargT,
             std::enable_if_t<std::is_class_v<std::decay_t<TargT>>>* = nullptr>
    operator TargT()
    {
        return get<TargT>();
    }

    operator jl_value_t*() { return _boxed_value; }

    template<typename ElemT>
    explicit operator array<ElemT>() noexcept
    {
        return reinterpret_cast<jl_array_t*>(_boxed_value);
    }

    bool operator==(const common_value& rhs) const
    {
        return static_cast<bool>(jl_egal(_boxed_value, rhs._boxed_value));
    }

    bool operator!=(const common_value& rhs) const { return !(rhs == *this); }

protected:
    jl_value_t* _boxed_value;
};

} // namespace impl

template<typename ValT>
class value : public impl::common_value
{
    using impl::common_value::common_value;

public:
    //    value() = default;

    value(ValT&& obj_) : common_value{impl::box(std::forward<ValT>(obj_))}
    {
        assert_const_match();
    }

    value(const ValT& obj_) : common_value{impl::box(obj_)}
    {
        assert_const_match();
    }

    template<typename std::enable_if_t<std::is_fundamental_v<ValT>>* = nullptr>
    ValT operator*()
    {
        return get<ValT>();
    }

    template<typename std::enable_if_t<!std::is_fundamental_v<ValT>>* = nullptr>
    ValT& operator*()
    {
        return impl::unbox<ValT&>(_boxed_value);
    }

    template<typename std::enable_if_t<!std::is_fundamental_v<ValT>>* = nullptr>
    const ValT& operator*() const
    {
        return impl::unbox<ValT&>(_boxed_value);
    }

    ValT* operator->() { return &**this; }

private:
    inline void assert_const_match()
    {
        if constexpr (!std::is_const_v<ValT> && !std::is_fundamental_v<ValT>)
            jlpp_assert(jl_is_mutable(jl_typeof(_boxed_value)));
    }
};

template<>
class value<jl::any> : public impl::common_value
{
    using impl::common_value::common_value;

public:
    value() = default;

    template<typename T>
    value(T&& obj_) : common_value{impl::box(std::forward<T>(obj_))}
    {
    }

    template<typename T>
    value(const T& obj_) : common_value{impl::box(obj_)}
    {
    }
};

} // namespace jl