#ifndef REPLAY_TEMPLATE_HEADER
#define REPLAY_TEMPLATE_HEADER

#include <stddef.h>

/**
 * The HTML page of the replay, which is the file "src/main/resources/replay-
 * template.html" turned into a C array when the compiler is built (see
 * "src/main/cmake/EmbedFile.cmake"). The array has a '\0' after its last byte,
 * so it can be read as a string too, and its length does not count it.
 */
extern const unsigned char replayTemplate[];
extern const size_t replayTemplateLength;

#endif
