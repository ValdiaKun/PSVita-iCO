# port/config: the port's settings (config.c: config.toml and ico-pc.ini as
# one store) and libscf's system configuration (sysconf.c: the language from
# [game] language or the host's locale). The parsing, writing and ini layering
# are port/platform/host_config.c, which ico_pc and the tests compile
# themselves; the clocks (platform/clock.c) are ico_platform's.
add_library(ico_config STATIC config.c sysconf.c)
target_include_directories(ico_config
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${ICO_COMPAT_DIR}" "${CMAKE_SOURCE_DIR}/port/platform")
target_compile_options(ico_config PRIVATE ${ICO_SEMANTIC_OPTIONS} ${ICO_PORT_WARNINGS})
# sysconf.c attaches the EE timers (platform/clock.c)
target_link_libraries(ico_config PUBLIC ico_platform)

# the window build: SDL's preferred locales and preference folder
# (SDL3 is found by port/render, which comes first)
if(NOT ICO_HEADLESS AND TARGET SDL3::SDL3-shared)
    target_compile_definitions(ico_config PRIVATE ICO_CONFIG_SDL=1)
    target_link_libraries(ico_config PUBLIC SDL3::SDL3-shared)
    if(TARGET ico_pc)
        target_compile_definitions(ico_pc PRIVATE ICO_HOST_SDL_PREFPATH=1)
    endif()
endif()

if(TARGET ico_pc)
    target_link_libraries(ico_pc PRIVATE ico_config)
endif()

# Test the Vita-specific storage policy with a disposable host directory. Keep
# this target beside ico_config so the standalone test links the same TOML and
# platform services as the real application.
add_executable(host_config_vita_test ../platform/test/host_config_vita_test.c
                                    ../platform/host_config.c)
target_compile_definitions(host_config_vita_test PRIVATE
    ICO_HOST_VITA=1 ICO_HOST_VITA_DATA_DIR=\"host_config_vita_test_dir\")
target_include_directories(host_config_vita_test PRIVATE "${ICO_COMPAT_DIR}"
                           "${CMAKE_SOURCE_DIR}/port/platform")
target_compile_options(host_config_vita_test PRIVATE ${ICO_SEMANTIC_OPTIONS} ${ICO_PORT_WARNINGS})
target_link_libraries(host_config_vita_test PRIVATE ico_config ico_platform)
if(ICO_RUN_TESTS)
    add_test(NAME host_config_vita COMMAND host_config_vita_test)
endif()

# parse, write, round trip, precedence, the language, the clocks and timers
add_executable(config_test test/config_test.c ../platform/host_config.c)
target_include_directories(config_test PRIVATE "${ICO_COMPAT_DIR}" "${CMAKE_SOURCE_DIR}/port/platform")
target_compile_options(config_test PRIVATE ${ICO_SEMANTIC_OPTIONS} ${ICO_PORT_WARNINGS})
target_link_libraries(config_test PRIVATE ico_config ico_platform)
if(ICO_RUN_TESTS)
    add_test(NAME config COMMAND config_test "${CMAKE_CURRENT_BINARY_DIR}")
endif()
