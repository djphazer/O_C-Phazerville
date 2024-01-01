// Copyright 2020 Patrick Dowling
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
#ifndef UTIL_TEMPLATES_H_
#define UTIL_TEMPLATES_H_

#include <type_traits>

namespace util {

// C++11, >= 14 have their own
template <size_t... Is>
struct index_sequence {
  template <size_t N> using append = index_sequence<Is..., N>;
};

template <size_t size>
struct make_index_sequence {
  using type = typename make_index_sequence<size - 1>::type::template append<size - 1>;
};

template <>
struct make_index_sequence<0U> {
  using type = index_sequence<>;
};

template<typename T, T ...>
struct sum : std::integral_constant<T, 0> { };

template<typename T, T S, T ... Ss>
struct sum<T, S, Ss...> : std::integral_constant<T, S + sum<T, Ss...>::value > { };


} // util

#endif // UTIL_TEMPLATES_H_
