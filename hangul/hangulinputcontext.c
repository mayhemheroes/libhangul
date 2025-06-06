/* libhangul
 * Copyright (C) 2004 - 2016 Choe Hwanjin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#else
#define strcasecmp _stricmp
#endif
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>

#include "hangul-gettext.h"
#include "hangul.h"
#include "hangulinternals.h"

/**
 * @defgroup hangulic 한글 입력 기능 구현
 * 
 * @section hangulicusage Hangul Input Context의 사용법
 * 이 섹션에서는 한글 입력 기능을 구현하는 핵심 기능에 대해 설명한다.
 *
 * 먼저 preedit string과 commit string 이 두 용어에 대해서 설명하겠다.
 * 이 두가지 용어는 Unix 계열의 입력기 framework에서 널리 쓰이는 표현이다.
 *
 * preedit string은 아직 조합중으로 어플리케이션에 완전히 입력되지 않은 
 * 스트링을 가리킨다. 일반적으로 한글 입력기에서는 역상으로 보이고
 * 일본 중국어 입력기에서는 underline이 붙어 나타난다. 아직 완성이 되지
 * 않은 스트링이므로 어플리케이션에 전달이 되지 않고 사라질 수도 있다.
 *
 * commit string은 조합이 완료되어 어플리케이션에 전달되는 스트링이다.
 * 이 스트링은 실제 어플리케이션의 텍스트로 인식이 되므로 이 이후에는
 * 더이상 입력기가 관리할 수 있는 데이터가 아니다.
 *
 * 한글 입력과정은 다음과 같은 과정을 거치게 된다.
 * 입력된 영문 키를 그에 해당하는 한글 자모로 변환한후 한글 자모를 모아
 * 하나의 음절을 만든다. 여기까지 이루어지는 과정을 preedit string 형태로
 * 사용자에게 계속 보이게 하는 것이 필요하다.
 * 그리고는 한글 음절이 완성되고나면 그 글자를 어플리케이션에 commit 
 * string 형태로 보내여 입력을 완료하는 것이다. 다음 키를 받게 되면 
 * 이 과정을 반복해서 수행한다.
 * 
 * libhangul에서 한글 조합 기능은 @ref HangulInputContext 를 이용해서 구현하게
 * 되는데 기본 적인 방법은 @ref HangulInputContext 에 사용자로부터의 입력을
 * 순서대로 전달하면서 그 상태가 바뀜에 따라서 preedit 나 commit 스트링을
 * 상황에 맞게 변화시키는 것이다.
 * 
 * 입력 코드들은 GUI 코드와 밀접하게 붙어 있어서 키 이벤트를 받아서
 * 처리하도록 구현하는 것이 보통이다. 그런데 유닉스에는 많은 입력 프레임웍들이
 * 난립하고 있는 상황이어서 매 입력 프레임웍마다 한글 조합 루틴을 작성해서
 * 넣는 것은 비효율적이다. 간단한 API를 구현하여 여러 프레임웍에서 바로 
 * 사용할 수 있도록 구현하는 편이 사용성이 높아지게 된다.
 *
 * 그래서 libhangul에서는 키 이벤트를 따로 재정의하지 않고 ASCII 코드를 
 * 직접 사용하는 방향으로 재정의된 데이터가 많지 않도록 하였다.
 * 실제 사용 방법은 말로 설명하는 것보다 샘플 코드를 사용하는 편이
 * 이해가 빠를 것이다. 그래서 대략적인 진행 과정을 샘플 코드로 
 * 작성하였다.
 *
 * 아래 예제는 실제로는 존재하지 않는 GUI 라이브러리 코드를 사용하였다.
 * 실제 GUI 코드를 사용하면 코드가 너무 길어져서 설명이 어렵고 코드가
 * 길어지면 핵심을 놓치기 쉽기 때문에 가공의 함수를 사용하였다.
 * 또한 텍스트의 encoding conversion 관련된 부분도 생략하였다.
 * 여기서 사용한 가공의 GUI 코드는 TWin으로 시작하게 하였다.
 *    
 * @code

    HangulInputContext* hic = hangul_ic_new("2");
    ...

    // 아래는 키 입력만 처리하는 이벤트 루프이다.
    // 실제 GUI코드는 이렇게 단순하진 않지만
    // 편의상 키 입력만 처리하는 코드로 작성하였다.

    TWinKeyEvent event = TWinGetKeyEvent(); // 키이벤트를 받는 이런 함수가
					    // 있다고 치자
    while (ascii != 0) {
	bool res;
	if (event.isBackspace()) {
	    // backspace를 ascii로 변환하기가 좀 꺼림직해서
	    // libhangul에서는 backspace 처리를 위한 
	    // 함수를 따로 만들었다.
	    res = hangul_ic_backspace(hic);
	} else {
	    // 키 입력을 해당하는 ascii 코드로 변환한다.
	    // libhangul에서는 이 ascii 코드가 키 이벤트
	    // 코드와 마찬가지다.
	    int ascii = event.getAscii();

	    // 키 입력을 받았으면 이것을 hic에 먼저 보낸다.
	    // 그래야 hic가 이 키를 사용할 것인지 아닌지를 판단할 수 있다.
	    // 함수가 true를 리턴하면 이 키를 사용했다는 의미이므로 
	    // GUI 코드가 이 키 입력을 프로세싱하지 않도록 해야 한다.
	    // 그렇지 않으면 한 키입력이 두번 프로세싱된다.
	    res = hangul_ic_process(hic, ascii);
	}
	
	// hic는 한번 키입력을 받고 나면 내부 상태 변화가 일어나고
	// 완성된 글자를 어플리케이션에 보내야 하는 상황이 있을 수 있다.
	// 이것을 HangulInputContext에서는 commit 스트링이 있는지로
	// 판단한다. commit 스트링을 받아봐서 스트링이 있다면 
	// 그 스트링으로 입력이 완료된 걸로 본다.
	const ucschar commit;
	commit = hangul_ic_get_commit_string(hic);
	if (commit[0] != 0) {	// 스트링의 길이를 재서 commit 스트링이 있는지
				// 판단한다.
	    TWinInputUnicodeChars(commit);
	}

	// 키입력 후에는 preedit string도 역시 변화하게 되는데
	// 입력기 프레임웍에서는 이 스트링을 화면에 보여주어야
	// 조합중인 글자가 화면에 표시가 되는 것이다.
	const ucschar preedit;
	preedit = hangul_ic_get_preedit_string(hic);
	// 이 경우에는 스트링의 길이에 관계없이 항상 업데이트를 
	// 해야 한다. 왜냐하면 이전에 조합중이던 글자가 있다가
	// 조합이 완료되면서 조합중인 상태의 글자가 없어질 수도 있기 때문에
	// 스트링의 길이에 관계없이 현재 상태의 스트링을 preedit 
	// 스트링으로 보여주면 되는 것이다.
	TWinUpdatePreeditString(preedit);

	// 위 두작업이 끝난후에는 키 이벤트를 계속 프로세싱해야 하는지 
	// 아닌지를 처리해야 한다.
	// hic가 키 이벤트를 사용하지 않았다면 기본 GUI 코드에 계속해서
	// 키 이벤트 프로세싱을 진행하도록 해야 한다.
	if (!res)
	    TWinForwardKeyEventToUI(ascii);

	ascii = GetKeyEvent();
    }

    hangul_ic_delete(hic);
     
 * @endcode
 */

/**
 * @file hangulinputcontext.c
 */

/**
 * @ingroup hangulic
 * @typedef HangulInputContext
 * @brief 한글 입력 상태를 관리하기 위한 오브젝트
 *
 * libhangul에서 제공하는 한글 조합 루틴에서 상태 정보를 저장하는 opaque
 * 데이타 오브젝트이다. 이 오브젝트에 키입력 정보를 순차적으로 보내주면서
 * preedit 스트링이나, commit 스트링을 받아서 처리하면 한글 입력 기능을
 * 손쉽게 구현할 수 있다.
 * 내부의 데이터 멤버는 공개되어 있지 않다. 각각의 멤버는 accessor 함수로만
 * 참조하여야 한다.
 */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef void   (*HangulOnTranslate)  (HangulInputContext*,
				      int,
				      ucschar*,
				      void*);
typedef bool   (*HangulOnTransition) (HangulInputContext*,
				      ucschar,
				      const ucschar*,
				      void*);

struct _HangulBuffer {
    ucschar choseong;
    ucschar jungseong;
    ucschar jongseong;

    ucschar stack[12];
    int     index;
};

struct _HangulInputContext {
    int type;

    const HangulKeyboard*    keyboard;
    int tableid;

    HangulBuffer buffer;
    int output_mode;

    ucschar preedit_string[64];
    ucschar commit_string[64];
    ucschar flushed_string[64];

    HangulOnTranslate   on_translate;
    void*               on_translate_data;

    HangulOnTransition  on_transition;
    void*               on_transition_data;

    unsigned int use_jamo_mode_only : 1;
    unsigned int option_auto_reorder : 1;
    unsigned int option_combi_on_double_stroke : 1;
    unsigned int option_non_choseong_combi : 1;
};

static void    hangul_buffer_push(HangulBuffer *buffer, ucschar ch);
static ucschar hangul_buffer_pop (HangulBuffer *buffer);
static ucschar hangul_buffer_peek(HangulBuffer *buffer);

static void    hangul_buffer_clear(HangulBuffer *buffer);
static int     hangul_buffer_get_string(HangulBuffer *buffer, ucschar*buf, int buflen);
static int     hangul_buffer_get_jamo_string(HangulBuffer *buffer, ucschar *buf, int buflen);

static void    hangul_ic_flush_internal(HangulInputContext *hic);


static bool
hangul_buffer_is_empty(HangulBuffer *buffer)
{
    return buffer->choseong == 0 && buffer->jungseong == 0 &&
	   buffer->jongseong == 0;
}

static bool
hangul_buffer_has_choseong(HangulBuffer *buffer)
{
    return buffer->choseong != 0;
}

static bool
hangul_buffer_has_jungseong(HangulBuffer *buffer)
{
    return buffer->jungseong != 0;
}

static bool
hangul_buffer_has_jongseong(HangulBuffer *buffer)
{
    return buffer->jongseong != 0;
}

static void
hangul_buffer_push(HangulBuffer *buffer, ucschar ch)
{
    if (hangul_is_choseong(ch)) {
	buffer->choseong = ch;
    } else if (hangul_is_jungseong(ch)) {
	buffer->jungseong = ch;
    } else if (hangul_is_jongseong(ch)) {
	buffer->jongseong = ch;
    } else {
    }

    buffer->stack[++buffer->index] = ch;
}

static ucschar
hangul_buffer_pop(HangulBuffer *buffer)
{
    return buffer->stack[buffer->index--];
}

static ucschar
hangul_buffer_peek(HangulBuffer *buffer)
{
    if (buffer->index < 0)
	return 0;

    return buffer->stack[buffer->index];
}

static void
hangul_buffer_clear(HangulBuffer *buffer)
{
    buffer->choseong = 0;
    buffer->jungseong = 0;
    buffer->jongseong = 0;

    buffer->index = -1;
    buffer->stack[0]  = 0;
    buffer->stack[1]  = 0;
    buffer->stack[2]  = 0;
    buffer->stack[3]  = 0;
    buffer->stack[4]  = 0;
    buffer->stack[5]  = 0;
    buffer->stack[6]  = 0;
    buffer->stack[7]  = 0;
    buffer->stack[8]  = 0;
    buffer->stack[9]  = 0;
    buffer->stack[10] = 0;
    buffer->stack[11] = 0;
}

static int
hangul_buffer_get_jamo_string(HangulBuffer *buffer, ucschar *buf, int buflen)
{
    int n = 0;

    if (buffer->choseong || buffer->jungseong || buffer->jongseong) {
	if (buffer->choseong) {
	    buf[n++] = buffer->choseong;
	} else {
	    buf[n++] = HANGUL_CHOSEONG_FILLER;
	}
	if (buffer->jungseong) {
	    buf[n++] = buffer->jungseong;
	} else {
	    buf[n++] = HANGUL_JUNGSEONG_FILLER;
	}
	if (buffer->jongseong) {
	    buf[n++] = buffer->jongseong;
	}
    }

    buf[n] = 0;

    return n;
}

static int
hangul_jaso_to_string(ucschar cho, ucschar jung, ucschar jong,
		      ucschar *buf, int len)
{
    ucschar ch = 0;
    int n = 0;

    if (cho) {
	if (jung) {
	    /* have cho, jung, jong or no jong */
	    ch = hangul_jamo_to_syllable(cho, jung, jong);
	    if (ch != 0) {
		buf[n++] = ch;
	    } else {
		/* 한글 음절로 표현 불가능한 경우 */
		buf[n++] = cho;
		buf[n++] = jung;
		if (jong != 0)
		    buf[n++] = jong;
	    }
	} else {
	    if (jong) {
		/* have cho, jong */
		buf[n++] = cho;
		buf[n++] = HANGUL_JUNGSEONG_FILLER;
		buf[n++] = jong;
	    } else {
		/* have cho */
		ch = hangul_jamo_to_cjamo(cho);
		if (hangul_is_cjamo(ch)) {
		    buf[n++] = ch;
		} else {
		    buf[n++] = cho;
		    buf[n++] = HANGUL_JUNGSEONG_FILLER;
		}
	    }
	}
    } else {
	if (jung) {
	    if (jong) {
		/* have jung, jong */
		buf[n++] = HANGUL_CHOSEONG_FILLER;
		buf[n++] = jung;
		buf[n++] = jong;
	    } else {
		/* have jung */
		ch = hangul_jamo_to_cjamo(jung);
		if (hangul_is_cjamo(ch)) {
		    buf[n++] = ch;
		} else {
		    buf[n++] = HANGUL_CHOSEONG_FILLER;
		    buf[n++] = jung;
		}
	    }
	} else {
	    if (jong) { 
		/* have jong */
		ch = hangul_jamo_to_cjamo(jong);
		if (hangul_is_cjamo(ch)) {
		    buf[n++] = ch;
		} else {
		    buf[n++] = HANGUL_CHOSEONG_FILLER;
		    buf[n++] = HANGUL_JUNGSEONG_FILLER;
		    buf[n++] = jong;
		}
	    } else {
		/* have nothing */
		buf[n] = 0;
	    }
	}
    }
    buf[n] = 0;

    return n;
}

static int
hangul_buffer_get_string(HangulBuffer *buffer, ucschar *buf, int buflen)
{
    return hangul_jaso_to_string(buffer->choseong,
				 buffer->jungseong,
				 buffer->jongseong,
				 buf, buflen);
}

static bool
hangul_buffer_backspace(HangulBuffer *buffer)
{
    if (buffer->index >= 0) {
	ucschar ch = hangul_buffer_pop(buffer);
	if (ch == 0)
	    return false;

	if (buffer->index >= 0) {
	    if (hangul_is_choseong(ch)) {
		ch = hangul_buffer_peek(buffer);
		buffer->choseong = hangul_is_choseong(ch) ? ch : 0;
		return true;
	    } else if (hangul_is_jungseong(ch)) {
		ch = hangul_buffer_peek(buffer);
		buffer->jungseong = hangul_is_jungseong(ch) ? ch : 0;
		return true;
	    } else if (hangul_is_jongseong(ch)) {
		ch = hangul_buffer_peek(buffer);
		buffer->jongseong = hangul_is_jongseong(ch) ? ch : 0;
		return true;
	    }
	} else {
	    buffer->choseong = 0;
	    buffer->jungseong = 0;
	    buffer->jongseong = 0;
	    return true;
	}
    }
    return false;
}

static inline bool
hangul_ic_push(HangulInputContext *hic, ucschar c)
{
    ucschar buf[64] = { 0, };
    if (hic->on_transition != NULL) {
	ucschar cho, jung, jong;
	if (hangul_is_choseong(c)) {
	    cho  = c;
	    jung = hic->buffer.jungseong;
	    jong = hic->buffer.jongseong;
	} else if (hangul_is_jungseong(c)) {
	    cho  = hic->buffer.choseong;
	    jung = c;
	    jong = hic->buffer.jongseong;
	} else if (hangul_is_jongseong(c)) {
	    cho  = hic->buffer.choseong;
	    jung = hic->buffer.jungseong;
	    jong = c;
	} else {
	    hangul_ic_flush_internal(hic);
	    return false;
	}

	hangul_jaso_to_string(cho, jung, jong, buf, N_ELEMENTS(buf));
	if (!hic->on_transition(hic, c, buf, hic->on_transition_data)) {
	    hangul_ic_flush_internal(hic);
	    return false;
	}
    } else {
	if (!hangul_is_jamo(c)) {
	    hangul_ic_flush_internal(hic);
	    return false;
	}
    }

    hangul_buffer_push(&hic->buffer, c);
    return true;
}

static inline ucschar
hangul_ic_pop(HangulInputContext *hic)
{
    return hangul_buffer_pop(&hic->buffer);
}

static inline ucschar
hangul_ic_peek(HangulInputContext *hic)
{
    return hangul_buffer_peek(&hic->buffer);
}

static inline void
hangul_ic_save_preedit_string(HangulInputContext *hic)
{
    if (hic->output_mode == HANGUL_OUTPUT_JAMO) {
	hangul_buffer_get_jamo_string(&hic->buffer,
				      hic->preedit_string,
				      N_ELEMENTS(hic->preedit_string));
    } else {
	hangul_buffer_get_string(&hic->buffer,
				 hic->preedit_string,
				 N_ELEMENTS(hic->preedit_string));
    }
}

static inline void
hangul_ic_append_commit_string(HangulInputContext *hic, ucschar ch)
{
    int i;

    for (i = 0; i < N_ELEMENTS(hic->commit_string); i++) {
	if (hic->commit_string[i] == 0)
	    break;
    }

    if (i + 1 < N_ELEMENTS(hic->commit_string)) {
	hic->commit_string[i++] = ch;
	hic->commit_string[i] = 0;
    }
}

static inline void
hangul_ic_save_commit_string(HangulInputContext *hic)
{
    ucschar *string = hic->commit_string;
    int len = N_ELEMENTS(hic->commit_string);

    while (len > 0) {
	if (*string == 0)
	    break;
	len--;
	string++;
    }

    if (hic->output_mode == HANGUL_OUTPUT_JAMO) {
	hangul_buffer_get_jamo_string(&hic->buffer, string, len);
    } else {
	hangul_buffer_get_string(&hic->buffer, string, len);
    }

    hangul_buffer_clear(&hic->buffer);
}

static ucschar
hangul_ic_choseong_to_jongseong(HangulInputContext* hic, ucschar cho)
{
    ucschar jong = hangul_choseong_to_jongseong(cho);
    if (hangul_is_jongseong_conjoinable(jong)) {
	return jong;
    } else {
	/* 옛글 조합 규칙을 사용하는 자판의 경우에는 종성이 conjoinable
	 * 하지 않아도 상관없다 */
	int type = hangul_keyboard_get_type(hic->keyboard);
	switch (type) {
	case HANGUL_KEYBOARD_TYPE_JAMO_YET:
	case HANGUL_KEYBOARD_TYPE_JASO_YET:
	    return jong;
	}
    }

    return 0;
}

static ucschar
hangul_ic_combine(HangulInputContext* hic, ucschar first, ucschar second)
{
    if (!hic->option_combi_on_double_stroke) {
	if (hangul_keyboard_get_type(hic->keyboard) == HANGUL_KEYBOARD_TYPE_JAMO) {
	    /* 옛한글은 아래 규칙을 적용하지 않아야 입력가능한 글자가 있으므로
	     * 적용하지 않는다. */
	    if (first == second &&
		hangul_is_jamo_conjoinable(first)) {
		return 0;
	    }
	}
    }

    ucschar combined = 0;
    combined = hangul_keyboard_combine(hic->keyboard, 0, first, second);

    if (!hic->option_non_choseong_combi) {
	if (hangul_is_choseong(first) && hangul_is_choseong(second) &&
	    hangul_is_jongseong(combined)) {
	    return 0;
	}
    }

    return combined;
}

static bool
hangul_ic_process_jamo(HangulInputContext *hic, ucschar ch)
{
    ucschar jong;
    ucschar combined;

    if (!hangul_is_jamo(ch) && ch > 0) {
	hangul_ic_save_commit_string(hic);
	hangul_ic_append_commit_string(hic, ch);
	return true;
    }

    if (hic->buffer.jongseong) {
	if (hangul_is_choseong(ch)) {
	    jong = hangul_ic_choseong_to_jongseong(hic, ch);
	    combined = hangul_ic_combine(hic, hic->buffer.jongseong, jong);
	    if (hangul_is_jongseong(combined)) {
		if (!hangul_ic_push(hic, combined)) {
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    } else {
		hangul_ic_save_commit_string(hic);
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	} else if (hangul_is_jungseong(ch)) {
	    ucschar pop, peek;
	    pop = hangul_ic_pop(hic);
	    peek = hangul_ic_peek(hic);

	    if (hangul_is_jongseong(peek)) {
		ucschar choseong = hangul_jongseong_get_diff(peek,
						 hic->buffer.jongseong);
		if (choseong == 0) {
		    hangul_ic_save_commit_string(hic);
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		} else {
		    hic->buffer.jongseong = peek;
		    hangul_ic_save_commit_string(hic);
		    hangul_ic_push(hic, choseong);
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    } else {
		hic->buffer.jongseong = 0;
		hangul_ic_save_commit_string(hic);
		hangul_ic_push(hic, hangul_jongseong_to_choseong(pop));
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	} else {
	    goto flush;
	}
    } else if (hic->buffer.jungseong) {
	if (hangul_is_choseong(ch)) {
	    if (hic->buffer.choseong) {
		jong = hangul_ic_choseong_to_jongseong(hic, ch);
		if (hangul_is_jongseong(jong)) {
		    if (!hangul_ic_push(hic, jong)) {
			if (!hangul_ic_push(hic, ch)) {
			    return false;
			}
		    }
		} else {
		    hangul_ic_save_commit_string(hic);
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    } else {
		if (hic->option_auto_reorder) {
		    /* kr 처럼 자모가 역순인 경우 처리 */
		    if (!hangul_ic_push(hic, ch)) {
			if (!hangul_ic_push(hic, ch)) {
			    return false;
			}
		    }
		} else {
		    hangul_ic_save_commit_string(hic);
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    }
	} else if (hangul_is_jungseong(ch)) {
	    combined = hangul_ic_combine(hic, hic->buffer.jungseong, ch);
	    if (hangul_is_jungseong(combined)) {
		if (!hangul_ic_push(hic, combined)) {
		    return false;
		}
	    } else {
		hangul_ic_save_commit_string(hic);
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	} else {
	    goto flush;
	}
    } else if (hic->buffer.choseong) {
	if (hangul_is_choseong(ch)) {
	    combined = hangul_ic_combine(hic, hic->buffer.choseong, ch);
	    /* 초성을 입력한 combine 함수에서 종성이 나오게 된다면
	     * 이전 초성도 종성으로 바꿔 주는 편이 나머지 처리에 편리하다.
	     * 이 기능은 MS IME 호환기능으로 ㄳ을 입력하는데 사용한다. */
	    if (hangul_is_jongseong(combined)) {
		hic->buffer.choseong = 0;
		ucschar pop = hangul_ic_pop(hic);
		ucschar jong = hangul_choseong_to_jongseong(pop);
		hangul_ic_push(hic, jong);
	    }

	    if (!hangul_ic_push(hic, combined)) {
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	} else {
	    if (!hangul_ic_push(hic, ch)) {
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	}
    } else {
	if (!hangul_ic_push(hic, ch)) {
	    return false;
	}
    }

    hangul_ic_save_preedit_string(hic);
    return true;

flush:
    hangul_ic_flush_internal(hic);
    return false;
}

static bool
hangul_ic_process_jaso(HangulInputContext *hic, ucschar ch)
{
    if (hangul_is_choseong(ch)) {
	if (hic->buffer.choseong == 0) {
	    if (hic->option_auto_reorder) {
		if (!hangul_ic_push(hic, ch)) {
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    } else {
		if (hangul_ic_has_jungseong(hic) || hangul_ic_has_jongseong(hic)) {
		    hangul_ic_save_commit_string(hic);
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		} else {
		    if (!hangul_ic_push(hic, ch)) {
			if (!hangul_ic_push(hic, ch)) {
			    return false;
			}
		    }
		}
	    }
	} else {
	    ucschar choseong = 0;
	    if (hangul_is_choseong(hangul_ic_peek(hic))) {
		choseong = hangul_ic_combine(hic, hic->buffer.choseong, ch);
	    }
	    if (choseong) {
		if (!hangul_ic_push(hic, choseong)) {
		    if (!hangul_ic_push(hic, choseong)) {
			return false;
		    }
		}
	    } else {
		hangul_ic_save_commit_string(hic);
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	}
    } else if (hangul_is_jungseong(ch)) {
	if (hic->buffer.jungseong == 0) {
	    if (hic->option_auto_reorder) {
		if (!hangul_ic_push(hic, ch)) {
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    } else {
		if (hangul_ic_has_jongseong(hic)) {
		    hangul_ic_save_commit_string(hic);
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		} else {
		    if (!hangul_ic_push(hic, ch)) {
			if (!hangul_ic_push(hic, ch)) {
			    return false;
			}
		    }
		}
	    }
	} else {
	    ucschar jungseong = 0;
	    if (hangul_is_jungseong(hangul_ic_peek(hic))) {
		jungseong = hangul_ic_combine(hic, hic->buffer.jungseong, ch);
	    }
	    if (jungseong) {
		if (!hangul_ic_push(hic, jungseong)) {
		    if (!hangul_ic_push(hic, jungseong)) {
			return false;
		    }
		}
	    } else {
		hangul_ic_save_commit_string(hic);
		if (!hangul_ic_push(hic, ch)) {
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    }
	}
    } else if (hangul_is_jongseong(ch)) {
	if (hic->buffer.jongseong == 0) {
	    if (!hangul_ic_push(hic, ch)) {
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	} else {
	    ucschar jongseong = 0;
	    if (hangul_is_jongseong(hangul_ic_peek(hic))) {
		jongseong = hangul_ic_combine(hic, hic->buffer.jongseong, ch);
	    }
	    if (jongseong) {
		if (!hangul_ic_push(hic, jongseong)) {
		    if (!hangul_ic_push(hic, jongseong)) {
			return false;
		    }
		}
	    } else {
		hangul_ic_save_commit_string(hic);
		if (!hangul_ic_push(hic, ch)) {
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    }
	}
    } else if (ch > 0) {
	hangul_ic_save_commit_string(hic);
	hangul_ic_append_commit_string(hic, ch);
    } else {
	hangul_ic_save_commit_string(hic);
	return false;
    }

    hangul_ic_save_preedit_string(hic);
    return true;
}

static bool
hangul_ic_process_romaja(HangulInputContext *hic, int ascii, ucschar ch)
{
    ucschar jong;
    ucschar combined;

    if (!hangul_is_jamo(ch) && ch > 0) {
	hangul_ic_save_commit_string(hic);
	hangul_ic_append_commit_string(hic, ch);
	return true;
    }

    if (isupper(ascii)) {
	hangul_ic_save_commit_string(hic);
    }

    if (hic->buffer.jongseong) {
	if (ascii == 'x' || ascii == 'X') {
	    ch = 0x110c;
	    hangul_ic_save_commit_string(hic);
	    if (!hangul_ic_push(hic, ch)) {
		return false;
	    }
	} else if (hangul_is_choseong(ch) || hangul_is_jongseong(ch)) {
	    if (hangul_is_jongseong(ch))
		jong = ch;
	    else
		jong = hangul_ic_choseong_to_jongseong(hic, ch);
	    combined = hangul_ic_combine(hic, hic->buffer.jongseong, jong);
	    if (hangul_is_jongseong(combined)) {
		if (!hangul_ic_push(hic, combined)) {
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    } else {
		hangul_ic_save_commit_string(hic);
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	} else if (hangul_is_jungseong(ch)) {
	    if (hic->buffer.jongseong == 0x11bc) {
		hangul_ic_save_commit_string(hic);
		hic->buffer.choseong = 0x110b;
		hangul_ic_push(hic, ch);
	    } else {
		ucschar pop, peek;
		pop = hangul_ic_pop(hic);
		peek = hangul_ic_peek(hic);

		if (hangul_is_jungseong(peek)) {
		    if (pop == 0x11aa) {
			hic->buffer.jongseong = 0x11a8;
			pop = 0x11ba;
		    } else {
			hic->buffer.jongseong = 0;
		    }
		    hangul_ic_save_commit_string(hic);
		    hangul_ic_push(hic, hangul_jongseong_to_choseong(pop));
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		} else {
		    ucschar choseong = 0, jongseong = 0; 
		    hangul_jongseong_decompose(hic->buffer.jongseong,
					       &jongseong, &choseong);
		    hic->buffer.jongseong = jongseong;
		    hangul_ic_save_commit_string(hic);
		    hangul_ic_push(hic, choseong);
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    }
	} else {
	    goto flush;
	}
    } else if (hic->buffer.jungseong) {
	if (hangul_is_choseong(ch)) {
	    if (hic->buffer.choseong) {
		jong = hangul_ic_choseong_to_jongseong(hic, ch);
		if (hangul_is_jongseong(jong)) {
		    if (!hangul_ic_push(hic, jong)) {
			if (!hangul_ic_push(hic, ch)) {
			    return false;
			}
		    }
		} else {
		    hangul_ic_save_commit_string(hic);
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    } else {
		if (!hangul_ic_push(hic, ch)) {
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    }
	} else if (hangul_is_jungseong(ch)) {
	    combined = hangul_ic_combine(hic, hic->buffer.jungseong, ch);
	    if (hangul_is_jungseong(combined)) {
		if (!hangul_ic_push(hic, combined)) {
		    return false;
		}
	    } else {
		hangul_ic_save_commit_string(hic);
		hic->buffer.choseong = 0x110b;
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	} else if (hangul_is_jongseong(ch)) {
	    if (!hangul_ic_push(hic, ch)) {
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	} else {
	    goto flush;
	}
    } else if (hic->buffer.choseong) {
	if (hangul_is_choseong(ch)) {
	    combined = hangul_ic_combine(hic, hic->buffer.choseong, ch);
	    if (combined == 0) {
		hic->buffer.jungseong = 0x1173;
		hangul_ic_flush_internal(hic);
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    } else {
		if (!hangul_ic_push(hic, combined)) {
		    if (!hangul_ic_push(hic, ch)) {
			return false;
		    }
		}
	    }
	} else if (hangul_is_jongseong(ch)) {
	    hic->buffer.jungseong = 0x1173;
	    hangul_ic_save_commit_string(hic);
	    if (ascii == 'x' || ascii == 'X')
		ch = 0x110c;
	    if (!hangul_ic_push(hic, ch)) {
		return false;
	    }
	} else {
	    if (!hangul_ic_push(hic, ch)) {
		if (!hangul_ic_push(hic, ch)) {
		    return false;
		}
	    }
	}
    } else {
	if (ascii == 'x' || ascii == 'X') {
	    ch = 0x110c;
	}

	if (!hangul_ic_push(hic, ch)) {
	    return false;
	} else {
	    if (hic->buffer.choseong == 0 && hic->buffer.jungseong != 0)
		hic->buffer.choseong = 0x110b;
	}
    }

    hangul_ic_save_preedit_string(hic);
    return true;

flush:
    hangul_ic_flush_internal(hic);
    return false;
}

/**
 * @ingroup hangulic
 * @brief 키 입력을 처리하여 실제로 한글 조합을 하는 함수
 * @param hic @ref HangulInputContext 오브젝트
 * @param ascii 키 이벤트
 * @return @ref HangulInputContext 가 이 키를 사용했으면 true,
 *	     사용하지 않았으면 false
 *
 * ascii 값으로 주어진 키 이벤트를 받아서 내부의 한글 조합 상태를
 * 변화시키고, preedit, commit 스트링을 저장한다.
 *
 * libhangul의 키 이벤트 프로세스는 ASCII 코드 값을 기준으로 처리한다.
 * 이 키 값은 US Qwerty 자판 배열에서의 키 값에 해당한다.
 * 따라서 유럽어 자판을 사용하는 경우에는 해당 키의 ASCII 코드를 직접
 * 전달하면 안되고, 그 키가 US Qwerty 자판이었을 경우에 발생할 수 있는 
 * ASCII 코드 값을 주어야 한다.
 * 또한 ASCII 코드 이므로 Shift 상태는 대문자로 전달이 된다.
 * Capslock이 눌린 경우에는 대소문자를 뒤바꾸어 보내주지 않으면 
 * 마치 Shift가 눌린 것 처럼 동작할 수 있으므로 주의한다.
 * preedit, commit 스트링은 hangul_ic_get_preedit_string(),
 * hangul_ic_get_commit_string() 함수를 이용하여 구할 수 있다.
 * 
 * 이 함수의 사용법에 대한 설명은 @ref hangulicusage 부분을 참조한다.
 *
 * @remarks 이 함수는 @ref HangulInputContext 의 상태를 변화 시킨다.
 */
bool
hangul_ic_process(HangulInputContext *hic, int ascii)
{
    ucschar c;

    if (hic == NULL)
	return false;

    hic->preedit_string[0] = 0;
    hic->commit_string[0] = 0;

    c = hangul_keyboard_map_to_char(hic->keyboard, hic->tableid, ascii);
    if (hic->on_translate != NULL)
	hic->on_translate(hic, ascii, &c, hic->on_translate_data);

    if (ascii == '\b') {
	return hangul_ic_backspace(hic);
    }

    int type = hangul_keyboard_get_type(hic->keyboard);
    switch (type) {
    case HANGUL_KEYBOARD_TYPE_JASO:
    case HANGUL_KEYBOARD_TYPE_JASO_YET:
	return hangul_ic_process_jaso(hic, c);
    case HANGUL_KEYBOARD_TYPE_ROMAJA:
	return hangul_ic_process_romaja(hic, ascii, c);
    default:
	return hangul_ic_process_jamo(hic, c);
    }
}

/**
 * @ingroup hangulic
 * @brief 현재 상태의 preedit string을 구하는 함수
 * @param hic preedit string을 구하고자하는 입력 상태 object
 * @return UCS4 preedit 스트링, 이 스트링은 @a hic 내부의 데이터이므로 
 *         수정하거나 free해서는 안된다.
 * 
 * 이 함수는  @a hic 내부의 현재 상태의 preedit string을 리턴한다.
 * 따라서 hic가 다른 키 이벤트를 처리하고 나면 그 내용이 바뀔 수 있다.
 * 
 * @remarks 이 함수는 @ref HangulInputContext 의 상태를 변화 시키지 않는다.
 */
const ucschar*
hangul_ic_get_preedit_string(HangulInputContext *hic)
{
    if (hic == NULL)
	return NULL;

    return hic->preedit_string;
}

/**
 * @ingroup hangulic
 * @brief 현재 상태의 commit string을 구하는 함수
 * @param hic commit string을 구하고자하는 입력 상태 object
 * @return UCS4 commit 스트링, 이 스트링은 @a hic 내부의 데이터이므로 
 *         수정하거나 free해서는 안된다.
 * 
 * 이 함수는  @a hic 내부의 현재 상태의 commit string을 리턴한다.
 * 따라서 hic가 다른 키 이벤트를 처리하고 나면 그 내용이 바뀔 수 있다.
 *
 * @remarks 이 함수는 @ref HangulInputContext 의 상태를 변화 시키지 않는다.
 */
const ucschar*
hangul_ic_get_commit_string(HangulInputContext *hic)
{
    if (hic == NULL)
	return NULL;

    return hic->commit_string;
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 를 초기상태로 되돌리는 함수
 * @param hic @ref HangulInputContext 를 가리키는 포인터
 * 
 * 이 함수는 @a hic가 가리키는 @ref HangulInputContext 의 상태를 
 * 처음 상태로 되돌린다. preedit 스트링, commit 스트링, flush 스트링이
 * 없어지고, 입력되었던 키에 대한 기록이 없어진다.
 * 영어 상태로 바뀌는 것이 아니다.
 *
 * 비교: hangul_ic_flush()
 *
 * @remarks 이 함수는 @ref HangulInputContext 의 상태를 변화 시킨다.
 */
void
hangul_ic_reset(HangulInputContext *hic)
{
    if (hic == NULL)
	return;

    hic->preedit_string[0] = 0;
    hic->commit_string[0] = 0;
    hic->flushed_string[0] = 0;

    hangul_buffer_clear(&hic->buffer);
}

/* append current preedit to the commit buffer.
 * this function does not clear previously made commit string. */
static void
hangul_ic_flush_internal(HangulInputContext *hic)
{
    hic->preedit_string[0] = 0;

    hangul_ic_save_commit_string(hic);
    hangul_buffer_clear(&hic->buffer);
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 의 입력 상태를 완료하는 함수
 * @param hic @ref HangulInputContext 를 가리키는 포인터
 * @return 조합 완료된 스트링, 스트링의 길이가 0이면 조합 완료된 스트링이 
 *	  없는 것
 *
 * 이 함수는 @a hic가 가리키는 @ref HangulInputContext 의 입력 상태를 완료한다.
 * 조합중이던 스트링을 완성하여 리턴한다. 그리고 입력 상태가 초기 상태로 
 * 되돌아 간다. 조합중이던 글자를 강제로 commit하고 싶을때 사용하는 함수다.
 * 보통의 경우 입력 framework에서 focus가 나갈때 이 함수를 불러서 마지막 
 * 상태를 완료해야 조합중이던 글자를 잃어버리지 않게 된다.
 *
 * 비교: hangul_ic_reset()
 *
 * @remarks 이 함수는 @ref HangulInputContext 의 상태를 변화 시킨다.
 */
const ucschar*
hangul_ic_flush(HangulInputContext *hic)
{
    if (hic == NULL)
	return NULL;

    // get the remaining string and clear the buffer
    hic->preedit_string[0] = 0;
    hic->commit_string[0] = 0;
    hic->flushed_string[0] = 0;

    if (hic->output_mode == HANGUL_OUTPUT_JAMO) {
	hangul_buffer_get_jamo_string(&hic->buffer, hic->flushed_string,
				 N_ELEMENTS(hic->flushed_string));
    } else {
	hangul_buffer_get_string(&hic->buffer, hic->flushed_string,
				 N_ELEMENTS(hic->flushed_string));
    }

    hangul_buffer_clear(&hic->buffer);

    return hic->flushed_string;
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 가 backspace 키를 처리하도록 하는 함수
 * @param hic @ref HangulInputContext 를 가리키는 포인터
 * @return @a hic가 키를 사용했으면 true, 사용하지 않았으면 false
 * 
 * 이 함수는 @a hic가 가리키는 @ref HangulInputContext 의 조합중이던 글자를
 * 뒤에서부터 하나 지우는 기능을 한다. backspace 키를 눌렀을 때 발생하는 
 * 동작을 한다. 따라서 이 함수를 부르고 나면 preedit string이 바뀌므로
 * 반드시 업데이트를 해야 한다.
 *
 * @remarks 이 함수는 @ref HangulInputContext 의 상태를 변화 시킨다.
 */
bool
hangul_ic_backspace(HangulInputContext *hic)
{
    int ret;

    if (hic == NULL)
	return false;

    hic->preedit_string[0] = 0;
    hic->commit_string[0] = 0;

    ret = hangul_buffer_backspace(&hic->buffer);
    if (ret)
	hangul_ic_save_preedit_string(hic);
    return ret;
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 가 조합중인 글자를 가지고 있는지 확인하는 함수
 * @param hic @ref HangulInputContext 를 가리키는 포인터
 *
 * @ref HangulInputContext 가 조합중인 글자가 있으면 true를 리턴한다.
 *
 * @remarks 이 함수는 @ref HangulInputContext 의 상태를 변화 시키지 않는다.
 */
bool
hangul_ic_is_empty(HangulInputContext *hic)
{
    return hangul_buffer_is_empty(&hic->buffer);
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 가 조합중인 초성을 가지고 있는지 확인하는 함수
 * @param hic @ref HangulInputContext 를 가리키는 포인터
 *
 * @ref HangulInputContext 가 조합중인 글자가 초성이 있으면 true를 리턴한다.
 *
 * @remarks 이 함수는 @ref HangulInputContext 의 상태를 변화 시키지 않는다.
 */
bool
hangul_ic_has_choseong(HangulInputContext *hic)
{
    return hangul_buffer_has_choseong(&hic->buffer);
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 가 조합중인 중성을 가지고 있는지 확인하는 함수
 * @param hic @ref HangulInputContext 를 가리키는 포인터
 *
 * @ref HangulInputContext 가 조합중인 글자가 중성이 있으면 true를 리턴한다.
 *
 * @remarks 이 함수는 @ref HangulInputContext 의 상태를 변화 시키지 않는다.
 */
bool
hangul_ic_has_jungseong(HangulInputContext *hic)
{
    return hangul_buffer_has_jungseong(&hic->buffer);
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 가 조합중인 종성을 가지고 있는지 확인하는 함수
 * @param hic @ref HangulInputContext 를 가리키는 포인터
 *
 * @ref HangulInputContext 가 조합중인 글자가 종성이 있으면 true를 리턴한다.
 *
 * @remarks 이 함수는 @ref HangulInputContext 의 상태를 변화 시키지 않는다.
 */
bool
hangul_ic_has_jongseong(HangulInputContext *hic)
{
    return hangul_buffer_has_jongseong(&hic->buffer);
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 의 조합 옵션을 확인하는 함수
 * @param hic @ref HangulInputContext 를 가리키는 포인터
 * @param option @ref HangulInputContext 의 조합 처리 옵션.
 *        @ref hangul_ic_set_option 에서 사용 가능한 옵션을 확인하라.
 *
 * @return @ref HangulInputContext 에 설정된 옵션값을 리턴한다.
 */
bool
hangul_ic_get_option(HangulInputContext* hic, int option)
{
    switch (option) {
    case HANGUL_IC_OPTION_AUTO_REORDER:
	return hic->option_auto_reorder;
    case HANGUL_IC_OPTION_COMBI_ON_DOUBLE_STROKE:
	return hic->option_combi_on_double_stroke;
    case HANGUL_IC_OPTION_NON_CHOSEONG_COMBI:
	return hic->option_non_choseong_combi;
    }

    return false;
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 의 조합 옵션을 설정하는 함수
 * @param hic @ref HangulInputContext 를 가리키는 포인터
 * @param option @ref HangulInputContext 의 조합 처리 옵션.
 *               아래와 같은 옵션이 선택 가능하다.
 *    - HANGUL_IC_OPTION_AUTO_REORDER
 *      - 자동 순서 교정 옵션. 모아치기를 위해서는 켜야 한다.
 *        예) true면 ㅏ+ㄱ -> 가 로 완성시켜 줌.
 *    - HANGUL_IC_OPTION_COMBI_ON_DOUBLE_STROKE
 *      - 두벌식 자판에서 자음 연타로 된소리로 조합하는 옵션.
 *        원래 자판이 자음 연타로 동작하는 세벌식이나 옛한글
 *        자판에서는 옵션이 동작하지 않는다.
 *        MS IME와 호환을 위해서 사용.
 *        예) true면 ㄱ+ㄱ -> ㄲ 으로 조합시켜 줌.
 *    - HANGUL_IC_OPTION_NON_CHOSEONG_COMBI
 *      - 두벌식 자판에서 초성에 없는 겹자음을 조합허용하는 옵션.
 *        두벌식 자판 이외에는 옵션이 동작하지 않는다.
 *        MS IME와 호환을 위해서 사용.
 *        예) true면 ㄱ+ㅅ -> ㄳ 으로 조합시켜 줌.
 * @param value 설정하고자 하는 값, true 또는 false
 */
void
hangul_ic_set_option(HangulInputContext* hic, int option, bool value)
{
    switch (option) {
    case HANGUL_IC_OPTION_AUTO_REORDER:
	hic->option_auto_reorder = value;
	break;
    case HANGUL_IC_OPTION_COMBI_ON_DOUBLE_STROKE:
	hic->option_combi_on_double_stroke = value;
	break;
    case HANGUL_IC_OPTION_NON_CHOSEONG_COMBI:
	hic->option_non_choseong_combi = value;
	break;
    }
}

void
hangul_ic_set_output_mode(HangulInputContext *hic, int mode)
{
    if (hic == NULL)
	return;

    if (!hic->use_jamo_mode_only)
	hic->output_mode = mode;
}

void
hangul_ic_connect_translate (HangulInputContext* hic,
                             HangulOnTranslate callback,
                             void* user_data)
{
    if (hic != NULL) {
	hic->on_translate      = callback;
	hic->on_translate_data = user_data;
    }
}

void
hangul_ic_connect_transition(HangulInputContext* hic,
                             HangulOnTransition callback,
                             void* user_data)
{
    if (hic != NULL) {
	hic->on_transition      = callback;
	hic->on_transition_data = user_data;
    }
}

void hangul_ic_connect_callback(HangulInputContext* hic, const char* event,
				void* callback, void* user_data)
{
    if (hic == NULL || event == NULL)
	return;

    if (strcasecmp(event, "translate") == 0) {
        *(void**)(&hic->on_translate) = callback;
	hic->on_translate_data = user_data;
    } else if (strcasecmp(event, "transition") == 0) {
        *(void**)(&hic->on_transition) = callback;
	hic->on_transition_data = user_data;
    }
}

void
hangul_ic_set_keyboard(HangulInputContext *hic, const HangulKeyboard* keyboard)
{
    if (hic == NULL)
	return;

    hic->keyboard = keyboard;
    hic->tableid = 0;
}

void
hangul_ic_switch_keyboard_table(HangulInputContext *hic, int tableid)
{
    if (hic == NULL)
        return;

    hic->tableid = tableid;
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 의 자판 배열을 바꾸는 함수
 * @param hic @ref HangulInputContext 오브젝트
 * @param id 선택하고자 하는 자판, 아래와 같은 값을 선택할 수 있다.
 *   @li "2"   @ref layout_2 자판
 *   @li "2y"  @ref layout_2y 자판
 *   @li "3f"  @ref layout_3f 자판
 *   @li "39"  @ref layout_390 자판
 *   @li "3s"  @ref layout_3s 자판
 *   @li "3y"  @ref layout_3y 자판
 *   @li "32"  @ref layout_32 자판
 *   @li "ro"  @ref layout_ro 자판
 *
 *   libhangul이 지원하는 자판에 대한 정보는 @ref hangulkeyboards 페이지를
 *   참조하라.
 * @return 없음
 * 
 * 이 함수는 @ref HangulInputContext 의 자판을 @a id로 지정된 것으로 변경한다.
 * 
 * @remarks 이 함수는 @ref HangulInputContext 의 내부 조합 상태에는 영향을
 * 미치지 않는다.  따라서 입력 중간에 자판을 변경하더라도 조합 상태는 유지된다.
 */
void
hangul_ic_select_keyboard(HangulInputContext *hic, const char* id)
{
    const HangulKeyboard* keyboard;

    if (hic == NULL)
	return;

    if (id == NULL)
	id = "2";

    keyboard = hangul_keyboard_list_get_keyboard(id);
    hangul_ic_set_keyboard(hic, keyboard);
}

void
hangul_ic_set_combination(HangulInputContext *hic,
			  const HangulCombination* combination)
{
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 오브젝트를 생성한다.
 * @param keyboard 사용하고자 하는 키보드, 사용 가능한 값에 대해서는
 *	hangul_ic_select_keyboard() 함수 설명을 참조한다.
 * @return 새로 생성된 @ref HangulInputContext 에 대한 포인터
 * 
 * 이 함수는 한글 조합 기능을 제공하는 @ref HangulInputContext 오브젝트를 
 * 생성한다. 생성할때 지정한 자판은 나중에 hangul_ic_select_keyboard() 함수로
 * 다른 자판으로 변경이 가능하다.
 * 더이상 사용하지 않을 때에는 hangul_ic_delete() 함수로 삭제해야 한다.
 */
HangulInputContext*
hangul_ic_new(const char* keyboard)
{
    HangulInputContext *hic;

    hic = malloc(sizeof(HangulInputContext));
    if (hic == NULL)
	return NULL;

    hic->keyboard = NULL;
    hic->tableid = 0;

    hic->preedit_string[0] = 0;
    hic->commit_string[0] = 0;
    hic->flushed_string[0] = 0;

    hic->on_translate      = NULL;
    hic->on_translate_data = NULL;

    hic->on_transition      = NULL;
    hic->on_transition_data = NULL;

    hic->use_jamo_mode_only = FALSE;

    hic->option_auto_reorder = false;
    hic->option_combi_on_double_stroke = false;
    hic->option_non_choseong_combi = true;

    hangul_ic_set_output_mode(hic, HANGUL_OUTPUT_SYLLABLE);
    hangul_ic_select_keyboard(hic, keyboard);

    hangul_buffer_clear(&hic->buffer);

    return hic;
}

/**
 * @ingroup hangulic
 * @brief @ref HangulInputContext 를 삭제하는 함수
 * @param hic @ref HangulInputContext 오브젝트
 * 
 * @a hic가 가리키는 @ref HangulInputContext 오브젝트의 메모리를 해제한다.
 * hangul_ic_new() 함수로 생성된 모든 @ref HangulInputContext 오브젝트는
 * 이 함수로 메모리해제를 해야 한다.
 * 메모리 해제 과정에서 상태 변화는 일어나지 않으므로 마지막 입력된 
 * 조합중이던 내용은 사라지게 된다.
 */
void
hangul_ic_delete(HangulInputContext *hic)
{
    if (hic == NULL)
	return;

    free(hic);
}

/** @deprecated 이 함수 대신 @ref hangul_keyboard_list_get_count 를 사용하라 */
unsigned int
hangul_ic_get_n_keyboards()
{
    return hangul_keyboard_list_get_count();
}

/** @deprecated 이 함수 대신 @ref hangul_keyboard_list_get_keyboard_id 를 사용하라 */
const char*
hangul_ic_get_keyboard_id(unsigned index_)
{
    return hangul_keyboard_list_get_keyboard_id(index_);
}

/** @deprecated 이 함수 대신 @ref hangul_keyboard_list_get_keyboard_name 을 사용하라 */
const char*
hangul_ic_get_keyboard_name(unsigned index_)
{
    return hangul_keyboard_list_get_keyboard_name(index_);
}

/**
 * @ingroup hangulic
 * @brief 주어진 hic가 transliteration method인지 판별
 * @param hic 상태를 알고자 하는 HangulInputContext 포인터
 * @return hic가 transliteration method인 경우 true를 리턴, 아니면 false
 *
 * 이 함수는 @a hic 가 transliteration method인지 판별하는 함수다.
 * 이 함수가 false를 리턴할 경우에는 process 함수에 keycode를 넘기기 전에
 * 키보드 자판 배열에 독립적인 값으로 변환한 후 넘겨야 한다.
 * 그렇지 않으면 유럽어 자판과 한국어 자판을 같이 쓸때 한글 입력이 제대로
 * 되지 않는다.
 */
bool
hangul_ic_is_transliteration(HangulInputContext *hic)
{
    int type;

    if (hic == NULL)
	return false;

    type = hangul_keyboard_get_type(hic->keyboard);
    if (type == HANGUL_KEYBOARD_TYPE_ROMAJA)
	return true;

    return false;
}

/**
 * @ingroup hangulic
 * @brief libhangul을 초기화 하는 함수.
 * @param user_defined_keyboard_path 한글 키보드 파일을 읽을 디렉토리 패스.
 *        PATH 환경 변수와 같이 :으로 구분하여 여러개를 지정할 수 있다.
 *        NULL을 주면 내장 기본값을 사용한다.
 *
 * libhangul의 함수를 사용하기 전에 호출해야 한다.
 */
int
hangul_init(const char* user_defined_keyboard_path)
{
    int res;
    res = hangul_keyboard_list_init(user_defined_keyboard_path);
    return res;
}

/**
 * @ingroup hangulic
 * @brief libhangul에서 사용한 리소스를 해제하는 함수.
 *
 * libhangul의 함수의 사용이 끝나면 호출해야 한다.
 */
int
hangul_fini()
{
    int res;
    res = hangul_keyboard_list_fini();
    return res;
}
