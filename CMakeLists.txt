idf_component_register(
	SRCS "FreeRTOS_Support.c"
	INCLUDE_DIRS "."
	PRIV_REQUIRES hal_esp32 printfx syslog systiming
)
