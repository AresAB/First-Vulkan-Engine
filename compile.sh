#!/bin/bash

cmake -B build -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_SHARED_LIBS=OFF -DKTX_FEATURE_TESTS=OFF -DKTX_FEATURE_ETC_UNPACK=OFF

if [ $? = 0 ]; then
	mv -f build/compile_commands.json .
	ninja -C build
fi
