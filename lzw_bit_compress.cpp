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
    for (std::uintmax_t p = 1 << (bits_needed - 1); p > 1; p >>= 1) {
        if (symbol & p) {
            output.push_back(1);
            symbol -= p;
        } else {
            output.push_back(0);
        }
    }
    output.push_back(symbol);
    return output;
}

uintmax_t deserialise(std::vector<bool> bits) {
    uintmax_t result = 0;
    for (auto bit : bits) {
        result <<= 1;
        result |= bit;
    }
    return result;
}

#include <bitset>

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
            print_bits(p);
            std::cout << " -> ";
            for (auto bit : serialise_for(string_table[p], string_table.size())) {
                std::cout << bit;
                *result = bit;
                ++result;
            }
            std::cout << std::endl;
            // NOTE: If you want to restrict the string table size, here's where you'd do it
            // FIME: Currently, there is no restriction, which can eat up all the
            // memory for large files. We should maybe consider changing this...
            // if (string_table.size() < 256) {
            string_table[pc] = string_table.size();
            // }
            p = {c};
        }
    }
    print_bits(p);
    std::cout << " -> ";
    // write out last remaining symbol left on output
    for (auto bit : serialise_for(string_table[p], string_table.size())) {
        std::cout << bit;
        *result = bit;
        ++result;
    }
    std::cout << std::endl;
    return result;
}

template <class InputIterator, class OutputIterator>
OutputIterator lzw_bit_decompress(InputIterator first, InputIterator last, OutputIterator result) {
    std::map<std::uintmax_t, std::vector<bool>> string_table = {
        {0, {0}}, {1, {1}}
    };
    // first bit is always encoded verbatim
    bool c = *first;
    ++first;
    *result = c;
    ++result;
    std::vector<bool> p = {c};
    while (first != last) {
        // need to know how many bits currently needed to store codes in dictionary
        std::size_t bits_needed = std::ceil(std::log2(string_table.size() + 1));
        std::vector<bool> codeword_bits(bits_needed);
        // attempt to read this many bits into vector, bailing if we exhaust the source
        for (std::size_t i = 0; i < codeword_bits.size(); i++) {
            if (first == last) {
                // this wasn't a codeword, this was trailing padding data at eof
                return result; // TODO: do we need to emit anything else here?
            }
            codeword_bits[i] = *first;
            ++first;
        }
        // now we have a codeword as a sequence of bits, convert to codeword and look up
        std::uintmax_t codeword = deserialise(codeword_bits);
        if (string_table.contains(codeword)) {
            // if codeword was found in the dictionary
            auto entry = string_table[codeword];
            // output it
            print_bits(codeword_bits);
            std::cout << " -> ";
            print_bits(entry);
            std::cout << std::endl;
            for (auto bit : entry) {
                *result = bit;
                ++result;
            }
            // add p + entry[0] to dictionary
            p.push_back(entry[0]);
            string_table[string_table.size()] = p;
            p = entry;
        } else {
            p.push_back(p[0]);
            for (auto bit : p) {
                *result = bit;
                ++result;
            }
            string_table[string_table.size()] = p;
        }
    }
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

int main(int, char* argv[]) {
    char mode = argv[1][0];
    auto input_file = std::ifstream(argv[2], std::ifstream::binary);
    auto output_file = std::ofstream(argv[3], std::ofstream::binary);
    auto file_reader = std::istreambuf_iterator<char>(input_file);
    auto file_writer = std::ostreambuf_iterator<char>(output_file);
    if (mode == 'c') {
        lzw_bit_compress(
            char_bit_input_iterator<std::istreambuf_iterator, char>(file_reader),
            char_bit_input_iterator<std::istreambuf_iterator, char>(),
            char_bit_output_iterator<std::ostreambuf_iterator, char>(file_writer)
        ); // any unwritten partial-byte bitstreams get written here as the output iterator goes out of scope
    } else if (mode == 'd') {
        lzw_bit_decompress(
            char_bit_input_iterator<std::istreambuf_iterator, char>(file_reader),
            char_bit_input_iterator<std::istreambuf_iterator, char>(),
            char_bit_output_iterator<std::ostreambuf_iterator, char>(file_writer)
        ); // any unwritten partial-byte bitstreams get written here as the output iterator goes out of scope
    } else {
        std::cerr << "Usage: ./program [c|d] <input file> <output file>" << std::endl;
    }
    std::size_t input_size = input_file.tellg();
    std::size_t output_size = output_file.tellp();
    std::cout << input_size << " bytes -> " << output_size << " bytes (" << std::ceil((double)output_size / input_size * 100) << "%)" << std::endl;
    // files close automatically thanks to RAII
}
