#include <stdio.h>
#include <stdbool.h>

#include "efh/vec.h"
#include "efh/ss.h"
#include "efh/sb.h"

#include "macros.h"

typedef struct {
  int id;
  ss_t title;
  VEC(ss_t) authors;
  ss_t path;
  VEC(ss_t) tags;
  bool ignore;
  int relevance;
  ss_t doi;
  ss_t issn;
  ss_t url;
  ss_t abstract;
} source_t;

typedef struct {
  ss_t name;
  VEC(source_t) sources;
} library_t;

ss_t parse_field_str(ss_t src) {
  src = ss_trim(ss_advance_after(src, "="));
  return ss_sub(src, 1, src.len -1); //trim quotes
}

int parse_field_int(ss_t src) {
  src = ss_trim(ss_advance_after(src, "="));
  return ss_atoi(src);
}

bool parse_field_bool(ss_t src) {
  src = ss_trim(ss_advance_after(src, "="));
  return ss_eq_cstr(src, "true");
}

#define TRY_PARSE(src, name, type) \
  if (ss_starts_with_cstr(src, # name)) { source->name = parse_field_ ## type(src); continue; }

bool deserialize_source(ss_t* src, source_t* source) {
  // ss_t line = ss_trim(ss_tok(&src, '\n'));
  // if (!ss_eq_cstr(line, "[[source]]")) return false;
  //
  while(src->len) {
    ss_t prev = *src;
    ss_t line = ss_trim(ss_tok(src, '\n'));
    // printf("<" PRI_SS ">\n", SS_FMT(line));

    TRY_PARSE(line, id,         int);
    TRY_PARSE(line, title,      str);
    TRY_PARSE(line, path,       str);
    TRY_PARSE(line, ignore,     bool);
    TRY_PARSE(line, relevance,  int);
    TRY_PARSE(line, abstract,   str);
    TRY_PARSE(line, doi,        str);
    TRY_PARSE(line, issn,       str);
    TRY_PARSE(line, url,        str);

    if (ss_starts_with_cstr(line, "tags")) {
      line = ss_trim(ss_advance_after(line, "="));
      line = ss_trim(ss_advance_after(line, "["));
      line = ss_trim(ss_advance_after(line, "\""));

      vec_push(&source->tags, ss_tok(&line, '"'));

      while (line.len) {
        line = ss_trim(ss_advance_after(line, ","));
        line = ss_trim(ss_advance_after(line, "\""));

        vec_push(&source->tags, ss_tok(&line, '"'));

        if (ss_starts_with_cstr(line, "]")) break;
      }

      continue;
    }

    if (ss_starts_with_cstr(line, "authors")) {
      line = ss_trim(ss_advance_after(line, "="));
      line = ss_trim(ss_advance_after(line, "["));
      line = ss_trim(ss_advance_after(line, "\""));

      vec_push(&source->authors, ss_tok(&line, '"'));

      while (line.len) {
        line = ss_trim(ss_advance_after(line, ","));
        line = ss_trim(ss_advance_after(line, "\""));

        vec_push(&source->authors, ss_tok(&line, '"'));

        if (ss_starts_with_cstr(line, "]")) break;
      }

      continue;
    }

    // found next table
    if (ss_starts_with_cstr(line, "[")) {
      *src = prev; // rewind;
      return true;
    }
  }

  return true;
}

bool deserialize_library(ss_t src, library_t* library) {
  while(src.len) {
    ss_t line = ss_trim(ss_tok(&src, '\n'));

    if (ss_starts_with_cstr(line, "name")) { library->name = parse_field_str(line); continue; }

    if (ss_starts_with_cstr(line, "[[source]]")) {
      source_t source = { 0 };
      deserialize_source(&src, &source);
      vec_push(&library->sources, source);
    }
  }

  return true;
}

bool deserialize_library_from_file(const char* path, library_t* library) {
  sb_t sb = { 0 };

  if (sb_read_file(path, &sb) < 0) return false;

  return deserialize_library(ss_from_sb(sb), library);
}

void serialize_source(sb_t* sb, const source_t* source) {
  sb_appendf(sb, "[[source]]\n");

  sb_appendf(sb, "  title = \"" PRI_SS "\"\n", SS_FMT(source->title));
  sb_appendf(sb, "  authors: ["); {
    bool first = true;
    vec_foreach(author, &source->authors) {
      sb_appendf(sb, "%s" PRI_SS, first ? "" : ", ", SS_FMT(*author));
      first = false;
    }
    sb_appendf(sb, "]\n");
  }
  sb_appendf(sb, "  path = \""  PRI_SS "\"\n", SS_FMT(source->path));
  sb_appendf(sb, "  ignore = %s\n", source->ignore ? "true" : "false");
  sb_appendf(sb, "  relevance = %d\n", source->relevance);
  sb_appendf(sb, "  tags: ["); {
    bool first = true;
    vec_foreach(tag, &source->tags) {
      sb_appendf(sb, "%s" PRI_SS, first ? "" : ", ", SS_FMT(*tag));
      first = false;
    }
    sb_appendf(sb, "]\n");
  }
  sb_appendf(sb, "  doi = \"" PRI_SS "\"\n", SS_FMT(source->doi));
  sb_appendf(sb, "  issn = \"" PRI_SS "\"\n", SS_FMT(source->issn));
  sb_appendf(sb, "  url = \"" PRI_SS "\"\n", SS_FMT(source->url));
  sb_appendf(sb, "  abstract = \"" PRI_SS "\"\n", SS_FMT(source->abstract));
}

void serialize_library(sb_t* sb, const library_t* library) {
  sb_appendf(sb, "name = \"" PRI_SS "\"\n", SS_FMT(library->name));

  vec_foreach(p, &library->sources) {
    sb_appendf(sb, "\n");

    serialize_source(sb, p);
  }
}

bool serialize_library_to_file(const char* path, const library_t* library) {
  sb_t sb = { 0 };

  serialize_library(&sb, library);

  return sb_write_to_file(path, &sb) > 0;
}

enum order_by {
  ORD_NONE,
  ORD_REL_DESC,
  ORD_REL_ASC,
};

int ord_cmp_desc(const void* a, const void* b) {
    const source_t* arg1 = (const source_t*)a;
    const source_t* arg2 = (const source_t*)b;
    return (arg2->relevance > arg1->relevance) - (arg2->relevance < arg1->relevance);
}

int ord_cmp_asc(const void* a, const void* b) {
    const source_t* arg1 = (const source_t*)a;
    const source_t* arg2 = (const source_t*)b;
    return (arg1->relevance > arg2->relevance) - (arg1->relevance < arg2->relevance);
}                                                                                                                                 


static char* arg_shift(int *argc, char ***argv) {
    assert(*argc > 0);
    char *result = **argv;
    *argv += 1;
    *argc -= 1;
    return result;
}

typedef struct {
  const char* name;
  int (*fn)(const char*, int, char**);
  const char* desc;
} command_t;

void list_commands(FILE* stream);
static void print_heading(FILE* stream, const char* heading);
static void print_line(FILE* stream, ss_t line);
static void print_paragraph(FILE* stream, ss_t par);

const int padding  = 5;
const int line_len = 100;

#define COMMAND(n, f, d) (command_t){ .name = n, .fn = f, .desc = d }

#define OPT_ARG_PREFIX "--"

typedef struct {
  const char* name;
  const char* desc;
  const char* type;
  bool opt;
  bool no_value;
  bool present;
  const char* default_val;
  const char* value;
} arg_t;

typedef struct {
  VEC(arg_t) pos;
  VEC(arg_t) opt;
  const char* last_error_message;
} arg_ctx_t;

arg_ctx_t g_arg_ctx = { 0 };

void clear_arg_ctx() {
  vec_reset(&g_arg_ctx.pos);
  vec_reset(&g_arg_ctx.opt);
  g_arg_ctx.last_error_message = NULL;
}

struct _arg_def {
  const char* desc;
  const char* type;
  bool no_value;
  bool opt;
  const char* default_val;
};

#define POS_BIT ((size_t)1 << ((sizeof(size_t) * 8) - 1))

#define arg(name, ...) __arg_impl(name, (struct _arg_def){ __VA_ARGS__  }) 
size_t __arg_impl(const char* name, struct _arg_def arg_def) {
  arg_t arg = {
    .name = name,
    .desc = arg_def.desc,
    .type = arg_def.type,
    .no_value = arg_def.no_value,
    .default_val = arg_def.default_val,
    .opt = arg_def.opt,
  };

  if (arg_def.opt) {
    vec_push(&g_arg_ctx.opt, arg);
    return vec_len(&g_arg_ctx.opt) - 1;
  } else {
    vec_push(&g_arg_ctx.pos, arg);
    return vec_len(&g_arg_ctx.pos) - 1 | POS_BIT;
  }
}

void print_arg(FILE* stream, const arg_t* arg) {
  sb_t sb = { 0 };

  if (arg->opt) sb_appendf(&sb, OPT_ARG_PREFIX "%-13s ", arg->name);
  else          sb_appendf(&sb, "%-15s ", arg->name);

  sb_appendf(&sb, "%-8s ", arg->no_value ? "" : arg->type);
  sb_appendf(&sb, "%s ", arg->desc);
  if (arg->default_val)
    sb_appendf(&sb, "DEFAULT: %s.", arg->default_val);

  print_line(stream, ss_from_sb(sb));
}

void print_usage(FILE* stream, const char* progname) {
  print_heading(stream, "USAGE");

  sb_t sb = { 0 };
  sb_appendf(&sb, "%s ", progname);
  vec_foreach(a, &g_arg_ctx.pos) {
    sb_appendf(&sb, "<%s> ", a->name);
  }
  sb_append(&sb, "[OPTIONS]");
  print_line(stream, ss_from_sb(sb));

  sb_free(&sb);
}

void print_args(FILE* stream) {
  print_heading(stream, "ARGS");
  vec_foreach(a, &g_arg_ctx.pos) {
    print_arg(stream, a);
  }
  print_heading(stream, "OPTIONS");
  vec_foreach(a, &g_arg_ctx.opt) {
    print_arg(stream, a);
  }
}

void print_arg_last_error(FILE* stream) {
  print_heading(stream, "ERROR");
  print_line(stream, ss_from_cstr(g_arg_ctx.last_error_message));
}

size_t arg_pos_count() {
  return vec_len(&g_arg_ctx.pos);
}

// TODO: auto parsing of values
const char* arg_value(size_t arg_idx) {
  arg_t arg = (arg_idx & POS_BIT)                         ?
              vec_get(&g_arg_ctx.pos, arg_idx & ~POS_BIT) :
              vec_get(&g_arg_ctx.opt, arg_idx & ~POS_BIT) ;

  return arg.present ? arg.value : arg.default_val;
}

bool arg_present(size_t arg_idx) {
  if (arg_idx & POS_BIT) return vec_get(&g_arg_ctx.pos, arg_idx & ~POS_BIT).present;
  else                   return vec_get(&g_arg_ctx.opt, arg_idx & ~POS_BIT).present;
}

bool arg_parse(int* argc, char*** argv, int limit) {
  sb_t sb = { 0 };
  //
  // if (*argc < arg_pos_count()) {
  //   sb_appendf(&sb, "Not enough positional arguments passed: Missing <%s>.", vec_get(&g_arg_ctx.pos, *argc).name);
  //   sb_appendz(&sb, "");
  //   g_arg_ctx.last_error_message = sb.data;
  //   return false;
  // }

  size_t positional = 0;
  while (*argc && limit--) {
    const char* arg = arg_shift(argc, argv);

    if (*arg != *OPT_ARG_PREFIX && positional < arg_pos_count()) {
      vec_get(&g_arg_ctx.pos, positional).present = true;
      vec_get(&g_arg_ctx.pos, positional).value   = arg;
      positional++;
    } else {
      bool found = false;
      vec_foreach(a, &g_arg_ctx.opt) {
        if (strcmp(arg + (sizeof(OPT_ARG_PREFIX) - 1), a->name) == 0) {
          a->present = found = true;
          
          if (!a->no_value) {
            if (*argc == 0) {
              sb_appendf(&sb, "No value provided for `" OPT_ARG_PREFIX "%s` option.", a->name);
              sb_appendz(&sb, "");
              g_arg_ctx.last_error_message = sb.data;
              return false;
            }
            a->value = arg_shift(argc, argv);
          }

          break;
        }
      }
      if (found) continue;

      sb_appendf(&sb, "Unrecognized optional argument `%s`", arg);
      sb_appendz(&sb, "");
      g_arg_ctx.last_error_message = sb.data;
      return false;
    }
  }

  if (positional < arg_pos_count()) {
    sb_appendf(&sb, "Not enough positional arguments passed: Missing <%s>.", vec_get(&g_arg_ctx.pos, positional).name);
    sb_appendz(&sb, "");
    g_arg_ctx.last_error_message = sb.data;
    return false;
  }

  return true;
}

int do_list(const char* progname, int argc, char** argv) {
  clear_arg_ctx();

  library_t lib = { 0 };

  size_t file_opt  = arg("file", .opt = true, .type = "str", .desc = "Path to library file.", .default_val = "library.toml");
  size_t help_opt  = arg("help", .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exit with 0." );
  size_t ord_opt   = arg("sort", .opt = true, .type = "str", .desc = "Sort sources by relevance. Available options: [NONE, ASC, DESC].", .default_val = "DESC");

  sb_t sb = { 0 };
  sb_appendf(&sb, "%s list", progname);
  sb_appendz(&sb, "");

  if (!arg_parse(&argc, &argv, -1)) {
    print_usage(stderr, sb.data);
    print_args(stderr);
    print_arg_last_error(stderr);
    return 1;
  }

  if (arg_present(help_opt)) {
    print_usage(stdout, sb.data);
    print_args(stdout);
    return 0;
  }

  deserialize_library_from_file(arg_value(file_opt), &lib);

  enum order_by ob = ORD_NONE;

  const char* ord = arg_value(ord_opt);

       if (strcmp(ord, "ASC") == 0)  ob = ORD_REL_ASC; 
  else if (strcmp(ord, "DESC") == 0) ob = ORD_REL_DESC; 

  print_heading(stdout, "LIBRARY");
  print_line(stdout, lib.name);

  print_heading(stdout, "SOURCES");

  switch (ob) {
    case ORD_NONE: break;
    case ORD_REL_DESC: vec_sort(&lib.sources, ord_cmp_desc); break;
    case ORD_REL_ASC:  vec_sort(&lib.sources, ord_cmp_asc); break;
    default:
      UNREACHABLE("Unknown display_list order by");
  }

  print_line(stdout, ss_from_cstr("RELEV.   ID     TITLE"));
  vec_foreach(p, &lib.sources) {
    sb_t sb = { 0 };
    sb_appendf(&sb, "%d ", p->relevance);
    for (size_t i=0; i < 5; i++) {
      if (i < p->relevance) sb_appendf(&sb, "*");
      else                  sb_appendf(&sb, " ");
    }
    sb_appendf(&sb, "  %04d   ", p->id);
    sb_appendf(&sb, PRI_SS, SS_FMT(p->title));
    print_line(stdout, ss_from_sb(sb));
  }

  return 0;
}

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

static void print_heading(FILE* stream, const char* heading) {
  fprintf(stream, "\n" ANSI_PRE ";" ANSI_BOLD ANSI_POST "%s" ANSI_PRE ANSI_NONE ANSI_POST "\n", heading);
}

static void print_line(FILE* stream, ss_t line) {
  fprintf(stream, "%*s" PRI_SS "\n", padding, "", SS_FMT(line));
}

static void print_paragraph(FILE* stream, ss_t par) {
  while (par.len) {
    ss_t line = ss_sub(par, 0, line_len);
    if (line.len >= line_len)
      while (line.len && !isspace(line.data[--line.len]));
    ss_advance_by_in_place(&par, line.len + 1);

    print_line(stream, line);
  }
}

int do_info(const char* progname, int argc, char** argv) {
  clear_arg_ctx();

  library_t library = { 0 };

  size_t id_arg    = arg("id",   .type = "int", .desc = "ID of the source in the <file>. Use list command to get list of IDs." );

  size_t file_opt  = arg("file", .opt = true, .type = "str", .desc = "Path to library file.", .default_val = "library.toml");
  size_t help_opt  = arg("help", .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exits with 0." );

  sb_t sb = { 0 };
  sb_appendf(&sb, "%s info", progname);
  sb_appendz(&sb, "");

  if (!arg_parse(&argc, &argv, -1)) {
    print_usage(stderr, sb.data);
    print_args(stderr);
    print_arg_last_error(stderr);
    return 1;
  }

  if (arg_present(help_opt)) {
    print_usage(stdout, sb.data);
    print_args(stdout);
    return 0;
  }

  int id = atoi(arg_value(id_arg));

  deserialize_library_from_file(arg_value(file_opt), &library);

  vec_foreach(s, &library.sources) {
    if (s->id == id) {
      print_heading(stdout, "TITLE");
      print_paragraph(stdout, s->title);

      print_heading(stdout, "AUTHORS");

      sb_t sb = { 0 };
      bool first = true;
      vec_foreach(a, &s->authors) {
        sb_appendf(&sb, "%s" PRI_SS, first ? "" : ", ", SS_FMT(*a));
        first = false;
      }
      print_paragraph(stdout, ss_from_sb(sb));
      sb_free(&sb);

      print_heading(stdout, "ABSTRACT");
      print_paragraph(stdout, s->abstract);

      print_heading(stdout, "PATH");
      print_line(stdout, s->path);

      return 0;
    }
  }

  sb_t err = { 0 };
  sb_appendf(&err, "No source found with id `%s` in library `" PRI_SS "`.", arg_value(id_arg), SS_FMT(library.name));
  print_heading(stderr, "ERROR");
  print_line(stderr, ss_from_sb(err));

  return 1;
}

int do_help(const char* progname, int argc, char** argv) {
  (void)argc;
  (void)argv;
  print_usage(stdout, progname);
  print_args(stdout);
  list_commands(stdout);
  return 0;
  return 0;
}

command_t commands[] = {
  COMMAND("list", do_list, "Lists all sources in a library."),
  COMMAND("info", do_info, "Displays informations on a source."),
  COMMAND("help", do_help, "Displays this page and exits with 0."),
};

void list_commands(FILE* stream) {
  print_heading(stream, "COMMANDS");
  for (size_t i=0; i < sizeof(commands)/sizeof(*commands); i++) {
    sb_t sb = { 0 };
    sb_appendf(&sb, "%-24s %s", commands[i].name, commands[i].desc);

    print_line(stream, ss_from_sb(sb));
  }
}

int main(int argc, char** argv) {
  const char* progname = arg_shift(&argc, &argv);

  size_t cmd_arg  = arg("command", .type = "str", .desc = "Command to be executed.");

  size_t help_opt  = arg("help", .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exits with 0." );

  if (!arg_parse(&argc, &argv, 1)) {
    print_usage(stdout, progname);
    print_args(stdout);
    list_commands(stderr);
    print_arg_last_error(stderr);
    return 1;
  }

  if (arg_present(help_opt)) {
    print_usage(stdout, progname);
    print_args(stdout);
    list_commands(stdout);
    return 0;
  }

  for (size_t i=0; i < sizeof(commands)/sizeof(*commands); i++) {
    if (strcmp(commands[i].name, arg_value(cmd_arg)) == 0) return commands[i].fn(progname, argc, argv);
  }

  fprintf(stderr, "Unrecognized command `%s`\n", arg_value(cmd_arg));
  list_commands(stderr);

  return 1;
}

#define VEC_IMPLEMENTATION
#include "efh/vec.h"
#define SS_IMPLEMENTATION
#include "efh/ss.h"
#define SB_IMPLEMENTATION
#include "efh/sb.h"
