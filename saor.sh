#!/bin/bash

scenepath=""
if [ -f .current ]; then
	scenepath=$(cat .current)
fi

[[ ! -d $scenepath || "$scenepath" = "" ]]
no_scenepath=$?

compile() {
	cmake -B build -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_SHARED_LIBS=OFF -DKTX_FEATURE_TESTS=OFF -DKTX_FEATURE_ETC_UNPACK=OFF -DSCENE=$scenepath
	if [ $? = 0 ]; then
		mv -f build/compile_commands.json .
		ninja -C build
	fi
}

run() {
	./build/bin/vulkan_engine.exe $scenepath
}

case $1 in
	-h | --help)
		echo "Sandy Shores (saor) v0.1"
		echo
		echo "Usage: saor <COMMAND>"
		echo
		echo "Commands:"
		echo "  -h | --help:   Shows this page"
		echo "  load | l [DIR]:   Loads [DIR] in as current scene that other commands will interact with. If no [DIR] is provided, simply outputs currently loaded scene"
		echo "  create | c [DIR1] [DIR2]:   Copies [DIR2] into a new [DIR1]"
		echo "  duplicate | d [DIR]:   Copies currently loaded scene into a new [DIR]"
		echo "  switch | s [DIR]:   Duplicates and loads [DIR]"
		echo "  compile | com:   Compiles currently loaded scene"
		echo "  run | r:   Runs currently loaded scene without validation layers"
		echo "  validate | v:   Runs currently loaded scene with validation layers on"
		echo "  build | b:   Compiles and validates currently loaded scene"
		echo "  process | p:   (NOT COMPLETE) Goes through currently loaded scene's results directory and determines whether to process or remove each one based on user input"
		;;
	load | l)
		if [ "$2" != "" ]; then
			if [ -d $2 ]; then
				echo $2 > .current
				echo "Loading scene \"$2\""
			else
				echo "ERROR: scenepath \"$2\" doesn't exist"
			fi
		else
			if [[ "$scenepath" != "" && $no_scenepath = 1 ]]; then
				echo "Remaining in scene \"$scenepath\""
			else
				echo "ERROR: Currently loaded scene doesn't exist"
				exit 1
			fi
		fi
		;;
	create | c)
		if [ $no_scenepath = 0 ]; then
			echo "ERROR: Currently loaded scene doesn't exist"
			exit 1
		fi
		if [ "$3" = "" ]; then
			echo "ERROR: No template provided to create new project from"
			exit 1
		fi
		if [ ! -d $3 ]; then
			echo "ERROR: Provided template is not a real scenepath"
			exit 1
		fi
		cp -r $3 $2
		echo "New scene \"$2\" made from template \"$3\""
		;;
	duplicate | d)
		if [ $no_scenepath = 0 ]; then
			echo "ERROR: Currently loaded scene doesn't exist"
			exit 1
		fi
		cp -r $scenepath $2
		echo "New scene \"$2\" made from template \"$scenepath\""
		;;
	switch | s)
		if [ $no_scenepath = 0 ]; then
			echo "ERROR: Currently loaded scene doesn't exist"
			exit 1
		fi
		cp -r $scenepath $2
		echo $2 > .current
		echo "Loading scene \"$2\""
		;;
	build | b)
		if [ $no_scenepath = 0 ]; then
			echo "ERROR: Currently loaded scene doesn't exist"
			exit 1
		fi
		compile
		if [ $? = 0 ]; then
			export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
			run
			export VK_INSTANCE_LAYERS=
		fi
		;;
	compile | com)
		if [ $no_scenepath = 0 ]; then
			echo "ERROR: Currently loaded scene doesn't exist"
			exit 1
		fi
		compile
		;;
	validate | v)
		if [ $no_scenepath = 0 ]; then
			echo "ERROR: Currently loaded scene doesn't exist"
			exit 1
		fi
		export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
		run
		export VK_INSTANCE_LAYERS=
		;;
	run | r)
		if [ $no_scenepath = 0 ]; then
			echo "ERROR: Currently loaded scene doesn't exist"
			exit 1
		fi
		run
		;;
	process | p)
		echo "processing"
		;;
	*)
		echo "ERROR: No first argument \"$1\" known to program"
		exit 1
		;;
esac
