/* Fuzzes common/src/framebuffer.c: the KVMFR shared-memory frame reader/writer.
 *
 * Input layout (little endian):
 *   u8 op | u8 bpp | u16 width | u16 height | u16 pitch | u16 dstPitch | payload...
 * The payload fills the frame buffer; the rest of it is zero.
 *
 * Preconditions (from the library's contract, KVMFR.h / framebuffer.h):
 *  - frame->data is vector aligned: the host publishes frames at aligned offsets and the SSE4.1/AVX2
 *    writers use aligned stores, so the harness puts data on a 64 byte boundary.
 *  - the producer has finished: wp is set to the full frame, because waiting on a partial frame is a
 *    10ms+ spin that only a second thread could end.
 *  - the frame and the destination buffers are at least as large as the sizes handed to the functions.
 * Dimensions are rejected, not repaired, when the frame would exceed MAX_BYTES.
 */
#include "common/framebuffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BYTES ((size_t)3 << 20)

static uint16_t rd16(const uint8_t * p) { return (uint16_t)(p[0] | (p[1] << 8)); }

struct Sum { uint64_t sum; size_t rows; size_t stopAt; };

static bool sumRow(void * opaque, const void * src, size_t size)
{
  struct Sum * s = opaque;
  const uint8_t * b = src;
  for (size_t i = 0; i < size; ++i)
    s->sum += b[i];
  return ++s->rows != s->stopAt;
}

static void * alignedAlloc(size_t size)
{
  void * p = NULL;
  if (posix_memalign(&p, 64, size ? size : 1) != 0)
    abort();
  return p;
}

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
  if (size < 12)
    return -1;

  const unsigned op     = data[0] % 5;
  const size_t   bpp    = data[1] & 15;
  const size_t   width  = rd16(data + 2);
  const size_t   height = rd16(data + 4);
  const size_t   pitch  = rd16(data + 6);
  const size_t   dstPitch = rd16(data + 8);
  const uint8_t * payload = data + 10;
  const size_t   payloadSize = size - 10;

  if (height * pitch > MAX_BYTES || height * dstPitch > MAX_BYTES)
    return -1;

  const size_t srcBytes = height * pitch;
  const size_t dstBytes = height * dstPitch;

  /* data lands on a 64 byte boundary, wp sits in the 4 bytes before it */
  uint8_t * raw = alignedAlloc(64 + srcBytes);
  memset(raw, 0, 64 + srcBytes);
  KVMFRFrameBuffer * frame = (KVMFRFrameBuffer *)(raw + 64 - KVMFR_FRAMEBUFFER_WP_SIZE);
  memcpy(framebuffer_get_data(frame), payload, payloadSize < srcBytes ? payloadSize : srcBytes);
  framebuffer_set_write_ptr(frame, srcBytes);

  switch (op)
  {
    case 0:
    {
      uint8_t * dst = malloc(dstBytes ? dstBytes : 1);
      if (!dst)
        abort();
      framebuffer_read(frame, dst, dstPitch, height, width, bpp, pitch);
      uint64_t waited = 0;
      framebuffer_read_timed(frame, dst, dstPitch, height, width, bpp, pitch, &waited);
      free(dst);
      break;
    }

    case 1:
    {
      uint8_t * dst = malloc(srcBytes ? srcBytes : 1);
      if (!dst)
        abort();
      framebuffer_read_linear(frame, dst, srcBytes);
      if (srcBytes && memcmp(dst, framebuffer_get_buffer(frame), srcBytes) != 0)
        abort();
      free(dst);
      break;
    }

    case 2:
    {
      struct Sum s = { 0, 0, payload[0] % 4 ? 0 : 2 };
      framebuffer_read_fn(frame, height, width, bpp, pitch, sumRow, &s);
      break;
    }

    case 3:
    {
      uint8_t * src = alignedAlloc(srcBytes);
      memcpy(src, framebuffer_get_buffer(frame), srcBytes);
      framebuffer_prepare(frame);
      memset(framebuffer_get_data(frame), 0, srcBytes);
      framebuffer_write(frame, src, srcBytes);
      if (srcBytes && memcmp(src, framebuffer_get_buffer(frame), srcBytes) != 0)
        abort();
      free(src);
      break;
    }

    default:
    {
      framebuffer_wait(frame, srcBytes);
      framebuffer_wait_timed(frame, srcBytes / 2, NULL);
      framebuffer_wait(frame, (size_t)UINT_LEAST32_MAX + 1 + width);
      break;
    }
  }

  free(raw);
  return 0;
}
