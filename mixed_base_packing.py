class Max:
    @staticmethod
    def determine_packing(bases):
        MAX = 2 ** 64
        current = 1
        chunk = []
        for base in bases:
            if current * base < MAX:
                current *= base
                chunk.append(base)
            else:
                yield tuple(chunk)
                chunk = [base]
                current = base
        if len(chunk) != 0:
            yield tuple(chunk)

from fractions import Fraction

class Dense:
    @staticmethod
    def determine_packing(b):
        bases = list(b)
        MAX = 2 ** 64
        while len(bases) > 0:
            current = 1
            best = (Fraction(0), [])
            chunk = []
            for base in bases:
                if current * base < MAX:
                    current *= base
                    chunk.append(base)
                    density = Fraction(current, 2 ** current.bit_length())
                    if density > best[0]:
                        best = (density, list(chunk))
                else:
                    break
            yield tuple(best[1])
            bases = [bases[i] for i in range(len(bases)) if i > len(best[1]) - 1 or bases[i] != best[1][i]]

from math import prod

def bitlength(iterable):
    return sum(prod(it).bit_length() for it in iterable)


for m, d in zip_longest(Max.determine_packing(range(3, 101)), Dense.determine_packing(range(33, 101)), fillvalue=()):
    print(m, d)