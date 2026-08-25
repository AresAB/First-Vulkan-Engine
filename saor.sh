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
	arguments="$scenepath "
	# iterate through arguments, only add them once r or run is seen
	flag=0
	for arg in "$@"; do
		if [ $flag = 0 ]; then
			if [[ "$arg" = "r" || "$arg" = "run" || "$arg" = "v" || "$arg" = "validate" || "$arg" = "b" || "$arg" = "build" ]]; then
				flag=1
			fi
		else
			arguments="$arguments $arg"
		fi
	done
	./build/bin/SandyShoresEngine.exe $arguments
}

case $1 in
	-h | --help)
		echo "Sandy Shores (saor) v1.3"
		echo
		echo "Usage: saor.sh <COMMAND>"
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
		echo "  process | p [FILE] ... :   Goes through provided KTX files (or untitled ktx files in results if none are provided) and determines whether to process or remove each one based on user input. Uses ktx_viewer, which is just a built scenes/ktx_viewer"
		echo "  ktx | k [FILE1] [FILE2]:   Converts [FILE1] to ktx and puts it into the assets directory as [FILE2]"
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
			run $*
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
		run $*
		export VK_INSTANCE_LAYERS=
		;;
	run | r)
		if [ $no_scenepath = 0 ]; then
			echo "ERROR: Currently loaded scene doesn't exist"
			exit 1
		fi
		run $*
		;;
	process | p)
		if [ "$2" = "" ]; then
			args=$(ls -tr results/untitled*.ktx)
			if [[ $? != 0 ]]; then
				echo "No screenshots needed to be processed"
			fi
		else
			args=$@
		fi
		echo "Processing items:"
		for arg in $args; do
			echo "  $arg"
		done
		echo
		for arg in $args; do
			if [[ $arg = "p" || $arg = "process" ]]; then
				continue
			fi
			if [[ ! -f $arg ]]; then
				echo "File $arg does not exist."
				echo ""
				continue
			fi
			echo "Processing file $arg"
			echo "View file?"
			while read -p "> " input; do
				if [[ $input = "y" || $input = "yes" || $input = "n" || $input = "no" || $input = "s" || $input = "skip" ]]; then
					break
				fi
				echo -en "\033[1A\033[2K"
			done
			if [[ $input = "s" || $input = "skip" ]]; then
				echo ""
				continue
			fi
			if [[ $input = "y" || $input = "yes" ]]; then
				echo "Viewing file, wait for engine to load up."
				ktx_viewer/bin/SandyShoresEngine.exe scenes/ktx_viewer/shaders/shader.slang $arg &
			fi
			echo "Remove file?"
			while read -p "> " input; do
				if [[ $input = "y" || $input = "yes" || $input = "n" || $input = "no" || $input = "s" || $input = "skip" ]]; then
					break
				fi
				echo -en "\033[1A\033[2K"
			done
			if [[ $input = "s" || $input = "skip" ]]; then
				echo ""
				continue
			fi
			if [[ $input = "y" || $input = "yes" ]]; then
				echo "Are you sure you want to delete $arg?"
				echo "(file will not be recoverable)"
				while read -p "> " input; do
					if [[ $input = "y" || $input = "yes" || $input = "n" || $input = "no" || $input = "s" || $input = "skip" ]]; then
						break
					fi
					echo -en "\033[1A\033[2K"
				done
				if [[ $input = "s" || $input = "skip" ]]; then
					echo ""
					continue
				fi
				if [[ $input = "y" || $input = "yes" ]]; then
					echo "Removing $arg"
					echo ""
					rm $arg
					continue
				fi
			fi
			echo "Rename screenshot (don't include filetype)"
			while read -p "> " input; do
				path="results/$input"
				if [[ -f "$path.ktx" || -f "$path.png" ]]; then
					echo "File already exists, do you wish to overwrite it?"
					while read -p "> " input; do
						if [[ $input = "y" || $input = "yes" || $input = "n" || $input = "no" || $input = "s" || $input = "skip" ]]; then
							break
						fi
						echo -en "\033[1A\033[2K"
					done
					if [[ $input = "n" || $input = "no" || $input = "s" || $input = "skip" ]]; then
						echo "Rename screenshot (don't include filetype)"
						continue
					fi
				fi
				ktx_viewer/ktx.exe extract $arg $path.png
				if [ $? = 0 ]; then
					break
				fi
				echo -en "\033[1A\033[2K"
				echo "Put in valid name"
			done
			mv $arg $path.ktx
			echo "Do you want to write a note about the screenshot?"
			while read -p "> " input; do
				if [[ $input = "y" || $input = "yes" || $input = "n" || $input = "no" || $input = "s" || $input = "skip" ]]; then
					break
				fi
				echo -en "\033[1A\033[2K"
			done
			if [[ $input = "y" || $input = "yes" ]]; then
				nvim $path.txt
			fi
			echo "Finished with processing $arg -> $path.ktx"
			echo ""
		done
		;;
	ktx | k)
		if [[ ! -f $2 ]]; then
			echo "ERROR: Input file does not exist"
			exit 1
		fi
		ktx_viewer/ktx.exe create --format=R8G8B8A8_SRGB $2 $3
		;;
	*)
		echo "ERROR: No first argument \"$1\" known to program"
		exit 1
		;;
esac
