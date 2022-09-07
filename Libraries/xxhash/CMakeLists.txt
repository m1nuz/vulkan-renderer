set(LIB_NAME xxHash)

add_library(${LIB_NAME}
    STATIC
    ${xxhash_SOURCE_DIR}/xxhash.c
)

target_include_directories(${LIB_NAME}
    PUBLIC
    "$<BUILD_INTERFACE:${xxhash_SOURCE_DIR}>"
)