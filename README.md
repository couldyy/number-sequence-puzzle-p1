# Puzzle solver

Bruteforce solution, with some optimisations on memory allocation and child finding.

After all numbers are parsed, simple hash table (name `bucket` in code) is created, where hash keys are number start keys itself (since keys are already unique, there is no point in additinoal hashing).
Each node than fills its `nodes_out` and `nodes_in` fileds with corresponging pointer to numbers, that match keys.

For allocation arena allocator is used, since it is faster and there is no need to free memory, because program is short-lived.

## Build & run

```bash
make
./solver
```
