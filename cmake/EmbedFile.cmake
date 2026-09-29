# cmake -DIN=file -DOUT=header -DNAME=ident -P EmbedFile.cmake
# NAME in snake_case becomes k<PascalCase>Wgsl, e.g. bifurcation_compute -> kBifurcationComputeWgsl.
file(READ ${IN} _content)
string(REPLACE "_" ";" _parts ${NAME})
set(_ident "")
foreach(_part ${_parts})
  string(SUBSTRING ${_part} 0 1 _first)
  string(TOUPPER ${_first} _first)
  string(SUBSTRING ${_part} 1 -1 _rest)
  string(APPEND _ident ${_first}${_rest})
endforeach()
file(WRITE ${OUT} "#pragma once\n// Generated from ${IN}\nconstexpr const char* k${_ident}Wgsl = R\"__wgsl__(${_content})__wgsl__\";\n")
