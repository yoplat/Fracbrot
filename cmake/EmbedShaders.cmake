# Embeds every shaders/*.wgsl as a C++ string so no runtime file IO is needed
# (required for the web build). Generates <build>/generated/shaders/<name>.wgsl.h
# exposing `constexpr const char* k<Name>Wgsl`.
function(fracbrot_embed_shaders target shader_dir)
  file(GLOB _shaders CONFIGURE_DEPENDS ${shader_dir}/*.wgsl)
  set(_out_dir ${CMAKE_BINARY_DIR}/generated/shaders)
  set(_outputs)
  foreach(_src ${_shaders})
    get_filename_component(_name ${_src} NAME_WE)
    set(_out ${_out_dir}/${_name}.wgsl.h)
    add_custom_command(
      OUTPUT ${_out}
      COMMAND ${CMAKE_COMMAND} -DIN=${_src} -DOUT=${_out} -DNAME=${_name}
              -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFile.cmake
      DEPENDS ${_src} ${CMAKE_SOURCE_DIR}/cmake/EmbedFile.cmake
      VERBATIM)
    list(APPEND _outputs ${_out})
  endforeach()
  add_custom_target(${target}_shaders DEPENDS ${_outputs})
  add_dependencies(${target} ${target}_shaders)
  target_include_directories(${target} PRIVATE ${CMAKE_BINARY_DIR}/generated)
endfunction()
