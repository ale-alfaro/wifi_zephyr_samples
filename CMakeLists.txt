#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mqtt)

# Include files that are common for all modules
add_subdirectory(src/common)

# Include mandatory module source folders
add_subdirectory(src/modules/trigger)
add_subdirectory(src/modules/sampler)
add_subdirectory(src/modules/network)
add_subdirectory_ifdef(CONFIG_MQTT_EXAMPLE src/modules/transport)
add_subdirectory(src/modules/error)

# Optional modules
add_subdirectory_ifdef(CONFIG_MQTT_SAMPLE_LED src/modules/led)
add_subdirectory_ifdef(CONFIG_HTTP_CLIENT_EXAMPLE src/modules/http_client)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

if (CONFIG_HTTP_CLIENT_EXAMPLE)

generate_inc_file_for_target(
    app
    src/modules/http_client/https-cert.der
    ${gen_dir}/https-cert.der.inc
    )
generate_inc_file_for_target(
      app
      src/modules/http_client/globalsign_r1.der
      ${gen_dir}/globalsign_r1.der.inc
)
endif()



if (CONFIG_HTTP_SERVER_EXAMPLE OR CONFIG_DUMB_HTTP_SERVER_EXAMPLE)
add_subdirectory(src/modules/http_server)
foreach(inc_file
	mt-http-server-cert.der
	mt-http-server-key.der
	response_big.html.bin
	response_100k.html.bin
    response_small.html.bin
    )
  generate_inc_file_for_target(
    app
    src/modules/http_server/${inc_file}
    ${gen_dir}/${inc_file}.inc
    )
endforeach()
endif()

