# Writes a C file that holds the bytes of a file, as the array "replayTemplate" (with a
# '\0' after the last byte, so that it can be read as a string) and its length.
#
# Usage: cmake -DINPUT=<file> -DOUTPUT=<C file> -P EmbedFile.cmake

file(READ "${INPUT}" content HEX)
string(LENGTH "${content}" hexLength)
math(EXPR byteCount "${hexLength} / 2")

# 32 bytes per line, as "0x41,0x42,...".
set(lines "")
set(offset 0)
while(offset LESS hexLength)
	math(EXPR remaining "${hexLength} - ${offset}")
	if(remaining GREATER 64)
		set(remaining 64)
	endif()
	string(SUBSTRING "${content}" ${offset} ${remaining} chunk)
	string(REGEX REPLACE [[([0-9a-f][0-9a-f])]] [[0x\1,]] chunk "${chunk}")
	string(APPEND lines "${chunk}\n")
	math(EXPR offset "${offset} + 64")
endwhile()

file(WRITE "${OUTPUT}" "#include <stddef.h>\n\nconst unsigned char replayTemplate[] = {\n${lines}0x00\n};\n\nconst size_t replayTemplateLength = ${byteCount};\n")
