# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dispenser)

target_sources(app PRIVATE 
                src/main.c 
                src/mqtt_handler.c
                src/servo.c
                src/ir_beam
                src/wifi.c
)
