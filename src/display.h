#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdio.h>

#include "efh/ss.h"

#include "args.h"

void print_heading(FILE* stream, const char* heading);
void print_line(FILE* stream, ss_t line);
void print_line_fmt(FILE* stream, const char* fmt, ...);
void print_paragraph(FILE* stream, ss_t par);

#endif
