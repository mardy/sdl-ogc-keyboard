find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL REQUIRED IMPORTED_TARGET sdl2 SDL2_ttf)

set(PROG ogc-osk-tool)

set(SOURCES
    ogc-osk-tool.c
)

add_executable(${PROG}
    ${SOURCES}
)
target_link_libraries(${PROG} PRIVATE
    PkgConfig::SDL
    OskCommon
)
