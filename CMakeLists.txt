cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED)
project(myapp)

set(BOARD rp2350b_core/rp2350b/m33)

target_sources(app PRIVATE src/main.c)


