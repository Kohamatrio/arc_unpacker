#pragma once

#include "io/istream.h"

namespace au {
namespace algo {
namespace pack {

    enum class ZlibKind : u8
    {
        RawDeflate = 0,
        PlainZlib  = 1, // == RawDeflate + 6 bytes of header+footer
        Gzip       = 2, // == PlainZlib + variable header data
    };

    bstr zlib_inflate(
        io::IStream &input_stream, const ZlibKind kind = ZlibKind::PlainZlib);

    bstr zlib_inflate(
        const bstr &input, const ZlibKind kind = ZlibKind::PlainZlib);

    bstr zlib_deflate(
        const bstr &input, const ZlibKind kind = ZlibKind::PlainZlib);


} } }
