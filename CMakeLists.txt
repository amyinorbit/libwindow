
set(SDK_ROOT "${LIBACFUTILS}/SDK")
add_library(window STATIC window.c capture.c capture_impl.h window/window.h window/capture.h)

message("SDK: ${SDK_ROOT}/CHeaders/XPLM")

include_directories(window PROJECT
    "${SDK_ROOT}/CHeaders/XPLM"
    "${SDK_ROOT}/CHeaders/Widgets"
    "${SDK_ROOT}/CHeaders/Wrappers"
)

target_include_directories(window PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DXPLM200=1 -DXPLM210=1 -DXPLM300=1 -DXPLM301=1 -DDEBUG=1)
target_compile_options(window PUBLIC -Wall -Wextra -Werror)
set_property(TARGET window PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(window PUBLIC ${LIBACFUTILS_LIBS})

if(WIN32)
    add_definitions(-DAPL=0 -DIBM=1 -DLIN=0)
elseif(APPLE)
    add_definitions(-DAPL=1 -DIBM=0 -DLIN=0)
else()
    add_definitions(-DAPL=0 -DIBM=0 -DLIN=1)
endif()

if(APPLE)
    find_library(XPLM_LIBRARY XPLM ${SDK_ROOT}/Libraries/Mac/)
    find_library(XPWIDGETS_LIBRARY XPWidgets ${SDK_ROOT}/Libraries/Mac/)
    find_library(CocoaLib Cocoa)
    
    target_link_libraries(window PUBLIC ${XPLM_LIBRARY})
    target_link_libraries(window PUBLIC ${XPWIDGETS_LIBRARY})
    target_link_libraries(window PUBLIC ${CocoaLib})
elseif(WIN32)
    set(CMAKE_LIBRARY_PATH ${SDK_ROOT}/Libraries/Win/)
    find_library(XPLM_LIBRARY
        NAMES XPLM_64
        PATHS ${SDK_ROOT}/Libraries/Win/
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )
    find_library(XPWIDGETS_LIBRARY
        XPWidgets_64
        ${SDK_ROOT}/Libraries/Win/
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )
    target_link_libraries(window PUBLIC ${XPLM_LIBRARY} ${XPWIDGETS_LIBRARY})
endif()
