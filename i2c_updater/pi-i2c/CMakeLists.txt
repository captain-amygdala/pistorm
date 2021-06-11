cmake_minimum_required(VERSION 3.15.2)
project(pistorm_i2c_updater C CXX)

file(GLOB i2c_updater_sources src/*.cpp src/*.hpp)
add_executable(i2c_updater ${i2c_updater_sources})
set_property(TARGET i2c_updater PROPERTY CXX_STANDARD 17)
target_compile_options(i2c_updater PRIVATE -Wall)
