#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>

#include "efh/vec.h"

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

typedef VEC(arg_t) arg_vec_t;

typedef struct {
  const char* prog_name;
  arg_vec_t pos;
  arg_vec_t opt;
  const char* last_error_message;
} arg_ctx_t;

struct _arg_def {
  const char* desc;
  const char* type;
  bool no_value;
  bool opt;
  const char* default_val;
};

#define POS_BIT ((size_t)1 << ((sizeof(size_t) * 8) - 1))

#define arg(name, ...) __arg_impl(name, (struct _arg_def){ __VA_ARGS__  }) 
size_t __arg_impl(const char* name, struct _arg_def arg_def);

void        arg_clear_ctx();
size_t      arg_pos_count();
const char* arg_value(size_t arg_idx);
bool        arg_present(size_t arg_idx);
arg_vec_t*  arg_pos_args();
arg_vec_t*  arg_opt_args();
const char* arg_last_error_message();
const char* arg_progname();
bool        arg_parse(int* argc, char*** argv, int limit);

#endif
