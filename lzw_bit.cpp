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
    // std::cout << "serialise_for(" << symbol << ", " << space_size << ")" << std::endl;
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
#include <deque>
#include <memory>
#include <optional>

class CodeTable {
public:
    // B+ tree we use for storing the code table nodes --this is most useful for converting strings to codewords
    struct Node {
        Node* parent; // non-owning ref to parent, only tree parent has none
        std::shared_ptr<Node> children[2]; // owning refs to children for 0 and 1 paths
        std::weak_ptr<Node> next; // non-owning ref to next node, if any --regardless of whether next node is coded or not
        bool bit; // the bit in the string at the position represented by this node
        std::optional<std::size_t> codeword; // only allowed if not both children exist, else forbidden
        std::size_t length; // how many bits long is the bitstring whose end is marked by this node

        // ctor for use in building the special non-bit node that represents the trunk of the tree
        Node()
          : children{std::make_shared<Node>(this, 0, 0), std::make_shared<Node>(this, 1, 1)}
          , length(0) {
            children[0]->next = children[1];
        }
        // ctor for use when initialising the code table with first two entries or adding new entries
        Node(Node* parent, bool bit, std::size_t codeword, std::size_t length=1)
          : parent(parent)
          , bit(bit)
          , codeword(codeword)
          , length(length)
          {}
        // ease of access of the node for the next bit
        std::shared_ptr<Node> operator[](bool bit) {
            return children[bit];
        }
        // get the bitstring for this node
        std::vector<bool> bitstring() const {
            std::vector<bool> bits(length);
            const Node* cursor = this;
            // WARN: this loop will segfault if length is not correct!
            for (auto it = bits.rbegin(); it != bits.rend(); ++it) {
                *it = cursor->bit;
                cursor = cursor->parent;
            }
            return bits;
        }
    };
    // type declaration for the "other side" of the lookup --converting codewords to strings
    using LookupTable = std::deque<std::shared_ptr<Node>>;
    // default constructor, auto-initialises a code table with all 1-bit strings
    // and a special "EOF" symbol
    CodeTable() : _index{_tree->children[0], _tree->children[1]} {}
    // use codetable += <bit string> to add a code to the table
    CodeTable& operator+=(const std::vector<bool>& string) {
        // XXX: not checking if code already is present, BE CAREFUL!
        auto prefix = string;
        prefix.pop_back();
        if (auto previous = find(prefix)) {
            bool bit = string.back();
            // WARN: this *WILL* overwrite existing nodes if not used correctly
            auto new_node = std::make_shared<Node>(previous.get(), bit, _index.size(), string.size());
            previous->children[bit] = new_node;
            // link previous last Node to this new one
            _index.back()->next = new_node;
            _index.push_back(new_node);
            // XXX: Optimisation, identify any "shadowed" redundant codes from table
            if (previous->codeword.has_value() and previous->children[!bit]) {
                _redundant_codes.push_back(previous->codeword.value());
            }
        } else {
            std::cerr << "FATAL ERROR --tried to add new code not prefixed by anything" << std::endl;
        }
        return *this;
    }
    // find by string, may return empty pointer
    std::shared_ptr<Node> find(const std::vector<bool>& string) {
        std::shared_ptr<Node> cursor = _tree;
        for (auto bit : string) {
            cursor = cursor->children[bit];
            if (not cursor) { break; }
        }
        return cursor;
    }
    // find by codeword, may return empty pointer
    std::shared_ptr<Node> find(std::size_t codeword) {
        if (codeword > _index.size() - 1) { return {}; }
        return _index[codeword];
    }
    // remove the code for the given string from the table
    // NOTE: the string remains present in the table but is now uncoded
    CodeTable& operator-=(std::size_t codeword) {
        auto entry = _index[codeword];
        // uncode the entry
        _index.erase(_index.begin() + codeword);
        entry->codeword = std::nullopt;
        // now advance through the rest of the entries after and reduce the codeword value for any that are uncoded
        for (auto next = entry->next.lock(); next; next = next->next.lock()) {
            if (next->codeword.has_value()) { next->codeword = next->codeword.value() - 1; }
        }
        return *this;
    }
    // check to see if a string is in the table
    bool contains(const std::vector<bool>& string) {
        return (bool)find(string);
    }
    // check to see if a code is in the table
    bool contains(std::size_t codeword) {
        return codeword < _index.size();
    }
    // retrieve the codeword for the given bit string, or std::nullopt if the codeword is uncoded
    // (NOTE if it's not present at all, which is different to "uncoded", an error is raised)
    // NOTE: using optional here is just for debugging, it should never return nullopt if our theory is correct
    // TODO: remove it later and error if it can't be found
    std::optional<std::size_t> operator[](const std::vector<bool>& string) {
        return find(string)->codeword;
    }
    // retrieve the bit-string encoded by the given codeword
    std::vector<bool> operator[](std::size_t codeword) {
        return find(codeword)->bitstring();
    }
    // uncodes the least recently identified redundant code
    void drop_oldest_redundant_code() {
        if (not _redundant_codes.empty()) {
            *this -= _redundant_codes.front();
            _redundant_codes.pop_front();
        }
    }
    // give codes back to any uncoded (dropped) strings from the table
    // NOTE: strings are not guaranteed to get back their original codewords
    void restore_dropped_codes() {
        // simplest thing to do is just re-fill the codewords with successive values
        std::size_t i = 0;
        _index.clear();
        for (auto cursor = _tree->children[0]; cursor; cursor = cursor->next.lock()) {
            cursor->codeword = i;
            _index.push_back(cursor);
            ++i;
        }
    }
    // returns the number of *coded* strings in the table. This can be less than
    // the total number of strings stored in the table, if for example any have
    // been uncoded.
    // Knowing the number of assigned codes is essential for serialising and
    // deserialising codewords in a space-efficient way.
    std::size_t size() const {
        return _index.size(); // index only stores entries for coded strings
    }
    void print() const {
        std::cout << "==========================================" << std::endl;
        for (auto entry = _tree->children[0]; entry; entry = entry->next.lock()) {
            if (entry->codeword.has_value()) {
                std::cout << entry->codeword.value();
            }
            std::cout << "\t";
            print_bits(entry->bitstring());
            std::cout << std::endl;
        }
    }
private:
    // useful for converting strings to codewords, and stores the actual code table
    std::shared_ptr<Node> _tree = std::make_shared<Node>();
    // used only for converting codewords to strings, and is non-owning, relying upon the tree for storage
    LookupTable _index;
    // sequence of codes for future removal
    // TODO: consider converting to optional of just one element, don't think we'll ever have more than one at a time?
    std::deque<std::size_t> _redundant_codes;
};

template <class InputIterator, class OutputIterator>
OutputIterator lzw_bit_compress(InputIterator first, InputIterator last, OutputIterator result) {
    CodeTable string_table;
    std::vector<bool> p;
    for (; first != last; ++first) {
        // string_table.print();
        bool c = *first;
        std::vector<bool> pc = p;
        pc.push_back(c);
        if (string_table.contains(pc)) {
            p = pc;
        } else {
            // print_bits(p);
            // std::cout << " -> ";
            // NOTE: +1 is to account for the special "END" symbol, not in table
            for (auto bit : serialise_for(*string_table[p], string_table.size() + 1)) {
                // std::cout << bit;
                *result = bit;
                ++result;
            }
            // std::cout << std::endl;
            string_table.drop_oldest_redundant_code();
            // NOTE: If you want to restrict the string table size, here's where you'd do it
            // FIME: Currently, there is no restriction, which can eat up all the
            // memory for large files. We should maybe consider changing this...
            // if (string_table.size() < 256) {
            string_table += pc;
            // }
            p = {c};
        }
        // string_table.print();
    }
    // print_bits(p);
    // std::cout << " -> ";
    // send out the "END" code
    for (auto bit : serialise_for(string_table.size(), string_table.size() + 1)) {
        // std::cout << bit;
        *result = bit;
        ++result;
    }
    // std::cout << " ";
    // restore all previously-dropped symbol codes
    string_table.restore_dropped_codes();
    // write out last remaining symbol left on output
    for (auto bit : serialise_for(*string_table[p], string_table.size())) {
        // std::cout << bit;
        *result = bit;
        ++result;
    }
    // std::cout << std::endl;
    return result;
}

// helper function for lzw_bit_decompress()
// reads one symbol from the input stream
// this is useful because symbols are variable-width --it depends on current table size
template <class InputIterator>
std::vector<bool> read_next_symbol(InputIterator& first, InputIterator& last, std::size_t code_table_size) {
    // need to know how many bits currently needed to store codes in dictionary
    std::size_t bits_needed = std::ceil(std::log2(code_table_size));
    std::vector<bool> codeword_bits(bits_needed);
    // attempt to read this many bits into vector, bailing if we exhaust the source
    for (std::size_t i = 0; i < codeword_bits.size(); i++) {
        if (first == last) {
            // if we didn't get enough bits, this is padding data and must be ignored
            return {};
        }
        codeword_bits[i] = *first;
        ++first;
    }
    // print_bits(codeword_bits);
    // now we have a codeword as a sequence of bits, convert to codeword and look up
    return codeword_bits;
}

template <class OutputIterator>
void output_string(std::vector<bool> string, OutputIterator& result) {
    for (auto bit : string) {
        // std::cout << bit;
        *result = bit;
        ++result;
    }
}

template <class InputIterator, class OutputIterator>
OutputIterator lzw_bit_decompress(InputIterator first, InputIterator last, OutputIterator result) {
    CodeTable string_table;
    // NOTE: +1 is to account for the special "END" symbol, not in table
    auto w = read_next_symbol(first, last, string_table.size() + 1);
    if (w.empty()) { return result; } // no more symbols left to decode
    auto k = deserialise(w);
    std::vector<bool> entry;
    // std::cout << " -> ";
    w = string_table[k];
    output_string(w, result);
    // std::cout << std::endl;
    while (first != last) {
        // string_table.print();
        // +1 to table size is because every new symbol read adds another to the table
        // additional +1 is to account for the special "END" symbol, which is not in table
        auto next_symbol = read_next_symbol(first, last, string_table.size() + 2);
        if (next_symbol.empty()) { break; } // no more symbols left to decode
        k = deserialise(next_symbol);
        if (k == string_table.size() + 1) { // "END" symbol encountered
            // std::cout << " ";
            string_table.restore_dropped_codes();
            continue;
        }
        // std::cout << " -> ";
        if (auto found = string_table.find(k)) {
            entry = found->bitstring();
            output_string(entry, result);
            auto extra_code = w;
            extra_code.push_back(entry[0]);
            string_table += extra_code;
            string_table.drop_oldest_redundant_code();
            w = entry;
        } else {
            entry = w;
            entry.push_back(w[0]);
            output_string(entry, result);
            string_table += entry;
            string_table.drop_oldest_redundant_code();
            w = entry;
        }
        // std::cout << std::endl;
        // string_table.print();
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
        // std::cout << std::endl << std::endl << std::endl;
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
    // std::cout << std::endl;
    // files close automatically thanks to RAII
}
