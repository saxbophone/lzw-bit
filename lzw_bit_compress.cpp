#include <iostream>
#include <unordered_map>
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

#include <algorithm>
#include <memory>
#include <optional>

class CodeTable {
public:
    // special tag-like type, used only to request the "END" symbol
    struct End {} static constexpr END = {};
    // not the most efficient structure for searching, but it'll do for now
    // whilst we get the logic sorted out
    struct string_entry {
        std::vector<bool> string;
        std::optional<std::size_t> codeword;
    };
    // default constructor, auto-initialises a code table with all 1-bit strings
    // and a special "EOF" symbol
    CodeTable() : _entries{{{0}, 0}, {{1}, 1}} {}
    // use codetable += <bit string> to add a code to the table
    CodeTable& operator+=(const std::vector<bool>& string) {
        // XXX: not checking if code already is present, BE CAREFUL!
        string_entry entry = {string, size()};
        _entries.push_back(entry);
        return *this;
    }
    // find by string and return an iterator, which might be .end()
    std::vector<string_entry>::iterator find(const std::vector<bool>& string) {
        auto first = _entries.begin();
        for ( ; first != _entries.end(); ++first) {
            if (first->string == string) { break; }
        }
        return first;
    }
    // find by codeword and return an iterator, which might be .end()
    std::vector<string_entry>::iterator find(std::size_t codeword) {
        auto first = _entries.begin();
        for ( ; first != _entries.end(); ++first) {
            if (first->codeword == codeword) { break; }
        }
        return first;
    }
    // remove the code for the given string from the table
    // NOTE: the string remains present in the table but is now uncoded
    CodeTable& operator-=(const std::vector<bool>& string) {
        auto entry = find(string);
        if (entry != _entries.end()) {
            // uncode the entry
            entry->codeword = std::nullopt;
            // now advance through the rest of the entries after and reduce the codeword value for any that are uncoded
            for ( ; entry != _entries.end(); ++entry) {
                if (entry->codeword) { entry->codeword = entry->codeword.value() + 1; }
            }
        }
        return *this;
    }
    // check to see if a string is in the table
    bool contains(const std::vector<bool>& string) {
        return find(string) != _entries.end();
    }
    // check to see if a code is in the table
    bool contains(std::size_t codeword) {
        return find(codeword) != _entries.end();
    }
    // retrieve the codeword for the given bit string, or std::nullopt if the codeword is uncoded
    // (NOTE if it's not present at all, which is different to "uncoded", an error is raised)
    // NOTE: using optional here is just for debugging, it should never return nullopt if our theory is correct
    // TODO: remove it later and error if it can't be found
    std::optional<std::size_t> operator[](const std::vector<bool>& string) {
        return find(string)->codeword;
    }
    // retreive the codeword used for the special "End of Data" symbol
    std::size_t operator[](const End&) {
        return size() - 1;
    }
    // retrieve the bit-string encoded by the given codeword
    std::vector<bool> operator[](std::size_t codeword) {
        return find(codeword)->string;
    }
    // give codes back to any uncoded (dropped) strings from the table
    // NOTE: strings are not guaranteed to get back their original codewords
    void restore_dropped_codes() {
        // simplest thing to do is just re-fill the codewords with successive values
        for (std::size_t i = 0; i < _entries.size(); i++) {
            _entries[i].codeword = i;
        }
    }
    // returns the number of *coded* strings in the table. This can be less than
    // the total number of strings stored in the table, if for example any have
    // been uncoded.
    // Knowing the number of assigned codes is essential for serialising and
    // deserialising codewords in a space-efficient way.
    std::size_t size() const {
        // count only the non-uncoded entries and then add 1 (for the implicit "END" symbol)
        return std::count_if(
            _entries.begin(),
            _entries.end(),
            [](auto entry) { return entry.codeword.has_value(); }
        ) + 1;
    }
private:
    std::vector<string_entry> _entries;
};

template <class InputIterator, class OutputIterator>
OutputIterator lzw_bit_compress(InputIterator first, InputIterator last, OutputIterator result) {
    CodeTable string_table;
    std::vector<bool> p;
    for (; first != last; ++first) {
        bool c = *first;
        std::vector<bool> pc = p;
        pc.push_back(c);
        if (string_table.contains(pc)) {
            p = pc;
        } else {
            // print_bits(p);
            // std::cout << " -> ";
            for (auto bit : serialise_for(*string_table[p], string_table.size())) {
                // std::cout << bit;
                *result = bit;
                ++result;
            }
            // std::cout << std::endl;
            // NOTE: If you want to restrict the string table size, here's where you'd do it
            // FIME: Currently, there is no restriction, which can eat up all the
            // memory for large files. We should maybe consider changing this...
            // if (string_table.size() < 256) {
            string_table += pc;
            // }
            p = {c};
        }
    }
    // print_bits(p);
    // std::cout << " -> ";
    // write out last remaining symbol left on output
    for (auto bit : serialise_for(*string_table[p], string_table.size())) {
        // std::cout << bit;
        *result = bit;
        ++result;
    }
    // std::cout << std::endl;
    return result;
}

template <class InputIterator, class OutputIterator>
OutputIterator lzw_bit_decompress(InputIterator first, InputIterator last, OutputIterator result) {
    CodeTable string_table;
    // first bit is always encoded verbatim
    bool c = *first;
    ++first;
    *result = c;
    ++result;
    // std::cout << c << " -> " << c << std::endl;
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
        // print_bits(codeword_bits);
        // std::cout << " -> ";
        if (string_table.contains(codeword)) {
            // if codeword was found in the dictionary
            auto entry = string_table[codeword];
            // output it
            // print_bits(entry);
            // std::cout << std::endl;
            for (auto bit : entry) {
                *result = bit;
                ++result;
            }
            // add p + entry[0] to dictionary
            p.push_back(entry[0]);
            string_table += p;
            p = entry;
        } else {
            p.push_back(p[0]);
            // print_bits(p);
            // std::cout << std::endl;
            for (auto bit : p) {
                *result = bit;
                ++result;
            }
            string_table += p;
        }
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////

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
        std::cerr << "Usage: " << argv[0] << " [c|d] <input file> <output file>" << std::endl;
    }
    std::size_t input_size = input_file.tellg();
    std::size_t output_size = output_file.tellp();
    std::cout << input_size << " bytes -> " << output_size << " bytes (" << std::ceil((double)output_size / input_size * 100) << "%)" << std::endl;
    // files close automatically thanks to RAII
}
