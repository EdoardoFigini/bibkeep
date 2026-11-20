#ifndef LIB_H
#define LIB_H

#include "efh/vec.h"
#include "efh/ss.h"
#include "efh/sb.h"

typedef VEC(ss_t) ss_vec_t;

typedef struct {
  int id;
  ss_t title;
  ss_vec_t authors;
  ss_t path;
  ss_vec_t tags;
  ss_vec_t keywords;
  int relevance;
  ss_t doi;
  ss_t issn;
  ss_t url;
  ss_t abstract;
  int year;
  ss_t venue;
  ss_t type;
} source_t;

typedef struct {
  ss_t name;
  VEC(source_t) sources;
} library_t;

bool deserialize_library_from_file(const char* path, library_t* library);
bool serialize_library_to_file(const char* path, const library_t* library);

#endif
