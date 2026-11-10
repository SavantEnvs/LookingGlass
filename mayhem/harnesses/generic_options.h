/* A representative option table covering every option type of common/option.c. */
#ifndef MAYHEM_GENERIC_OPTIONS_H
#define MAYHEM_GENERIC_OPTIONS_H

#include "common/option.h"

#include <stdlib.h>
#include <string.h>

static bool listParser(struct Option * opt, const char * str)
{
  if (!str)
    return false;
  opt->value.x_int = (int)strlen(str);
  return strchr(str, ',') != NULL;
}

static char * listToString(struct Option * opt)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "list:%d", opt->value.x_int);
  return strdup(buf);
}

static bool listValidator(struct Option * opt, const char ** error)
{
  if (opt->value.x_int >= 0)
    return true;
  *error = "negative";
  return false;
}

static struct Option g_genericOptions[] =
{
  {
    .module = "win", .name = "width", .description = "Window width",
    .shortopt = 'w', .type = OPTION_TYPE_INT, .value.x_int = 640
  },
  {
    .module = "win", .name = "title", .description = "Window title",
    .shortopt = 't', .type = OPTION_TYPE_STRING, .value.x_string = "default"
  },
  {
    .module = "win", .name = "vsync", .description = "Enable vsync",
    .shortopt = 'v', .type = OPTION_TYPE_BOOL, .value.x_bool = false
  },
  {
    .module = "win", .name = "scale", .description = "Scale factor",
    .shortopt = 's', .type = OPTION_TYPE_FLOAT, .value.x_float = 1.0f
  },
  {
    .module = "net", .name = "hosts", .description = "Host list",
    .shortopt = 'H', .type = OPTION_TYPE_CUSTOM,
    .parser = listParser, .toString = listToString, .validator = listValidator
  },
  {
    .module = "net", .name = "port", .description = "Port",
    .old_module = "network", .old_name = "portnum",
    .type = OPTION_TYPE_INT, .value.x_int = 5900, .preset = true
  },
  {
    .module = "net", .name = "host", .description = "Host name",
    .type = OPTION_TYPE_STRING, .value.x_string = "localhost", .preset = true
  },
  {0}
};

#endif
