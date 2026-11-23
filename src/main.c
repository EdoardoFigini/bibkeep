#include <stdio.h>
#include <stdbool.h>

#include "efh/vec.h"
#include "efh/ss.h"
#include "efh/sb.h"

#include "args.h"
#include "lib.h"
#include "display.h"
#include "macros.h"

#ifdef _WIN32
#pragma comment(lib, "user32")
#else
#error "Platform not supported"
#endif

void print_arg(FILE* stream, const arg_t* arg);
void print_usage(FILE* stream, const char* progname);
void print_usage_command(FILE* stream, const char* progname, const char* command);
void print_args(FILE* stream);
void print_arg_last_error(FILE* stream);

typedef struct {
  const char* name;
  int (*fn)(const char*, int, char**);
  const char* desc;
} command_t;

#define COMMAND(n, f, d) (command_t){ .name = n, .fn = f, .desc = d }

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
int do_list(const char* progname, int argc, char** argv) {
  arg_clear_ctx();

  library_t lib = { 0 };

  size_t lib_opt   = arg("lib",   .opt = true, .type = "str", .desc = "Path to library file.", .default_val = "library.toml");
  size_t ord_opt   = arg("sort",  .opt = true, .type = "str", .desc = "Sort sources by relevance. Available options: [NONE, ASC, DESC].", .default_val = "DESC");
  size_t paths_opt = arg("paths", .opt = true, .type = "bool", .no_value = true, .desc = "Displays paths instead of titles.");
  size_t help_opt  = arg("help",  .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exit with 0." );

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

  if(!deserialize_library_from_file(arg_value(lib_opt), &lib)) return 1;

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
    if (arg_present(paths_opt))
      sb_appendf(&sb, PRI_SS, SS_FMT(p->path));
    else
      sb_appendf(&sb, PRI_SS, SS_FMT(p->title));
    print_line(stdout, ss_from_sb(sb));
  }

  return 0;
}

int do_info(const char* progname, int argc, char** argv) {
  arg_clear_ctx();

  library_t library = { 0 };

  size_t id_arg    = arg("id",   .type = "int", .desc = "ID of the source in the library. Use list command to get list of IDs." );

  size_t lib_opt   = arg("lib", .opt = true, .type = "str", .desc = "Path to library file.", .default_val = "library.toml");
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

  if(!deserialize_library_from_file(arg_value(lib_opt), &library)) return 1;

  // NOTE: could I assume ids map to array indeces? lib.sources[id]
  vec_foreach(s, &library.sources) {
    if (s->id == id) {
      sb_t sb = { 0 };
      print_heading(stdout, "TITLE");
      print_paragraph(stdout, s->title);

      if (s->authors.len > 0) {
        print_heading(stdout, "AUTHORS");
        bool first = true;
        vec_foreach(a, &s->authors) {
          sb_appendf(&sb, "%s" PRI_SS, first ? "" : ", ", SS_FMT(*a));
          first = false;
        }
        print_paragraph(stdout, ss_from_sb(sb));
      }
      sb_reset(&sb);

      if (s->keywords.len > 0) {
        print_heading(stdout, "KEYWORDS");
        bool first = true;
        vec_foreach(a, &s->keywords) {
          sb_appendf(&sb, "%s" PRI_SS, first ? "" : ", ", SS_FMT(*a));
          first = false;
        }
        print_paragraph(stdout, ss_from_sb(sb));
      }
      sb_reset(&sb);

      if (s->abstract.len > 0) {
        print_heading(stdout, "ABSTRACT");
        print_paragraph(stdout, s->abstract);
      }

      if (s->path.len > 0) {
        print_heading(stdout, "PATH");
        print_line(stdout, s->path);
      }

      if (s->url.len > 0) {
        print_heading(stdout, "URL");
        print_line(stdout, s->url);
      }

      print_heading(stdout, "IDENTIFIERS");
      print_line_fmt(stdout, "Lib Id: %d", s->id);
      if (s->doi.len > 0)
        print_line_fmt(stdout, "DOI:    " PRI_SS, SS_FMT(s->doi));
      if (s->issn.len > 0)
        print_line_fmt(stdout, "ISSN:   " PRI_SS, SS_FMT(s->issn));

      sb_free(&sb);
      return 0;
    }
  }

  print_heading(stderr, "ERROR");
  print_line_fmt(stderr, "No source found with id `%s` in library `" PRI_SS "`.", arg_value(id_arg), SS_FMT(library.name));

  return 1;
}

// TODO: query language
// TODO: regex matching for titles/keyword/abstract
int do_search(const char* progname, int argc, char** argv) {
  arg_clear_ctx();

  library_t lib = { 0 };

  size_t kw_opt    = arg("kw",    .opt = true, .type = "str", .desc = "Keyword to look for." );
  size_t exact_opt = arg("exact", .opt = true, .type = "bool", .no_value = true, .desc = "Matches keword exactly.");
  size_t lib_opt   = arg("lib",  .opt = true, .type = "str", .desc = "Path to library file.", .default_val = "library.toml");
  size_t help_opt  = arg("help",  .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exits with 0." );

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

  if (!arg_present(kw_opt)) return 0;

  if(!deserialize_library_from_file(arg_value(lib_opt), &lib)) return 1;

  VEC(source_t) matches = { 0 };

  if (arg_present(exact_opt)) {
    vec_foreach(s, &lib.sources) {
      vec_foreach(kw, &s->keywords) {
        if (ss_eq_cstr(*kw, arg_value(kw_opt))) vec_push(&matches, *s);
      }
    }
  } else {
    vec_foreach(s, &lib.sources) {
      vec_foreach(kw, &s->keywords) {
        if (ss_index_of_cstr(*kw, arg_value(kw_opt)) < kw->len) vec_push(&matches, *s);
      }
    }
  }

  print_heading(stdout, "MATCHES");
  if (matches.len == 0) {
    print_line(stdout, ss_from_cstr("No matches found."));
    return 0;
  }

  print_line(stdout, ss_from_cstr("RELEV.   ID     TITLE"));
  vec_foreach(p, &matches) {
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

int do_add(const char* progname, int argc, char** argv) {
  arg_clear_ctx();

  library_t lib = { 0 };

  size_t lib_opt   = arg("lib",       .opt = true, .type = "str", .desc = "Path to library file.", .default_val = "library.toml");
  size_t title_opt = arg("title",     .opt = true, .type = "str", .desc = "Specify the source title.", .default_val = "");
  size_t path_opt  = arg("path",      .opt = true, .type = "str", .desc = "Specify the source path.", .default_val = "");
  size_t url_opt   = arg("url",       .opt = true, .type = "str", .desc = "Specify the source url.", .default_val = "");
  size_t doi_opt   = arg("doi",       .opt = true, .type = "str", .desc = "Specify the source doi.", .default_val = "");
  size_t issn_opt  = arg("issn",      .opt = true, .type = "str", .desc = "Specify the source issn.", .default_val = "");
  size_t abs_opt   = arg("abstract",  .opt = true, .type = "str", .desc = "Specify the source abstract.", .default_val = "");
  size_t rel_opt   = arg("relevance", .opt = true, .type = "int", .desc = "Specify the source relevance.", .default_val = "0");
  size_t year_opt  = arg("year",      .opt = true, .type = "int", .desc = "Specify the source year.", .default_val = "0");
  size_t type_opt  = arg("type",      .opt = true, .type = "str", .desc = "Specify the source type.", .default_val = "");
  size_t venue_opt = arg("venue",     .opt = true, .type = "str", .desc = "Specify the source venue.", .default_val = "");
  size_t help_opt  = arg("help",      .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exits with 0." );

  if (!arg_parse(&argc, &argv, -1)) {
    print_usage_command(stderr, progname, arg_progname());
    print_args(stderr);
    print_arg_last_error(stderr);
    return 1;
  }

  if (arg_present(help_opt)) {
    print_usage_command(stdout, progname, arg_progname());
    print_args(stdout);
    return 0;
  }

  if(!deserialize_library_from_file(arg_value(lib_opt), &lib)) return 1;

  source_t source = { 
    .id        = lib.sources.len + 1,
    .title     = ss_from_cstr(arg_value(title_opt)),
    .path      = ss_from_cstr(arg_value(path_opt)),
    .abstract  = ss_from_cstr(arg_value(abs_opt)),
    .url       = ss_from_cstr(arg_value(url_opt)),
    .doi       = ss_from_cstr(arg_value(doi_opt)),
    .issn      = ss_from_cstr(arg_value(issn_opt)),
    .relevance = atoi(arg_value(rel_opt)), 
    .year      = atoi(arg_value(year_opt)), 
    .venue     = ss_from_cstr(arg_value(venue_opt)),
    .type      = ss_from_cstr(arg_value(type_opt)),
  };

  vec_push(&lib.sources, source);

  return !serialize_library_to_file(arg_value(lib_opt), &lib);
}

static void bib_field(sb_t* sb, const char* name, ss_t value) {
  if (value.len > 0) sb_appendf(sb, "  %-12s = {" PRI_SS "},\n", name, SS_FMT(value));
}

static bool copy_to_clipboard(const sb_t* sb) {
#ifdef _WIN32
  int wlen = MultiByteToWideChar(CP_UTF8, 0, sb->data, (int)sb->len, NULL, 0);
  HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(WCHAR));
  if (!h) return false;
  WCHAR* w = GlobalLock(h);
  MultiByteToWideChar(CP_UTF8, 0, sb->data, (int)sb->len, w, wlen);
  w[wlen] = 0;
  GlobalUnlock(h);

  if (!OpenClipboard(NULL)) { GlobalFree(h); return false; }
  EmptyClipboard();
  bool ok = SetClipboardData(CF_UNICODETEXT, h) != NULL;
  CloseClipboard();
  if (!ok) GlobalFree(h);
  return ok;
#else
  (void)sb;
  return false;
#endif
}

int do_bibtex(const char* progname, int argc, char** argv) {
  arg_clear_ctx();

  library_t lib = { 0 };

  size_t id_arg   = arg("id",   .type = "int", .desc = "ID of the source in the library. Use list command to get list of IDs." );

  size_t lib_opt  = arg("lib",  .opt = true, .type = "str", .desc = "Path to library file.", .default_val = "library.toml");
  size_t out_opt  = arg("o",    .opt = true, .type = "str", .desc = "Writes the BibTeX entry to a file.");
  size_t clip_opt = arg("clip", .opt = true, .type = "bool", .no_value = true, .desc = "Copies the BibTeX entry to the clipboard.");
  size_t help_opt = arg("help", .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exits with 0." );

  if (!arg_parse(&argc, &argv, -1)) {
    print_usage_command(stderr, progname, arg_progname());
    print_args(stderr);
    print_arg_last_error(stderr);
    return 1;
  }

  if (arg_present(help_opt)) {
    print_usage_command(stdout, progname, arg_progname());
    print_args(stdout);
    return 0;
  }

  int id = atoi(arg_value(id_arg));

  if (!deserialize_library_from_file(arg_value(lib_opt), &lib)) return 1;

  vec_foreach(s, &lib.sources) {
    if (s->id != id) continue;

    sb_t sb = { 0 };
    sb_t list = { 0 };

    ss_t type = s->type.len > 0 ? s->type : ss_from_cstr("misc");
    sb_appendf(&sb, "@" PRI_SS "{src%04d,\n", SS_FMT(type), s->id);
    bib_field(&sb, "title", s->title);

    vec_foreach(a, &s->authors) {
      if (a->len == 0) continue;
      sb_appendf(&list, "%s" PRI_SS, list.len ? " and " : "", SS_FMT(*a));
    }
    bib_field(&sb, "author", ss_from_sb(list));
    sb_reset(&list);

    // the BibTeX field holding the venue depends on the entry type
    const char* venue_field = "howpublished";
         if (ss_eq_cstr(type, "article"))       venue_field = "journal";
    else if (ss_eq_cstr(type, "inproceedings") ||
             ss_eq_cstr(type, "incollection")  ||
             ss_eq_cstr(type, "conference"))    venue_field = "booktitle";
    else if (ss_eq_cstr(type, "book"))          venue_field = "publisher";
    bib_field(&sb, venue_field, s->venue);

    if (s->year > 0) sb_appendf(&sb, "  %-12s = {%d},\n", "year", s->year);

    bib_field(&sb, "doi",  s->doi);
    bib_field(&sb, "issn", s->issn);
    bib_field(&sb, "url",  s->url);

    vec_foreach(k, &s->keywords) {
      sb_appendf(&list, "%s" PRI_SS, list.len ? ", " : "", SS_FMT(*k));
    }
    bib_field(&sb, "keywords", ss_from_sb(list));
    sb_free(&list);

    sb_appendf(&sb, "}\n");

    print_heading(stdout, "BIBTEX");
    ss_t rest = ss_from_sb(sb);
    while (rest.len) print_line(stdout, ss_tok(&rest, '\n'));

    int ret = 0;

    if (arg_present(out_opt)) {
      if (sb_write_to_file(arg_value(out_opt), &sb) < 0) {
        print_heading(stderr, "ERROR");
        print_line_fmt(stderr, "Could not write to file `%s`.", arg_value(out_opt));
        ret = 1;
      } else {
        print_heading(stdout, "OUTPUT");
        print_line_fmt(stdout, "Written to `%s`.", arg_value(out_opt));
      }
    }

    if (arg_present(clip_opt)) {
      if (!copy_to_clipboard(&sb)) {
        print_heading(stderr, "ERROR");
        print_line(stderr, ss_from_cstr("Could not copy to clipboard."));
        ret = 1;
      } else {
        print_heading(stdout, "OUTPUT");
        print_line(stdout, ss_from_cstr("Copied to clipboard."));
      }
    }

    sb_free(&sb);
    return ret;
  }

  print_heading(stderr, "ERROR");
  print_line_fmt(stderr, "No source found with id `%s` in library `" PRI_SS "`.", arg_value(id_arg), SS_FMT(lib.name));

  return 1;
}

int do_edit(const char* progname, int argc, char** argv) {
  arg_clear_ctx();

  library_t lib = { 0 };

  size_t id_arg   = arg("id",   .type = "int", .desc = "ID of the source in the library. Use list command to get list of IDs." );

  size_t lib_opt   = arg("lib",       .opt = true, .type = "str", .desc = "Path to library file.", .default_val = "library.toml");
  size_t title_opt = arg("title",     .opt = true, .type = "str", .desc = "Specify the source title.", .default_val = "");
  size_t path_opt  = arg("path",      .opt = true, .type = "str", .desc = "Specify the source path.", .default_val = "");
  size_t url_opt   = arg("url",       .opt = true, .type = "str", .desc = "Specify the source url.", .default_val = "");
  size_t doi_opt   = arg("doi",       .opt = true, .type = "str", .desc = "Specify the source doi.", .default_val = "");
  size_t issn_opt  = arg("issn",      .opt = true, .type = "str", .desc = "Specify the source issn.", .default_val = "");
  size_t abs_opt   = arg("abstract",  .opt = true, .type = "str", .desc = "Specify the source abstract.", .default_val = "");
  size_t rel_opt   = arg("relevance", .opt = true, .type = "int", .desc = "Specify the source relevance.", .default_val = "0");
  size_t year_opt  = arg("year",      .opt = true, .type = "int", .desc = "Specify the source year.", .default_val = "0");
  size_t type_opt  = arg("type",      .opt = true, .type = "str", .desc = "Specify the source type.", .default_val = "");
  size_t venue_opt = arg("venue",     .opt = true, .type = "str", .desc = "Specify the source venue.", .default_val = "");
  size_t help_opt  = arg("help",      .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exits with 0." );

  if (!arg_parse(&argc, &argv, -1)) {
    print_usage_command(stderr, progname, arg_progname());
    print_args(stderr);
    print_arg_last_error(stderr);
    return 1;
  }

  if (arg_present(help_opt)) {
    print_usage_command(stdout, progname, arg_progname());
    print_args(stdout);
    return 0;
  }

  int id = atoi(arg_value(id_arg));

  if(!deserialize_library_from_file(arg_value(lib_opt), &lib)) return 1;

  vec_foreach(s, &lib.sources) {
    if (s->id != id) continue;

    if(arg_present(title_opt)) s->title     = ss_from_cstr(arg_value(title_opt));
    if(arg_present(path_opt))  s->path      = ss_from_cstr(arg_value(path_opt));
    if(arg_present(abs_opt))   s->abstract  = ss_from_cstr(arg_value(abs_opt));
    if(arg_present(url_opt))   s->url       = ss_from_cstr(arg_value(url_opt));
    if(arg_present(doi_opt))   s->doi       = ss_from_cstr(arg_value(doi_opt));
    if(arg_present(issn_opt))  s->issn      = ss_from_cstr(arg_value(issn_opt));
    if(arg_present(rel_opt))   s->relevance = atoi(arg_value(rel_opt)); 
    if(arg_present(year_opt))  s->year      = atoi(arg_value(year_opt)); 
    if(arg_present(venue_opt)) s->venue     = ss_from_cstr(arg_value(venue_opt));
    if(arg_present(type_opt))  s->type      = ss_from_cstr(arg_value(type_opt));

    return !serialize_library_to_file(arg_value(lib_opt), &lib);
  }

  print_heading(stderr, "ERROR");
  print_line_fmt(stderr, "No source found with id `%s` in library `" PRI_SS "`.", arg_value(id_arg), SS_FMT(lib.name));

  return 1;
}

int do_new(const char* progname, int argc, char** argv) {
  arg_clear_ctx();

  library_t lib = { 0 };

  size_t name_arg = arg("name", .type = "str", .desc = "Specify the name of the library.", .default_val = "My Library");

  size_t lib_opt   = arg("lib",  .opt = true, .type = "str", .desc = "Path to library file.", .default_val = "library.toml");
  size_t help_opt  = arg("help", .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exits with 0." );

  if (!arg_parse(&argc, &argv, -1)) {
    print_usage_command(stderr, progname, arg_progname());
    print_args(stderr);
    print_arg_last_error(stderr);
    return 1;
  }

  if (arg_present(help_opt)) {
    print_usage_command(stdout, progname, arg_progname());
    print_args(stdout);
    return 0;
  }

  lib.name = ss_from_cstr(arg_value(name_arg)); 

  return !serialize_library_to_file(arg_value(lib_opt), &lib);
}

void print_command_list(FILE* stream);

int do_help(const char* progname, int argc, char** argv) {
  (void)argc;
  (void)argv;
  print_usage(stdout, progname);
  print_args(stdout);
  print_command_list(stdout);
  return 0;
}

command_t commands[] = {
  COMMAND("list",   do_list,   "Lists all sources in a library."),
  COMMAND("info",   do_info,   "Displays informations on a source."),
  COMMAND("search", do_search, "Search for a source."),
  COMMAND("add",    do_add,    "Adds a source to a library."),
  COMMAND("edit",   do_edit,   "Edits a source."),
  COMMAND("new",    do_new,    "Creates a new library."),
  COMMAND("bibtex", do_bibtex, "Outputs a BibTeX entry for a source."),
  COMMAND("help",   do_help,   "Displays this page and exits with 0."),
};

void print_command_list(FILE* stream) {
  print_heading(stream, "COMMANDS");
  for (size_t i=0; i < sizeof(commands)/sizeof(*commands); i++) {
    print_line_fmt(stream, "%-24s %s", commands[i].name, commands[i].desc);
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
  vec_foreach(a, arg_pos_args()) {
    sb_appendf(&sb, "<%s> ", a->name);
  }
  sb_append(&sb, "[OPTIONS]");
  print_line(stream, ss_from_sb(sb));

  sb_free(&sb);
}

void print_usage_command(FILE* stream, const char* progname, const char* command) {
  print_heading(stream, "USAGE");

  sb_t sb = { 0 };
  sb_appendf(&sb, "%s %s ", progname, command);
  vec_foreach(a, arg_pos_args()) {
    sb_appendf(&sb, "<%s> ", a->name);
  }
  sb_append(&sb, "[OPTIONS]");
  print_line(stream, ss_from_sb(sb));

  sb_free(&sb);
}

void print_args(FILE* stream) {
  print_heading(stream, "ARGS");
  vec_foreach(a, arg_pos_args()) {
    print_arg(stream, a);
  }
  print_heading(stream, "OPTIONS");
  vec_foreach(a, arg_opt_args()) {
    print_arg(stream, a);
  }
}

void print_arg_last_error(FILE* stream) {
  print_heading(stream, "ERROR");
  print_line(stream, ss_from_cstr(arg_last_error_message()));
}

int main(int argc, char** argv) {
  size_t cmd_arg  = arg("command", .type = "str", .desc = "Command to be executed.");

  size_t help_opt  = arg("help", .opt = true, .type = "bool", .no_value = true, .desc = "Displays this page and exits with 0." );

  if (!arg_parse(&argc, &argv, 1)) {
    print_usage(stdout, arg_progname());
    print_args(stdout);
    print_command_list(stderr);
    print_arg_last_error(stderr);
    return 1;
  }

  const char* progname = arg_progname();

  if (arg_present(help_opt)) {
    print_usage(stdout, progname);
    print_args(stdout);
    print_command_list(stdout);
    return 0;
  }

  for (size_t i=0; i < sizeof(commands)/sizeof(*commands); i++) {
    if (strcmp(commands[i].name, arg_value(cmd_arg)) == 0) {
      // rewind parsed command
      return commands[i].fn(progname, argc + 1, argv - 1);
    }
  }

  fprintf(stderr, "Unrecognized command `%s`\n", arg_value(cmd_arg));
  print_command_list(stderr);

  return 1;
}

#define VEC_IMPLEMENTATION
#include "efh/vec.h"
#define SS_IMPLEMENTATION
#include "efh/ss.h"
#define SB_IMPLEMENTATION
#include "efh/sb.h"
