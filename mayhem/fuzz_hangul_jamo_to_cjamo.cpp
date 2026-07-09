#include <stdint.h>
#include <stddef.h>
#include <vector>

#include <fuzzer/FuzzedDataProvider.h>
#include "hangul.h"

// Fuzz the jamo/cjamo/syllable conversion pipeline: per-codepoint
// classification and conversion, syllable (de)composition, and the
// whole-buffer jamo->syllable converter and syllable iterators.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);

    std::vector<ucschar> buf;
    while (provider.remaining_bytes() >= sizeof(ucschar)) {
        ucschar ch = provider.ConsumeIntegral<ucschar>();
        buf.push_back(ch);

        hangul_jamo_to_cjamo(ch);
        hangul_is_choseong(ch);
        hangul_is_jungseong(ch);
        hangul_is_jongseong(ch);
        hangul_is_choseong_conjoinable(ch);
        hangul_is_jungseong_conjoinable(ch);
        hangul_is_jongseong_conjoinable(ch);
        hangul_is_jamo_conjoinable(ch);
        hangul_is_syllable(ch);
        hangul_is_jamo(ch);
        hangul_is_cjamo(ch);

        if (hangul_is_syllable(ch)) {
            ucschar jamo[4] = { 0, 0, 0, 0 };
            hangul_syllable_to_jamo(ch, &jamo[0], &jamo[1], &jamo[2]);
            hangul_jamo_to_syllable(jamo[0], jamo[1], jamo[2]);
        }
    }

    if (buf.size() >= 3) {
        hangul_jamo_to_syllable(buf[0], buf[1], buf[2]);
    }

    if (!buf.empty()) {
        int len = (int)buf.size();
        hangul_syllable_len(buf.data(), len);

        const ucschar* iter = buf.data();
        const ucschar* end = buf.data() + len;
        while (iter < end)
            iter = hangul_syllable_iterator_next(iter, end);
        iter = end;
        while (iter > buf.data())
            iter = hangul_syllable_iterator_prev(iter, buf.data());

        std::vector<ucschar> dest(buf.size() + 1);
        hangul_jamos_to_syllables(dest.data(), (int)dest.size(),
                                  buf.data(), len);
    }

    return 0;
}
