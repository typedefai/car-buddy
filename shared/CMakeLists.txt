cmake_minimum_required(VERSION 3.16)
project(car_buddy_protocol C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

add_executable(car_protocol_test
  protocol/car_protocol.c
  test/test_protocol.c
)
target_include_directories(car_protocol_test PRIVATE protocol)

add_test(NAME car_protocol_test COMMAND car_protocol_test)
