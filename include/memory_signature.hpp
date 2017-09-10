/*
* Copyright 2017 Justas Masiulis
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef JM_MEMORY_SIGNATURE_HPP
#define JM_MEMORY_SIGNATURE_HPP

#include <memory>    // unique_ptr
#include <algorithm> // search
#include <bitset>    // bitset

namespace jm {

    namespace detail {

        template<typename InIt, typename Cmp>
        inline unsigned char find_wildcard(InIt first, InIt last, Cmp cmp)
        {
            std::bitset<256> bits;

            for (; first != last; ++first)
                bits[*first] = cmp(*first);

            for (auto i = 0; i < 256; ++i)
                if (!bits[i])
                    return static_cast<unsigned char>(i);

            throw std::range_error("unable to find unused byte in the provided pattern");
        }

        template<typename InIt, typename Cmp>
        unsigned char find_wildcard_masked(InIt first, InIt last, InIt mask_first, unsigned char unknown)
        {
            return find_wildcard(first, last, [=]() mutable {
                return *mask_first++ != unknown;
            });
        }


    }

    class memory_signature {
        std::unique_ptr<unsigned char[]> _pattern;
        unsigned char                    *_end;
        unsigned char                    _wildcard;

        std::size_t size() const noexcept { return _end - _pattern.get(); }

        template<class ForwardItPat, class ForwardItSig>
        void
        masked_to_wilcard(ForwardItPat first_pat, ForwardItPat last_pat, ForwardItSig first_mask, unsigned char unknown)
        {
            auto my_pat = _pattern.get();
            for (; first_pat != last_pat; ++first_pat, ++first_mask, ++my_pat) {
                if (*first_mask != unknown)
                    *my_pat = *first_pat;
                else
                    *my_pat = _wildcard;
            }
        }

    public:
        /// \brief Construct a new signature that is empty
        explicit constexpr memory_signature() noexcept
                : _pattern(nullptr)
                , _end(nullptr)
                , _wildcard(0) {}

        /// \brief Destroys the stored signature
        ~memory_signature() noexcept = default;

        memory_signature(const memory_signature &other)
                : _wildcard(other._wildcard)
        {
            auto new_pat = std::unique_ptr<unsigned char[]>(new unsigned char[other.size()]);
            std::copy(other._pattern.get(), other._end, new_pat.get());
            _pattern = std::move(new_pat);
            _end     = _pattern.get() + other.size();
        }

        memory_signature &operator=(const memory_signature &other)
        {
            auto new_pat = std::unique_ptr<unsigned char[]>(new unsigned char[other.size()]);
            std::copy(other._pattern.get(), other._end, new_pat.get());
            _pattern  = std::move(new_pat);
            _end      = _pattern.get() + other.size();
            _wildcard = other._wildcard;
            return *this;
        }

        memory_signature(memory_signature &&other) noexcept
                : _pattern(std::move(other._pattern))
                , _end(other._end)
                , _wildcard(other._wildcard) {}

        memory_signature &operator=(memory_signature &&other) noexcept
        {
            _pattern  = std::move(other._pattern);
            _end      = other._end;
            _wildcard = other._wildcard;
            return *this;
        }


        /// wildcard signature constructors ----------------------------------------------------------------------------
        template<class Wildcard>
        memory_signature(const std::string &pattern, Wildcard wildcard)
                : _pattern(std::unique_ptr<unsigned char[]>(new unsigned char[pattern.size()]))
                , _end(_pattern.get() + pattern.size())
                , _wildcard(static_cast<unsigned char>(wildcard))
        {
            std::copy(std::begin(pattern), std::end(pattern), _pattern.get());
        }

        template<class Wildcard>
        memory_signature(std::initializer_list<unsigned char> pattern, Wildcard wildcard)
                : _pattern(std::unique_ptr<unsigned char[]>(new unsigned char[pattern.size()]))
                , _end(_pattern.get() + pattern.size())
                , _wildcard(static_cast<unsigned char>(wildcard))
        {
            std::copy(std::begin(pattern), std::end(pattern), _pattern.get());
        }

        /// masked signature constructors ------------------------------------------------------------------------------
        template<class Unknown>
        memory_signature(const std::string &pattern, const std::string &mask, Unknown unknown_byte_identifier = '?')
                : _pattern(std::unique_ptr<unsigned char[]>(new unsigned char[pattern.size()]))
                , _end(_pattern.get() + pattern.size())
                , _wildcard(detail::find_wildcard_masked(pattern.begin(), pattern.end(), mask.begin()
                                                         , unknown_byte_identifier))
        {
            if (pattern.size() != mask.size())
                throw std::invalid_argument("pattern size did not match mask size");

            masked_to_wilcard(pattern.begin(), pattern.end(), mask.begin(), unknown_byte_identifier);
        }

        template<class Byte>
        memory_signature(std::initializer_list<Byte> pattern, std::initializer_list<Byte> mask
                         , Byte unknown_byte_identifier = '?')
                : _pattern(std::unique_ptr<unsigned char[]>(new unsigned char[pattern.size()]))
                , _end(_pattern.get() + pattern.size())
                , _wildcard(detail::find_wildcard_masked(pattern.begin(), pattern.end(), mask.begin()
                                                         , unknown_byte_identifier))
        {
            if (pattern.size() != mask.size())
                throw std::invalid_argument("pattern size did not match mask size");

            masked_to_wilcard(pattern.begin(), pattern.end(), mask.begin(), unknown_byte_identifier);
        }

        /// \brief Searches for first occurrence of stored signature in the range [first, last - signature_length).
        /// \return Returns iterator to the beginning of signature.
        ///         If no such signature is found or if signature is empty returns last.
        template<class ForwardIt>
        ForwardIt find(ForwardIt first, ForwardIt last) const
        {
            if (_pattern.get() == _end)
                return last;

            return std::search(first, last, _pattern.get(), _end, [wildcard = _wildcard](unsigned char lhs
                                                                                         , unsigned char rhs) {
                return lhs == rhs || rhs == wildcard;
            });
        }
    };

}

#endif // include guard
