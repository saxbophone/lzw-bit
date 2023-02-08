#include <iostream>
#include <map>
#include <vector>

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

#include <bitset>

// NB: this function would be much more straightforward if we had a special
// iterator which could map two and from bit and byte sequences:
// https://github.com/saxbophone/cpp_utils/issues/2
template <class InputIterator, class OutputIterator>
OutputIterator lzw_bit_compress(InputIterator first, InputIterator last, OutputIterator result) {
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
            }
            // if (string_table.size() < 256) {
                string_table[pc] = string_table.size();
                // print_bits(pc);
                // std::cout << " -> " << string_table[pc] << std::endl;
            // }
            p = {c};
        }
    }
    for (auto bit : serialise_for(string_table[p], string_table.size())) {
        *result = bit;
    }
    // XXX: print decoder table just for info
    for (auto kp : string_table) {
        print_bits(kp.first);
        std::cout << " -> ";
        print_bits(serialise_for(kp.second, string_table.size()));
        std::cout << std::endl;
    }
    std::cout << "Code table size: " << string_table.size() << " entries" << std::endl;
    // std::cout << "Output size: " << out_bits / 8 << std::endl;
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

#include <fstream>

#include "bit_iterator.hpp"

int main(int argc, char* argv[]) {
    auto input_file = std::ifstream(argv[1], std::ifstream::binary);
    auto output_file = std::ofstream(argv[2], std::ofstream::binary);
    auto file_reader = std::istreambuf_iterator<char>(input_file);
    auto file_writer = std::ostreambuf_iterator<char>(output_file);
    lzw_bit_compress(
        char_bit_input_iterator<std::istreambuf_iterator<char>>(file_reader),
        char_bit_input_iterator<std::istreambuf_iterator<char>>(),
        char_bit_output_iterator<std::ostreambuf_iterator<char>>(file_writer)
    );
    std::size_t input_size = input_file.tellg();
    std::size_t output_size = output_file.tellp();
    std::cout << input_size << " bytes -> " << output_size << " bytes (" << std::ceil((double)output_size / input_size * 100) << "%)" << std::endl;
    // files close automatically thanks to RAII
}