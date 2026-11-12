#ifndef HTML_GENERATOR_HEADER
#define HTML_GENERATOR_HEADER

#include "../simulation/Trace.h"
#include <stdbool.h>
#include <stdio.h>

/**
 * Writes the trace of a battle as a single HTML page that replays it, with no
 * other file and nothing to load from anywhere: the page is the template of the
 * replay with the data of the battle (see the comment of "HtmlGenerator.c" for
 * their format) in a JSON block. A trace that stops halfway (a failed
 * simulation) is written as far as it goes, and its error goes in the data. It
 * only reads the trace.
 *
 * Returns false only when it runs out of memory.
 */
bool generateHtml(const Trace * trace, FILE * out);

#endif
