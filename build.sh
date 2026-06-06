#!/bin/bash
./compile.sh
if [ $? = 0 ]; then
	echo
	./main.sh
fi
