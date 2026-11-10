#ifndef MAYHEM_TMPINPUT_H
#define MAYHEM_TMPINPUT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Writes the input to a mkstemp() file under $TMPDIR (default /tmp); returns 0 on success. */
static int writeTempInput(const uint8_t * data, size_t size, char * path, size_t pathSize)
{
  const char * dir = getenv("TMPDIR");
  if (!dir || !*dir)
    dir = "/tmp";
  snprintf(path, pathSize, "%s/lg-fuzz-XXXXXX", dir);
  int fd = mkstemp(path);
  if (fd < 0)
    return -1;

  size_t done = 0;
  while (done < size)
  {
    ssize_t n = write(fd, data + done, size - done);
    if (n <= 0)
    {
      close(fd);
      unlink(path);
      return -1;
    }
    done += (size_t)n;
  }
  close(fd);
  return 0;
}

#endif
