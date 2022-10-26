# Build as static library
add_library(logger logger.cpp)

# Options section 
add_compile_options(
	-O2
	-Wall 
)

# Configuration options: should external logger-object be created while building 
option(SHARED_LOG "Create one shared logger-object for whole project" ON)

if(SHARED_LOG)
	add_compile_definitions(_SHARED_LOG)

	# Adding '_SHARED_LOG' definition for whole project, not just for current dir.
	set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND
					PROPERTY COMPILE_DEFINITIONS "_SHARED_LOG")

endif()

# Add includes that library needs, but client code doesn't
target_include_directories(logger 
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)