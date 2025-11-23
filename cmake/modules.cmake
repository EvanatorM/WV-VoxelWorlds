include(FetchContent)

macro(_add_git_project NAME REPO COMMIT)
  message(STATUS "Module: ${NAME} @ ${COMMIT}")
  FetchContent_Declare(${NAME}
    GIT_REPOSITORY ${REPO}
    GIT_TAG        ${COMMIT}      # exact commit from lockfile
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED FALSE
  )
  FetchContent_MakeAvailable(${NAME})
endmacro()

# Add core module
_add_git_project(WVCore https://github.com/EvanatorM/WV-Core.git alpha-v0.2.3)
target_link_libraries(${PROJECT_NAME} PRIVATE WVCore)
target_include_directories(${PROJECT_NAME} PUBLIC ${wvcore_SOURCE_DIR}/include)