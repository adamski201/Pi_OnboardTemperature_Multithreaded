add_executable(onboard_temperature onboard_temperature.c)

# pull in common dependencies and additional rtc hardware support
target_link_libraries(onboard_temperature
        pico_stdlib
        pico_multicore
        hardware_adc
        hardware_rtc)

# enable uart output, disable usb output
pico_enable_stdio_uart(onboard_temperature 1)
pico_enable_stdio_usb(onboard_temperature 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(onboard_temperature)

# add url via pico_set_program_url
example_auto_set_url(onboard_temperature)

