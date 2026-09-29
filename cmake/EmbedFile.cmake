# cmake -DIN=file -DOUT=header -DNAME=ident -P EmbedFile.cmake
file(READ ${IN} _content)
string(SUBSTRING ${NAME} 0 1 _first)
string(TOUPPER ${_first} _first)
string(SUBSTRING ${NAME} 1 -1 _rest)
string(REGEX REPLACE "_([a-z])" "\\1" _rest ${_rest})
file(WRITE ${OUT} "#pragma once\n// Generated from ${IN}\nconstexpr const char* k${_first}${_rest}Wgsl = R\"__wgsl__(${_content})__wgsl__\";\n")
