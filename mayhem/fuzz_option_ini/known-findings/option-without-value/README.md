# option_load: `name=` with an empty value passes NULL to the default parsers

`common/src/option.c`: `int_parser`, `bool_parser`, `float_parser` (and custom parsers that do not
check) are called with `str == NULL` when an ini line has a name and `=` but no value, because
`option_load()` only allocates `value` once a value character is seen.

Reproducer: `repro.ini` (`[win]` / `width=`). `fuzz_option_ini-standalone repro.ini` stops with a
UBSan null-pointer error at option.c:60 (`atol(NULL)`; `strcasecmp(NULL, ...)` in `bool_parser`).

Impact: a crash (denial of service) while the client loads any ini file that contains an empty value
for an int, bool or float option.

One-line fix: in `option_set()` (or `process_option_line()`) reject a NULL value for every option type
except `OPTION_TYPE_CUSTOM` (which already handles NULL) and for `OPTION_TYPE_BOOL` treat it as "toggle".
