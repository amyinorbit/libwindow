
set(SDK_ROOT "${LIBACFUTILS}/SDK")
add_library(window STATIC window.c capture.c capture_impl.h window/window.h window/capture.h)

message("SDK: ${SDK_ROOT}/CHeaders/XPLM")

include_directories(window PROJECT
    "${SDK_ROOT}/CHeaders/XPLM"
    "${SDK_ROOT}/CHeaders/Widgets"
    "${SDK_ROOT}/CHeaders/Wrappers"
)

target_include_directories(window PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(window PUBLIC -Wall -Wextra -Werror)
target_link_libraries(window PUBLIC acfutils xplm xpwidgets)
set_target_properties(window
PROPERTIES
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
    LINK_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fno-stack-protector"
)

