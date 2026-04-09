cmake_minimum_required(VERSION 3.20.0)

set(BOARD rp2350b_core/rp2350b/m33)

find_package(Zephyr REQUIRED)
project(ergodoxcustom)

target_sources(app PRIVATE src/main.c)


