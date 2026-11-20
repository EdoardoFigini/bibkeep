#include "lib.h"

#include "display.h"

static ss_t parse_field_str(ss_t src) {
  src = ss_trim(ss_advance_after(src, "="));
  return ss_sub(src, 1, src.len -1); //trim quotes
}

static int parse_field_int(ss_t src) {
  src = ss_trim(ss_advance_after(src, "="));
  return ss_atoi(src);
}

static bool parse_field_bool(ss_t src) {
  src = ss_trim(ss_advance_after(src, "="));
  return ss_eq_cstr(src, "true");
}

static void parse_field_str_array(ss_t* src, ss_vec_t* out) {
  ss_t s = ss_advance_after(*src, "[");

  while (s.len && *s.data != ']') {
    char q = *s.data;
    if (q == '"' || q == '\'') {
      ss_advance_by_in_place(&s, 1);
      const char* start = s.data;
      while (s.len && *s.data != q) {
        ss_advance_by_in_place(&s, (q == '"' && *s.data == '\\' && s.len > 1) ? 2 : 1);
      }
      vec_push(out, ss_from_extents(start, s.data));
    }
    if (s.len) ss_advance_by_in_place(&s, 1);
  }

  if (s.len) ss_advance_by_in_place(&s, 1);
  *src = s;
}

#define TRY_PARSE(src, name, type) \
  if (ss_starts_with_cstr(src, # name)) { source->name = parse_field_ ## type(src); continue; }

#define TRY_PARSE_ARRAY(line, src, name) \
  if (ss_starts_with_cstr(line, # name)) { \
    *src = ss_from_extents(line.data, src->data + src->len); \
    parse_field_str_array(src, &source->name); \
    continue; \
  }

static bool deserialize_source(ss_t* src, source_t* source) {
  while(src->len) {
    ss_t prev = *src;
    ss_t line = ss_trim(ss_tok(src, '\n'));

    TRY_PARSE(line, id,         int);
    TRY_PARSE(line, title,      str);
    TRY_PARSE(line, path,       str);
    TRY_PARSE(line, relevance,  int);
    TRY_PARSE(line, abstract,   str);
    TRY_PARSE(line, doi,        str);
    TRY_PARSE(line, issn,       str);
    TRY_PARSE(line, url,        str);
    TRY_PARSE(line, year,       int);
    TRY_PARSE(line, venue,      str);
    TRY_PARSE(line, type,       str);

    TRY_PARSE_ARRAY(line, src, tags);
    TRY_PARSE_ARRAY(line, src, keywords);
    TRY_PARSE_ARRAY(line, src, authors);

    // found next table
    if (ss_starts_with_cstr(line, "[")) {
      *src = prev; // rewind;
      return true;
    }
  }

  return true;
}

static bool deserialize_library(ss_t src, library_t* library) {
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

  if (sb_read_file(path, &sb) < 0) {
    print_heading(stderr, "ERROR");
    print_line_fmt(stderr, "Could not read file `%s`", path);
    return false;
  }

  return deserialize_library(ss_from_sb(sb), library);
}

static void serialize_source(sb_t* sb, const source_t* source) {
  sb_appendf(sb, "[[source]]\n");

  sb_appendf(sb, "id = %d\n", source->id);
  sb_appendf(sb, "title = \"" PRI_SS "\"\n", SS_FMT(source->title));
  sb_appendf(sb, "authors = ["); {
    bool first = true;
    vec_foreach(author, &source->authors) {
      sb_appendf(sb, "%s\"" PRI_SS "\"", first ? "" : ", ", SS_FMT(*author));
      first = false;
    }
    sb_appendf(sb, "]\n");
  }
  sb_appendf(sb, "path = \""  PRI_SS "\"\n", SS_FMT(source->path));
  sb_appendf(sb, "relevance = %d\n", source->relevance);
  sb_appendf(sb, "tags = ["); {
    bool first = true;
    vec_foreach(tag, &source->tags) {
      sb_appendf(sb, "%s\"" PRI_SS "\"", first ? "" : ", ", SS_FMT(*tag));
      first = false;
    }
    sb_appendf(sb, "]\n");
  }
  sb_appendf(sb, "keywords = ["); {
    bool first = true;
    vec_foreach(kw, &source->keywords) {
      sb_appendf(sb, "%s\"" PRI_SS "\"", first ? "" : ", ", SS_FMT(*kw));
      first = false;
    }
    sb_appendf(sb, "]\n");
  }
  sb_appendf(sb, "doi = \"" PRI_SS "\"\n", SS_FMT(source->doi));
  sb_appendf(sb, "issn = \"" PRI_SS "\"\n", SS_FMT(source->issn));
  sb_appendf(sb, "url = \"" PRI_SS "\"\n", SS_FMT(source->url));
  sb_appendf(sb, "abstract = \"" PRI_SS "\"\n", SS_FMT(source->abstract));

  sb_appendf(sb, "year = %d\n", source->year);
  sb_appendf(sb, "type = \"" PRI_SS "\"\n", SS_FMT(source->type));
  sb_appendf(sb, "venue = \"" PRI_SS "\"\n", SS_FMT(source->venue));
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
