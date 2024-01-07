cmake_minimum_required(VERSION 3.11)
project(pendulum LANGUAGES C)
set(CMAKE_C_STANDARD 11)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

string(APPEND CMAKE_C_FLAGS " -Wno-deprecated-declarations") # OpenSSL RC4 is deprecated
add_executable(${PROJECT_NAME} "src/pendulum.c")

target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})
