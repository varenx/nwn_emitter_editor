/*
 * This file is part of NWN Emitter Editor.
 * Copyright (C) 2025 Varenx
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "stb_dds.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// DDS constants
#define DDS_MAGIC 0x20534444
#define FOURCC_DXT1 0x31545844
#define FOURCC_DXT3 0x33545844
#define FOURCC_DXT5 0x35545844

// Bioware DDS header structure
struct BioDDSHeader
{
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    uint32_t linearSize;
    float alphaPremultiplier;
};

// Standard DDS structures
struct DDSPixelFormat
{
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
};

struct DDSHeader
{
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDSPixelFormat ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};

static void rgb565_to_rgb888(uint16_t color565, uint8_t* rgb)
{
    rgb[0] = ((color565 >> 11) & 0x1F) << 3;// Red: 5 bits -> 8 bits
    rgb[1] = ((color565 >> 5) & 0x3F) << 2; // Green: 6 bits -> 8 bits
    rgb[2] = (color565 & 0x1F) << 3;        // Blue: 5 bits -> 8 bits
}

static void decompress_dxt1_block(const uint8_t* block, uint8_t* output, int width, int channels)
{
    // Read color endpoints
    uint16_t color0 = block[0] | (block[1] << 8);
    uint16_t color1 = block[2] | (block[3] << 8);

    // Convert RGB565 to RGB888
    uint8_t colors[4][3];
    rgb565_to_rgb888(color0, colors[0]);
    rgb565_to_rgb888(color1, colors[1]);

    // Interpolate colors
    if (color0 > color1)
    {
        // 4-color mode
        colors[2][0] = (2 * colors[0][0] + colors[1][0]) / 3;
        colors[2][1] = (2 * colors[0][1] + colors[1][1]) / 3;
        colors[2][2] = (2 * colors[0][2] + colors[1][2]) / 3;

        colors[3][0] = (colors[0][0] + 2 * colors[1][0]) / 3;
        colors[3][1] = (colors[0][1] + 2 * colors[1][1]) / 3;
        colors[3][2] = (colors[0][2] + 2 * colors[1][2]) / 3;
    } else
    {
        // 3-color mode with transparent black
        colors[2][0] = (colors[0][0] + colors[1][0]) / 2;
        colors[2][1] = (colors[0][1] + colors[1][1]) / 2;
        colors[2][2] = (colors[0][2] + colors[1][2]) / 2;

        colors[3][0] = colors[3][1] = colors[3][2] = 0;// Transparent black
    }

    // Read color indices (2 bits per pixel)
    uint32_t indices = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);

    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            int pixelIndex = y * 4 + x;
            int colorIndex = (indices >> (pixelIndex * 2)) & 0x3;

            int outputIndex = (y * width + x) * channels;
            if (outputIndex >= 0)
            {
                output[outputIndex + 0] = colors[colorIndex][0];// R
                output[outputIndex + 1] = colors[colorIndex][1];// G
                output[outputIndex + 2] = colors[colorIndex][2];// B
                if (channels == 4)
                {
                    output[outputIndex + 3] = (color0 > color1 || colorIndex < 3) ? 255 : 0;// A
                }
            }
        }
    }
}

static void decompress_dxt5_block(const uint8_t* block, uint8_t* output, int width)
{
    // DXT5 has interpolated alpha (first 8 bytes) + DXT1 color (last 8 bytes)

    // Read alpha endpoints
    uint8_t alpha0 = block[0];
    uint8_t alpha1 = block[1];

    // Read alpha indices (3 bits per pixel)
    uint64_t alphaIndices = 0;
    for (int i = 2; i < 8; ++i)
    {
        alphaIndices |= static_cast<uint64_t>(block[i]) << ((i - 2) * 8);
    }

    // Generate alpha palette
    uint8_t alphas[8];
    alphas[0] = alpha0;
    alphas[1] = alpha1;

    if (alpha0 > alpha1)
    {
        // 8-alpha mode
        for (int i = 1; i < 7; ++i)
        {
            alphas[i + 1] = ((7 - i) * alpha0 + i * alpha1) / 7;
        }
    } else
    {
        // 6-alpha mode + transparent endpoints
        for (int i = 1; i < 5; ++i)
        {
            alphas[i + 1] = ((5 - i) * alpha0 + i * alpha1) / 5;
        }
        alphas[6] = 0;  // Transparent
        alphas[7] = 255;// Opaque
    }

    // Read color data (last 8 bytes)
    const uint8_t* colorBlock = block + 8;
    uint16_t color0 = colorBlock[0] | (colorBlock[1] << 8);
    uint16_t color1 = colorBlock[2] | (colorBlock[3] << 8);

    // Convert colors
    uint8_t colors[4][3];
    rgb565_to_rgb888(color0, colors[0]);
    rgb565_to_rgb888(color1, colors[1]);

    // Always use 4-color mode for DXT5
    colors[2][0] = (2 * colors[0][0] + colors[1][0]) / 3;
    colors[2][1] = (2 * colors[0][1] + colors[1][1]) / 3;
    colors[2][2] = (2 * colors[0][2] + colors[1][2]) / 3;

    colors[3][0] = (colors[0][0] + 2 * colors[1][0]) / 3;
    colors[3][1] = (colors[0][1] + 2 * colors[1][1]) / 3;
    colors[3][2] = (colors[0][2] + 2 * colors[1][2]) / 3;

    // Read color indices
    uint32_t colorIndices =
        colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | (colorBlock[7] << 24);

    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            int pixelIndex = y * 4 + x;

            // Extract alpha (3 bits)
            int alphaIndex = (alphaIndices >> (pixelIndex * 3)) & 0x7;
            uint8_t alpha = alphas[alphaIndex];

            // Extract color (2 bits)
            int colorIndex = (colorIndices >> (pixelIndex * 2)) & 0x3;

            int outputIndex = (y * width + x) * 4;
            if (outputIndex >= 0)
            {
                output[outputIndex + 0] = colors[colorIndex][0];// R
                output[outputIndex + 1] = colors[colorIndex][1];// G
                output[outputIndex + 2] = colors[colorIndex][2];// B
                output[outputIndex + 3] = alpha;                // A
            }
        }
    }
}

static void decompress_dxt3_block(const uint8_t* block, uint8_t* output, int width)
{
    // DXT3 has explicit alpha (4 bits per pixel) + DXT1 color

    // Read alpha data (first 8 bytes)
    uint64_t alphaData = 0;
    for (int i = 0; i < 8; ++i)
    {
        alphaData |= static_cast<uint64_t>(block[i]) << (i * 8);
    }

    // Read color data (last 8 bytes)
    const uint8_t* colorBlock = block + 8;
    uint16_t color0 = colorBlock[0] | (colorBlock[1] << 8);
    uint16_t color1 = colorBlock[2] | (colorBlock[3] << 8);

    // Convert colors
    uint8_t colors[4][3];
    rgb565_to_rgb888(color0, colors[0]);
    rgb565_to_rgb888(color1, colors[1]);

    // Always use 4-color mode for DXT3
    colors[2][0] = (2 * colors[0][0] + colors[1][0]) / 3;
    colors[2][1] = (2 * colors[0][1] + colors[1][1]) / 3;
    colors[2][2] = (2 * colors[0][2] + colors[1][2]) / 3;

    colors[3][0] = (colors[0][0] + 2 * colors[1][0]) / 3;
    colors[3][1] = (colors[0][1] + 2 * colors[1][1]) / 3;
    colors[3][2] = (colors[0][2] + 2 * colors[1][2]) / 3;

    // Read color indices
    uint32_t colorIndices =
        colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | (colorBlock[7] << 24);

    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            int pixelIndex = y * 4 + x;

            // Extract alpha (4 bits)
            int alphaIndex = pixelIndex * 4;
            uint8_t alpha = ((alphaData >> alphaIndex) & 0xF) * 17;// Scale 4-bit to 8-bit

            // Extract color (2 bits)
            int colorIndex = (colorIndices >> (pixelIndex * 2)) & 0x3;

            int outputIndex = (y * width + x) * 4;
            if (outputIndex >= 0)
            {
                output[outputIndex + 0] = colors[colorIndex][0];// R
                output[outputIndex + 1] = colors[colorIndex][1];// G
                output[outputIndex + 2] = colors[colorIndex][2];// B
                output[outputIndex + 3] = alpha;                // A
            }
        }
    }
}

static int is_bioware_dds(const uint8_t* data, int len)
{
    if (len < 20)
        return 0;

    // Check if file starts with DDS magic number
    uint32_t magic = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    if (magic == DDS_MAGIC)
    {
        return 0;// Standard DDS file
    }

    // For Bioware DDS, the first 4 bytes should be width (power of 2, reasonable size)
    uint32_t possibleWidth = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

    // Check if it's a reasonable texture size (power of 2, between 1 and 4096)
    if (possibleWidth > 0 && possibleWidth <= 4096 && (possibleWidth & (possibleWidth - 1)) == 0)
    {
        return 1;// Likely Bioware DDS
    }

    return 0;
}

int stbi_dds_test_memory(unsigned char const* buffer, int len)
{
    if (len < 4)
        return 0;

    // Check for standard DDS magic
    uint32_t magic = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
    if (magic == DDS_MAGIC)
        return 1;

    // Check for Bioware DDS
    return is_bioware_dds(buffer, len);
}

int stbi_dds_test(char const* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
        return 0;

    uint8_t header[20];
    size_t read = fread(header, 1, sizeof(header), f);
    fclose(f);

    if (read < sizeof(header))
        return 0;

    return stbi_dds_test_memory(header, sizeof(header));
}

unsigned char* stbi_load_dds_from_memory(unsigned char const* buffer, int len, int* x, int* y,
                                         int* channels_in_file, int desired_channels)
{
    if (!stbi_dds_test_memory(buffer, len))
    {
        return nullptr;// Not a DDS file
    }

    const uint8_t* data = buffer;

    if (is_bioware_dds(data, len))
    {
        // Load Bioware DDS
        if (len < sizeof(BioDDSHeader))
            return nullptr;

        BioDDSHeader header;
        memcpy(&header, data, sizeof(BioDDSHeader));
        data += sizeof(BioDDSHeader);

        *x = header.width;
        *y = header.height;
        *channels_in_file = (header.channels == 3) ? 3 : 4;

        int output_channels = desired_channels ? desired_channels : *channels_in_file;
        unsigned char* result = (unsigned char*) malloc(*x * *y * output_channels);
        if (!result)
            return nullptr;

        // Decompress blocks
        int blocksWide = (*x + 3) / 4;
        int blocksHigh = (*y + 3) / 4;

        for (int blockY = 0; blockY < blocksHigh; ++blockY)
        {
            for (int blockX = 0; blockX < blocksWide; ++blockX)
            {
                int blockIndex = blockY * blocksWide + blockX;
                const uint8_t* blockData = data + blockIndex * 8;

                // Calculate output position
                int outputX = blockX * 4;
                int outputY = blockY * 4;
                uint8_t* outputPtr = result + (outputY * *x + outputX) * output_channels;

                if (header.channels == 3)
                {
                    decompress_dxt1_block(blockData, outputPtr, *x, output_channels);
                } else
                {
                    decompress_dxt5_block(blockData, outputPtr, *x);
                }
            }
        }

        return result;
    } else
    {
        // Load Standard DDS
        if (len < 4 + sizeof(DDSHeader))
            return nullptr;

        // Skip magic number
        data += 4;

        DDSHeader header;
        memcpy(&header, data, sizeof(DDSHeader));
        data += sizeof(DDSHeader);

        if (header.size != 124)
            return nullptr;

        *x = header.width;
        *y = header.height;

        int output_channels;
        if (header.ddspf.fourCC == FOURCC_DXT1)
        {
            *channels_in_file = 3;
            output_channels = desired_channels ? desired_channels : 3;

            auto* result = static_cast<unsigned char*>(malloc(*x * *y * output_channels));
            if (!result)
                return nullptr;

            int blocksWide = (*x + 3) / 4;
            int blocksHigh = (*y + 3) / 4;

            for (int blockY = 0; blockY < blocksHigh; ++blockY)
            {
                for (int blockX = 0; blockX < blocksWide; ++blockX)
                {
                    int blockIndex = blockY * blocksWide + blockX;
                    const uint8_t* blockData = data + blockIndex * 8;

                    int outputX = blockX * 4;
                    int outputY = blockY * 4;
                    uint8_t* outputPtr = result + (outputY * *x + outputX) * output_channels;

                    decompress_dxt1_block(blockData, outputPtr, *x, output_channels);
                }
            }

            return result;
        } else if (header.ddspf.fourCC == FOURCC_DXT3)
        {
            *channels_in_file = 4;
            output_channels = desired_channels ? desired_channels : 4;

            auto* result = static_cast<unsigned char*>(malloc(*x * *y * output_channels));
            if (!result)
                return nullptr;

            int blocksWide = (*x + 3) / 4;
            int blocksHigh = (*y + 3) / 4;

            for (int blockY = 0; blockY < blocksHigh; ++blockY)
            {
                for (int blockX = 0; blockX < blocksWide; ++blockX)
                {
                    int blockIndex = blockY * blocksWide + blockX;
                    const uint8_t* blockData = data + blockIndex * 16;

                    int outputX = blockX * 4;
                    int outputY = blockY * 4;
                    uint8_t* outputPtr = result + (outputY * *x + outputX) * output_channels;

                    decompress_dxt3_block(blockData, outputPtr, *x);
                }
            }

            return result;
        } else if (header.ddspf.fourCC == FOURCC_DXT5)
        {
            *channels_in_file = 4;
            output_channels = desired_channels ? desired_channels : 4;

            auto* result = static_cast<unsigned char*>(malloc(*x * *y * output_channels));
            if (!result)
                return nullptr;

            int blocksWide = (*x + 3) / 4;
            int blocksHigh = (*y + 3) / 4;

            for (int blockY = 0; blockY < blocksHigh; ++blockY)
            {
                for (int blockX = 0; blockX < blocksWide; ++blockX)
                {
                    int blockIndex = blockY * blocksWide + blockX;
                    const uint8_t* blockData = data + blockIndex * 16;

                    int outputX = blockX * 4;
                    int outputY = blockY * 4;
                    uint8_t* outputPtr = result + (outputY * *x + outputX) * output_channels;

                    decompress_dxt5_block(blockData, outputPtr, *x);
                }
            }

            return result;
        }
    }

    return nullptr;// Unsupported format
}

unsigned char* stbi_load_dds(char const* filename, int* x, int* y, int* channels_in_file,
                             int desired_channels)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
        return nullptr;

    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(f);
        return nullptr;
    }

    // Read entire file
    auto* buffer = static_cast<unsigned char*>(malloc(file_size));
    if (!buffer)
    {
        fclose(f);
        return nullptr;
    }

    size_t read = fread(buffer, 1, file_size, f);
    fclose(f);

    if (read != static_cast<size_t>(file_size))
    {
        free(buffer);
        return nullptr;
    }

    // Load from memory
    unsigned char* result =
        stbi_load_dds_from_memory(buffer, file_size, x, y, channels_in_file, desired_channels);
    free(buffer);

    return result;
}