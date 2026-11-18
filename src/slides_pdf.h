#pragma once

#include "slides_allocator.h"
#include "third_party/choc/choc/containers/choc_zlib.h"
#include <algorithm>
#include <charconv>
#include <cstdio>

namespace chowdsp::slides
{
// Writes a PDF where every page is a single full-bleed image.
// Pages are compressed losslessly with Flate, so text and equations stay sharp.
//
// The number of pages and their pixel size are fixed up front, so the object layout is known ahead of time:
// objects 1 and 2 are the catalog and page tree, then each page gets an image, a content stream, and a page object.
struct Pdf_Writer
{
    // Page width in points (1/72 inch). 960pt matches a 13.33in wide 16:9 PowerPoint slide.
    static constexpr size_t page_width_pt = 960;
    static constexpr int compression_level = 6;

    size_t num_pages {};
    int width {};
    int height {};
    size_t page_height_pt {};

    // Holds the object offsets for the whole document, followed by per-page scratch space that is rewound after each page.
    Allocator arena;
    std::span<size_t> object_offsets {}; // object_offsets[n - 1] is the byte offset of object n
    size_t pages_written {};

    FILE* file {};
    size_t bytes_written {};
    bool write_failed {};

    Pdf_Writer (std::string_view path, size_t total_pages, int width_px, int height_px)
        : num_pages { total_pages },
          width { width_px },
          height { height_px },
          page_height_pt { (page_width_pt * (size_t) height_px + (size_t) width_px / 2) / (size_t) width_px },
          arena { arena_size (total_pages, width_px, height_px) }
    {
        object_offsets = arena.make_span<size_t> (num_objects());
        std::fill (object_offsets.begin(), object_offsets.end(), 0);

        // path comes from Allocator::copy_string, so it's null-terminated.
        file = std::fopen (path.data(), "wb");
        write ("%PDF-1.4\n%\xE2\xE3\xCF\xD3\n");
    }

    ~Pdf_Writer()
    {
        if (file != nullptr)
            std::fclose (file);
    }

    Pdf_Writer (const Pdf_Writer&) = delete;
    Pdf_Writer& operator= (const Pdf_Writer&) = delete;

    bool is_open() const { return file != nullptr && ! write_failed; }

    // Adds a page from tightly packed RGBA pixels (the alpha channel is dropped).
    // The image must match the size passed to the constructor.
    bool add_page (const uint8_t* rgba, int image_width, int image_height)
    {
        if (image_width != width || image_height != height || pages_written == num_pages)
            return false;

        const auto page_mark = arena.mark();

        const auto num_pixels = (size_t) width * (size_t) height;
        auto* rgb = arena.allocate<uint8_t> (num_pixels * 3);
        for (size_t px = 0; px < num_pixels; ++px)
        {
            rgb[px * 3 + 0] = rgba[px * 4 + 0];
            rgb[px * 3 + 1] = rgba[px * 4 + 1];
            rgb[px * 3 + 2] = rgba[px * 4 + 2];
        }

        const auto compressed_capacity = compress_bound (num_pixels * 3);
        auto* compressed = arena.allocate<uint8_t> (compressed_capacity);
        const auto compressed_size = compress (rgb, num_pixels * 3, compressed, compressed_capacity);
        if (compressed_size == 0)
        {
            arena.rewind (page_mark);
            return false;
        }

        const auto image_id = 3 + 3 * pages_written;
        const auto content_id = image_id + 1;
        const auto page_id = image_id + 2;

        begin_object (image_id);
        write (arena_format (arena,
                             "<< /Type /XObject /Subtype /Image /Width {} /Height {} /ColorSpace /DeviceRGB"
                             " /BitsPerComponent 8 /Filter /FlateDecode /Length {} >>\nstream\n",
                             num (width),
                             num (height),
                             num (compressed_size)));
        write ({ (const char*) compressed, compressed_size });
        write ("\nendstream\nendobj\n");

        // Scales the unit-square image up to cover the whole page.
        const auto content = arena_format (arena, "q {} 0 0 {} 0 0 cm /Im0 Do Q", num (page_width_pt), num (page_height_pt));
        begin_object (content_id);
        write (arena_format (arena, "<< /Length {} >>\nstream\n{}\nendstream\nendobj\n", num (content.size()), content));

        begin_object (page_id);
        write (arena_format (arena,
                             "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {} {}]"
                             " /Resources << /XObject << /Im0 {} 0 R >> >> /Contents {} 0 R >>\nendobj\n",
                             num (page_width_pt),
                             num (page_height_pt),
                             num (image_id),
                             num (content_id)));

        pages_written++;
        arena.rewind (page_mark);
        return ! write_failed;
    }

    // Writes the catalog, page tree, and cross-reference table, then closes the file.
    // Returns false if any write failed or if fewer pages were added than promised.
    bool finish()
    {
        if (file == nullptr)
            return false;

        const auto finish_mark = arena.mark();

        begin_object (1);
        write ("<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

        begin_object (2);
        write (arena_format (arena, "<< /Type /Pages /Count {} /Kids [", num (pages_written)));
        for (size_t page = 0; page < pages_written; ++page)
            write (arena_format (arena, " {} 0 R", num (5 + 3 * page)));
        write (" ] >>\nendobj\n");

        // Each cross-reference entry is exactly 20 bytes: a 10 digit offset, a 5 digit generation, and a type.
        static constexpr size_t xref_entry_size = 20;
        const auto xref_offset = bytes_written;
        write (arena_format (arena, "xref\n0 {}\n0000000000 65535 f \n", num (num_objects() + 1)));

        auto* entries = arena.allocate<char> (xref_entry_size * object_offsets.size());
        for (size_t i = 0; i < object_offsets.size(); ++i)
        {
            auto* entry = entries + i * xref_entry_size;
            auto offset = object_offsets[i];
            for (int digit = 9; digit >= 0; --digit, offset /= 10)
                entry[digit] = (char) ('0' + offset % 10);
            std::memcpy (entry + 10, " 00000 n \n", 10);
        }
        write ({ entries, xref_entry_size * object_offsets.size() });

        write (arena_format (arena,
                             "trailer\n<< /Size {} /Root 1 0 R >>\nstartxref\n{}\n%%EOF\n",
                             num (num_objects() + 1),
                             num (xref_offset)));
        arena.rewind (finish_mark);

        const auto close_failed = std::fclose (file) != 0;
        file = nullptr;
        return ! write_failed && ! close_failed && pages_written == num_pages;
    }

private:
    size_t num_objects() const { return 2 + 3 * num_pages; }

    // Big enough to hold the object offsets and one page's worth of scratch space in a single chunk.
    static size_t arena_size (size_t total_pages, int width_px, int height_px)
    {
        const auto rgb_size = (size_t) width_px * (size_t) height_px * 3;
        const auto offsets_size = (2 + 3 * total_pages) * sizeof (size_t);
        const auto xref_size = (2 + 3 * total_pages) * 20;
        return rgb_size + compress_bound (rgb_size) + offsets_size + xref_size + (1 << 14);
    }

    // Conservative worst-case size of zlib output (matches zlib's deflateBound() for non-default settings).
    static size_t compress_bound (size_t source_size)
    {
        return source_size + ((source_size + 7) >> 3) + ((source_size + 63) >> 6) + 11;
    }

    // Compresses into a zlib stream (which is what /FlateDecode expects). Returns the compressed size, or 0 on failure.
    static size_t compress (uint8_t* in, size_t in_size, uint8_t* out, size_t out_capacity)
    {
        using zlib = choc::zlib::zlib; // choc keeps its zlib implementation inside a struct
        if (in_size > UINT32_MAX || out_capacity > UINT32_MAX)
            return 0;

        zlib::DeflateStream stream;
        if (stream.initialise (compression_level, zlib::Z_DEFLATED, zlib::MAX_WBITS, zlib::DEF_MEM_LEVEL) != zlib::ErrorCode::OK)
            return 0;

        stream.next_in = in;
        stream.avail_in = (uint32_t) in_size;
        stream.next_out = out;
        stream.avail_out = (uint32_t) out_capacity;

        while (true)
        {
            const auto result = stream.deflate (zlib::FlushState::Z_FINISH);
            if (result == zlib::ErrorCode::STREAM_END)
                return out_capacity - stream.avail_out;
            if (result != zlib::ErrorCode::OK || stream.avail_out == 0)
                return 0;
        }
    }

    // Formats a number into the arena, so it can be passed to arena_format().
    std::string_view num (size_t value)
    {
        static constexpr size_t max_digits = 20;
        auto* buffer = arena.allocate<char> (max_digits);
        const auto* end = std::to_chars (buffer, buffer + max_digits, value).ptr;
        return { buffer, (size_t) (end - buffer) };
    }

    std::string_view num (int value) { return num ((size_t) value); }

    void begin_object (size_t id)
    {
        object_offsets[id - 1] = bytes_written;
        write (arena_format (arena, "{} 0 obj\n", num (id)));
    }

    void write (std::string_view data)
    {
        if (file == nullptr || write_failed)
            return;
        write_failed = std::fwrite (data.data(), 1, data.size(), file) != data.size();
        bytes_written += data.size();
    }
};
} // namespace chowdsp::slides
