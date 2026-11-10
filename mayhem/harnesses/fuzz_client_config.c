/* Fuzzes the Looking Glass client's real command line / option table (client/src/config.c): the
 * option parsers (renderer, window position/size, scancodes, rotation, transport, mic, resampler)
 * and validators, via config_load().
 *
 * The input is split on newlines into argv[1..]. Rejected (return -1) rather than repaired:
 *  - any argument naming app:configFile or -C: it makes config_load read an arbitrary path;
 *  - -h / --help / --rst-help: option.c keeps the help mode in a process-global that option_free()
 *    never resets, so one such input would short-circuit validation for all later inputs.
 * config_load() also reads /etc/looking-glass-client.ini and the user's own ini files, which do not
 * exist in the build environment.
 */
#include "main.h"
#include "config.h"

#include "common/debug.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int LLVMFuzzerInitialize(int * argc, char *** argv)
{
  debug_init();
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
  char * text = malloc(size + 1);
  if (!text)
    abort();
  memcpy(text, data, size);
  text[size] = '\0';

  int count = 2;
  for (size_t i = 0; i < size; ++i)
    if (text[i] == '\n')
      ++count;

  char ** argv = malloc(sizeof(*argv) * (count + 1));
  if (!argv)
    abort();
  int argc = 1;
  argv[0]  = "looking-glass-client";
  argv[argc++] = text;
  for (char * p = text; *p; ++p)
    if (*p == '\n')
    {
      *p = '\0';
      argv[argc++] = p + 1;
    }
  argv[argc] = NULL;

  bool rejected = false;
  for (int i = 1; i < argc; ++i)
    if (strcasestr(argv[i], "configfile") || strcmp(argv[i], "-C") == 0 ||
        strcasecmp(argv[i], "-h") == 0 || strcasecmp(argv[i], "--help") == 0 ||
        strcasecmp(argv[i], "--rst-help") == 0)
      rejected = true;

  if (!rejected)
  {
    config_init();
    config_load(argc, argv);
    config_free();
  }

  free(argv);
  free(text);
  return rejected ? -1 : 0;
}
