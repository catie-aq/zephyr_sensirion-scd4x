# Copyright (c) 2024 CATIE
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(scd4x)

target_sources(app PRIVATE src/main.c)