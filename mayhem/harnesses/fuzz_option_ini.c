/* Fuzzes the INI configuration loader and validator in common/src/option.c. */
#include "generic_options.h"
#include "tmpinput.h"

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
  char path[4096];
  if (writeTempInput(data, size, path, sizeof(path)) != 0)
    abort();

  /* Fresh option state for every input: register, load, validate, dump, free. */
  option_register(g_genericOptions);
  option_load(path);
  option_validate();
  option_dump_preset(g_null);
  option_free();

  unlink(path);
  return 0;
}
