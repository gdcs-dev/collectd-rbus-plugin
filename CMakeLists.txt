cmake_minimum_required(VERSION 3.10)
project(collectd_rbus_plugin C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-format-truncation")

find_library(RBUS_LIBRARY NAMES rbus)
find_library(RBUS_CORE_LIBRARY NAMES rbuscore)
find_path(RBUS_INCLUDE_DIR NAMES rbus.h PATH_SUFFIXES rbus)
find_path(COLLECTD_INCLUDE_DIR NAMES collectd/core/daemon/plugin.h)
find_package(Threads REQUIRED)

if(NOT RBUS_LIBRARY OR NOT RBUS_CORE_LIBRARY OR NOT RBUS_INCLUDE_DIR)
   message(FATAL_ERROR "rbus/rbuscore library or headers not found (expected the rbus-elements/rbus package)")
endif()

if(NOT COLLECTD_INCLUDE_DIR)
   message(FATAL_ERROR "collectd plugin headers not found (install collectd-dev)")
endif()

set(COLLECTD_INCLUDE_DIRS ${COLLECTD_INCLUDE_DIR} ${COLLECTD_INCLUDE_DIR}/collectd)

add_library(
   collectd_rbus_plugin MODULE
   src/rbus_plugin.c
   src/rbus_config.c
   src/rbus_mapping.c
   src/rbus_dispatch.c
   src/rbus_tables.c
   src/rbus_subscribe.c)

# collectd's own headers assume two different include roots depending on which
# header does the including: plugin.h uses "collectd/core/daemon/..." (root =
# COLLECTD_INCLUDE_DIR), while configfile.h uses "liboconfig/oconfig.h" (root =
# COLLECTD_INCLUDE_DIR/collectd). Add both so quoted-include fallback resolves
# either way.
target_include_directories(collectd_rbus_plugin PRIVATE ${RBUS_INCLUDE_DIR}
                                                          ${COLLECTD_INCLUDE_DIRS})
target_link_libraries(collectd_rbus_plugin PRIVATE ${RBUS_LIBRARY}
                                                    ${RBUS_CORE_LIBRARY} Threads::Threads)

# collectd's plugin loader dlopen()s "<PluginDir>/<name>.so" -- no "lib" prefix.
set_target_properties(collectd_rbus_plugin PROPERTIES PREFIX "" OUTPUT_NAME "rbus")

include(CTest)
if(BUILD_TESTING)
   add_executable(test_rbus_mapping tests/test_mapping.c src/rbus_mapping.c
                                     tests/support/plugin_log_stub.c)
   target_include_directories(test_rbus_mapping PRIVATE ${RBUS_INCLUDE_DIR}
                                                         ${COLLECTD_INCLUDE_DIRS} src)
   target_compile_options(test_rbus_mapping PRIVATE -UNDEBUG)
   target_link_libraries(test_rbus_mapping PRIVATE ${RBUS_LIBRARY} ${RBUS_CORE_LIBRARY})
   add_test(NAME test_rbus_mapping COMMAND test_rbus_mapping)

   add_executable(test_rbus_config tests/test_config.c src/rbus_config.c
                                    tests/support/plugin_log_stub.c)
   target_include_directories(test_rbus_config PRIVATE ${RBUS_INCLUDE_DIR}
                                                        ${COLLECTD_INCLUDE_DIRS} src)
   target_compile_options(test_rbus_config PRIVATE -UNDEBUG)
   target_link_libraries(test_rbus_config PRIVATE ${RBUS_LIBRARY} ${RBUS_CORE_LIBRARY})
   add_test(NAME test_rbus_config COMMAND test_rbus_config)
endif()
