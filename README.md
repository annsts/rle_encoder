# RLE (Run-Length Encoding) Encoder

## Overview

This project implements a run-length encoding (RLE) encoder in C, capable of processing files sequentially or in parallel using multiple threads. The encoded output is written to `stdout`.

## Files

- `encoder.c`: Main program implementing the encoding logic and threading.
- `utils.c`: Contains helper functions for file processing and encoding.
- `utils.h`: Header file for utility functions and data structures.
- `Makefile`: Build script for compiling the project.

## Compilation

To compile the project, run:
```sh
make
```

## Usage

After compiling, you can run the encoder as follows:
```sh
./encoder [options] <file1> <file2> ...
```

### Options

- `-j <num_threads>`: Specify the number of threads to use for parallel encoding. Default is 1 (sequential).

### Example

Encode a text file sequentially and redirect the output to a file:
```sh
./encoder input.in > output.out
```

Encode a binary file using 4 threads and redirect the output to a file:
```sh
./encoder -j 4 input.in > output.out
```

## Platform

This project was developed and tested on an x86_64 Rocky Linux 8 container with gcc 12.1.1.