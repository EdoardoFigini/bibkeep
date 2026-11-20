#include "display.h"

#include "efh/sb.h"
#include "efh/ss.h"

static const int padding  = 5;
static const int line_len = 100;

#define ANSI_PRE "\033["
#define ANSI_POST "m"
#define ANSI_RED_T    "31"
#define ANSI_GREEN_T  "32"
#define ANSI_YELLOW_T "33"
#define ANSI_BLUE_T   "34"
#define ANSI_VIOLET_T "35"
#define ANSI_CYAN_T   "36"
#define ANSI_GRAY_T   "37"
#define ANSI_RED_B    "41"
#define ANSI_GREEN_B  "42"
#define ANSI_YELLOW_B "43"
#define ANSI_BLUE_B   "44"
#define ANSI_VIOLET_B "45"
#define ANSI_CYAN_B   "46"
#define ANSI_GRAY_B   "47"
#define ANSI_NONE "0"
#define ANSI_BOLD "1"


void print_heading(FILE* stream, const char* heading) {
  fprintf(stream, "\n" ANSI_PRE ";" ANSI_BOLD ANSI_POST "%s" ANSI_PRE ANSI_NONE ANSI_POST "\n", heading);
}

void print_line(FILE* stream, ss_t line) {
  fprintf(stream, "%*s" PRI_SS "\n", padding, "", SS_FMT(line));
}

void print_line_fmt(FILE* stream, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  fprintf(stream, "%*s", padding, "");
  vfprintf(stream, fmt, args);
  fprintf(stream, "\n");

  va_end(args);
}

void print_paragraph(FILE* stream, ss_t par) {
  while (par.len) {
    ss_t line = ss_sub(par, 0, line_len);
    if (line.len >= line_len)
      while (line.len && !isspace(line.data[--line.len]));
    ss_advance_by_in_place(&par, line.len + 1);

    print_line(stream, line);
  }
}
