#pragma once

// Minimal dependency-free PNG writer for RGBA8 images. Uses stored (uncompressed)
// deflate blocks, so files are larger than optimal but simple and valid.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace PngWriter
{
    namespace detail
    {
        inline std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0)
        {
            crc = ~crc;
            for (std::size_t i = 0; i < size; ++i)
            {
                crc ^= data[i];
                for (int k = 0; k < 8; ++k)
                {
                    crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
                }
            }
            return ~crc;
        }

        inline void putBE32(std::vector<std::uint8_t>& out, std::uint32_t v)
        {
            out.push_back(static_cast<std::uint8_t>(v >> 24));
            out.push_back(static_cast<std::uint8_t>(v >> 16));
            out.push_back(static_cast<std::uint8_t>(v >> 8));
            out.push_back(static_cast<std::uint8_t>(v));
        }

        inline void appendChunk(std::vector<std::uint8_t>& out, const char type[4],
                                const std::vector<std::uint8_t>& data)
        {
            putBE32(out, static_cast<std::uint32_t>(data.size()));
            const std::size_t typeStart = out.size();
            out.insert(out.end(), type, type + 4);
            out.insert(out.end(), data.begin(), data.end());
            putBE32(out, crc32(out.data() + typeStart, out.size() - typeStart));
        }
    }

    // rgba: width * height * 4 bytes, top row first.
    inline bool write(const std::string& path, std::uint32_t width, std::uint32_t height,
                      const std::vector<std::uint8_t>& rgba)
    {
        if (rgba.size() != static_cast<std::size_t>(width) * height * 4)
        {
            return false;
        }

        // Raw scanlines, each prefixed with filter type 0.
        std::vector<std::uint8_t> raw;
        raw.reserve((static_cast<std::size_t>(width) * 4 + 1) * height);
        for (std::uint32_t y = 0; y < height; ++y)
        {
            raw.push_back(0);
            const auto* row = rgba.data() + static_cast<std::size_t>(y) * width * 4;
            raw.insert(raw.end(), row, row + static_cast<std::size_t>(width) * 4);
        }

        // zlib stream: header, stored blocks, Adler-32.
        std::vector<std::uint8_t> zlib{0x78, 0x01};
        std::size_t offset = 0;
        do
        {
            const std::size_t block = std::min<std::size_t>(65535, raw.size() - offset);
            const bool last = offset + block == raw.size();
            zlib.push_back(last ? 1 : 0);
            zlib.push_back(static_cast<std::uint8_t>(block & 0xFF));
            zlib.push_back(static_cast<std::uint8_t>(block >> 8));
            zlib.push_back(static_cast<std::uint8_t>(~block & 0xFF));
            zlib.push_back(static_cast<std::uint8_t>((~block >> 8) & 0xFF));
            zlib.insert(zlib.end(), raw.begin() + offset, raw.begin() + offset + block);
            offset += block;
        } while (offset < raw.size());

        std::uint32_t a = 1, b = 0;
        for (std::uint8_t byte : raw)
        {
            a = (a + byte) % 65521;
            b = (b + a) % 65521;
        }
        detail::putBE32(zlib, (b << 16) | a);

        std::vector<std::uint8_t> out{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

        std::vector<std::uint8_t> ihdr;
        detail::putBE32(ihdr, width);
        detail::putBE32(ihdr, height);
        ihdr.push_back(8);  // bit depth
        ihdr.push_back(6);  // colour type: RGBA
        ihdr.push_back(0);  // compression
        ihdr.push_back(0);  // filter
        ihdr.push_back(0);  // interlace
        detail::appendChunk(out, "IHDR", ihdr);
        detail::appendChunk(out, "IDAT", zlib);
        detail::appendChunk(out, "IEND", {});

        std::FILE* file = std::fopen(path.c_str(), "wb");
        if (!file)
        {
            return false;
        }
        const bool ok = std::fwrite(out.data(), 1, out.size(), file) == out.size();
        return std::fclose(file) == 0 && ok;
    }
}
