cmake_minimum_required(VERSION 3.20)
project("multithreaded-traffic-sim-with-C-plus-plus" CXX)

# Imposta lo standard C++ (es. C++17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Definisci l'eseguibile e i file sorgente
add_executable(traffico 
    traffico.cpp 
    Intersection.cpp 
    Traffic_algorithm.cpp
    )

find_package(Threads REQUIRED)

target_link_libraries(traffico PRIVATE Threads::Threads)    

set_target_properties(traffico PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(test_traffico test.cpp
    Intersection.cpp 
    Traffic_algorithm.cpp)

target_link_libraries(test_traffico
    PRIVATE
    GTest::gtest_main
)

set_target_properties(test_traffico PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)