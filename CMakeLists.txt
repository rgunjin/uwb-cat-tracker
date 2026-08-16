cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(uwb-cat-tracker)

target_sources(app PRIVATE 
    src/main.c
    src/dw1000_spi.c
    vendor/decadriver/deca_device.c
	vendor/decadriver/deca_params_init.c
)

target_include_directories(app PRIVATE
	src
	vendor/decadriver
)
