include(FetchContent)

FetchContent_Declare(
    xxhash
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG v0.8.1
    UPDATE_DISCONNECTED ON
)

FetchContent_MakeAvailable(xxhash)