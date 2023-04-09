# lzw-bit
Bit-by-bit LZW compressor with redundant-code-elimination

## Rationale
To explore the effectiveness of LZW compression when applied to 1-bit symbols rather than the more typical 1-byte symbols that it is normally used with.

Additionally, to explore the possibility of removing further redundancies from the LZW algorithm by removing codes from the codetable once additional codes have been added such that the older code will never again be used.

## Reading and writing files bit-by-bit
An iterator wrapper was produced, which is intended to wrap the file stream iterators (such as `std::istreambuf_iterator`) and which allows iteration bit-by-bit, translating this back to calls to the wrapped iterator to iterate byte-by-byte.

## Redundant Code Eliminatiom
A small redundancy in the original LZW algorithm was discovered —namely, that as the LZW code-matching algorithm is greedy, once a prefix code $p$ has descendant codes $p,0$ and $p,1$ in the codetable, then $p$ will never again be matched alone (except until possibly the end of the file, a case we handle specially and shall describe later). Therefore, to increase compression efficiency, we can remove (we use the term "shadow") the code $p$ from the table, to reduce the size of the encoding table and/or allow new codes to be added without increasing the table size. This tends to lend to a slightly more efficient compression of up to a few percent.

### Example of redundant-code-elimination

| Code | Bitstring                                                                               |
|------|-----------------------------------------------------------------------------------------|
| 0    | 0                                                                                       |
| 1    | 1                                                                                       |
| 2    | 01                                                                                      |
| 3    | 10                                                                                      |
| 4    | **11** <-- at this point we can remove code `1: 1` because both `10` and `11` shadow it |

_New table after RCE:_

| Code       | Bitstring                                                                                     |
|------------|-----------------------------------------------------------------------------------------------|
| 0          | 0                                                                                             |
| _shadowed_ | 1 <-- Note that this code is kept in the table structure but it is no longer part of the code |
| 1          | 01                                                                                            |
| 2          | 10                                                                                            |
| 3          | 11                                                                                            |

To handle the special case at the end of the file, where we might indeed need code $p$ or any other "shadowed" code from the codetable once more, we use the common _meta-symbol_ for **End of File** as a cue for when to restore all the previously "shadowed" codes from the code table, and then proceed to encode the remaining bits with a symbol from said table.

## Results
First thing to note, is this algorithm is **_SLOW AS HELL!_** Really, it's not very practical for real-world usage. The factors contributing to this are firstly, the fact that we compress a bit at a time, and secondly, the processing overhead introduced by RCE. Before RCE was introduced, compression was still slow but not as slow as it currently is. It may be possible to make this slightly more efficient by only running RCE every time the codetable size would increase to the next whole number of bits, rather than every time a new code is inserted. This could be efficiently done by remembering the index of the first new code that hasn't been checked for RCE, then checking this code and all those after it for RCE when the time comes. Modifying the technique to work in this way is not expected to increase compressed file size at all (it should remain exactly the same), however it will change the exact codes that are output. The performance increase however is only expected to be marginal, if any at all.
