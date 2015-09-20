// PMS image
//
// Company:   Alice Soft
// Engine:    AliceSystem / NV-SYSTEM
// Extension: .pm, .PMS
// Related:   AJP, ALD
//
// Known games:
// - [Alice Soft] [970404] Kichikuou Rance
// - [Alice Soft] [971218] Atlach-Nacha
// - [Alice Soft] [971218] Dalk
// - [Alice Soft] [971218] Dr. Stop!
// - [Alice Soft] [971218] Ningen Gari
// - [Alice Soft] [971218] Rance 1 - Hikari o Motomete
// - [Alice Soft] [971218] Rance 2 - Hangyaku no Shoujo-tachi
// - [Alice Soft] [971218] Rance 3 - Leazas Kanraku
// - [Alice Soft] [971218] Toushin Toshi
// - [Alice Soft] [971218] Zero
// - [Alice Soft] [011130] Daiakuji
// - [Alice Soft] [040827] Rance 6 - Zeth Houkai -

#include "fmt/alice_soft/pms_converter.h"
#include "err.h"
#include "io/buffered_io.h"
#include "util/format.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::alice_soft;

static const bstr magic1 = "PM\x01\x00"_b;
static const bstr magic2 = "PM\x02\x00"_b;

bstr PmsConverter::decompress_8bit(
    io::IO &input_io, size_t width, size_t height)
{
    bstr output(width * height);
    auto output_ptr = output.get<u8>();
    auto output_end = output.end<u8>();

    while (output_ptr < output_end)
    {
        u8 c = input_io.read_u8();

        switch (c)
        {
            case 0xFF:
            {
                auto n = input_io.read_u8() + 3;
                while (n-- && output_ptr < output_end)
                {
                    *output_ptr = output_ptr[-width];
                    output_ptr++;
                }
                break;
            }

            case 0xFE:
            {
                auto n = input_io.read_u8() + 3;
                while (n-- && output_ptr < output_end)
                {
                    *output_ptr = output_ptr[-width * 2];
                    output_ptr++;
                }
                break;
            }

            case 0xFD:
            {
                auto n = input_io.read_u8() + 4;
                auto color = input_io.read_u8();
                while (n-- && output_ptr < output_end)
                    *output_ptr++ = color;
                break;
            }

            case 0xFC:
            {
                auto n = input_io.read_u8() + 3;
                auto color1 = input_io.read_u8();
                auto color2 = input_io.read_u8();
                while (n-- && output_ptr < output_end)
                {
                    *output_ptr++ = color1;
                    *output_ptr++ = color2;
                }
                break;
            }

            case 0xF8:
                *output_ptr++ = input_io.read_u8();
                break;

            default:
                *output_ptr++ = c;
        }
    }

    return output;
}

bstr PmsConverter::decompress_16bit(
    io::IO &input_io, size_t width, size_t height)
{
    bstr output(width * height * 2);
    auto output_ptr = output.get<u16>();
    auto output_end = output.end<const u16>();

    while (output_ptr < output_end)
    {
        u8 c = input_io.read_u8();

        switch (c)
        {
            case 0xFF:
            {
                auto n = input_io.read_u8() + 2;
                while (n-- && output_ptr < output_end)
                {
                    *output_ptr = output_ptr[-width];
                    output_ptr++;
                }
                break;
            }

            case 0xFE:
            {
                auto n = input_io.read_u8() + 2;
                while (n-- && output_ptr < output_end)
                {
                    *output_ptr = output_ptr[-width * 2];
                    output_ptr++;
                }
                break;
            }

            case 0xFD:
            {
                auto size = input_io.read_u8() + 3;
                auto color = input_io.read_u16_le();
                while (size-- && output_ptr < output_end)
                    *output_ptr++ = color;
                break;
            }

            case 0xFC:
            {
                auto n = input_io.read_u8() + 2;
                auto color1 = input_io.read_u16_le();
                auto color2 = input_io.read_u16_le();
                while (n-- && output_ptr < output_end)
                {
                    *output_ptr++ = color1;
                    *output_ptr++ = color2;
                }
                break;
            }

            case 0xFB:
                *output_ptr = output_ptr[-width - 1];
                output_ptr++;
                break;

            case 0xFA:
                *output_ptr = output_ptr[-width + 1];
                output_ptr++;
                break;

            case 0xF9:
            {
                auto n = input_io.read_u8() + 1;
                auto byte1 = input_io.read_u8();
                auto half1
                    = ((byte1 & 0b11100000) << 8)
                    | ((byte1 & 0b00011000) << 6)
                    | ((byte1 & 0b00000111) << 2);
                while (n-- && output_ptr < output_end)
                {
                    auto byte2 = input_io.read_u8();
                    auto half2
                        = ((byte2 & 0b11000000) << 5)
                        | ((byte2 & 0b00111100) << 3)
                        | ((byte2 & 0b00000011) << 0);
                    *output_ptr++ = half1 | half2;
                }
                break;
            }

            case 0xF8:
                *output_ptr++ = input_io.read_u16_le();
                break;

            default:
                *output_ptr++ = c | (input_io.read_u8() << 8);
                break;
        }
    }

    return output;
}

std::unique_ptr<util::Image> PmsConverter::decode_to_image(const bstr &data)
{
    io::BufferedIO input_io(data);
    input_io.skip(2);
    auto version = input_io.read_u16_le();
    input_io.skip(2);
    auto depth = input_io.read_u16_le();
    input_io.skip(4 * 4);
    auto width = input_io.read_u32_le();
    auto height = input_io.read_u32_le();
    auto data_offset = input_io.read_u32_le();
    auto palette_offset = input_io.read_u32_le();

    std::unique_ptr<pix::Grid> pixels;

    if (depth == 8)
    {
        input_io.seek(data_offset);
        auto pixel_data = decompress_8bit(input_io, width, height);
        input_io.seek(palette_offset);
        pix::Palette palette(
            256,
            input_io,
            version == 1 ? pix::Format::RGB888 : pix::Format::BGR888);
        pixels.reset(new pix::Grid(width, height, pixel_data, palette));
    }
    else if (depth == 16)
    {
        input_io.seek(data_offset);
        auto pixel_data = decompress_16bit(input_io, width, height);
        pixels.reset(new pix::Grid(
            width, height, pixel_data, pix::Format::BGR565));
        if (palette_offset)
        {
            input_io.seek(palette_offset);
            auto alpha = decompress_8bit(input_io, width, height);
            pix::Grid alpha_channel(width, height, alpha, pix::Format::Gray8);
            for (auto y : util::range(height))
            for (auto x : util::range(width))
                pixels->at(x, y).a = alpha_channel.at(x, y).r;
        }
    }
    else
    {
        throw err::UnsupportedBitDepthError(depth);
    }

    return util::Image::from_pixels(*pixels);
}

bool PmsConverter::is_recognized_internal(File &file) const
{
    if (file.io.read(magic1.size()) == magic1)
        return true;
    file.io.seek(0);
    return file.io.read(magic2.size()) == magic2;
}

std::unique_ptr<File> PmsConverter::decode_internal(File &file) const
{
    auto image = decode_to_image(file.io.read_to_eof());
    return image->create_file(file.name);
}

static auto dummy = fmt::Registry::add<PmsConverter>("alice/pms");