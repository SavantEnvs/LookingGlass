/* Fuzzes the command line parser (option_parse) in common/src/option.c.
 * The input is split on newlines into argv[1..]. option_validate() is deliberately not called:
 * --help sets a process-global flag that option_free() never resets, so validating would let one
 * input change the behaviour of every later one. */
#include "generic_options.h"

#include "common/debug.h"

#include <stdint.h>
#include <stdio.h>

static FILE * g_null;

int LLVMFuzzerInitialize(int * argc, char *** argv)
{
  debug_init();
  g_null = fopen("/dev/null", "w");
  if (!g_null)
    abort();
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
  char * text = malloc(size + 1);
  if (!text)
    abort();
  memcpy(text, data, size);
  text[size] = '\0';

  int argc = 2;
  for (size_t i = 0; i < size; ++i)
    if (text[i] == '\n')
      ++argc;

  char ** argv = malloc(sizeof(*argv) * (argc + 1));
  if (!argv)
    abort();
  argv[0]   = "looking-glass-client";
  int count = 1;
  argv[count++] = text;
  for (char * p = text; *p; ++p)
    if (*p == '\n')
    {
      *p = '\0';
      argv[count++] = p + 1;
    }
  argv[count] = NULL;

  option_register(g_genericOptions);
  option_parse(count, argv);
  option_get_int("win", "width");
  option_get_string("win", "title");
  option_get_bool("win", "vsync");
  option_get_float("win", "scale");
  option_dump_preset(g_null);
  option_free();

  free(argv);
  free(text);
  return 0;
}
