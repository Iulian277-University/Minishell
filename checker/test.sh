#!/bin/bash

cd ../src
make clean && make -j
cp mini-shell ../checker
cd ../checker
./run_all.sh
