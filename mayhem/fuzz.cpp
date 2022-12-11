#include <stdint.h>
#include <stdio.h>
#include <climits>

#include <fuzzer/FuzzedDataProvider.h>
#include "hangul.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    ucschar ch = provider.ConsumeIntegral<ucschar>();

    hangul_jamo_to_cjamo(ch);

    return 0;
}