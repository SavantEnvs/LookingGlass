/* Known-answer probes for the code the fuzz targets exercise. Built with normal flags and run by
 * mayhem/test.sh, which compares every printed line against its expected value. */
#include "generic_options.h"
#include "tmpinput.h"

#include "common/debug.h"
#include "common/framebuffer.h"
#include "common/rects.h"

#include <stdio.h>
#include <string.h>

static void ini(void)
{
  static const char text[] =
    "; comment\n"
    "[win]\n"
    "width = 800\n"
    "title=hello world  \n"
    "vsync=Yes\n"
    "scale=2.5\n"
    "[net]\n"
    "port=1234\n"
    "hosts=a,b\n"
    "unknown=1\n";

  char path[4096];
  if (writeTempInput((const uint8_t *)text, sizeof(text) - 1, path, sizeof(path)) != 0)
  {
    puts("ini: tempfile failed");
    return;
  }

  option_register(g_genericOptions);
  printf("ini load=%d\n", option_load(path));
  printf("ini validate=%d\n", option_validate());
  printf("ini width=%d title=[%s] vsync=%d scale=%.2f port=%d host=%s\n",
      option_get_int("win", "width"), option_get_string("win", "title"),
      option_get_bool("win", "vsync"), option_get_float("win", "scale"),
      option_get_int("net", "port"), option_get_string("net", "host"));
  option_dump_preset(stdout);
  option_free();
  unlink(path);
}

static void args(void)
{
  char * argv[] = { "prog", "win:width=1920", "-t", "My Title", "-v", "win:scale=0.5", "bogus", "net:port=77", NULL };
  option_register(g_genericOptions);
  option_parse(8, argv);
  printf("args width=%d title=[%s] vsync=%d scale=%.2f port=%d\n",
      option_get_int("win", "width"), option_get_string("win", "title"),
      option_get_bool("win", "vsync"), option_get_float("win", "scale"),
      option_get_int("net", "port"));
  option_free();
}

static void rects(void)
{
  KVMFRFrameDamageRect r[4] =
  {
    { .x =  0, .y =  0, .width = 10, .height = 10 },
    { .x =  5, .y =  5, .width = 10, .height = 10 },
    { .x =  2, .y =  2, .width =  3, .height =  3 },
    { .x = 40, .y = 40, .width =  4, .height =  4 },
  };
  int merged = rectsMergeOverlapping(r, 4);
  printf("rects merged=%d first=%u,%u,%u,%u second=%u,%u,%u,%u\n", merged,
      r[0].x, r[0].y, r[0].width, r[0].height, r[1].x, r[1].y, r[1].width, r[1].height);

  KVMFRFrameDamageRect c[3] =
  {
    { .x = 0, .y = 0, .width = 10, .height = 10 },
    { .x = 2, .y = 2, .width =  3, .height =  3 },
    { .x = 20, .y = 20, .width = 1, .height = 1 },
  };
  printf("rects kept=%d\n", rectsRejectContained(c, 3));

  /* 8x4 image, 1 byte per pixel; copy a 3x2 rectangle at (2,1) into the framebuffer */
  enum { W = 8, H = 4 };
  uint8_t src[W * H];
  for (int i = 0; i < W * H; ++i)
    src[i] = (uint8_t)(i + 1);

  uint8_t * raw = NULL;
  if (posix_memalign((void **)&raw, 64, 64 + W * H) != 0)
    return;
  memset(raw, 0, 64 + W * H);
  KVMFRFrameBuffer * fb = (KVMFRFrameBuffer *)(raw + 64 - KVMFR_FRAMEBUFFER_WP_SIZE);
  KVMFRFrameDamageRect d = { .x = 2, .y = 1, .width = 3, .height = 2 };
  framebuffer_prepare(fb);
  printf("rects tofb=%d\n", rectsBufferToFramebuffer(&d, 1, 1, fb, W, H, src, W));
  const uint8_t * p = framebuffer_get_buffer(fb);
  unsigned sum = 0;
  for (int i = 0; i < W * H; ++i)
    sum += p[i];
  printf("rects fb row1=%u,%u,%u,%u,%u,%u row2=%u,%u,%u sum=%u\n",
      p[W + 1], p[W + 2], p[W + 3], p[W + 4], p[W + 5], p[W + 6], p[2 * W + 2], p[2 * W + 4], p[2 * W + 5], sum);

  KVMFRFrameDamageRect bad = { .x = 7, .y = 0, .width = 2, .height = 1 };
  printf("rects oob=%d\n", rectsBufferToFramebuffer(&bad, 1, 1, fb, W, H, src, W));

  uint8_t dst[W * H];
  memset(dst, 0, sizeof(dst));
  printf("rects fromfb=%d\n", rectsFramebufferToBuffer(&d, 1, 1, dst, W, H, fb, W));
  printf("rects dst row1=%u,%u,%u,%u row2=%u,%u,%u,%u\n",
      dst[W + 1], dst[W + 2], dst[W + 4], dst[W + 5], dst[2 * W + 2], dst[2 * W + 4], dst[2 * W + 5], dst[3 * W + 2]);
  free(raw);
}

static void fb(void)
{
  enum { W = 4, H = 3, BPP = 2, PITCH = 12 };
  uint8_t src[PITCH * H];
  for (int i = 0; i < PITCH * H; ++i)
    src[i] = (uint8_t)(i * 3);

  uint8_t * raw = NULL;
  if (posix_memalign((void **)&raw, 64, 64 + sizeof(src)) != 0)
    return;
  KVMFRFrameBuffer * frame = (KVMFRFrameBuffer *)(raw + 64 - KVMFR_FRAMEBUFFER_WP_SIZE);
  uint8_t * s = NULL;
  if (posix_memalign((void **)&s, 64, sizeof(src)) != 0)
    return;
  memcpy(s, src, sizeof(src));

  framebuffer_prepare(frame);
  printf("fb write=%d\n", framebuffer_write(frame, s, sizeof(src)));
  printf("fb wait=%d toobig=%d\n", framebuffer_wait(frame, sizeof(src)), framebuffer_wait(frame, (size_t)UINT_LEAST32_MAX + 1));

  uint8_t dst[W * BPP * H];
  memset(dst, 0xff, sizeof(dst));
  printf("fb read=%d\n", framebuffer_read(frame, dst, W * BPP, H, W, BPP, PITCH));
  printf("fb dst=%u,%u,%u,%u,%u,%u\n", dst[0], dst[7], dst[8], dst[15], dst[16], dst[23]);
  printf("fb bad=%d\n", framebuffer_read(frame, dst, W * BPP, H, W + 2, BPP, PITCH));
  free(s);
  free(raw);
}

int main(void)
{
  debug_init();
  ini();
  args();
  rects();
  fb();
  return 0;
}
