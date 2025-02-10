#!/bin/bash

FILE=intcalc

clang ${FILE}.c -o ${FILE} -O2 -mavx -std=c2x\
	-Iinclude -lm \
	-Wall -Wextra -Wno-attributes -Wno-unused-function -Wno-unused-variable \
	-Wno-unused-label -Wno-unused-parameter -Wno-unused-but-set-variable \
	$1 $2 $3 $4
