#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <type_traits>

#include <cstddef>

template <typename CharInputIterator>
struct char_bit_input_iterator {
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = bool;
    using pointer = bool*;
    /*
     * from the docs for LegacyInputIterator:
     * The reference type for an input iterator that is not also a
     * LegacyForwardIterator does not have to be a reference type: dereferencing
     * an input iterator may return a proxy object or value_type itself by value
     * (as in the case of std::istreambuf_iterator). 
     */
    using reference = bool;
    // NB: used for a different purpose than std::istreambuf_iterator::char_type
    using char_type = std::make_unsigned<typename CharInputIterator::value_type>::type;

    // psuedo-default ctor instantiates the end-of-stream iterator
    constexpr char_bit_input_iterator() : _wrapped_iterator(_end_of_stream) {}
    // main ctor that actually takes a char stream iterator to wrap
    constexpr char_bit_input_iterator(CharInputIterator& chit) : _wrapped_iterator(chit), _current_char(*_wrapped_iterator) {}
private:
    // this is a bit of a fudge, but these "orphan" iterator nodes allow us to
    // follow the API that std::istreambuf_iterator uses for when a dangling
    // is left behind by it++
    constexpr char_bit_input_iterator(bool orphan) : _is_orphan(true), _orphan(orphan) {}
    // reads the current bit without advancing the iterator
    constexpr bool _peek() const {
        return (_current_char & (1u << (_char_offset - 1))) >> (_char_offset - 1);
    }
public:
    constexpr reference operator*() const {
        return _peek();
    }
    // we don't provide an object-access pointer because istreambuf_iterator no
    // longer does and it makes no sense to provide a pointer to a bit temporary
    constexpr char_bit_input_iterator& operator++() {
        _char_offset--;
        if (_char_offset == 0) {
            ++_wrapped_iterator;
            _char_offset = BITS_PER_CHAR;
            _current_char = *_wrapped_iterator;
        }
        return *this;
    }
    constexpr char_bit_input_iterator operator++(int) {
        char_bit_input_iterator dangling(_peek());
        operator++();
        return dangling;
    }
    // needed because auto-generated equality operator refuses to cmp references
    constexpr friend bool operator==(const char_bit_input_iterator& a, const char_bit_input_iterator& b) {
        return a._is_orphan == b._is_orphan and a._wrapped_iterator == b._wrapped_iterator and a._char_offset == b._char_offset;
    }

private:
    // orphan stuff only used for dangling iterators left behind after postfix increment
    bool _is_orphan = false;
    bool _orphan;
    CharInputIterator _end_of_stream;
    CharInputIterator& _wrapped_iterator;
    // we iterate bits big-endian by default
    // TODO: allow this to be modified optionally
    static constexpr std::size_t BITS_PER_CHAR = (std::size_t)std::numeric_limits<char_type>::digits;
    std::size_t _char_offset = BITS_PER_CHAR;
    char_type _current_char;
};

struct char_bit_output_iterator {
    // TODO: implement output iterator for converting bits->chars
    // analogous to std::ostreambuf_iterator
};

// int main(int, char* argv[]) {
//     std::ifstream program_source(argv[1], std::ifstream::binary);
//     auto file_reader = std::istreambuf_iterator<char>(program_source);
//     if (program_source) for (auto first = char_bit_input_iterator<std::istreambuf_iterator<char>>(file_reader); first != char_bit_input_iterator<std::istreambuf_iterator<char>>(); ++first) {
//         std::cout << *first;
//     }
//     std::cout << std::endl;
// }
