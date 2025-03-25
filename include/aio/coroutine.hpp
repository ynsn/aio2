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

#ifndef AIO_COROUTINE_HPP
#define AIO_COROUTINE_HPP

#include <coroutine>
#include <exception>

#include "detail/macros.hpp"

namespace aio {

  /**
   * \defgroup coroutine coroutine
   * \brief The `coroutine` module provides utilities for working with coroutines and awaitables.
   */

  namespace detail {
    template <class T>
    inline constexpr auto is_coroutine_handle_v = false;
    template <class T>
    inline constexpr auto is_coroutine_handle_v<std::coroutine_handle<T>> = true;

    template <class T>
    concept await_suspend_result = std::same_as<T, void> || std::same_as<T, bool> || is_coroutine_handle_v<T>;

    template <class T>
    // ReSharper disable once CppFunctionIsNotImplemented
    auto as_lvalue(T &&) -> T &;
  }  // namespace detail

  /// \ingroup coroutine
  ///
  /// \brief Concept defining requirements for awaiter types in C++ coroutines
  ///
  /// This concept specifies the requirements for a type to be usable as an awaiter
  /// in C++ coroutines. Awaiters are the fundamental building blocks that determine
  /// how coroutine suspension and resumption behave.
  ///
  /// An awaiter must provide three specific methods:
  /// - await_ready(): Determines if the coroutine should suspend or continue execution
  /// - await_suspend(handle): Called when suspending the coroutine
  /// - await_resume(): Called when resuming the coroutine to produce a result
  ///
  /// \tparam T The type being tested for awaiter capabilities
  /// \tparam Promise The promise type of the coroutine handle, defaults to void
  ///
  /// \note The await_suspend method must return void, bool, or a coroutine_handle
  /// as defined by detail::await_suspend_result
  template <class T, class Promise = void>
  concept awaiter = requires(T &&awaiter, std::coroutine_handle<Promise> handle) {
    { awaiter.await_ready() ? 1 : 0 };
    { awaiter.await_suspend(handle) } -> detail::await_suspend_result;
    { awaiter.await_resume() };
  };

  /// \ingroup coroutine
  ///
  /// \brief Concept defining awaiter types that return a specific result type
  ///
  /// This concept extends the base awaiter concept by adding a constraint that
  /// the awaiter's await_resume() method must return a specific result type.
  /// This is useful for ensuring type safety when working with coroutines that
  /// need to yield values of particular types.
  ///
  /// \tparam T The type being tested for awaiter capabilities
  /// \tparam Result The specific type that await_resume() must return
  /// \tparam Promise The promise type of the coroutine handle, defaults to void
  ///
  /// \see awaiter The base concept that this concept extends
  template <class T, class Result, class Promise = void>
  concept awaiter_of = aio::awaiter<T> && requires(T &&awaiter) {
    { awaiter.await_resume() } -> std::same_as<Result>;
  };

  /// \ingroup coroutine
  ///
  /// \brief Extracts an awaiter from an awaitable object
  ///
  /// This function template obtains the awaiter for a given awaitable object using
  /// the appropriate mechanism. It attempts the following approaches in order:
  /// 1. Call the member `operator co_await()` if available
  /// 2. Call the free function `operator co_await()` if available
  /// 3. Return the awaitable directly (when the awaitable is already an awaiter)
  ///
  /// This utility function is a key part of implementing the C++ coroutine protocol
  /// as it handles the extraction of awaiters in a uniform way regardless of how
  /// they're provided.
  ///
  /// \tparam Awaitable The awaitable type from which to extract an awaiter
  /// \param awaitable The awaitable object to extract an awaiter from
  /// \param _ Optional parameter, unused (defaults to nullptr)
  ///
  /// \return The awaiter object that will control the coroutine's suspension and resumption
  ///
  /// \note This overload handles the case where no promise type is involved or known
  template <class Awaitable>
  static constexpr decltype(auto) get_awaiter(Awaitable &&awaitable, void * = nullptr) {
    if constexpr (requires { AIO_FWD(awaitable).operator co_await(); }) {
      return AIO_FWD(awaitable).operator co_await();
    } else if constexpr (requires { operator co_await(AIO_FWD(awaitable)); }) {
      return operator co_await(AIO_FWD(awaitable));
    } else {
      return AIO_FWD(awaitable);
    }
  }

  /// \ingroup coroutine
  ///
  /// \brief Extracts an awaiter from an awaitable object using a promise's await_transform
  ///
  /// This function template overload obtains an awaiter for a given awaitable object
  /// when a promise with an await_transform method is involved. The function first
  /// transforms the awaitable using the promise's await_transform method, then extracts
  /// an awaiter from the result using the following approaches in order:
  /// 1. Call the member `operator co_await()` on the transformed object if available
  /// 2. Call the free function `operator co_await()` with the transformed object if available
  /// 3. Use the transformed object directly as an awaiter
  ///
  /// This overload is particularly important for coroutines that customize the awaiting
  /// behavior through their promise type's await_transform method.
  ///
  /// \tparam Awaitable The awaitable type to extract an awaiter from
  /// \tparam Promise The promise type that will transform the awaitable
  /// \param awaitable The awaitable object to extract an awaiter from
  /// \param promise Pointer to the promise object that will transform the awaitable
  ///
  /// \return The awaiter object that will control the coroutine's suspension and resumption
  ///
  /// \see get_awaiter The non-promise overload of this function
  template <class Awaitable, class Promise>
  static constexpr awaiter decltype(auto) get_awaiter(Awaitable &&awaitable, Promise *promise)
    requires requires { promise->await_transform(AIO_FWD(awaitable)); }
  {
    if constexpr (requires { promise->await_transform(AIO_FWD(awaitable)).operator co_await(); }) {
      return promise->await_transform(AIO_FWD(awaitable)).operator co_await();
    } else if constexpr (requires { operator co_await(promise->await_transform(AIO_FWD(awaitable))); }) {
      return operator co_await(promise->await_transform(AIO_FWD(awaitable)));
    } else {
      return promise->await_transform(AIO_FWD(awaitable));
    }
  }

  /// \ingroup coroutine
  ///
  /// \brief Concept defining requirements for awaitable types in C++ coroutines
  ///
  /// This concept specifies the requirements for a type to be awaitable in C++ coroutines.
  /// An awaitable is any object that can be used with the co_await operator, meaning
  /// it can be converted to an awaiter object that controls the coroutine's suspension
  /// and resumption.
  ///
  /// A type is awaitable if:
  /// - It provides a member operator co_await(), or
  /// - It works with a free operator co_await() function, or
  /// - It is already an awaiter, or
  /// - It can be transformed by a promise's await_transform method into any of the above
  ///
  /// This concept is central to the coroutine mechanism as it determines which objects
  /// can be used with the co_await operator.
  ///
  /// \tparam Awaitable The type being tested for awaitable capabilities
  /// \tparam Promise The promise type that might transform the awaitable, defaults to void
  ///
  /// \see awaiter The concept that defines the requirements for the extracted awaiter
  /// \see get_awaiter The function used to extract the awaiter from an awaitable
  template <class Awaitable, class Promise = void>
  concept awaitable = requires(Awaitable &&awaitable, Promise *promise) {
    { aio::get_awaiter(AIO_FWD(awaitable), promise) } -> awaiter<Promise>;
  };

  /// \ingroup coroutine
  ///
  /// \brief Concept defining awaitable types that yield a specific result type
  ///
  /// This concept extends the base awaitable concept by adding a constraint that
  /// the awaiter extracted from the awaitable must return a specific result type
  /// from its await_resume() method. This ensures type safety when working with
  /// coroutines that expect to receive values of particular types from co_await
  /// expressions.
  ///
  /// A type satisfies this concept if:
  /// - It is awaitable (can be used with co_await), and
  /// - The awaiter extracted from it returns the specified Result type from await_resume()
  ///
  /// \tparam Awaitable The type being tested for awaitable capabilities
  /// \tparam Result The specific type that the awaiter's await_resume() must return
  /// \tparam Promise The promise type that might transform the awaitable, defaults to void
  ///
  /// \see awaitable The base concept that this concept extends
  /// \see awaiter_of The related concept that constrains the awaiter's result type
  /// \see get_awaiter The function used to extract the awaiter from an awaitable
  template <class Awaitable, class Result, class Promise = void>
  concept awaitable_of = aio::awaitable<Awaitable, Promise> && requires(Awaitable &&awaitable, Promise *promise) {
    { aio::get_awaiter(AIO_FWD(awaitable), promise) } -> awaiter_of<Result>;
  };

  /// \ingroup coroutine
  ///
  /// \brief Type alias that determines the awaiter type for an awaitable object
  ///
  /// This template alias extracts the specific awaiter type that would be used when
  /// co_awaiting an awaitable object. It represents the concrete type that controls
  /// the coroutine's suspension and resumption behavior when the specified awaitable
  /// is used with co_await.
  ///
  /// This is useful for metaprogramming and when implementing coroutine utilities
  /// that need to reason about the specific awaiter type derived from an awaitable.
  ///
  /// \tparam Awaitable The awaitable type from which to extract the awaiter type
  ///
  /// \see awaitable The concept that constrains the input type
  /// \see get_awaiter The function used to extract the awaiter from an awaitable
  /// \see await_result_t The related alias that extracts the result type
  template <class Awaitable>
    requires aio::awaitable<Awaitable>
  using awaiter_type_t = decltype(aio::get_awaiter(std::declval<Awaitable>(), static_cast<void *>(nullptr)));

  /// \ingroup coroutine
  ///
  /// \brief Type alias that extracts the result type from awaiting an awaitable object
  ///
  /// This template alias determines the type that would be returned when a coroutine
  /// awaits on the specified awaitable type. It extracts the return type of the
  /// await_resume() method from the awaiter derived from the awaitable.
  ///
  /// This is particularly useful for:
  /// - Metaprogramming with coroutines
  /// - Static type checking of coroutine expressions
  /// - Building generic coroutine utilities that need to know result types
  ///
  /// \tparam Awaitable The awaitable type from which to extract the result type
  /// \tparam Promise The promise type that might transform the awaitable, defaults to void
  ///
  /// \see awaitable The concept that constrains the input type
  /// \see get_awaiter The function used to extract the awaiter from an awaitable
  /// \see awaiter_type_t The related alias that extracts the awaiter type
  template <class Awaitable, class Promise = void>
    requires aio::awaitable<Awaitable, Promise>
  using await_result_t =
      decltype(detail::as_lvalue(aio::get_awaiter(std::declval<Awaitable>(), static_cast<Promise *>(nullptr))).await_resume());

  template <class Promise = void>
  class continuation_handle;

  /// \ingroup coroutine
  ///
  /// \brief Specialization of continuation_handle for void promise types
  ///
  /// This class provides a type-erased wrapper around coroutine handles that can
  /// manage continuations regardless of the specific promise type. It particularly
  /// handles the case of coroutines that might be stopped without special handling
  /// elsewhere in the code.
  ///
  /// The void specialization enables working with arbitrary coroutine handles through
  /// a common interface, while preserving type-specific behavior for unhandled
  /// stopped coroutines.
  ///
  /// \see continuation_handle The primary template that this specializes
  template <>
  class continuation_handle<void> {
   public:
    constexpr continuation_handle() = default;
    template <class Promise>
    constexpr explicit continuation_handle(std::coroutine_handle<Promise> coro) noexcept : _handle(coro) {
      if constexpr (requires(Promise &promise) { promise.unhandled_stopped(); }) {
        _callback = [](void *addr) noexcept -> std::coroutine_handle<> {
          return std::coroutine_handle<Promise>::from_address(addr).promise().unhandled_stopped();
        };
      }
    }

   public:
    [[nodiscard]] constexpr auto handle() const noexcept -> std::coroutine_handle<> { return _handle; }
    [[nodiscard]] constexpr auto unhandled_stopped() const noexcept -> std::coroutine_handle<> {
      return _callback(_handle.address());
    }

   private:
    using callback_type = auto (*)(void *) noexcept -> std::coroutine_handle<>;

    std::coroutine_handle<> _handle{};
    callback_type _callback = [](void *) noexcept -> std::coroutine_handle<> { std::terminate(); };
  };

  /// \ingroup coroutine
  ///
  /// \brief Primary template for continuation_handle that wraps coroutine handles with specific promise types
  ///
  /// This class template provides a wrapper around coroutine handles with known promise types,
  /// allowing for type-safe access to the underlying coroutine while still supporting
  /// the unhandled stopped case. Unlike the void specialization, this template preserves
  /// the static type information of the promise.
  ///
  /// \tparam Promise The specific promise type of the wrapped coroutine handle
  ///
  /// \see continuation_handle<void> The specialization that handles type erasure
  template <class Promise>
  class continuation_handle {
   public:
    constexpr continuation_handle() = default;
    constexpr explicit continuation_handle(std::coroutine_handle<Promise> coro) noexcept : _handle(coro) {}

   public:
    [[nodiscard]] constexpr auto handle() const noexcept -> std::coroutine_handle<Promise> {
      return std::coroutine_handle<Promise>::from_address(_handle.handle().address());
    }

    [[nodiscard]] constexpr auto unhanded_stopped() const noexcept -> std::coroutine_handle<> {
      return _handle.unhandled_stopped();
    }

   private:
    continuation_handle<> _handle{};
  };
}  // namespace aio

#endif  // AIO_COROUTINE_HPP
