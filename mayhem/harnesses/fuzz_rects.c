/* Fuzzes common/src/rects.c: guest-supplied damage rectangle merging and the rectangle-driven
 * framebuffer copies.
 *
 * Input layout (little endian):
 *   u8 op | u8 count | u8 bpp | u8 pad | u16 height | u16 pitch | count * {u32 x,y,w,h}... | payload
 * The payload fills the source image; the rest of it is zero.
 *
 * Preconditions (from rects.h / KVMFR.h): count is 1..KVMFR_MAX_DAMAGE_RECTS (the protocol's array
 * size; rectsRejectContained sizes a VLA by it), the buffers hold at least height*pitch bytes, and
 * frame->data is vector aligned (see fuzz_framebuffer.c). The rectangles themselves are NOT
 * validated here: bounds-checking untrusted rectangles is the library's job. The producer is
 * finished (wp = full frame). Images larger than MAX_BYTES are rejected.
 */
#include "common/rects.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BYTES ((size_t)1 << 20)

static uint16_t rd16(const uint8_t * p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t * p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
  if (size < 8)
    return -1;

  const unsigned op     = data[0] % 4;
  const int      count  = data[1];
  const int      bpp    = data[2];
  const int      height = rd16(data + 4);
  const int      pitch  = rd16(data + 6);

  if (count < 1 || count > KVMFR_MAX_DAMAGE_RECTS)
    return -1;
  if (size < 8 + (size_t)count * 16)
    return -1;
  if ((size_t)height * (size_t)pitch > MAX_BYTES)
    return -1;

  KVMFRFrameDamageRect rects[KVMFR_MAX_DAMAGE_RECTS];
  for (int i = 0; i < count; ++i)
  {
    const uint8_t * p = data + 8 + i * 16;
    rects[i].x      = rd32(p);
    rects[i].y      = rd32(p + 4);
    rects[i].width  = rd32(p + 8);
    rects[i].height = rd32(p + 12);
  }

  const uint8_t * payload     = data + 8 + (size_t)count * 16;
  const size_t    payloadSize = size - 8 - (size_t)count * 16;
  const size_t    bytes       = (size_t)height * (size_t)pitch;

  if (op == 0)
  {
    int merged = rectsMergeOverlapping(rects, count);
    if (merged < 1 || merged > count)
      abort();
    int kept = rectsRejectContained(rects, merged);
    if (kept < 1 || kept > merged)
      abort();
    return 0;
  }

  uint8_t * raw = NULL;
  if (posix_memalign((void **)&raw, 64, 64 + (bytes ? bytes : 1)) != 0)
    abort();
  memset(raw, 0, 64 + bytes);
  KVMFRFrameBuffer * frame = (KVMFRFrameBuffer *)(raw + 64 - KVMFR_FRAMEBUFFER_WP_SIZE);
  uint8_t * buf = calloc(bytes ? bytes : 1, 1);
  if (!buf)
    abort();
  memcpy(buf, payload, payloadSize < bytes ? payloadSize : bytes);

  switch (op)
  {
    case 1:
      framebuffer_prepare(frame);
      rectsBufferToFramebuffer(rects, count, bpp, frame, pitch, height, buf, pitch);
      break;

    case 2:
      memcpy(framebuffer_get_data(frame), buf, bytes);
      framebuffer_set_write_ptr(frame, bytes);
      memset(buf, 0, bytes);
      rectsFramebufferToBuffer(rects, count, bpp, buf, pitch, height, frame, pitch);
      break;

    default:
    {
      /* distinct source and destination strides, derived from the pitch */
      const int srcPitch = pitch + (data[3] & 63);
      const size_t srcBytes = (size_t)height * (size_t)srcPitch;
      if (srcBytes > MAX_BYTES + ((size_t)height << 6))
        break;
      uint8_t * src = calloc(srcBytes ? srcBytes : 1, 1);
      if (!src)
        abort();
      memcpy(src, payload, payloadSize < srcBytes ? payloadSize : srcBytes);
      framebuffer_prepare(frame);
      rectsBufferToFramebuffer(rects, count, bpp, frame, pitch, height, src, srcPitch);
      free(src);
      break;
    }
  }

  free(buf);
  free(raw);
  return 0;
}
