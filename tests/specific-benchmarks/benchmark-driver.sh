#! /bin/sh
./insert_throughput.out --table-capacity 1
./read_throughput.out
./read_insert_throughput.out --table-capacity 1
