#!/bin/bash

FILE=intcalc

gcc ${FILE}.c -o ${FILE} -ggdb \
	-Iinclude -lm \
	-Wall -Wextra -Wno-attributes -Wno-unused-function -Wno-unused-variable \
	-Wno-unused-label -Wno-unused-parameter -Wno-unused-but-set-variable \
	-Wno-switch \
	-w \
	$1 $2 $3 $4
