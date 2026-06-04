#!/bin/bash

cmake -B build -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if [ $? = 0 ]; then
	mv -f build/compile_commands.json .
	ninja -C build
fi
