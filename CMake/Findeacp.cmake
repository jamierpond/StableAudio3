include(CPM)

# The fork branch carries the library work this app needs. Once it merges into
# eyalamirmusic/eacp, point this at that repo and its develop branch.
CPMAddPackage(
        NAME eacp
        GITHUB_REPOSITORY jamierpond/eacp
        GIT_TAG de34af92bc9ff3a8d6c0cfe602f2a3d9ce0bc9a4 # jp/stable-audio-infrence
)

# eacp's own Find modules (NanoTest) and helpers, which its CMakeLists only
# puts on the module path in its own scope.
list(APPEND CMAKE_MODULE_PATH "${eacp_SOURCE_DIR}/CMake")
