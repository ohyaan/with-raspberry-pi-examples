include(FetchContent)

# Declare external repository (e.g. nlohmann/json library)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.10.5
)

# Download and load the project targets
FetchContent_MakeAvailable(json)

# Link it to your executable
target_link_libraries(app PRIVATE nlohmann_json::nlohmann_json)
