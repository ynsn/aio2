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

namespace aio {

  /**
   * \defgroup result Result
   * \brief The `result` module provides facilities for error handling without exceptions and forcing the user to check and handle errors.
   */

  /// \ingroup result
  /// \brief The `result<T, E>` class is a utility for handling errors without exceptions.
  template <class T, class E>
  class [[nodiscard]] result;
}  // namespace aio

#endif  // AIO_RESULT_HPP
