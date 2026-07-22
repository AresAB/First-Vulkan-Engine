#!/bin/bash
./compile.sh
if [ $? = 0 ]; then
	echo
	./validate.sh
fi
