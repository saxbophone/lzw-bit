#ifndef COM_SAXBOPHONE_CRYPTO_ASSIGNMENTS_ENCODER_HPP
#define COM_SAXBOPHONE_CRYPTO_ASSIGNMENTS_ENCODER_HPP

#include <cassert>
#include <cstddef> // size_t
#include <cstdint> // uintmax_t
#include <vector>
#include <tuple>


namespace {
    // log-n optimised integer power function. Computes x to the power of y
    constexpr std::uintmax_t pow(std::uintmax_t x, std::uintmax_t y) {
        // base cases for efficiency and guaranteed termination
        switch (y) {
        case 0:
            return 1;
        case 1:
            return x;
        case 2:
            return x * x;
        case 3:
            return x * x * x;
        }
        // OTHERWISE:
        std::uintmax_t square_root = pow(x, y / 2);
        // otherwise, work out if y is a multiple of 2 or not
        if (y % 2 == 0) {
            return square_root * square_root;
        } else {
            return square_root * square_root * x;
        }
    }
}

class Encoder {
public:
    constexpr Encoder(std::uintmax_t base, std::uintmax_t min_length=0)
      : _base(base)
      , _min_length(min_length)
      {}
    std::uintmax_t encode(std::vector<bool> digits) {
        std::uintmax_t accumulator = _get_prefix(digits.size());
        std::size_t power = 1;
        for (auto dig = digits.rbegin(); dig != digits.rend(); ++dig) {
            accumulator += *dig * power;
            power *= _base;
        }
        return accumulator;
    }
    std::vector<bool> decode(std::uintmax_t code) {
        auto [length, prefix] = _get_length_and_prefix(code);
        std::uintmax_t combination = code - prefix;
        std::vector<bool> digits;
        for (std::size_t n = 0; n < length; n++) {
            std::uintmax_t q = combination / _base;
            std::uintmax_t r = combination % _base;
            combination = q;
            digits.insert(digits.begin(), r);
        }
        return digits;
    }
private:
    // Saxby's Polynomial is useful for iterative calculation of code lengths
    constexpr std::uintmax_t _saxbys_polynomial(std::uintmax_t n_1) const {
        return n_1 * _base + _saxbys_power;
    }
    // Saxby's improved Polynomial is useful for calculating prefixes in one go
    constexpr std::uintmax_t _saxbys_improved_polynomial(std::uintmax_t n) const {
        return (pow(_base, n) - _saxbys_power) / _saxbys_denominator;
    }
    constexpr std::uintmax_t _get_prefix(std::size_t length) const {
        assert(length >= _min_length); // TODO: improve this error handling!
        return _saxbys_improved_polynomial(length);
    }
    constexpr std::tuple<std::size_t, std::uintmax_t> _get_length_and_prefix(std::uintmax_t code) const {
        std::uintmax_t prefix = 0;
        std::uintmax_t space_size = _saxbys_power;
        std::size_t digits = _min_length;
        while (space_size <= code) {
            prefix = space_size;
            space_size = _saxbys_polynomial(space_size);
            digits++;
        }
        return {digits, prefix};
    }
    // can be removed later if unneeded
    constexpr std::size_t _get_length(std::uintmax_t code) const {
        std::size_t length;
        std::tie(length, std::ignore) = _get_length_and_prefix(code);
        return length;
    }
    std::uintmax_t _base;
    std::uintmax_t _min_length;
    // pre-calculated constants that are useful for Saxby's Polynomials:
    std::uintmax_t _saxbys_power = pow(_base, _min_length);
    std::uintmax_t _saxbys_denominator = _base - 1;
};

#endif // include guard

///////////////////////////////////////////////////////////////////////////////

#include <map>
#include <vector>

struct KeyComparator {
    bool operator()(const std::vector<bool>& lhs, const std::vector<bool>& rhs) const {
        auto encoder = Encoder(2, 0);
        return encoder.encode(lhs) < encoder.encode(rhs);
    }
};

#include <iostream>

template <class Iterable>
void print_bits(Iterable bits) {
    for (auto bit : bits) {
        std::cout << bit;
    }
}

#include <cmath>

std::vector<bool> serialise_for(uintmax_t symbol, std::size_t space_size) {
    std::size_t bits_needed = std::ceil(std::log2(space_size));
    std::vector<bool> output;
    // std::cout << symbol << " -> ";
    for (std::uintmax_t p = 1 << (bits_needed - 1); p > 1; p >>= 1) {
        if (symbol & p) {
            output.push_back(1);
            symbol -= p;
        } else {
            output.push_back(0);
        }
    }
    output.push_back(symbol);
    // print_bits(output);
    // std::cout << std::endl;
    return output;
}

template <class InputIterator, class OutputIterator>
OutputIterator lzw_bit_compress(InputIterator first, InputIterator last, OutputIterator result) {
    // std::map<std::vector<bool>, std::size_t, KeyComparator> string_table = {
    //     {{0}, 0}, {{1}, 1}
    // };
    std::map<std::vector<bool>, std::size_t> string_table = {
        {{0}, 0}, {{1}, 1}
    };
    std::vector<bool> p;
    for (; first != last; ++first) {
        bool c = *first;
        std::vector<bool> pc = p;
        pc.push_back(c);
        if (string_table.contains(pc)) {
            p = pc;
        } else {
            for (auto bit : serialise_for(string_table[p], string_table.size())) {
                *result = bit;
                ++result;
            }
            // if (string_table.size() < 256) {
                string_table[pc] = string_table.size();
            // }
            p = {c};
        }
    }
    for (auto bit : serialise_for(string_table[p], string_table.size())) {
        *result = bit;
        ++result;
    }
    // XXX: print decoder table just for info
    // for (auto kp : string_table) {
    //     print_bits(kp.first);
    //     std::cout << " -> " << kp.second << std::endl;
    // }
    std::cout << "Code table size: " << string_table.size() << " entries" << std::endl;
    return result;
}

///////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <random>


template <class Iterable>
void print(Iterable bits) {
    for (auto bit : bits) {
        std::cout << bit << " ";
    }
    std::cout << std::endl;
}

template <class InputIterator, class OutputIterator>
OutputIterator bytes_to_bits(InputIterator first, InputIterator last, OutputIterator result) {
    for (; first != last; ++first) {
        for (auto bit : serialise_for(*first, 256)) {
            *result = bit;
            ++result;
        }
    }
    return result;
}

#include <fstream>

int main(int argc, char* argv[]) {
    auto file = std::ifstream(argv[1], std::ifstream::binary);
    std::vector<bool> input;
    bytes_to_bits(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), std::back_inserter(input));
    file.close();
    // {
    //     std::random_device rd;
    //     std::mt19937 gen(rd());
    //     std::bernoulli_distribution dist;
    //     std::generate_n(std::back_inserter(input), 16777216, [&] () { return dist(gen); });
    // }
    // print_bits(input);
    // std::cout << std::endl;
    std::vector<uintmax_t> output;
    lzw_bit_compress(input.begin(), input.end(), std::back_inserter(output));
    // print_bits(output);
    std::cout << input.size() << " bits -> " << output.size() << " bits" << std::endl;
}
