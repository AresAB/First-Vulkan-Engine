#!/bin/bash
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
./build/bin/vulkan_engine.exe $1
export VK_INSTANCE_LAYERS=
