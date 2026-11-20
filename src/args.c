#include "args.h"

#include "macros.h"

#include "efh/sb.h"

static char* arg_shift(int *argc, char ***argv) {
    ASSERT(*argc > 0, "Not enough arguments");
    char *result = **argv;
    *argv += 1;
    *argc -= 1;
    return result;
}

static arg_ctx_t g_arg_ctx = { 0 };

void arg_clear_ctx() {
  vec_reset(&g_arg_ctx.pos);
  vec_reset(&g_arg_ctx.opt);
  g_arg_ctx.last_error_message = NULL;
  g_arg_ctx.prog_name = NULL;
}

arg_vec_t* arg_pos_args() {
  return &g_arg_ctx.pos;
}

arg_vec_t* arg_opt_args() {
  return &g_arg_ctx.opt;
}

const char* arg_last_error_message() {
  return g_arg_ctx.last_error_message;
}

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

const char* arg_progname() {
  return g_arg_ctx.prog_name;
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

  g_arg_ctx.prog_name = arg_shift(argc, argv);

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
