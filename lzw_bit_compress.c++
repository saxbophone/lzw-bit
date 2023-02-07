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
    std::bitset<8> output_buffer;
    std::size_t output_index = 0;
    std::map<std::vector<bool>, std::size_t> string_table = {
        {{0}, 0}, {{1}, 1}
    };
    std::vector<bool> p;
    // std::size_t out_bits = 0;
    for (; first != last; ++first) {
        for (bool c : serialise_for(*first, 256)) {
            std::vector<bool> pc = p;
            pc.push_back(c);
            if (string_table.contains(pc)) {
                p = pc;
            } else {
                for (auto bit : serialise_for(string_table[p], string_table.size())) {
                    output_buffer[output_index++] = bit;
                    if (output_index == 8) {
                        // flush byte out
                        *result = (char)(output_buffer.to_ulong() & 0xFF);
                        ++result;
                        output_index = 0;
                    }
                    // out_bits++;
                }
                // if (string_table.size() < 256) {
                    string_table[pc] = string_table.size();
                // }
                p = {c};
            }
        }
    }
    for (auto bit : serialise_for(string_table[p], string_table.size())) {
        output_buffer[output_index++] = bit;
        if (output_index == 8) {
            // flush byte out
            *result = (char)(output_buffer.to_ulong() & 0xFF);
            ++result;
            output_index = 0;
        }
        // out_bits++;
    }
    // XXX: print decoder table just for info
    // for (auto kp : string_table) {
    //     print_bits(kp.first);
    //     std::cout << " -> " << kp.second << std::endl;
    // }
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

int main(int argc, char* argv[]) {
    auto input_file = std::ifstream(argv[1], std::ifstream::binary);
    auto output_file = std::ofstream(argv[2], std::ofstream::binary);
    lzw_bit_compress(std::istreambuf_iterator<char>(input_file), std::istreambuf_iterator<char>(), std::ostreambuf_iterator<char>(output_file));
    std::size_t input_size = input_file.tellg();
    std::size_t output_size = output_file.tellp();
    std::cout << input_size << " bytes -> " << output_size << " bytes (" << std::ceil((double)output_size / input_size * 100) << "%)" << std::endl;
    // files close automatically thanks to RAII
}
