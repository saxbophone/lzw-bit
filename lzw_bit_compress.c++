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

template <class InputIterator, class OutputIterator>
OutputIterator lzw_bit_compress(InputIterator first, InputIterator last, OutputIterator result) {
    std::map<std::vector<bool>, std::size_t> string_table = {
        {{0}, 0}, {{1}, 1}
    };
    std::vector<bool> p;
    for (; first != last; ++first) {
        for (bool c : serialise_for(*first, 256)) {
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

#include <fstream>

int main(int argc, char* argv[]) {
    auto file = std::ifstream(argv[1], std::ifstream::binary);
    std::vector<uintmax_t> output;
    lzw_bit_compress(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), std::back_inserter(output));
    std::size_t input_size = file.tellg();
    std::size_t output_size = std::ceil((double)output.size() / 8);
    std::cout << input_size << " bytes -> " << output_size << " bytes (" << std::ceil((double)output_size / input_size * 100) << "%)" << std::endl;
    file.close();
}
