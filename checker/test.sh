#!/bin/bash

cd ../src
make clean && make -j
cd ../checker
./run_all.sh
