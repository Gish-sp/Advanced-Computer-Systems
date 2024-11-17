# Advanced Computer Systems

## Project 1:
### Cache and Memory Performance Profiling:

The objective of this project is to gain deeper understanding of cache and memory hierarchy in modern computers. 

## Project 2: 
### Dense/Sparse Matrix-Matrix Multiplication
The objective of this design project is to implement a C/C++ module that carries out high-speed dense/sparse
matrix-matrix multiplication by explicitly utilizing (i) multiple threads, (ii) x86 SIMD instructions, and/or (iii)
techniques to minimize cache miss rate via restructuring data access patterns or data compression (as discussed in
class). Matrix-matrix multiplication is one of the most important data processing kernels in numerous real-life
applications, e.g., machine learning, computer vision, signal processing, and scientific computing.

## Project 3:
### SSD Performance Profiling
This project helps you to gain first-hands experience on profiling the performance of modern SSDs (assuming the
SSD in your computer is modern enough). The task is simple: Use the Flexible IO tester (FIO), which is available at
https://github.com/axboe/fio and may already be included in the OS on your machine, to profile the performance
of your SSD. Its man page is https://linux.die.net/man/1/fio. FIO is a storage device testing tool widely used in the
industry. Like Project 1, you should design a set of experiments to measure the SSD performance (latency and
throughput) under different combinations of the following parameters: (1) data access size (e.g.,
4KB/16KB/32KB/128KB), (2) read vs. write intensity ratio (e.g., read-only, write-only, 50%:50% and 70%:30% read
vs. write), and (3) I/O queue depth (e.g., 0~1024). Note that throughput is typically represented in terms of IOPS (IO
per second) for small access size (e.g., 64KB and below), and represented in terms of MB/s for large access size (e.g.,
128KB and above).

## Project 4:
### Implementation of Dictionary Codec
The objective of this project is to implement a dictionary codec. As discussed in class, dictionary encoding is being
widely used in real-world data analytics systems to compress data with relatively low cardinality and speed up
search/scan operations. In essence, a dictionary encoder scans the to-be-compressed data to build the dictionary
that consists of all the unique data items and replaces each data item with its dictionary ID. To accelerate dictionary
look-up, one may use certain indexing data structure such as hash-table or B-tree to better manage the dictionary.
In addition to reducing the data footprint, dictionary encoding makes it possible to apply SIMD instructions to
significantly speed up the search/scan operations.
