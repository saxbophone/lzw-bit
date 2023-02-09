    func(
        char_bit_input_iterator<std::istreambuf_iterator, char>(file_reader),
        char_bit_input_iterator<std::istreambuf_iterator, char>(),
        char_bit_output_iterator<std::ostreambuf_iterator, char>(file_writer)
    ); // any unwritten partial-byte bitstreams get written here as the output iterator goes out of scope