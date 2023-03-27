// Copyright 2023 Karma Krafts & associates
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @author Alexander Hinze
 * @since 27/03/2023
 */

#pragma once

#include <optional>
#include <functional>

#include "concepts.hpp"
#include "stream_fwd.hpp"
#include "iterator_streamable.hpp"
#include "singlet_streamable.hpp"
#include "basic_stream.hpp"
#include "filtering_stream.hpp"
#include "limiting_stream.hpp"
#include "mapping_stream.hpp"

namespace cxxstreams {
    template<typename T, typename S, typename IMPL> //
    requires(concepts::is_streamable<S>)
    class Stream {
        protected:

        [[maybe_unused]] S _streamable;

        [[nodiscard]] constexpr auto get_self() noexcept -> IMPL& {
            static_assert(concepts::is_streamable<IMPL>, "Implementation is not streamable");
            return static_cast<IMPL&>(*this);
        }

        public:

        explicit constexpr Stream(S streamable) noexcept:
                _streamable(std::move(streamable)) {
        }

        template<typename F>
        requires(std::is_convertible_v<F, std::function<void(T&)>>)
        [[nodiscard]] constexpr auto filter(F&& filter) noexcept -> FilteringStream<IMPL, F> {
            return FilteringStream<IMPL, F>(std::move(get_self()), std::forward<F>(filter));
        }

        template<typename M, typename R = std::invoke_result_t<M, T>>
        requires(std::is_convertible_v<M, std::function<R(T)>>)
        [[nodiscard]] constexpr auto map(M&& mapper) noexcept -> MappingStream<R, IMPL, M> {
            return MappingStream<R, IMPL, M>(std::move(get_self()), std::forward<M>(mapper));
        }

        [[nodiscard]] constexpr auto limit(size_t max_count) noexcept -> LimitingStream<IMPL> {
            return LimitingStream<IMPL>(std::move(get_self()), max_count);
        }

        [[nodiscard]] constexpr auto find_first() noexcept -> std::optional<T> {
            return get_self().next();
        }

        template<typename F>
        requires(std::is_convertible_v<F, std::function<T(T, T)>>)
        [[nodiscard]] constexpr auto reduce(F&& function) noexcept -> std::optional<T> {
            auto& self = get_self();
            auto element = self.next();

            if (!element) {
                return std::nullopt;
            }

            auto acc = std::move(*element);
            auto next = self.next();

            while (next) {
                acc = std::move(function(std::move(acc), std::move(*next)));
                next = self.next();
            }

            return std::make_optional(acc);
        }

        [[nodiscard]] constexpr auto sum() noexcept -> std::optional<T> {
            static_assert(concepts::has_add<T>, "Stream value type doesn't implement operator+");

            return reduce([](auto a, auto b) {
                return a + b;
            });
        }

        [[nodiscard]] constexpr auto min() noexcept -> std::optional<T> {
            static_assert(concepts::has_lth<T> || concepts::has_gth<T>, "Stream value type doesn't implement operator< or operator>");

            auto& self = get_self();
            auto element = self.next();

            if (!element) {
                return std::nullopt;
            }

            auto result = std::move(*element);
            element = self.next();

            while (element) {
                auto value = std::move(*element);

                if constexpr (concepts::has_lth<T>) {
                    if (value < result) {
                        result = std::move(value);
                    }
                }
                else {
                    if (!(value > result)) { // NOLINT: clang-tidy doesn't understand what we're doing here
                        result = std::move(value);
                    }
                }

                element = self.next();
            }

            return std::make_optional(result);
        }

        [[nodiscard]] constexpr auto max() noexcept -> std::optional<T> {
            static_assert(concepts::has_lth<T> || concepts::has_gth<T>, "Stream value type doesn't implement operator< or operator>");

            auto& self = get_self();
            auto element = self.next();

            if (!element) {
                return std::nullopt;
            }

            auto result = std::move(*element);
            element = self.next();

            while (element) {
                auto value = std::move(*element);

                if constexpr (concepts::has_lth<T>) {
                    if (value > result) {
                        result = std::move(value);
                    }
                }
                else {
                    if (!(value < result)) { // NOLINT: clang-tidy doesn't understand what we're doing here
                        result = std::move(value);
                    }
                }

                element = self.next();
            }

            return std::make_optional(result);
        }

        [[nodiscard]] constexpr auto count() noexcept -> size_t {
            size_t result = 0;
            auto& self = get_self();
            auto element = self.next();

            while (element) {
                result++;
                element = self.next();
            }

            return result;
        }

        template<template<typename, typename...> typename C>
        requires(std::is_default_constructible_v<C<T>> && (concepts::has_add_assign<C<T>> || concepts::has_push_back<C<T>>))
        [[nodiscard]] constexpr auto collect() noexcept -> C <T> {
            C<T> result;
            auto& self = get_self();
            auto element = self.next();

            while (element) {
                if constexpr (concepts::has_add_assign<C<T>>) {
                    result += std::move(*element);
                }
                else {
                    result.push_back(std::move(*element));
                }

                element = self.next();
            }

            return result;
        }
    };

    template<typename T, template<typename, typename...> typename C>
    requires(concepts::is_const_iterable<C<T>>)
    [[nodiscard]] constexpr auto make_stream(const C<T>& container) noexcept -> BasicStream<IteratorStreamable<typename C<T>::const_iterator>> {
        return BasicStream(IteratorStreamable(container.cbegin(), container.cend()));
    }

    template<typename T, template<typename, typename...> typename C>
    requires(concepts::is_const_reverse_iterable<C<T>>)
    [[nodiscard]] constexpr auto make_reverse_stream(const C<T>& container) noexcept -> BasicStream<IteratorStreamable<typename C<T>::const_iterator>> {
        return BasicStream(IteratorStreamable(container.crbegin(), container.crend()));
    }
}