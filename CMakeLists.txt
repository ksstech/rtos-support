# RTOS SUPPORT
set( srcs "FreeRTOS_Support.c" )
set( include_dirs "." )
set( requires 
	common
)
set( priv_requires
	hal_esp32 main printfx syslog systiming
)
idf_component_register(SRCS ${srcs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires} PRIV_REQUIRES ${priv_requires})
