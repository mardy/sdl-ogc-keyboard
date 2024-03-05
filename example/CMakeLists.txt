find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL REQUIRED IMPORTED_TARGET sdl2 SDL2_ttf)

set(PROG osk-example)

add_resource(RESOURCES "DejaVuSans.ttf")

set(SOURCES
    main.c
)

add_executable(${PROG}
    ${SOURCES}
    ${RESOURCES}
)
target_link_libraries(${PROG} PUBLIC
    PkgConfig::SDL
    sdl-ogcosk
)

ogc_create_dol(${PROG})
