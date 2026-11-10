# option_load: option name without `=` is not NUL-terminated

`common/src/option.c`, `option_load()` / `process_option_line()`.

The name buffer is NUL-terminated only when the `=` is seen (`name[nameLen] = '\0'`). A line that ends
(newline or EOF) after the name but before any `=` hands the unterminated heap buffer to
`process_option_line()`, which passes it to `option_get()` (`strcasecmp`) and to `DEBUG_WARN("%s")`.

Reproducer: `repro.ini` (`[win]` followed by `width` and no newline/`=`).
`fuzz_option_ini-standalone repro.ini` reports a heap-buffer-overflow read in `vfprintf` called from
`process_option_line` (option.c:489).

Impact: out-of-bounds heap read (information disclosure into the log / crash) from any ini file the
client loads (`/etc/looking-glass-client.ini`, `~/.looking-glass-client.ini`, `client.ini`, `app:configFile`).

One-line fix: in the `'\n'` case and after the loop, NUL-terminate `name` (`name[nameLen] = '\0'`) before
calling `process_option_line()`, or reject a line with no `=`.
