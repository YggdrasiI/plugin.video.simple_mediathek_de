#!/bin/bash 

valgrind --tool=massif --pages-as-heap=yes --massif-out-file=massif.out $@ ;

M=$(grep mem_heap_B massif.out | sed -e 's/mem_heap_B=\(.*\)/\1/' | sort -g | tail -n 1)

# LC_NUMERIC=en_US printf "%i\n" $M # reqiuire locale..
echo "$M" >&2
echo -n "mem=" >&2
echo "000000000${M}" | sed "s/^.*\(...\)\(...\)\(...\)$/\1\'\2\'\3/g" >&2

