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

void display_list(FILE* stream, library_t lib, enum order_by order_by) {
  fprintf(stream, "Listing sources of library: \"" PRI_SS "\"\n", SS_FMT(lib.name));

  switch (order_by) {
    case ORD_NONE: break;
    case ORD_REL_DESC: vec_sort(&lib.sources, ord_cmp_desc); break;
    case ORD_REL_ASC:  vec_sort(&lib.sources, ord_cmp_asc); break;
    default:
      UNREACHABLE("Unknown display_list order by");
  }

  vec_foreach(p, &lib.sources) {
    fprintf(stream, "[%d/5] (%04d) \"" PRI_SS "\"\n", p->relevance, p->id, SS_FMT(p->title));
  }
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

  if (arg->opt) sb_appendf(&sb, "  " OPT_ARG_PREFIX "%-13s ", arg->name);
  else          sb_appendf(&sb, "  %-15s ", arg->name);

  sb_appendf(&sb, "%-8s %-30s ", arg->no_value ? "" : arg->type, arg->desc);
  if (arg->default_val)
    sb_appendf(&sb, "Default: %s.", arg->default_val);

  fprintf(stream, PRI_SB "\n", SB_FMT(sb));
}

void print_args(FILE* stream) {
  fprintf(stream, "ARGS:\n");
  vec_foreach(a, &g_arg_ctx.pos) {
    print_arg(stream, a);
  }
  fprintf(stream, "OPTIONS:\n");
  vec_foreach(a, &g_arg_ctx.opt) {
    print_arg(stream, a);
  }
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

bool arg_parse(int* argc, char*** argv) {
  sb_t sb = { 0 };

  if (*argc < arg_pos_count()) {
    sb_appendf(&sb, "ERROR: Not enough positional arguments passed.");
    sb_appendz(&sb, "");
    g_arg_ctx.last_error_message = sb.data;
    return false;
  }

  size_t positional = 0;
  while (*argc) {
    const char* arg = arg_shift(argc, argv);

    if (*arg != *OPT_ARG_PREFIX) {
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
              sb_appendf(&sb, "ERROR: No value provided for `" OPT_ARG_PREFIX "%s` option.", a->name);
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

      sb_appendf(&sb, "ERROR: Unrecognized optional argument `%s`", arg);
      sb_appendz(&sb, "");
      g_arg_ctx.last_error_message = sb.data;
      return false;
    }
  }

  if (positional + 1 < arg_pos_count()) {
    sb_appendf(&sb, "ERROR: Not enough positional arguments passed.");
    sb_appendz(&sb, "");
    g_arg_ctx.last_error_message = sb.data;
    return false;
  }

  return true;
}

int do_list(const char* progname, int argc, char** argv) {
  library_t library = { 0 };

  size_t file_arg  = arg("file", .type = "str", .desc = "Path to library file.");

  size_t help_opt  = arg("help", .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exit with 0." );
  size_t ord_opt   = arg("ord", .opt = true, .type = "str", .desc = "Order sources by relevance. Available options: [NONE, ASC, DESC].", .default_val = "DESC");

  if (!arg_parse(&argc, &argv)) {
    fprintf(stderr, "USAGE: %s list <file> [OPTIONS]\n", progname);
    print_args(stderr);
    fprintf(stderr, "%s\n", g_arg_ctx.last_error_message);
    return 1;
  }

  if (arg_present(help_opt)) {
    fprintf(stdout, "USAGE: %s list <file> [OPTIONS]\n", progname);
    print_args(stdout);
    return 0;
  }

  deserialize_library_from_file(arg_value(file_arg), &library);

  enum order_by ob = ORD_NONE;

  const char* ord = arg_value(ord_opt);

       if (strcmp(ord, "ASC") == 0)  ob = ORD_REL_ASC; 
  else if (strcmp(ord, "DESC") == 0) ob = ORD_REL_DESC; 

  display_list(stdout, library, ob);

  return 0;
}

int do_info(const char* progname, int argc, char** argv) {
  library_t library = { 0 };

  size_t file_arg  = arg("file", .type = "str", .desc = "Path to library file.");
  size_t id_arg    = arg("id",   .type = "int", .desc = "ID of the source in the <file>. Use list command to get list of IDs." );

  size_t help_opt  = arg("help", .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exit with 0." );

  if (!arg_parse(&argc, &argv)) {
    fprintf(stderr, "USAGE: %s info <file> <id> [OPTIONS]\n", progname);
    print_args(stderr);
    fprintf(stderr, "%s\n", g_arg_ctx.last_error_message);
    return 1;
  }

  if (arg_present(help_opt)) {
    fprintf(stdout, "USAGE: %s info <file> <id> [OPTIONS]\n", progname);
    print_args(stdout);
    return 0;
  }

  int id = atoi(arg_value(id_arg));

  deserialize_library_from_file(arg_value(file_arg), &library);

  vec_foreach(s, &library.sources) {
    if (s->id == id) {
      fprintf(stdout, "Title:   \"" PRI_SS "\"\n", SS_FMT(s->title));

      fprintf(stderr, "Authors: ");
      bool first = true;
      vec_foreach(a, &s->authors) {
        fprintf(stdout, "%s" PRI_SS, first ? "" : ", ", SS_FMT(*a));
        first = false;
      }
      fprintf(stdout, "\n");
      return 0;
    }
  }

  fprintf(stderr, "ERROR: No source found with id `%s` in library `%s`.\n", arg_value(id_arg), arg_value(file_arg));

  return 1;
}

int do_help(const char* progname, int argc, char** argv) {
  (void)argc;
  (void)argv;
  fprintf(stderr, "USAGE: %s <command>\n", progname);
  list_commands(stdout);
  return 0;
}

command_t commands[] = {
  COMMAND("list", do_list, "Lists all sources in a library."),
  COMMAND("info", do_info, "Displays informations on a source."),
  COMMAND("help", do_help, "Displays this page and exits with 0."),
};

void list_commands(FILE* stream) {
  fprintf(stream, "List of available commands:\n");
  for (size_t i=0; i < sizeof(commands)/sizeof(*commands); i++) {
    fprintf(stream, "  %-15s %s\n", commands[i].name, commands[i].desc);
  }
}

int main(int argc, char** argv) {
  const char* progname = arg_shift(&argc, &argv);

  if (argc < 1) {
    fprintf(stderr, "USAGE: %s <command>\n", progname);
    fprintf(stderr, "Missing command\n");
    list_commands(stderr);
    return 1;
  }

  const char* cmd = arg_shift(&argc, &argv); 

  for (size_t i=0; i < sizeof(commands)/sizeof(*commands); i++) {
    if (strcmp(commands[i].name, cmd) == 0) return commands[i].fn(progname, argc, argv);
  }

  fprintf(stderr, "Unrecognized command `%s`\n", cmd);
  list_commands(stderr);

  return 1;
}

#define VEC_IMPLEMENTATION
#include "efh/vec.h"
#define SS_IMPLEMENTATION
#include "efh/ss.h"
#define SB_IMPLEMENTATION
#include "efh/sb.h"
