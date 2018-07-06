file(WRITE CMAKE_BINARY_DIR "${CMAKE_BINARY_DIR}\n")

cargo_build(NAME cc_binding)

add_dependencies(cc_binding_static ccommon-static)
add_dependencies(cc_binding_shared ccommon-shared)
