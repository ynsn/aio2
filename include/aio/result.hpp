// Copyright (c) 2025 - present, Yoram Janssen
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// --- Optional exception to the license ---
//
// As an exception, if, as a result of your compiling your source code, portions
// of this Software are embedded into a machine-executable object form of such
// source code, you may redistribute such embedded portions in such object form
// without including the above copyright and permission notices.

#ifndef AIO_RESULT_HPP
#define AIO_RESULT_HPP

#include <type_traits>
#include <utility>
#include <memory>

#include "detail/concepts.hpp"
#include "detail/macros.hpp"

namespace aio {
  template <class T, class E>
  class [[nodiscard]] result;

  template <class E>
  class failure {
   public:
    static_assert(!std::is_same_v<E, void>, "E must not be void");

    failure() = delete;

    constexpr failure(const failure&) = default;
    constexpr failure(failure&&) = default;

    template <class Err = E>
    constexpr explicit failure(Err&& e) noexcept(std::is_nothrow_constructible_v<E, Err>) : val(std::forward<Err>(e)) {}

    template <class... Args>
    constexpr explicit failure(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : val(AIO_FWD(args)...) {}

    template <class U, class... Args>
    constexpr explicit failure(std::in_place_t, std::initializer_list<U> il,
                               Args&&... args) noexcept(std::is_nothrow_constructible_v<E, std::initializer_list<U>, Args...>)
        : val(il, AIO_FWD(args)...) {}

    constexpr failure& operator=(const failure&) = default;
    constexpr failure& operator=(failure&&) = default;

    constexpr auto error() const& noexcept -> const E& { return val; }
    constexpr auto error() & noexcept -> E& { return val; }
    constexpr auto error() const&& noexcept -> const E&& { return std::move(val); }
    constexpr auto error() && noexcept -> E&& { return std::move(val); }

    constexpr auto swap(failure& other) noexcept(std::is_nothrow_swappable_v<E>) -> void
      requires std::swappable<E>
    {
      using std::swap;
      swap(val, other.val);
    }

    template <class E2>
    friend constexpr auto operator==(const failure& x, const failure<E2>& y) -> bool {
      return x.error() == y.error();
    }

    friend constexpr auto swap(failure& x, failure& y) noexcept(noexcept(x.swap(y))) -> void
      requires std::swappable<E>
    {
      x.swap(y);
    }

   private:
    E val;
  };

  template <class E>
  failure(E) -> failure<E>;

  template <class T>
  struct is_failure : std::false_type {};

  template <class E>
  struct is_failure<failure<E>> : std::true_type {};

  template <class T>
  concept failure_type = is_failure<std::remove_cvref_t<T>>::value;

  // Helper to detect result type
  template <class T>
  struct is_result : std::false_type {};

  template <class T, class E>
  struct is_result<result<T, E>> : std::true_type {};

  template <class T>
  concept result_type = is_result<std::remove_cvref_t<T>>::value;

  namespace detail {
    template <class T, class E>
    class result_storage {
     public:
      constexpr result_storage() noexcept(std::is_nothrow_default_constructible_v<T>)
        requires aio::detail::default_constructible<T>
          : val_(), has_value_(true) {}

      template <class... Args>
      constexpr explicit result_storage(std::in_place_t, Args&&... args)
        requires std::constructible_from<T, Args...>
          : val_(std::forward<Args>(args)...), has_value_(true) {}

      template <class U, class... Args>
      constexpr explicit result_storage(std::in_place_t, std::initializer_list<U> il, Args&&... args)
        requires std::constructible_from<T, std::initializer_list<U>&, Args...>
          : val_(il, std::forward<Args>(args)...), has_value_(true) {}

      constexpr explicit result_storage(failure<E> f)
        requires std::move_constructible<E>
          : err_(std::move(f).error()), has_value_(false) {}

      // Special case for void value type
      constexpr result_storage()
        requires std::is_void_v<T>
          : has_value_(true) {}

      constexpr explicit result_storage(std::in_place_t)
        requires std::is_void_v<T>
          : has_value_(true) {}

      // Destructor - conditionally trivial
      constexpr ~result_storage()
        requires(aio::detail::trivially_destructible<T> && aio::detail::trivially_destructible<E>)
      = default;

      constexpr ~result_storage()
        requires(aio::detail::trivially_destructible<T> && !aio::detail::trivially_destructible<E>)
      {
        if (!has_value_) {
          err_.~E();
        }
      }

      constexpr ~result_storage()
        requires(!aio::detail::trivially_destructible<T> && aio::detail::trivially_destructible<E>)
      {
        if (has_value_ && !std::is_void_v<T>) {
          val_.~T();
        }
      }

      constexpr ~result_storage()
        requires(!aio::detail::trivially_destructible<T> && !aio::detail::trivially_destructible<E>)
      {
        if (has_value_) {
          if constexpr (!std::is_void_v<T>) {
            val_.~T();
          }
        } else {
          err_.~E();
        }
      }

      // Copy constructor - conditionally trivial
      constexpr result_storage(const result_storage& other)
        requires(aio::detail::trivially_copyable<T> && aio::detail::trivially_copyable<E>)
      = default;

      constexpr result_storage(const result_storage& other)
        requires((!aio::detail::trivially_copyable<T> || !aio::detail::trivially_copyable<E>) && std::copy_constructible<T> &&
                 std::copy_constructible<E>)
      {
        if (other.has_value_) {
          if constexpr (!std::is_void_v<T>) {
            std::construct_at(std::addressof(val_), other.val_);
          }
        } else {
          std::construct_at(std::addressof(err_), other.err_);
        }
        has_value_ = other.has_value_;
      }

      // Move constructor - conditionally trivial
      constexpr result_storage(result_storage&& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                                std::is_nothrow_move_constructible_v<E>)
        requires(aio::detail::trivially_movable<T> && aio::detail::trivially_movable<E>)
      = default;

      constexpr result_storage(result_storage&& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                                std::is_nothrow_move_constructible_v<E>)
        requires((!aio::detail::trivially_movable<T> || !aio::detail::trivially_movable<E>) && std::move_constructible<T> &&
                 std::move_constructible<E>)
      {
        if (other.has_value_) {
          if constexpr (!std::is_void_v<T>) {
            std::construct_at(std::addressof(val_), std::move(other.val_));
          }
        } else {
          std::construct_at(std::addressof(err_), std::move(other.err_));
        }
        has_value_ = other.has_value_;
      }

      // Copy assignment operator - conditionally trivial
      constexpr result_storage& operator=(const result_storage& other)
        requires(aio::detail::trivially_copyable<T> && aio::detail::trivially_copyable<E> &&
                 aio::detail::trivially_destructible<T> && aio::detail::trivially_destructible<E>)
      = default;

      constexpr result_storage& operator=(const result_storage& other)
        requires(!(aio::detail::trivially_copyable<T> && aio::detail::trivially_copyable<E> &&
                   aio::detail::trivially_destructible<T> && aio::detail::trivially_destructible<E>) &&
                 std::copy_constructible<T> && std::copy_constructible<E> && std::is_copy_assignable_v<T> &&
                 std::is_copy_assignable_v<E>)
      {
        if (has_value_ && other.has_value_) {
          if constexpr (!std::is_void_v<T>) {
            val_ = other.val_;
          }
        } else if (has_value_) {
          if constexpr (std::is_nothrow_copy_constructible_v<E>) {
            if constexpr (!std::is_void_v<T>) {
              val_.~T();
            }
            std::construct_at(std::addressof(err_), other.err_);
            has_value_ = false;
          } else {
            E tmp(other.err_);
            if constexpr (!std::is_void_v<T>) {
              val_.~T();
            }
            std::construct_at(std::addressof(err_), std::move(tmp));
            has_value_ = false;
          }
        } else if (other.has_value_) {
          err_.~E();
          if constexpr (!std::is_void_v<T>) {
            if constexpr (std::is_nothrow_copy_constructible_v<T>) {
              std::construct_at(std::addressof(val_), other.val_);
            } else {
              T tmp(other.val_);
              std::construct_at(std::addressof(val_), std::move(tmp));
            }
          }
          has_value_ = true;
        } else {
          err_ = other.err_;
        }
        return *this;
      }

      // Move assignment operator - conditionally trivial
      constexpr result_storage& operator=(result_storage&& other) noexcept(std::is_nothrow_move_assignable_v<T> &&
                                                                           std::is_nothrow_move_constructible_v<T> &&
                                                                           std::is_nothrow_move_assignable_v<E> &&
                                                                           std::is_nothrow_move_constructible_v<E>)
        requires(aio::detail::trivially_movable<T> && aio::detail::trivially_movable<E> &&
                 aio::detail::trivially_destructible<T> && aio::detail::trivially_destructible<E>)
      = default;

      constexpr result_storage& operator=(result_storage&& other) noexcept(std::is_nothrow_move_assignable_v<T> &&
                                                                           std::is_nothrow_move_constructible_v<T> &&
                                                                           std::is_nothrow_move_assignable_v<E> &&
                                                                           std::is_nothrow_move_constructible_v<E>)
        requires(!(aio::detail::trivially_movable<T> && aio::detail::trivially_movable<E> &&
                   aio::detail::trivially_destructible<T> && aio::detail::trivially_destructible<E>) &&
                 std::move_constructible<T> && std::move_constructible<E> && std::is_move_assignable_v<T> &&
                 std::is_move_assignable_v<E>)
      {
        if (has_value_ && other.has_value_) {
          if constexpr (!std::is_void_v<T>) {
            val_ = std::move(other.val_);
          }
        } else if (has_value_) {
          if constexpr (std::is_nothrow_move_constructible_v<E>) {
            if constexpr (!std::is_void_v<T>) {
              val_.~T();
            }
            std::construct_at(std::addressof(err_), std::move(other.err_));
            has_value_ = false;
          } else {
            E tmp(std::move(other.err_));
            if constexpr (!std::is_void_v<T>) {
              val_.~T();
            }
            std::construct_at(std::addressof(err_), std::move(tmp));
            has_value_ = false;
          }
        } else if (other.has_value_) {
          err_.~E();
          if constexpr (!std::is_void_v<T>) {
            if constexpr (std::is_nothrow_move_constructible_v<T>) {
              std::construct_at(std::addressof(val_), std::move(other.val_));
            } else {
              T tmp(std::move(other.val_));
              std::construct_at(std::addressof(val_), std::move(tmp));
            }
          }
          has_value_ = true;
        } else {
          err_ = std::move(other.err_);
        }
        return *this;
      }

      union {
        T val_;
        E err_;
      };
      bool has_value_;
    };

    // Specialized storage for void value type
    template <class E>
    class result_storage<void, E> {
     public:
      constexpr result_storage() noexcept : has_value_(true) {}

      constexpr explicit result_storage(std::in_place_t) noexcept : has_value_(true) {}

      constexpr explicit result_storage(failure<E> f)
        requires std::move_constructible<E>
          : err_(std::move(f).error()), has_value_(false) {}

      // Destructor - conditionally trivial
      constexpr ~result_storage()
        requires aio::detail::trivially_destructible<E>
      = default;

      constexpr ~result_storage()
        requires(!aio::detail::trivially_destructible<E>)
      {
        if (!has_value_) {
          err_.~E();
        }
      }

      // Copy constructor - conditionally trivial
      constexpr result_storage(const result_storage& other)
        requires aio::detail::trivially_copyable<E>
      = default;

      constexpr result_storage(const result_storage& other)
        requires(!aio::detail::trivially_copyable<E> && std::copy_constructible<E>)
      {
        if (!other.has_value_) {
          std::construct_at(std::addressof(err_), other.err_);
        }
        has_value_ = other.has_value_;
      }

      // Move constructor - conditionally trivial
      constexpr result_storage(result_storage&& other) noexcept(std::is_nothrow_move_constructible_v<E>)
        requires aio::detail::trivially_movable<E>
      = default;

      constexpr result_storage(result_storage&& other) noexcept(std::is_nothrow_move_constructible_v<E>)
        requires(!aio::detail::trivially_movable<E> && std::move_constructible<E>)
      {
        if (!other.has_value_) {
          std::construct_at(std::addressof(err_), std::move(other.err_));
        }
        has_value_ = other.has_value_;
      }

      // Copy assignment operator - conditionally trivial
      constexpr result_storage& operator=(const result_storage& other)
        requires(aio::detail::trivially_copyable<E> && aio::detail::trivially_destructible<E>)
      = default;

      constexpr result_storage& operator=(const result_storage& other)
        requires(!aio::detail::trivially_copyable<E> && !aio::detail::trivially_destructible<E> && std::copy_constructible<E> &&
                 std::is_copy_assignable_v<E>)
      {
        if (has_value_ && other.has_value_) {
          // Both have value, nothing to do
        } else if (has_value_) {
          if constexpr (std::is_nothrow_copy_constructible_v<E>) {
            std::construct_at(std::addressof(err_), other.err_);
            has_value_ = false;
          } else {
            E tmp(other.err_);
            std::construct_at(std::addressof(err_), std::move(tmp));
            has_value_ = false;
          }
        } else if (other.has_value_) {
          err_.~E();
          has_value_ = true;
        } else {
          err_ = other.err_;
        }
        return *this;
      }

      // Move assignment operator - conditionally trivial
      constexpr result_storage& operator=(result_storage&& other) noexcept(std::is_nothrow_move_assignable_v<E> &&
                                                                           std::is_nothrow_move_constructible_v<E>)
        requires(aio::detail::trivially_movable<E> && aio::detail::trivially_destructible<E>)
      = default;

      constexpr result_storage& operator=(result_storage&& other) noexcept(std::is_nothrow_move_assignable_v<E> &&
                                                                           std::is_nothrow_move_constructible_v<E>)
        requires(!aio::detail::trivially_movable<E> && !aio::detail::trivially_destructible<E> && std::move_constructible<E> &&
                 std::is_move_assignable_v<E>)
      {
        if (has_value_ && other.has_value_) {
          // Both have value, nothing to do
        } else if (has_value_) {
          if constexpr (std::is_nothrow_move_constructible_v<E>) {
            std::construct_at(std::addressof(err_), std::move(other.err_));
            has_value_ = false;
          } else {
            E tmp(std::move(other.err_));
            std::construct_at(std::addressof(err_), std::move(tmp));
            has_value_ = false;
          }
        } else if (other.has_value_) {
          err_.~E();
          has_value_ = true;
        } else {
          err_ = std::move(other.err_);
        }
        return *this;
      }

      union {
        std::monostate val_;
        E err_;
      };
      bool has_value_;
    };
  }  // namespace detail

  template <class T, class E>
  class result : private detail::result_storage<T, E> {
    using Base = detail::result_storage<T, E>;

   public:
    using value_type = T;
    using error_type = E;
    using failure_type = failure<E>;

    template <class U>
    using rebind = result<U, error_type>;

    constexpr result()
      requires(aio::detail::default_constructible<T> || std::is_void_v<T>)
        : Base() {}
    constexpr result(const result&) = default;
    constexpr result(result&&) = default;

    // Converting constructors from result<U, G>
    template <class U, class G>
      requires(!std::is_void_v<T> && std::constructible_from<T, const U&> && std::constructible_from<E, const G&>)
    constexpr explicit(!std::convertible_to<const U&, T> || !std::convertible_to<const G&, E>) result(const result<U, G>& other) {
      if (other.has_value()) {
        std::construct_at(std::addressof(this->val_), other.value());
        this->has_value_ = true;
      } else {
        std::construct_at(std::addressof(this->err_), other.error());
        this->has_value_ = false;
      }
    }

    template <class U, class G>
      requires(!std::is_void_v<T> && std::constructible_from<T, U> && std::constructible_from<E, G>)
    constexpr explicit(!std::convertible_to<U, T> || !std::convertible_to<G, E>) result(result<U, G>&& other) {
      if (other.has_value()) {
        std::construct_at(std::addressof(this->val_), std::move(other).value());
        this->has_value_ = true;
      } else {
        std::construct_at(std::addressof(this->err_), std::move(other).error());
        this->has_value_ = false;
      }
    }

    // Void result specialization for converting constructors
    template <class U, class G>
      requires(std::is_void_v<T> && std::is_void_v<U> && std::constructible_from<E, const G&>)
    constexpr explicit(!std::convertible_to<const G&, E>) result(const result<U, G>& other) {
      if (other.has_value()) {
        this->has_value_ = true;
      } else {
        std::construct_at(std::addressof(this->err_), other.error());
        this->has_value_ = false;
      }
    }

    template <class U, class G>
      requires(std::is_void_v<T> && std::is_void_v<U> && std::constructible_from<E, G>)
    constexpr explicit(!std::convertible_to<G, E>) result(result<U, G>&& other) {
      if (other.has_value()) {
        this->has_value_ = true;
      } else {
        std::construct_at(std::addressof(this->err_), std::move(other).error());
        this->has_value_ = false;
      }
    }

    // Constructor from value
    template <class U = T>
      requires(!std::is_void_v<T> && !aio::result_type<U> && !std::same_as<std::in_place_t, std::remove_cvref_t<U>> &&
               !aio::failure_type<U> && std::constructible_from<T, U>)
    constexpr explicit((!std::convertible_to<U, T>)) result(U&& v) : Base(std::in_place, std::forward<U>(v)) {}

    template <class G>
      requires std::constructible_from<E, const G&>
    constexpr explicit(!std::convertible_to<const G&, E>) result(const failure<G>& f) : Base(failure<E>(f.error())) {}

    template <class G>
      requires std::constructible_from<E, G>
    constexpr explicit(!std::convertible_to<G, E>) result(failure<G>&& f) : Base(failure<E>(std::move(f).error())) {}

    template <class... Args>
      requires std::constructible_from<T, Args...>
    constexpr explicit result(std::in_place_t, Args&&... args) : Base(std::in_place, std::forward<Args>(args)...) {}

    template <class U, class... Args>
      requires std::constructible_from<T, std::initializer_list<U>&, Args...>
    constexpr explicit result(std::in_place_t, std::initializer_list<U> il, Args&&... args)
        : Base(std::in_place, il, std::forward<Args>(args)...) {}

    template <class... Args>
      requires std::constructible_from<E, Args...>
    constexpr explicit result(failure<E> f) : Base(std::move(f)) {}

    constexpr result& operator=(const result&) = default;
    constexpr result& operator=(result&&) = default;

    template <class U = T>
      requires(!aio::result_type<U> && !aio::failure_type<U> && std::constructible_from<T, U> && std::assignable_from<T&, U>)
    constexpr result& operator=(U&& v) {
      if (this->has_value_) {
        this->val_ = std::forward<U>(v);
      } else {
        if constexpr (std::is_nothrow_constructible_v<T, U>) {
          this->err_.~E();
          std::construct_at(std::addressof(this->val_), std::forward<U>(v));
          this->has_value_ = true;
        } else {
          T tmp(std::forward<U>(v));
          this->err_.~E();
          std::construct_at(std::addressof(this->val_), std::move(tmp));
          this->has_value_ = true;
        }
      }
      return *this;
    }

    template <class G>
      requires std::constructible_from<E, const G&> && std::assignable_from<E&, const G&>
    constexpr result& operator=(const failure<G>& f) {
      if (this->has_value_) {
        if constexpr (std::is_nothrow_constructible_v<E, const G&>) {
          this->val_.~T();
          std::construct_at(std::addressof(this->err_), f.error());
          this->has_value_ = false;
        } else {
          E tmp(f.error());
          this->val_.~T();
          std::construct_at(std::addressof(this->err_), std::move(tmp));
          this->has_value_ = false;
        }
      } else {
        this->err_ = f.error();
      }
      return *this;
    }

    template <class G>
      requires std::constructible_from<E, G> && std::assignable_from<E&, G>
    constexpr result& operator=(failure<G>&& f) {
      if (this->has_value_) {
        if constexpr (std::is_nothrow_constructible_v<E, G>) {
          this->val_.~T();
          std::construct_at(std::addressof(this->err_), std::move(f).error());
          this->has_value_ = false;
        } else {
          E tmp(std::move(f).error());
          this->val_.~T();
          std::construct_at(std::addressof(this->err_), std::move(tmp));
          this->has_value_ = false;
        }
      } else {
        this->err_ = std::move(f).error();
      }
      return *this;
    }

    template <class... Args>
      requires std::constructible_from<T, Args...>
    constexpr T& emplace(Args&&... args) {
      if (this->has_value_) {
        this->val_.~T();
      } else {
        this->err_.~E();
        this->has_value_ = true;
      }
      return *std::construct_at(std::addressof(this->val_), std::forward<Args>(args)...);
    }

    template <class U, class... Args>
      requires std::constructible_from<T, std::initializer_list<U>&, Args...>
    constexpr T& emplace(std::initializer_list<U> il, Args&&... args) {
      if (this->has_value_) {
        this->val_.~T();
      } else {
        this->err_.~E();
        this->has_value_ = true;
      }
      return *std::construct_at(std::addressof(this->val_), il, std::forward<Args>(args)...);
    }

    constexpr void swap(result& other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T> &&
                                                std::is_nothrow_move_constructible_v<E> && std::is_nothrow_swappable_v<E>)
      requires std::swappable<T> && std::swappable<E> && std::move_constructible<T> && std::move_constructible<E>
    {
      if (this->has_value_ && other.has_value_) {
        using std::swap;
        swap(this->val_, other.val_);
      } else if (this->has_value_) {
        if constexpr (std::is_nothrow_move_constructible_v<E>) {
          E tmp(std::move(other.err_));
          other.err_.~E();

          if constexpr (std::is_nothrow_move_constructible_v<T>) {
            std::construct_at(std::addressof(other.val_), std::move(this->val_));
          } else {
            try {
              std::construct_at(std::addressof(other.val_), std::move(this->val_));
            } catch (...) {
              std::construct_at(std::addressof(other.err_), std::move(tmp));
              throw;
            }
          }

          this->val_.~T();
          std::construct_at(std::addressof(this->err_), std::move(tmp));

          std::swap(this->has_value_, other.has_value_);
        } else {
          // If E's move constructor can throw, we need a more careful approach
          E tmp(std::move(other.err_));
          other.err_.~E();

          try {
            std::construct_at(std::addressof(other.val_), std::move(this->val_));
            this->val_.~T();
            std::construct_at(std::addressof(this->err_), std::move(tmp));
            std::swap(this->has_value_, other.has_value_);
          } catch (...) {
            std::construct_at(std::addressof(other.err_), std::move(tmp));
            throw;
          }
        }
      } else if (other.has_value_) {
        other.swap(*this);
      } else {
        using std::swap;
        swap(this->err_, other.err_);
      }
    }

    constexpr auto operator->() const noexcept -> const T* { return std::addressof(this->val_); }
    constexpr auto operator->() noexcept -> T* { return std::addressof(this->val_); }
    constexpr auto operator*() const& noexcept -> const T& { return this->val_; }
    constexpr auto operator*() & noexcept -> T& { return this->val_; }
    constexpr auto operator*() const&& noexcept -> const T&& { return std::move(this->val_); }
    constexpr auto operator*() && noexcept -> T&& { return std::move(this->val_); }

    constexpr explicit operator bool() const noexcept { return this->has_value_; }
    constexpr auto has_value() const noexcept -> bool { return this->has_value_; }

    constexpr auto value() const& -> const T& { return this->val_; }
    constexpr auto value() & -> T& { return this->val_; }
    constexpr auto value() const&& -> const T&& { return std::move(this->val_); }
    constexpr auto value() && -> T&& { return std::move(this->val_); }

    constexpr auto error() const& noexcept -> const E& { return this->err_; }
    constexpr auto error() & noexcept -> E& { return this->err_; }
    constexpr auto error() const&& noexcept -> const E&& { return std::move(this->err_); }
    constexpr auto error() && noexcept -> E&& { return std::move(this->err_); }

    template <class U>
    constexpr auto value_or(U&& v) const& -> T {
      if (this->has_value_) return this->val_;
      return static_cast<T>(std::forward<U>(v));
    }

    template <class U>
    constexpr auto value_or(U&& v) && -> T {
      if (this->has_value_) return std::move(this->val_);
      return static_cast<T>(std::forward<U>(v));
    }

    template <class G = E>
    constexpr auto error_or(G&& e) const& -> E {
      if (!this->has_value_) return this->err_;
      return static_cast<E>(std::forward<G>(e));
    }

    template <class G = E>
    constexpr auto error_or(G&& e) && -> E {
      if (!this->has_value_) return std::move(this->err_);
      return static_cast<E>(std::forward<G>(e));
    }

    template <class F>
    constexpr auto and_then(F&& f) &
      requires std::invocable<F, T&> && aio::result_type<std::invoke_result_t<F, T&>>
    {
      if (this->has_value_) {
        return std::invoke(std::forward<F>(f), this->val_);
      }
      using U = typename std::invoke_result_t<F, T&>::value_type;
      return result<U, E>(failure<E>(this->err_));
    }

    template <class F>
    constexpr auto and_then(F&& f) const&
      requires std::invocable<F, const T&> && aio::result_type<std::invoke_result_t<F, const T&>>
    {
      if (this->has_value_) {
        return std::invoke(std::forward<F>(f), this->val_);
      }
      using U = typename std::invoke_result_t<F, const T&>::value_type;
      return result<U, E>(failure<E>(this->err_));
    }

    template <class F>
    constexpr auto and_then(F&& f) &&
      requires std::invocable<F, T&&> && aio::result_type<std::invoke_result_t<F, T&&>>
    {
      if (this->has_value_) {
        return std::invoke(std::forward<F>(f), std::move(this->val_));
      }
      using U = typename std::invoke_result_t<F, T&&>::value_type;
      return result<U, E>(failure<E>(std::move(this->err_)));
    }

    template <class F>
    constexpr auto and_then(F&& f) const&&
      requires std::invocable<F, const T&&> && aio::result_type<std::invoke_result_t<F, const T&&>>
    {
      if (this->has_value_) {
        return std::invoke(std::forward<F>(f), std::move(this->val_));
      }
      using U = typename std::invoke_result_t<F, const T&&>::value_type;
      return result<U, E>(failure<E>(std::move(this->err_)));
    }

    template <class F>
    constexpr auto or_else(F&& f) &
      requires std::invocable<F, E&> && aio::result_type<std::invoke_result_t<F, E&>>
    {
      if (!this->has_value_) {
        return std::invoke(std::forward<F>(f), this->err_);
      }
      using G = typename std::invoke_result_t<F, E&>::error_type;
      return result<T, G>(this->val_);
    }

    template <class F>
    constexpr auto or_else(F&& f) const&
      requires std::invocable<F, const E&> && aio::result_type<std::invoke_result_t<F, const E&>>
    {
      if (!this->has_value_) {
        return std::invoke(std::forward<F>(f), this->err_);
      }
      using G = typename std::invoke_result_t<F, const E&>::error_type;
      return result<T, G>(this->val_);
    }

    template <class F>
    constexpr auto or_else(F&& f) &&
      requires std::invocable<F, E&&> && aio::result_type<std::invoke_result_t<F, E&&>>
    {
      if (!this->has_value_) {
        return std::invoke(std::forward<F>(f), std::move(this->err_));
      }
      using G = typename std::invoke_result_t<F, E&&>::error_type;
      return result<T, G>(std::move(this->val_));
    }

    template <class F>
    constexpr auto or_else(F&& f) const&&
      requires std::invocable<F, const E&&> && aio::result_type<std::invoke_result_t<F, const E&&>>
    {
      if (!this->has_value_) {
        return std::invoke(std::forward<F>(f), std::move(this->err_));
      }
      using G = typename std::invoke_result_t<F, const E&&>::error_type;
      return result<T, G>(std::move(this->val_));
    }

    template <class F>
    constexpr auto transform(F&& f) &
      requires std::invocable<F, T&>
    {
      using U = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
      if (this->has_value_) {
        return result<U, E>(std::in_place, std::invoke(std::forward<F>(f), this->val_));
      }
      return result<U, E>(failure<E>(this->err_));
    }

    template <class F>
    constexpr auto transform(F&& f) const&
      requires std::invocable<F, const T&>
    {
      using U = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
      if (this->has_value_) {
        return result<U, E>(std::in_place, std::invoke(std::forward<F>(f), this->val_));
      }
      return result<U, E>(failure<E>(this->err_));
    }

    template <class F>
    constexpr auto transform(F&& f) &&
      requires std::invocable<F, T&&>
    {
      using U = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
      if (this->has_value_) {
        return result<U, E>(std::in_place, std::invoke(std::forward<F>(f), std::move(this->val_)));
      }
      return result<U, E>(failure<E>(std::move(this->err_)));
    }

    template <class F>
    constexpr auto transform(F&& f) const&&
      requires std::invocable<F, const T&&>
    {
      using U = std::remove_cvref_t<std::invoke_result_t<F, const T&&>>;
      if (this->has_value_) {
        return result<U, E>(std::in_place, std::invoke(std::forward<F>(f), std::move(this->val_)));
      }
      return result<U, E>(failure<E>(std::move(this->err_)));
    }

    template <class F>
    constexpr auto transform_error(F&& f) &
      requires std::invocable<F, E&>
    {
      using G = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
      if (this->has_value_) {
        return result<T, G>(std::in_place, this->val_);
      }
      return result<T, G>(failure<G>(std::invoke(std::forward<F>(f), this->err_)));
    }

    template <class F>
    constexpr auto transform_error(F&& f) const&
      requires std::invocable<F, const E&>
    {
      using G = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
      if (this->has_value_) {
        return result<T, G>(std::in_place, this->val_);
      }
      return result<T, G>(failure<G>(std::invoke(std::forward<F>(f), this->err_)));
    }

    template <class F>
    constexpr auto transform_error(F&& f) &&
      requires std::invocable<F, E&&>
    {
      using G = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
      if (this->has_value_) {
        return result<T, G>(std::in_place, std::move(this->val_));
      }
      return result<T, G>(failure<G>(std::invoke(std::forward<F>(f), std::move(this->err_))));
    }

    template <class F>
    constexpr auto transform_error(F&& f) const&&
      requires std::invocable<F, const E&&>
    {
      using G = std::remove_cvref_t<std::invoke_result_t<F, const E&&>>;
      if (this->has_value_) {
        return result<T, G>(std::in_place, std::move(this->val_));
      }
      return result<T, G>(failure<G>(std::invoke(std::forward<F>(f), std::move(this->err_))));
    }

    // Equality operators
    template <class T2, class E2>
    friend constexpr bool operator==(const result& x, const result<T2, E2>& y) {
      if (x.has_value() != y.has_value()) return false;
      if (x.has_value()) return x.value() == y.value();
      return x.error() == y.error();
    }

    template <class T2>
    friend constexpr bool operator==(const result& x, const T2& v) {
      return x.has_value() && static_cast<bool>(x.value() == v);
    }

    template <class E2>
    friend constexpr bool operator==(const result& x, const failure<E2>& f) {
      return !x.has_value() && static_cast<bool>(x.error() == f.error());
    }
  };

  template <class E>
  class result<void, E> : private detail::result_storage<void, E> {
    using Base = detail::result_storage<void, E>;

   public:
    using value_type = void;
    using error_type = E;
    using failure_type = failure<E>;

    template <class U>
    using rebind = result<U, error_type>;

    // Constructors
    constexpr result() noexcept : Base() {}

    constexpr result(const result&) = default;
    constexpr result(result&&) = default;

    template <class U, class G>
      requires std::is_void_v<U> && std::constructible_from<E, const G&>
    constexpr explicit(!std::convertible_to<const G&, E>) result(const result<U, G>& other) {
      if (other.has_value()) {
        this->has_value_ = true;
      } else {
        std::construct_at(std::addressof(this->err_), other.error());
        this->has_value_ = false;
      }
    }

    template <class U, class G>
      requires std::is_void_v<U> && std::constructible_from<E, G>
    constexpr explicit(!std::convertible_to<G, E>) result(result<U, G>&& other) {
      if (other.has_value()) {
        this->has_value_ = true;
      } else {
        std::construct_at(std::addressof(this->err_), std::move(other).error());
        this->has_value_ = false;
      }
    }

    template <class G>
      requires std::constructible_from<E, const G&>
    constexpr explicit(!std::convertible_to<const G&, E>) result(const failure<G>& f) : Base(failure<E>(f.error())) {}

    template <class G>
      requires std::constructible_from<E, G>
    constexpr explicit(!std::convertible_to<G, E>) result(failure<G>&& f) : Base(failure<E>(std::move(f).error())) {}

    constexpr explicit result(std::in_place_t) noexcept : Base(std::in_place) {}

    template <class... Args>
      requires std::constructible_from<E, Args...>
    constexpr explicit result(failure<E> f) : Base(std::move(f)) {}

    // Assignment operators
    constexpr result& operator=(const result&) = default;
    constexpr result& operator=(result&&) = default;

    template <class G>
      requires std::constructible_from<E, const G&> && std::assignable_from<E&, const G&>
    constexpr result& operator=(const failure<G>& f) {
      if (this->has_value_) {
        std::construct_at(std::addressof(this->err_), f.error());
        this->has_value_ = false;
      } else {
        this->err_ = f.error();
      }
      return *this;
    }

    template <class G>
      requires std::constructible_from<E, G> && std::assignable_from<E&, G>
    constexpr result& operator=(failure<G>&& f) {
      if (this->has_value_) {
        std::construct_at(std::addressof(this->err_), std::move(f).error());
        this->has_value_ = false;
      } else {
        this->err_ = std::move(f).error();
      }
      return *this;
    }

    // Modifiers
    constexpr void emplace() noexcept {
      if (!this->has_value_) {
        this->err_.~E();
        this->has_value_ = true;
      }
    }

    // Swap
    constexpr void swap(result& other) noexcept(std::is_nothrow_move_constructible_v<E> && std::is_nothrow_swappable_v<E>)
      requires std::swappable<E> && std::move_constructible<E>
    {
      if (this->has_value_ && other.has_value_) {
        // Both have value, nothing to do
      } else if (this->has_value_) {
        if constexpr (std::is_nothrow_move_constructible_v<E>) {
          std::construct_at(std::addressof(this->err_), std::move(other.err_));
          other.err_.~E();
          this->has_value_ = false;
          other.has_value_ = true;
        } else {
          // If E's move constructor can throw, we need a more careful approach
          E tmp(std::move(other.err_));
          other.err_.~E();

          try {
            std::construct_at(std::addressof(this->err_), std::move(tmp));
            this->has_value_ = false;
            other.has_value_ = true;
          } catch (...) {
            std::construct_at(std::addressof(other.err_), std::move(tmp));
            throw;
          }
        }
      } else if (other.has_value_) {
        other.swap(*this);
      } else {
        using std::swap;
        swap(this->err_, other.err_);
      }
    }

    // Observers
    constexpr explicit operator bool() const noexcept { return this->has_value_; }

    constexpr bool has_value() const noexcept { return this->has_value_; }

    constexpr void value() const& {
      if (!this->has_value_) {
        throw bad_result_access(this->err_);
      }
    }

    constexpr void value() && {
      if (!this->has_value_) {
        throw bad_result_access(std::move(this->err_));
      }
    }

    constexpr const E& error() const& noexcept { return this->err_; }

    constexpr E& error() & noexcept { return this->err_; }

    constexpr const E&& error() const&& noexcept { return std::move(this->err_); }

    constexpr E&& error() && noexcept { return std::move(this->err_); }

    template <class G = E>
    constexpr E error_or(G&& e) const& {
      if (!this->has_value_) {
        return this->err_;
      }
      return static_cast<E>(std::forward<G>(e));
    }

    template <class G = E>
    constexpr E error_or(G&& e) && {
      if (!this->has_value_) {
        return std::move(this->err_);
      }
      return static_cast<E>(std::forward<G>(e));
    }

    // Monadic operations
    template <class F>
    constexpr auto and_then(F&& f) &
      requires std::invocable<F> && aio::result_type<std::invoke_result_t<F>>
    {
      if (this->has_value_) {
        return std::invoke(std::forward<F>(f));
      }
      using U = typename std::invoke_result_t<F>::value_type;
      return result<U, E>(failure<E>(this->err_));
    }

    template <class F>
    constexpr auto and_then(F&& f) const&
      requires std::invocable<F> && aio::result_type<std::invoke_result_t<F>>
    {
      if (this->has_value_) {
        return std::invoke(std::forward<F>(f));
      }
      using U = typename std::invoke_result_t<F>::value_type;
      return result<U, E>(failure<E>(this->err_));
    }

    template <class F>
    constexpr auto and_then(F&& f) &&
      requires std::invocable<F> && aio::result_type<std::invoke_result_t<F>>
    {
      if (this->has_value_) {
        return std::invoke(std::forward<F>(f));
      }
      using U = typename std::invoke_result_t<F>::value_type;
      return result<U, E>(failure<E>(std::move(this->err_)));
    }

    template <class F>
    constexpr auto and_then(F&& f) const&&
      requires std::invocable<F> && aio::result_type<std::invoke_result_t<F>>
    {
      if (this->has_value_) {
        return std::invoke(std::forward<F>(f));
      }
      using U = typename std::invoke_result_t<F>::value_type;
      return result<U, E>(failure<E>(std::move(this->err_)));
    }

    template <class F>
    constexpr auto or_else(F&& f) &
      requires std::invocable<F, E&> && aio::result_type<std::invoke_result_t<F, E&>>
    {
      if (!this->has_value_) {
        return std::invoke(std::forward<F>(f), this->err_);
      }
      using G = typename std::invoke_result_t<F, E&>::error_type;
      return result<void, G>();
    }

    template <class F>
    constexpr auto or_else(F&& f) const&
      requires std::invocable<F, const E&> && aio::result_type<std::invoke_result_t<F, const E&>>
    {
      if (!this->has_value_) {
        return std::invoke(std::forward<F>(f), this->err_);
      }
      using G = typename std::invoke_result_t<F, const E&>::error_type;
      return result<void, G>();
    }

    template <class F>
    constexpr auto or_else(F&& f) &&
      requires std::invocable<F, E&&> && aio::result_type<std::invoke_result_t<F, E&&>>
    {
      if (!this->has_value_) {
        return std::invoke(std::forward<F>(f), std::move(this->err_));
      }
      using G = typename std::invoke_result_t<F, E&&>::error_type;
      return result<void, G>();
    }

    template <class F>
    constexpr auto or_else(F&& f) const&&
      requires std::invocable<F, const E&&> && aio::result_type<std::invoke_result_t<F, const E&&>>
    {
      if (!this->has_value_) {
        return std::invoke(std::forward<F>(f), std::move(this->err_));
      }
      using G = typename std::invoke_result_t<F, const E&&>::error_type;
      return result<void, G>();
    }

    template <class F>
    constexpr auto transform(F&& f) &
      requires std::invocable<F>
    {
      using U = std::remove_cvref_t<std::invoke_result_t<F>>;
      if (this->has_value_) {
        return result<U, E>(std::in_place, std::invoke(std::forward<F>(f)));
      }
      return result<U, E>(failure<E>(this->err_));
    }

    template <class F>
    constexpr auto transform(F&& f) const&
      requires std::invocable<F>
    {
      using U = std::remove_cvref_t<std::invoke_result_t<F>>;
      if (this->has_value_) {
        return result<U, E>(std::in_place, std::invoke(std::forward<F>(f)));
      }
      return result<U, E>(failure<E>(this->err_));
    }

    template <class F>
    constexpr auto transform(F&& f) &&
      requires std::invocable<F>
    {
      using U = std::remove_cvref_t<std::invoke_result_t<F>>;
      if (this->has_value_) {
        return result<U, E>(std::in_place, std::invoke(std::forward<F>(f)));
      }
      return result<U, E>(failure<E>(std::move(this->err_)));
    }

    template <class F>
    constexpr auto transform(F&& f) const&&
      requires std::invocable<F>
    {
      using U = std::remove_cvref_t<std::invoke_result_t<F>>;
      if (this->has_value_) {
        return result<U, E>(std::in_place, std::invoke(std::forward<F>(f)));
      }
      return result<U, E>(failure<E>(std::move(this->err_)));
    }

    template <class F>
    constexpr auto transform_error(F&& f) &
      requires std::invocable<F, E&>
    {
      using G = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
      if (this->has_value_) {
        return result<void, G>();
      }
      return result<void, G>(failure<G>(std::invoke(std::forward<F>(f), this->err_)));
    }

    template <class F>
    constexpr auto transform_error(F&& f) const&
      requires std::invocable<F, const E&>
    {
      using G = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
      if (this->has_value_) {
        return result<void, G>();
      }
      return result<void, G>(failure<G>(std::invoke(std::forward<F>(f), this->err_)));
    }

    template <class F>
    constexpr auto transform_error(F&& f) &&
      requires std::invocable<F, E&&>
    {
      using G = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
      if (this->has_value_) {
        return result<void, G>();
      }
      return result<void, G>(failure<G>(std::invoke(std::forward<F>(f), std::move(this->err_))));
    }

    template <class F>
    constexpr auto transform_error(F&& f) const&&
      requires std::invocable<F, const E&&>
    {
      using G = std::remove_cvref_t<std::invoke_result_t<F, const E&&>>;
      if (this->has_value_) {
        return result<void, G>();
      }
      return result<void, G>(failure<G>(std::invoke(std::forward<F>(f), std::move(this->err_))));
    }

    // Equality operators
    template <class T2, class E2>
    friend constexpr bool operator==(const result& x, const result<T2, E2>& y) {
      if (x.has_value() != y.has_value()) return false;
      if (x.has_value()) return true;
      return x.error() == y.error();
    }

    friend constexpr bool operator==(const result& x, const std::monostate&) { return x.has_value(); }

    template <class E2>
    friend constexpr bool operator==(const result& x, const failure<E2>& f) {
      return !x.has_value() && static_cast<bool>(x.error() == f.error());
    }
  };

  // Deduction guides
  template <class T, class E>
  result(T, E) -> result<T, E>;

  template <class T>
  result(T) -> result<T, std::monostate>;

  template <class E>
  result(failure<E>) -> result<void, E>;

  // swap specialization
  template <class T, class E>
    requires(std::swappable<T> && std::swappable<E> && std::move_constructible<T> && std::move_constructible<E>)
  constexpr void swap(result<T, E>& x, result<T, E>& y) noexcept(noexcept(x.swap(y))) {
    x.swap(y);
  }

  // Helper function to create result from value
  template <class T>
  constexpr auto success(T&& value) {
    return result<std::remove_cvref_t<T>, std::monostate>(std::forward<T>(value));
  }

  // Helper function to create void result
  constexpr auto success() { return result<void, std::monostate>(); }

  // Helper function to create result from failure
  template <class E>
  constexpr auto fail(E&& error) {
    return failure<std::remove_cvref_t<E>>(std::forward<E>(error));
  }

  template <class E, class... Args>
  constexpr auto fail(Args&&... args) {
    return failure<std::remove_cvref_t<E>>(std::in_place, std::forward<Args>(args)...);
  }
}  // namespace aio



#endif  // AIO_RESULT_HPP
