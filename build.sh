#!/bin/bash
./compile.sh
if [ $? = 0 ]; then
	./main.sh
fi
