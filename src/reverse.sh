#!/bin/bash

# Reverse a string from stdin
# Usage: echo "string" | ./reverse.sh

read -r string
echo "$string" | rev

