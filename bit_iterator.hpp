#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <type_traits>

#include <cstddef>

template <template<class> class CharInputIterator, class CharT>
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
    using char_type = std::make_unsigned<CharT>::type;
    using wrapped_type = CharInputIterator<CharT>;

    // psuedo-default ctor instantiates the end-of-stream iterator
    constexpr char_bit_input_iterator() : _wrapped_iterator(_end_of_stream) {}
    // main ctor that actually takes a char stream iterator to wrap
    constexpr char_bit_input_iterator(wrapped_type& chit) : _wrapped_iterator(chit), _current_char(*_wrapped_iterator) {}
private:
    // this is a bit of a fudge, but these "orphan" iterator nodes allow us to
    // follow the API that std::istreambuf_iterator uses for when a dangling
    // is left behind by it++
    constexpr char_bit_input_iterator(bool orphan) : _is_orphan(true), _orphan(orphan), _wrapped_iterator(_end_of_stream) {}
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
        if (--_char_offset == 0) {
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
    wrapped_type _end_of_stream;
    wrapped_type& _wrapped_iterator;
    // we iterate bits big-endian by default
    // TODO: allow this to be modified optionally
    static constexpr std::size_t BITS_PER_CHAR = (std::size_t)std::numeric_limits<char_type>::digits;
    std::size_t _char_offset = BITS_PER_CHAR;
    char_type _current_char;
};

template <template<class> class CharOutputIterator, class CharT>
struct char_bit_output_iterator {
    // output iterator for converting bits->chars
    // analogous to std::ostreambuf_iterator
    using iterator_category = std::output_iterator_tag; // a bit of a lie, this iterator is not copyable!
    using difference_type = std::ptrdiff_t;
    using value_type = void;
    using pointer = void;
    using reference = void;
    // NB: used for a different purpose than std::istreambuf_iterator::char_type
    using char_type = std::make_unsigned<CharT>::type;
    using wrapped_type = CharOutputIterator<CharT>;

    constexpr char_bit_output_iterator(wrapped_type& chout) : _wrapped_iterator(&chout) {}
    constexpr ~char_bit_output_iterator() {
        // use RAII-like mechanisms to ensure any lingering bits of an incomplete
        // byte are not lost, but are sent out to the onward char-stream we wrap
        flush();
    }
    // while technically not needed as we've defined custom move constructors,
    // which cause the implicitly-defined copy constructor to be deleted, we want
    // to make it absolutely clear that this iterator type is non-copyable
    constexpr char_bit_output_iterator(const char_bit_output_iterator&) = delete;
    // exactly the same for copy-assignment
    constexpr char_bit_output_iterator& operator=(const char_bit_output_iterator&) = delete;
    // we define a custom move constructor to ensure that copies don't double-flush the stream when moved from!
    constexpr char_bit_output_iterator(char_bit_output_iterator&& other) {
        *this = std::move(other);
    }
    // custom move-assignment operator
    // this allows you to at least do x = std::move(y), as this iterator is not copyable!
    constexpr char_bit_output_iterator& operator=(char_bit_output_iterator&& other) {
        if (this != &other) {
            // nothing to free, _wrapped_iterator is a non-owning pointer
            // all resources transfer from other to this:
            _wrapped_iterator = other._wrapped_iterator;
            _char_offset = other._char_offset;
            _current_char = other._current_char;
            // we then clear other's --crucial to prevent dtor flushing twice!
            other._wrapped_iterator = nullptr;
            other._char_offset = BITS_PER_CHAR;
            other._current_char = 0;
        }
        return *this;
    }
    constexpr char_bit_output_iterator& operator=(bool bit) {
        // write the bit
        _current_char |= (bit << (--_char_offset));
        // flush if a full char is ready to be written
        if(_char_offset == 0) {
            flush();
        }
        return *this;
    }
    // just like std::ostreambuf_iterator, provided only to satisfty LegacyOutputIterator requirements
    constexpr char_bit_output_iterator& operator*() {
        return *this;
    }
    // and the same with both prefix and postfix operator++ overloads:
    constexpr char_bit_output_iterator& operator++() {
        return *this;
    }
    constexpr char_bit_output_iterator& operator++(int) {
        return *this;
    }
    // used to ensure output is flushed, even if not enough bits for a whole char are written
    // the missing bits are sent as 0s
    constexpr void flush() {
        // only flush if there is any pending data to send
        if (_char_offset != BITS_PER_CHAR) {
            // _wrapped_iterator might be null if this object was moved from
            // although attempting to write bits out to a moved-from iterator
            // is poor practice, such code is not undefined behaviour so we need
            // to account for it to at least prevent null-deref, so we account
            // for it by making flush() on a moved-from iterator a no-op.
            if (_wrapped_iterator != nullptr) {
                *_wrapped_iterator = _current_char;
            }
            // reset internal index
            _char_offset = BITS_PER_CHAR;
            // reset char temporary
            _current_char = 0;
        }
    }

private:
    wrapped_type* _wrapped_iterator = nullptr;
    // we iterate bits big-endian by default
    // TODO: allow this to be modified optionally
    static constexpr std::size_t BITS_PER_CHAR = (std::size_t)std::numeric_limits<char_type>::digits;
    std::size_t _char_offset = BITS_PER_CHAR;
    char_type _current_char = 0;
};

// int main(int, char* argv[]) {
//     std::ifstream program_source(argv[1], std::ifstream::binary);
//     auto file_reader = std::istreambuf_iterator<char>(program_source);
//     if (program_source) for (auto first = char_bit_input_iterator<std::istreambuf_iterator<char>>(file_reader); first != char_bit_input_iterator<std::istreambuf_iterator<char>>(); ++first) {
//         std::cout << *first;
//     }
//     std::cout << std::endl;
// }
