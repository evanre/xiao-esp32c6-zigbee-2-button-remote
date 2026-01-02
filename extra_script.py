Import("env")

# Add Zigbee library link flags
env.Append(
    LINKFLAGS=[
        "-Wl,--start-group",
        "-L$PROJECT_DIR/managed_components/espressif__esp-zigbee-lib/lib/esp32c6",
        "-L$PROJECT_DIR/managed_components/espressif__esp-zboss-lib/lib/esp32c6",
        "-lesp_zb_api.ed",
        "-lzboss_port.native",
        "-lzboss_stack.ed",
        "-Wl,--end-group"
    ]
)
