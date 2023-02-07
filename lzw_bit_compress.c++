template <class InputIterator, class OutputIterator>
OutputIterator lzw_bit_compress(InputIterator first, InputIterator last, OutputIterator result) {
    for (; first != last; ++first) {
        // dummy version just copies bits to verify iteration is working
        *result = *first;
        ++result;
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

template <class Iterable>
void print_bits(Iterable bits) {
    for (auto bit : bits) {
        std::cout << bit;
    }
    std::cout << std::endl;
}

int main() {
    std::vector<bool> input;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::bernoulli_distribution dist;
        std::generate_n(std::back_inserter(input), 128, [&] () { return dist(gen); });
    }
    print_bits(input);
    std::vector<bool> output;
    lzw_bit_compress(input.begin(), input.end(), std::back_inserter(output));
    print_bits(output);
}