idf_build_get_property(target IDF_TARGET)

set(srcs
	"ota.c"
)

#set(include_dirs "include")
#set(priv_include_dirs "lwip" "private_include")

idf_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${include_dirs}"
	PRIV_INCLUDE_DIRS "${priv_include_dirs}"
	#REQUIRES esp_event
	PRIV_REQUIRES app_update
	#LDFRAGMENTS linker.lf
)

# target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
