import gc
import random
import time

# Adapted from https://gist.github.com/stedolan/4369a0fa27820e27d6e56bee5e412896
# See https://github.com/ocaml/ocaml/pull/10195
#
# In Python (free threading), this uses closer to 1.9 GiB of memory isntead of 800 MiB

# perf stat -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses ./python gc_big_800m.py

class Record:
    def __init__(self, a, b, c, d, e, f):
        self.a = a
        self.b = b
        self.c = c
        self.d = d
        self.e = e
        self.f = f

def make_arr():
    # translated via GPT-3
    # Create an array of size 2^20 with elements as Some(i) equivalent in Python
    nums = [i for i in range(1 << 20)]
    # Create an array of size 1024 with each element initialized to an empty list
    a = [[] for _ in range(1024)]
    # Iterate from 1 to 10,000,000
    for i in range(1, 10_000_001):
        # Calculate n using bitwise operations
        n = ((i * 34872841) >> 13) & 0x3ff
        # Create a dictionary to mimic the record structure in OCaml
        record = [
            nums[i & 0xfffff],
            0.0,
            0.0,
            0.0,
            0.0,
            42.0
        ]
        # Prepend the record to the list at index n
        a[n].append(record)
    
    return a


# class MyClass:
#     def __init__(self):
#         self.a = None
#         self.b = None
#         self.c = None
#         self.d = None
#         self.e = None
#         self.f = None
#         self.g = None

def main():
    arr = make_arr()
    
    for _ in range(10000000):
        start = time.perf_counter_ns()
        gc.collect()
        end = time.perf_counter_ns()
        delta = (end - start) / 1e6
        print(f"gc.collect() took {delta:3.1f} ms")


if __name__ == '__main__':
    main()
