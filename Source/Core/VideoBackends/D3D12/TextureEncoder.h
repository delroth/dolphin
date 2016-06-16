// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"

namespace DX12
{
// Maximum number of bytes that can occur in a texture block-row generated by
// the encoder
static constexpr unsigned int MAX_BYTES_PER_BLOCK_ROW = (EFB_WIDTH / 4) * 64;
// The maximum amount of data that the texture encoder can generate in one call
static constexpr unsigned int MAX_BYTES_PER_ENCODE = MAX_BYTES_PER_BLOCK_ROW * (EFB_HEIGHT / 4);

class TextureEncoder
{
public:
  virtual ~TextureEncoder() {}
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  // Returns size in bytes of encoded block of memory
  virtual void Encode(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
                      u32 memory_stride, PEControl::PixelFormat src_format,
                      const EFBRectangle& src_rect, bool is_intensity, bool scale_by_half) = 0;
};
}
