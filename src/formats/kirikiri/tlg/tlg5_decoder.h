#ifndef FORMATS_KIRIKIRI_TLG_TLG5_DECODER_H
#define FORMATS_KIRIKIRI_TLG_TLG5_DECODER_H
#include "file.h"

namespace Formats
{
    namespace Kirikiri
    {
        namespace Tlg
        {
            class Tlg5Decoder final
            {
            public:
                void decode(File &file);
            };
        }
    }
}

#endif
