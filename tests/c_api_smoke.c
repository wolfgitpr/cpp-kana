// The C ABI's own smoke test. Compiled as C (not C++), because the header has to work for a C compiler
// and linking from C is the thing the Python and C# shells rely on.
//
// It walks the parts a binding actually uses: options_init with the struct_size contract, a conversion in
// each direction, the per-mora array, the error channel, and the memory rules.

#include <cpp-kana2/ext/c_api.h>

#include <stdio.h>
#include <string.h>

static int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            printf("FAIL %s (%s:%d)\n", #condition, __FILE__, __LINE__);                                               \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

#define CHECK_STR(actual, expected)                                                                                    \
    do {                                                                                                               \
        const char *check_actual = (actual);                                                                           \
        const char *check_expected = (expected);                                                                       \
        if (check_actual == NULL || strcmp(check_actual, check_expected) != 0) {                                       \
            printf("FAIL %s (%s:%d): got %s, want %s\n", #actual, __FILE__, __LINE__,                                  \
                   check_actual == NULL ? "(null)" : check_actual, check_expected);                                    \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

int main(void) {
    // --- version / text -------------------------------------------------------------------------
    CHECK_STR(cppk2_version(), CPP_KANA2_VERSION_STRING);
    CHECK_STR(cppk2_code_text(CPPK2_ERR_INVALID_UTF8), "invalid UTF-8");
    CHECK_STR(cppk2_code_text(9999), "unknown error");

    // --- options --------------------------------------------------------------------------------
    cppk2_romaji_options romaji_options;
    memset(&romaji_options, 0, sizeof(romaji_options));
    CHECK(cppk2_romaji_options_init(&romaji_options, sizeof(romaji_options)) == CPPK2_OK);
    CHECK(romaji_options.struct_size == sizeof(romaji_options));
    CHECK(romaji_options.sokuon == CPPK2_SOKUON_GEMINATE);
    romaji_options.separator = CPPK2_SEPARATOR_NONE;

    // An options struct smaller than this header's sizeof must be refused, not read past.
    cppk2_romaji_options too_small;
    too_small.struct_size = 4;
    CHECK(cppk2_romaji_options_init(&too_small, 4) == CPPK2_ERR_INVALID_ARGUMENT);

    cppk2_kana_options kana_options;
    memset(&kana_options, 0, sizeof(kana_options));
    CHECK(cppk2_kana_options_init(&kana_options, sizeof(kana_options)) == CPPK2_OK);

    // --- kana -> romaji -------------------------------------------------------------------------
    cppk2_error error;
    memset(&error, 0, sizeof(error));
    cppk2_conversion *conversion = NULL;
    CHECK(cppk2_kana_to_romaji("きって", &romaji_options, &conversion, &error) == CPPK2_OK);
    CHECK(conversion != NULL);
    if (conversion != NULL) {
        CHECK_STR(conversion->rendered, "kitte");
        CHECK_STR(conversion->text, "きって");
        CHECK(conversion->mora_count == 3);
        CHECK(conversion->ok == 1);
        if (conversion->mora_count == 3) {
            CHECK_STR(conversion->moras[1].text, "っ");
            CHECK_STR(conversion->moras[1].rendered, "t");
            CHECK(conversion->moras[1].kind == CPPK2_MORA_SOKUON);
            CHECK(conversion->moras[1].code == CPPK2_OK);
        }
        cppk2_conversion_free(conversion);
        conversion = NULL;
    }

    // --- romaji -> kana -------------------------------------------------------------------------
    CHECK(cppk2_romaji_to_kana("konnichiha", &kana_options, &conversion, &error) == CPPK2_OK);
    CHECK(conversion != NULL);
    if (conversion != NULL) {
        CHECK_STR(conversion->rendered, "こんにちは");
        cppk2_conversion_free(conversion);
        conversion = NULL;
    }

    // --- script conversion ----------------------------------------------------------------------
    CHECK(cppk2_convert_script("ｶﾀｶﾅ", CPPK2_SCRIPT_HIRAGANA, CPPK2_UNKNOWN_KEEP, &conversion, &error) == CPPK2_OK);
    CHECK(conversion != NULL);
    if (conversion != NULL) {
        CHECK_STR(conversion->rendered, "かたかな");
        cppk2_conversion_free(conversion);
        conversion = NULL;
    }

    // --- failures are codes, not exceptions, and the detail is ours to free ---------------------
    CHECK(cppk2_kana_to_romaji("\xE3\x81", NULL, &conversion, &error) == CPPK2_ERR_INVALID_UTF8);
    CHECK(conversion == NULL);
    CHECK(error.code == CPPK2_ERR_INVALID_UTF8);
    CHECK(error.detail != NULL && strlen(error.detail) > 0);
    cppk2_string_free(error.detail);
    error.detail = NULL;

    // The out parameter must be cleared before ANY failure, not just before the ones that happen after the
    // argument check: a caller that frees *out after a failed call would otherwise free stack garbage.
    conversion = (cppk2_conversion *)0x1; /* a poison value: if the library leaves it alone, the CHECK below fails */
    CHECK(cppk2_kana_to_romaji(NULL, NULL, &conversion, &error) == CPPK2_ERR_INVALID_ARGUMENT);
    CHECK(conversion == NULL);
    CHECK(error.detail != NULL);
    cppk2_string_free(error.detail);
    error.detail = NULL;

    conversion = (cppk2_conversion *)0x1;
    CHECK(cppk2_romaji_to_kana(NULL, NULL, &conversion, &error) == CPPK2_ERR_INVALID_ARGUMENT);
    CHECK(conversion == NULL);
    cppk2_string_free(error.detail);
    error.detail = NULL;

    conversion = (cppk2_conversion *)0x1;
    CHECK(cppk2_convert_script(NULL, CPPK2_SCRIPT_HIRAGANA, CPPK2_UNKNOWN_KEEP, &conversion, &error) ==
          CPPK2_ERR_INVALID_ARGUMENT);
    CHECK(conversion == NULL);
    cppk2_string_free(error.detail);
    error.detail = NULL;

    // --- NULL is tolerated where it can be ------------------------------------------------------
    cppk2_conversion_free(NULL);
    cppk2_string_free(NULL);

    if (g_failures != 0) {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("c_api_smoke: all checks passed\n");
    return 0;
}
