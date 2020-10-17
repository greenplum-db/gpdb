/* This file was generated automatically by the Snowball to ISO C compiler */
/* http://snowballstem.org/ */

#include "header.h"

#ifdef __cplusplus
extern "C" {
#endif
extern int turkish_UTF_8_stem(struct SN_env * z);
#ifdef __cplusplus
}
#endif
static int r_stem_suffix_chain_before_ki(struct SN_env * z);
static int r_stem_noun_suffixes(struct SN_env * z);
static int r_stem_nominal_verb_suffixes(struct SN_env * z);
static int r_postlude(struct SN_env * z);
static int r_post_process_last_consonants(struct SN_env * z);
static int r_more_than_one_syllable_word(struct SN_env * z);
static int r_mark_suffix_with_optional_s_consonant(struct SN_env * z);
static int r_mark_suffix_with_optional_n_consonant(struct SN_env * z);
static int r_mark_suffix_with_optional_U_vowel(struct SN_env * z);
static int r_mark_suffix_with_optional_y_consonant(struct SN_env * z);
static int r_mark_ysA(struct SN_env * z);
static int r_mark_ymUs_(struct SN_env * z);
static int r_mark_yken(struct SN_env * z);
static int r_mark_yDU(struct SN_env * z);
static int r_mark_yUz(struct SN_env * z);
static int r_mark_yUm(struct SN_env * z);
static int r_mark_yU(struct SN_env * z);
static int r_mark_ylA(struct SN_env * z);
static int r_mark_yA(struct SN_env * z);
static int r_mark_possessives(struct SN_env * z);
static int r_mark_sUnUz(struct SN_env * z);
static int r_mark_sUn(struct SN_env * z);
static int r_mark_sU(struct SN_env * z);
static int r_mark_nUz(struct SN_env * z);
static int r_mark_nUn(struct SN_env * z);
static int r_mark_nU(struct SN_env * z);
static int r_mark_ndAn(struct SN_env * z);
static int r_mark_ndA(struct SN_env * z);
static int r_mark_ncA(struct SN_env * z);
static int r_mark_nA(struct SN_env * z);
static int r_mark_lArI(struct SN_env * z);
static int r_mark_lAr(struct SN_env * z);
static int r_mark_ki(struct SN_env * z);
static int r_mark_DUr(struct SN_env * z);
static int r_mark_DAn(struct SN_env * z);
static int r_mark_DA(struct SN_env * z);
static int r_mark_cAsInA(struct SN_env * z);
static int r_is_reserved_word(struct SN_env * z);
static int r_check_vowel_harmony(struct SN_env * z);
static int r_append_U_to_stems_ending_with_d_or_g(struct SN_env * z);
#ifdef __cplusplus
extern "C" {
#endif


extern struct SN_env * turkish_UTF_8_create_env(void);
extern void turkish_UTF_8_close_env(struct SN_env * z);


#ifdef __cplusplus
}
#endif
static const symbol s_0_0[1] = { 'm' };
static const symbol s_0_1[1] = { 'n' };
static const symbol s_0_2[3] = { 'm', 'i', 'z' };
static const symbol s_0_3[3] = { 'n', 'i', 'z' };
static const symbol s_0_4[3] = { 'm', 'u', 'z' };
static const symbol s_0_5[3] = { 'n', 'u', 'z' };
static const symbol s_0_6[4] = { 'm', 0xC4, 0xB1, 'z' };
static const symbol s_0_7[4] = { 'n', 0xC4, 0xB1, 'z' };
static const symbol s_0_8[4] = { 'm', 0xC3, 0xBC, 'z' };
static const symbol s_0_9[4] = { 'n', 0xC3, 0xBC, 'z' };

static const struct among a_0[10] =
{
/*  0 */ { 1, s_0_0, -1, -1, 0},
/*  1 */ { 1, s_0_1, -1, -1, 0},
/*  2 */ { 3, s_0_2, -1, -1, 0},
/*  3 */ { 3, s_0_3, -1, -1, 0},
/*  4 */ { 3, s_0_4, -1, -1, 0},
/*  5 */ { 3, s_0_5, -1, -1, 0},
/*  6 */ { 4, s_0_6, -1, -1, 0},
/*  7 */ { 4, s_0_7, -1, -1, 0},
/*  8 */ { 4, s_0_8, -1, -1, 0},
/*  9 */ { 4, s_0_9, -1, -1, 0}
};

static const symbol s_1_0[4] = { 'l', 'e', 'r', 'i' };
static const symbol s_1_1[5] = { 'l', 'a', 'r', 0xC4, 0xB1 };

static const struct among a_1[2] =
{
/*  0 */ { 4, s_1_0, -1, -1, 0},
/*  1 */ { 5, s_1_1, -1, -1, 0}
};

static const symbol s_2_0[2] = { 'n', 'i' };
static const symbol s_2_1[2] = { 'n', 'u' };
static const symbol s_2_2[3] = { 'n', 0xC4, 0xB1 };
static const symbol s_2_3[3] = { 'n', 0xC3, 0xBC };

static const struct among a_2[4] =
{
/*  0 */ { 2, s_2_0, -1, -1, 0},
/*  1 */ { 2, s_2_1, -1, -1, 0},
/*  2 */ { 3, s_2_2, -1, -1, 0},
/*  3 */ { 3, s_2_3, -1, -1, 0}
};

static const symbol s_3_0[2] = { 'i', 'n' };
static const symbol s_3_1[2] = { 'u', 'n' };
static const symbol s_3_2[3] = { 0xC4, 0xB1, 'n' };
static const symbol s_3_3[3] = { 0xC3, 0xBC, 'n' };

static const struct among a_3[4] =
{
/*  0 */ { 2, s_3_0, -1, -1, 0},
/*  1 */ { 2, s_3_1, -1, -1, 0},
/*  2 */ { 3, s_3_2, -1, -1, 0},
/*  3 */ { 3, s_3_3, -1, -1, 0}
};

static const symbol s_4_0[1] = { 'a' };
static const symbol s_4_1[1] = { 'e' };

static const struct among a_4[2] =
{
/*  0 */ { 1, s_4_0, -1, -1, 0},
/*  1 */ { 1, s_4_1, -1, -1, 0}
};

static const symbol s_5_0[2] = { 'n', 'a' };
static const symbol s_5_1[2] = { 'n', 'e' };

static const struct among a_5[2] =
{
/*  0 */ { 2, s_5_0, -1, -1, 0},
/*  1 */ { 2, s_5_1, -1, -1, 0}
};

static const symbol s_6_0[2] = { 'd', 'a' };
static const symbol s_6_1[2] = { 't', 'a' };
static const symbol s_6_2[2] = { 'd', 'e' };
static const symbol s_6_3[2] = { 't', 'e' };

static const struct among a_6[4] =
{
/*  0 */ { 2, s_6_0, -1, -1, 0},
/*  1 */ { 2, s_6_1, -1, -1, 0},
/*  2 */ { 2, s_6_2, -1, -1, 0},
/*  3 */ { 2, s_6_3, -1, -1, 0}
};

static const symbol s_7_0[3] = { 'n', 'd', 'a' };
static const symbol s_7_1[3] = { 'n', 'd', 'e' };

static const struct among a_7[2] =
{
/*  0 */ { 3, s_7_0, -1, -1, 0},
/*  1 */ { 3, s_7_1, -1, -1, 0}
};

static const symbol s_8_0[3] = { 'd', 'a', 'n' };
static const symbol s_8_1[3] = { 't', 'a', 'n' };
static const symbol s_8_2[3] = { 'd', 'e', 'n' };
static const symbol s_8_3[3] = { 't', 'e', 'n' };

static const struct among a_8[4] =
{
/*  0 */ { 3, s_8_0, -1, -1, 0},
/*  1 */ { 3, s_8_1, -1, -1, 0},
/*  2 */ { 3, s_8_2, -1, -1, 0},
/*  3 */ { 3, s_8_3, -1, -1, 0}
};

static const symbol s_9_0[4] = { 'n', 'd', 'a', 'n' };
static const symbol s_9_1[4] = { 'n', 'd', 'e', 'n' };

static const struct among a_9[2] =
{
/*  0 */ { 4, s_9_0, -1, -1, 0},
/*  1 */ { 4, s_9_1, -1, -1, 0}
};

static const symbol s_10_0[2] = { 'l', 'a' };
static const symbol s_10_1[2] = { 'l', 'e' };

static const struct among a_10[2] =
{
/*  0 */ { 2, s_10_0, -1, -1, 0},
/*  1 */ { 2, s_10_1, -1, -1, 0}
};

static const symbol s_11_0[2] = { 'c', 'a' };
static const symbol s_11_1[2] = { 'c', 'e' };

static const struct among a_11[2] =
{
/*  0 */ { 2, s_11_0, -1, -1, 0},
/*  1 */ { 2, s_11_1, -1, -1, 0}
};

static const symbol s_12_0[2] = { 'i', 'm' };
static const symbol s_12_1[2] = { 'u', 'm' };
static const symbol s_12_2[3] = { 0xC4, 0xB1, 'm' };
static const symbol s_12_3[3] = { 0xC3, 0xBC, 'm' };

static const struct among a_12[4] =
{
/*  0 */ { 2, s_12_0, -1, -1, 0},
/*  1 */ { 2, s_12_1, -1, -1, 0},
/*  2 */ { 3, s_12_2, -1, -1, 0},
/*  3 */ { 3, s_12_3, -1, -1, 0}
};

static const symbol s_13_0[3] = { 's', 'i', 'n' };
static const symbol s_13_1[3] = { 's', 'u', 'n' };
static const symbol s_13_2[4] = { 's', 0xC4, 0xB1, 'n' };
static const symbol s_13_3[4] = { 's', 0xC3, 0xBC, 'n' };

static const struct among a_13[4] =
{
/*  0 */ { 3, s_13_0, -1, -1, 0},
/*  1 */ { 3, s_13_1, -1, -1, 0},
/*  2 */ { 4, s_13_2, -1, -1, 0},
/*  3 */ { 4, s_13_3, -1, -1, 0}
};

static const symbol s_14_0[2] = { 'i', 'z' };
static const symbol s_14_1[2] = { 'u', 'z' };
static const symbol s_14_2[3] = { 0xC4, 0xB1, 'z' };
static const symbol s_14_3[3] = { 0xC3, 0xBC, 'z' };

static const struct among a_14[4] =
{
/*  0 */ { 2, s_14_0, -1, -1, 0},
/*  1 */ { 2, s_14_1, -1, -1, 0},
/*  2 */ { 3, s_14_2, -1, -1, 0},
/*  3 */ { 3, s_14_3, -1, -1, 0}
};

static const symbol s_15_0[5] = { 's', 'i', 'n', 'i', 'z' };
static const symbol s_15_1[5] = { 's', 'u', 'n', 'u', 'z' };
static const symbol s_15_2[7] = { 's', 0xC4, 0xB1, 'n', 0xC4, 0xB1, 'z' };
static const symbol s_15_3[7] = { 's', 0xC3, 0xBC, 'n', 0xC3, 0xBC, 'z' };

static const struct among a_15[4] =
{
/*  0 */ { 5, s_15_0, -1, -1, 0},
/*  1 */ { 5, s_15_1, -1, -1, 0},
/*  2 */ { 7, s_15_2, -1, -1, 0},
/*  3 */ { 7, s_15_3, -1, -1, 0}
};

static const symbol s_16_0[3] = { 'l', 'a', 'r' };
static const symbol s_16_1[3] = { 'l', 'e', 'r' };

static const struct among a_16[2] =
{
/*  0 */ { 3, s_16_0, -1, -1, 0},
/*  1 */ { 3, s_16_1, -1, -1, 0}
};

static const symbol s_17_0[3] = { 'n', 'i', 'z' };
static const symbol s_17_1[3] = { 'n', 'u', 'z' };
static const symbol s_17_2[4] = { 'n', 0xC4, 0xB1, 'z' };
static const symbol s_17_3[4] = { 'n', 0xC3, 0xBC, 'z' };

static const struct among a_17[4] =
{
/*  0 */ { 3, s_17_0, -1, -1, 0},
/*  1 */ { 3, s_17_1, -1, -1, 0},
/*  2 */ { 4, s_17_2, -1, -1, 0},
/*  3 */ { 4, s_17_3, -1, -1, 0}
};

static const symbol s_18_0[3] = { 'd', 'i', 'r' };
static const symbol s_18_1[3] = { 't', 'i', 'r' };
static const symbol s_18_2[3] = { 'd', 'u', 'r' };
static const symbol s_18_3[3] = { 't', 'u', 'r' };
static const symbol s_18_4[4] = { 'd', 0xC4, 0xB1, 'r' };
static const symbol s_18_5[4] = { 't', 0xC4, 0xB1, 'r' };
static const symbol s_18_6[4] = { 'd', 0xC3, 0xBC, 'r' };
static const symbol s_18_7[4] = { 't', 0xC3, 0xBC, 'r' };

static const struct among a_18[8] =
{
/*  0 */ { 3, s_18_0, -1, -1, 0},
/*  1 */ { 3, s_18_1, -1, -1, 0},
/*  2 */ { 3, s_18_2, -1, -1, 0},
/*  3 */ { 3, s_18_3, -1, -1, 0},
/*  4 */ { 4, s_18_4, -1, -1, 0},
/*  5 */ { 4, s_18_5, -1, -1, 0},
/*  6 */ { 4, s_18_6, -1, -1, 0},
/*  7 */ { 4, s_18_7, -1, -1, 0}
};

static const symbol s_19_0[7] = { 'c', 'a', 's', 0xC4, 0xB1, 'n', 'a' };
static const symbol s_19_1[6] = { 'c', 'e', 's', 'i', 'n', 'e' };

static const struct among a_19[2] =
{
/*  0 */ { 7, s_19_0, -1, -1, 0},
/*  1 */ { 6, s_19_1, -1, -1, 0}
};

static const symbol s_20_0[2] = { 'd', 'i' };
static const symbol s_20_1[2] = { 't', 'i' };
static const symbol s_20_2[3] = { 'd', 'i', 'k' };
static const symbol s_20_3[3] = { 't', 'i', 'k' };
static const symbol s_20_4[3] = { 'd', 'u', 'k' };
static const symbol s_20_5[3] = { 't', 'u', 'k' };
static const symbol s_20_6[4] = { 'd', 0xC4, 0xB1, 'k' };
static const symbol s_20_7[4] = { 't', 0xC4, 0xB1, 'k' };
static const symbol s_20_8[4] = { 'd', 0xC3, 0xBC, 'k' };
static const symbol s_20_9[4] = { 't', 0xC3, 0xBC, 'k' };
static const symbol s_20_10[3] = { 'd', 'i', 'm' };
static const symbol s_20_11[3] = { 't', 'i', 'm' };
static const symbol s_20_12[3] = { 'd', 'u', 'm' };
static const symbol s_20_13[3] = { 't', 'u', 'm' };
static const symbol s_20_14[4] = { 'd', 0xC4, 0xB1, 'm' };
static const symbol s_20_15[4] = { 't', 0xC4, 0xB1, 'm' };
static const symbol s_20_16[4] = { 'd', 0xC3, 0xBC, 'm' };
static const symbol s_20_17[4] = { 't', 0xC3, 0xBC, 'm' };
static const symbol s_20_18[3] = { 'd', 'i', 'n' };
static const symbol s_20_19[3] = { 't', 'i', 'n' };
static const symbol s_20_20[3] = { 'd', 'u', 'n' };
static const symbol s_20_21[3] = { 't', 'u', 'n' };
static const symbol s_20_22[4] = { 'd', 0xC4, 0xB1, 'n' };
static const symbol s_20_23[4] = { 't', 0xC4, 0xB1, 'n' };
static const symbol s_20_24[4] = { 'd', 0xC3, 0xBC, 'n' };
static const symbol s_20_25[4] = { 't', 0xC3, 0xBC, 'n' };
static const symbol s_20_26[2] = { 'd', 'u' };
static const symbol s_20_27[2] = { 't', 'u' };
static const symbol s_20_28[3] = { 'd', 0xC4, 0xB1 };
static const symbol s_20_29[3] = { 't', 0xC4, 0xB1 };
static const symbol s_20_30[3] = { 'd', 0xC3, 0xBC };
static const symbol s_20_31[3] = { 't', 0xC3, 0xBC };

static const struct among a_20[32] =
{
/*  0 */ { 2, s_20_0, -1, -1, 0},
/*  1 */ { 2, s_20_1, -1, -1, 0},
/*  2 */ { 3, s_20_2, -1, -1, 0},
/*  3 */ { 3, s_20_3, -1, -1, 0},
/*  4 */ { 3, s_20_4, -1, -1, 0},
/*  5 */ { 3, s_20_5, -1, -1, 0},
/*  6 */ { 4, s_20_6, -1, -1, 0},
/*  7 */ { 4, s_20_7, -1, -1, 0},
/*  8 */ { 4, s_20_8, -1, -1, 0},
/*  9 */ { 4, s_20_9, -1, -1, 0},
/* 10 */ { 3, s_20_10, -1, -1, 0},
/* 11 */ { 3, s_20_11, -1, -1, 0},
/* 12 */ { 3, s_20_12, -1, -1, 0},
/* 13 */ { 3, s_20_13, -1, -1, 0},
/* 14 */ { 4, s_20_14, -1, -1, 0},
/* 15 */ { 4, s_20_15, -1, -1, 0},
/* 16 */ { 4, s_20_16, -1, -1, 0},
/* 17 */ { 4, s_20_17, -1, -1, 0},
/* 18 */ { 3, s_20_18, -1, -1, 0},
/* 19 */ { 3, s_20_19, -1, -1, 0},
/* 20 */ { 3, s_20_20, -1, -1, 0},
/* 21 */ { 3, s_20_21, -1, -1, 0},
/* 22 */ { 4, s_20_22, -1, -1, 0},
/* 23 */ { 4, s_20_23, -1, -1, 0},
/* 24 */ { 4, s_20_24, -1, -1, 0},
/* 25 */ { 4, s_20_25, -1, -1, 0},
/* 26 */ { 2, s_20_26, -1, -1, 0},
/* 27 */ { 2, s_20_27, -1, -1, 0},
/* 28 */ { 3, s_20_28, -1, -1, 0},
/* 29 */ { 3, s_20_29, -1, -1, 0},
/* 30 */ { 3, s_20_30, -1, -1, 0},
/* 31 */ { 3, s_20_31, -1, -1, 0}
};

static const symbol s_21_0[2] = { 's', 'a' };
static const symbol s_21_1[2] = { 's', 'e' };
static const symbol s_21_2[3] = { 's', 'a', 'k' };
static const symbol s_21_3[3] = { 's', 'e', 'k' };
static const symbol s_21_4[3] = { 's', 'a', 'm' };
static const symbol s_21_5[3] = { 's', 'e', 'm' };
static const symbol s_21_6[3] = { 's', 'a', 'n' };
static const symbol s_21_7[3] = { 's', 'e', 'n' };

static const struct among a_21[8] =
{
/*  0 */ { 2, s_21_0, -1, -1, 0},
/*  1 */ { 2, s_21_1, -1, -1, 0},
/*  2 */ { 3, s_21_2, -1, -1, 0},
/*  3 */ { 3, s_21_3, -1, -1, 0},
/*  4 */ { 3, s_21_4, -1, -1, 0},
/*  5 */ { 3, s_21_5, -1, -1, 0},
/*  6 */ { 3, s_21_6, -1, -1, 0},
/*  7 */ { 3, s_21_7, -1, -1, 0}
};

static const symbol s_22_0[4] = { 'm', 'i', 0xC5, 0x9F };
static const symbol s_22_1[4] = { 'm', 'u', 0xC5, 0x9F };
static const symbol s_22_2[5] = { 'm', 0xC4, 0xB1, 0xC5, 0x9F };
static const symbol s_22_3[5] = { 'm', 0xC3, 0xBC, 0xC5, 0x9F };

static const struct among a_22[4] =
{
/*  0 */ { 4, s_22_0, -1, -1, 0},
/*  1 */ { 4, s_22_1, -1, -1, 0},
/*  2 */ { 5, s_22_2, -1, -1, 0},
/*  3 */ { 5, s_22_3, -1, -1, 0}
};

static const symbol s_23_0[1] = { 'b' };
static const symbol s_23_1[1] = { 'c' };
static const symbol s_23_2[1] = { 'd' };
static const symbol s_23_3[2] = { 0xC4, 0x9F };

static const struct among a_23[4] =
{
/*  0 */ { 1, s_23_0, -1, 1, 0},
/*  1 */ { 1, s_23_1, -1, 2, 0},
/*  2 */ { 1, s_23_2, -1, 3, 0},
/*  3 */ { 2, s_23_3, -1, 4, 0}
};

static const unsigned char g_vowel[] = { 17, 65, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 8, 0, 0, 0, 0, 0, 0, 1 };

static const unsigned char g_U[] = { 1, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 1 };

static const unsigned char g_vowel1[] = { 1, 64, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

static const unsigned char g_vowel2[] = { 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 130 };

static const unsigned char g_vowel3[] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

static const unsigned char g_vowel4[] = { 17 };

static const unsigned char g_vowel5[] = { 65 };

static const unsigned char g_vowel6[] = { 65 };

static const symbol s_0[] = { 0xC4, 0xB1 };
static const symbol s_1[] = { 0xC3, 0xB6 };
static const symbol s_2[] = { 0xC3, 0xBC };
static const symbol s_3[] = { 'k', 'i' };
static const symbol s_4[] = { 'k', 'e', 'n' };
static const symbol s_5[] = { 'p' };
static const symbol s_6[] = { 0xC3, 0xA7 };
static const symbol s_7[] = { 't' };
static const symbol s_8[] = { 'k' };
static const symbol s_9[] = { 0xC4, 0xB1 };
static const symbol s_10[] = { 0xC4, 0xB1 };
static const symbol s_11[] = { 'i' };
static const symbol s_12[] = { 'u' };
static const symbol s_13[] = { 0xC3, 0xB6 };
static const symbol s_14[] = { 0xC3, 0xBC };
static const symbol s_15[] = { 0xC3, 0xBC };
static const symbol s_16[] = { 'a', 'd' };
static const symbol s_17[] = { 's', 'o', 'y' };

static int r_check_vowel_harmony(struct SN_env * z) { /* backwardmode */
    {   int m_test1 = z->l - z->c; /* test, line 110 */
        if (out_grouping_b_U(z, g_vowel, 97, 305, 1) < 0) return 0; /* goto */ /* grouping vowel, line 112 */
        {   int m2 = z->l - z->c; (void)m2; /* or, line 114 */
            if (z->c <= z->lb || z->p[z->c - 1] != 'a') goto lab1; /* literal, line 114 */
            z->c--;
            if (out_grouping_b_U(z, g_vowel1, 97, 305, 1) < 0) goto lab1; /* goto */ /* grouping vowel1, line 114 */
            goto lab0;
        lab1:
            z->c = z->l - m2;
            if (z->c <= z->lb || z->p[z->c - 1] != 'e') goto lab2; /* literal, line 115 */
            z->c--;
            if (out_grouping_b_U(z, g_vowel2, 101, 252, 1) < 0) goto lab2; /* goto */ /* grouping vowel2, line 115 */
            goto lab0;
        lab2:
            z->c = z->l - m2;
            if (!(eq_s_b(z, 2, s_0))) goto lab3; /* literal, line 116 */
            if (out_grouping_b_U(z, g_vowel3, 97, 305, 1) < 0) goto lab3; /* goto */ /* grouping vowel3, line 116 */
            goto lab0;
        lab3:
            z->c = z->l - m2;
            if (z->c <= z->lb || z->p[z->c - 1] != 'i') goto lab4; /* literal, line 117 */
            z->c--;
            if (out_grouping_b_U(z, g_vowel4, 101, 105, 1) < 0) goto lab4; /* goto */ /* grouping vowel4, line 117 */
            goto lab0;
        lab4:
            z->c = z->l - m2;
            if (z->c <= z->lb || z->p[z->c - 1] != 'o') goto lab5; /* literal, line 118 */
            z->c--;
            if (out_grouping_b_U(z, g_vowel5, 111, 117, 1) < 0) goto lab5; /* goto */ /* grouping vowel5, line 118 */
            goto lab0;
        lab5:
            z->c = z->l - m2;
            if (!(eq_s_b(z, 2, s_1))) goto lab6; /* literal, line 119 */
            if (out_grouping_b_U(z, g_vowel6, 246, 252, 1) < 0) goto lab6; /* goto */ /* grouping vowel6, line 119 */
            goto lab0;
        lab6:
            z->c = z->l - m2;
            if (z->c <= z->lb || z->p[z->c - 1] != 'u') goto lab7; /* literal, line 120 */
            z->c--;
            if (out_grouping_b_U(z, g_vowel5, 111, 117, 1) < 0) goto lab7; /* goto */ /* grouping vowel5, line 120 */
            goto lab0;
        lab7:
            z->c = z->l - m2;
            if (!(eq_s_b(z, 2, s_2))) return 0; /* literal, line 121 */
            if (out_grouping_b_U(z, g_vowel6, 246, 252, 1) < 0) return 0; /* goto */ /* grouping vowel6, line 121 */
        }
    lab0:
        z->c = z->l - m_test1;
    }
    return 1;
}

static int r_mark_suffix_with_optional_n_consonant(struct SN_env * z) { /* backwardmode */
    {   int m1 = z->l - z->c; (void)m1; /* or, line 132 */
        if (z->c <= z->lb || z->p[z->c - 1] != 'n') goto lab1; /* literal, line 131 */
        z->c--;
        {   int m_test2 = z->l - z->c; /* test, line 131 */
            if (in_grouping_b_U(z, g_vowel, 97, 305, 0)) goto lab1; /* grouping vowel, line 131 */
            z->c = z->l - m_test2;
        }
        goto lab0;
    lab1:
        z->c = z->l - m1;
        {   int m3 = z->l - z->c; (void)m3; /* not, line 133 */
            {   int m_test4 = z->l - z->c; /* test, line 133 */
                if (z->c <= z->lb || z->p[z->c - 1] != 'n') goto lab2; /* literal, line 133 */
                z->c--;
                z->c = z->l - m_test4;
            }
            return 0;
        lab2:
            z->c = z->l - m3;
        }
        {   int m_test5 = z->l - z->c; /* test, line 133 */
            {   int ret = skip_utf8(z->p, z->c, z->lb, 0, -1);
                if (ret < 0) return 0;
                z->c = ret; /* next, line 133 */
            }
            if (in_grouping_b_U(z, g_vowel, 97, 305, 0)) return 0; /* grouping vowel, line 133 */
            z->c = z->l - m_test5;
        }
    }
lab0:
    return 1;
}

static int r_mark_suffix_with_optional_s_consonant(struct SN_env * z) { /* backwardmode */
    {   int m1 = z->l - z->c; (void)m1; /* or, line 143 */
        if (z->c <= z->lb || z->p[z->c - 1] != 's') goto lab1; /* literal, line 142 */
        z->c--;
        {   int m_test2 = z->l - z->c; /* test, line 142 */
            if (in_grouping_b_U(z, g_vowel, 97, 305, 0)) goto lab1; /* grouping vowel, line 142 */
            z->c = z->l - m_test2;
        }
        goto lab0;
    lab1:
        z->c = z->l - m1;
        {   int m3 = z->l - z->c; (void)m3; /* not, line 144 */
            {   int m_test4 = z->l - z->c; /* test, line 144 */
                if (z->c <= z->lb || z->p[z->c - 1] != 's') goto lab2; /* literal, line 144 */
                z->c--;
                z->c = z->l - m_test4;
            }
            return 0;
        lab2:
            z->c = z->l - m3;
        }
        {   int m_test5 = z->l - z->c; /* test, line 144 */
            {   int ret = skip_utf8(z->p, z->c, z->lb, 0, -1);
                if (ret < 0) return 0;
                z->c = ret; /* next, line 144 */
            }
            if (in_grouping_b_U(z, g_vowel, 97, 305, 0)) return 0; /* grouping vowel, line 144 */
            z->c = z->l - m_test5;
        }
    }
lab0:
    return 1;
}

static int r_mark_suffix_with_optional_y_consonant(struct SN_env * z) { /* backwardmode */
    {   int m1 = z->l - z->c; (void)m1; /* or, line 153 */
        if (z->c <= z->lb || z->p[z->c - 1] != 'y') goto lab1; /* literal, line 152 */
        z->c--;
        {   int m_test2 = z->l - z->c; /* test, line 152 */
            if (in_grouping_b_U(z, g_vowel, 97, 305, 0)) goto lab1; /* grouping vowel, line 152 */
            z->c = z->l - m_test2;
        }
        goto lab0;
    lab1:
        z->c = z->l - m1;
        {   int m3 = z->l - z->c; (void)m3; /* not, line 154 */
            {   int m_test4 = z->l - z->c; /* test, line 154 */
                if (z->c <= z->lb || z->p[z->c - 1] != 'y') goto lab2; /* literal, line 154 */
                z->c--;
                z->c = z->l - m_test4;
            }
            return 0;
        lab2:
            z->c = z->l - m3;
        }
        {   int m_test5 = z->l - z->c; /* test, line 154 */
            {   int ret = skip_utf8(z->p, z->c, z->lb, 0, -1);
                if (ret < 0) return 0;
                z->c = ret; /* next, line 154 */
            }
            if (in_grouping_b_U(z, g_vowel, 97, 305, 0)) return 0; /* grouping vowel, line 154 */
            z->c = z->l - m_test5;
        }
    }
lab0:
    return 1;
}

static int r_mark_suffix_with_optional_U_vowel(struct SN_env * z) { /* backwardmode */
    {   int m1 = z->l - z->c; (void)m1; /* or, line 159 */
        if (in_grouping_b_U(z, g_U, 105, 305, 0)) goto lab1; /* grouping U, line 158 */
        {   int m_test2 = z->l - z->c; /* test, line 158 */
            if (out_grouping_b_U(z, g_vowel, 97, 305, 0)) goto lab1; /* non vowel, line 158 */
            z->c = z->l - m_test2;
        }
        goto lab0;
    lab1:
        z->c = z->l - m1;
        {   int m3 = z->l - z->c; (void)m3; /* not, line 160 */
            {   int m_test4 = z->l - z->c; /* test, line 160 */
                if (in_grouping_b_U(z, g_U, 105, 305, 0)) goto lab2; /* grouping U, line 160 */
                z->c = z->l - m_test4;
            }
            return 0;
        lab2:
            z->c = z->l - m3;
        }
        {   int m_test5 = z->l - z->c; /* test, line 160 */
            {   int ret = skip_utf8(z->p, z->c, z->lb, 0, -1);
                if (ret < 0) return 0;
                z->c = ret; /* next, line 160 */
            }
            if (out_grouping_b_U(z, g_vowel, 97, 305, 0)) return 0; /* non vowel, line 160 */
            z->c = z->l - m_test5;
        }
    }
lab0:
    return 1;
}

static int r_mark_possessives(struct SN_env * z) { /* backwardmode */
    if (z->c <= z->lb || z->p[z->c - 1] >> 5 != 3 || !((67133440 >> (z->p[z->c - 1] & 0x1f)) & 1)) return 0; /* among, line 165 */
    if (!(find_among_b(z, a_0, 10))) return 0;
    {   int ret = r_mark_suffix_with_optional_U_vowel(z); /* call mark_suffix_with_optional_U_vowel, line 167 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_sU(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 171 */
        if (ret <= 0) return ret;
    }
    if (in_grouping_b_U(z, g_U, 105, 305, 0)) return 0; /* grouping U, line 172 */
    {   int ret = r_mark_suffix_with_optional_s_consonant(z); /* call mark_suffix_with_optional_s_consonant, line 173 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_lArI(struct SN_env * z) { /* backwardmode */
    if (z->c - 3 <= z->lb || (z->p[z->c - 1] != 105 && z->p[z->c - 1] != 177)) return 0; /* among, line 177 */
    if (!(find_among_b(z, a_1, 2))) return 0;
    return 1;
}

static int r_mark_yU(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 181 */
        if (ret <= 0) return ret;
    }
    if (in_grouping_b_U(z, g_U, 105, 305, 0)) return 0; /* grouping U, line 182 */
    {   int ret = r_mark_suffix_with_optional_y_consonant(z); /* call mark_suffix_with_optional_y_consonant, line 183 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_nU(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 187 */
        if (ret <= 0) return ret;
    }
    if (!(find_among_b(z, a_2, 4))) return 0; /* among, line 188 */
    return 1;
}

static int r_mark_nUn(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 192 */
        if (ret <= 0) return ret;
    }
    if (z->c - 1 <= z->lb || z->p[z->c - 1] != 110) return 0; /* among, line 193 */
    if (!(find_among_b(z, a_3, 4))) return 0;
    {   int ret = r_mark_suffix_with_optional_n_consonant(z); /* call mark_suffix_with_optional_n_consonant, line 194 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_yA(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 198 */
        if (ret <= 0) return ret;
    }
    if (z->c <= z->lb || (z->p[z->c - 1] != 97 && z->p[z->c - 1] != 101)) return 0; /* among, line 199 */
    if (!(find_among_b(z, a_4, 2))) return 0;
    {   int ret = r_mark_suffix_with_optional_y_consonant(z); /* call mark_suffix_with_optional_y_consonant, line 200 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_nA(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 204 */
        if (ret <= 0) return ret;
    }
    if (z->c - 1 <= z->lb || (z->p[z->c - 1] != 97 && z->p[z->c - 1] != 101)) return 0; /* among, line 205 */
    if (!(find_among_b(z, a_5, 2))) return 0;
    return 1;
}

static int r_mark_DA(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 209 */
        if (ret <= 0) return ret;
    }
    if (z->c - 1 <= z->lb || (z->p[z->c - 1] != 97 && z->p[z->c - 1] != 101)) return 0; /* among, line 210 */
    if (!(find_among_b(z, a_6, 4))) return 0;
    return 1;
}

static int r_mark_ndA(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 214 */
        if (ret <= 0) return ret;
    }
    if (z->c - 2 <= z->lb || (z->p[z->c - 1] != 97 && z->p[z->c - 1] != 101)) return 0; /* among, line 215 */
    if (!(find_among_b(z, a_7, 2))) return 0;
    return 1;
}

static int r_mark_DAn(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 219 */
        if (ret <= 0) return ret;
    }
    if (z->c - 2 <= z->lb || z->p[z->c - 1] != 110) return 0; /* among, line 220 */
    if (!(find_among_b(z, a_8, 4))) return 0;
    return 1;
}

static int r_mark_ndAn(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 224 */
        if (ret <= 0) return ret;
    }
    if (z->c - 3 <= z->lb || z->p[z->c - 1] != 110) return 0; /* among, line 225 */
    if (!(find_among_b(z, a_9, 2))) return 0;
    return 1;
}

static int r_mark_ylA(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 229 */
        if (ret <= 0) return ret;
    }
    if (z->c - 1 <= z->lb || (z->p[z->c - 1] != 97 && z->p[z->c - 1] != 101)) return 0; /* among, line 230 */
    if (!(find_among_b(z, a_10, 2))) return 0;
    {   int ret = r_mark_suffix_with_optional_y_consonant(z); /* call mark_suffix_with_optional_y_consonant, line 231 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_ki(struct SN_env * z) { /* backwardmode */
    if (!(eq_s_b(z, 2, s_3))) return 0; /* literal, line 235 */
    return 1;
}

static int r_mark_ncA(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 239 */
        if (ret <= 0) return ret;
    }
    if (z->c - 1 <= z->lb || (z->p[z->c - 1] != 97 && z->p[z->c - 1] != 101)) return 0; /* among, line 240 */
    if (!(find_among_b(z, a_11, 2))) return 0;
    {   int ret = r_mark_suffix_with_optional_n_consonant(z); /* call mark_suffix_with_optional_n_consonant, line 241 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_yUm(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 245 */
        if (ret <= 0) return ret;
    }
    if (z->c - 1 <= z->lb || z->p[z->c - 1] != 109) return 0; /* among, line 246 */
    if (!(find_among_b(z, a_12, 4))) return 0;
    {   int ret = r_mark_suffix_with_optional_y_consonant(z); /* call mark_suffix_with_optional_y_consonant, line 247 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_sUn(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 251 */
        if (ret <= 0) return ret;
    }
    if (z->c - 2 <= z->lb || z->p[z->c - 1] != 110) return 0; /* among, line 252 */
    if (!(find_among_b(z, a_13, 4))) return 0;
    return 1;
}

static int r_mark_yUz(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 256 */
        if (ret <= 0) return ret;
    }
    if (z->c - 1 <= z->lb || z->p[z->c - 1] != 122) return 0; /* among, line 257 */
    if (!(find_among_b(z, a_14, 4))) return 0;
    {   int ret = r_mark_suffix_with_optional_y_consonant(z); /* call mark_suffix_with_optional_y_consonant, line 258 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_sUnUz(struct SN_env * z) { /* backwardmode */
    if (z->c - 4 <= z->lb || z->p[z->c - 1] != 122) return 0; /* among, line 262 */
    if (!(find_among_b(z, a_15, 4))) return 0;
    return 1;
}

static int r_mark_lAr(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 266 */
        if (ret <= 0) return ret;
    }
    if (z->c - 2 <= z->lb || z->p[z->c - 1] != 114) return 0; /* among, line 267 */
    if (!(find_among_b(z, a_16, 2))) return 0;
    return 1;
}

static int r_mark_nUz(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 271 */
        if (ret <= 0) return ret;
    }
    if (z->c - 2 <= z->lb || z->p[z->c - 1] != 122) return 0; /* among, line 272 */
    if (!(find_among_b(z, a_17, 4))) return 0;
    return 1;
}

static int r_mark_DUr(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 276 */
        if (ret <= 0) return ret;
    }
    if (z->c - 2 <= z->lb || z->p[z->c - 1] != 114) return 0; /* among, line 277 */
    if (!(find_among_b(z, a_18, 8))) return 0;
    return 1;
}

static int r_mark_cAsInA(struct SN_env * z) { /* backwardmode */
    if (z->c - 5 <= z->lb || (z->p[z->c - 1] != 97 && z->p[z->c - 1] != 101)) return 0; /* among, line 281 */
    if (!(find_among_b(z, a_19, 2))) return 0;
    return 1;
}

static int r_mark_yDU(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 285 */
        if (ret <= 0) return ret;
    }
    if (!(find_among_b(z, a_20, 32))) return 0; /* among, line 286 */
    {   int ret = r_mark_suffix_with_optional_y_consonant(z); /* call mark_suffix_with_optional_y_consonant, line 290 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_ysA(struct SN_env * z) { /* backwardmode */
    if (z->c - 1 <= z->lb || z->p[z->c - 1] >> 5 != 3 || !((26658 >> (z->p[z->c - 1] & 0x1f)) & 1)) return 0; /* among, line 295 */
    if (!(find_among_b(z, a_21, 8))) return 0;
    {   int ret = r_mark_suffix_with_optional_y_consonant(z); /* call mark_suffix_with_optional_y_consonant, line 296 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_ymUs_(struct SN_env * z) { /* backwardmode */
    {   int ret = r_check_vowel_harmony(z); /* call check_vowel_harmony, line 300 */
        if (ret <= 0) return ret;
    }
    if (z->c - 3 <= z->lb || z->p[z->c - 1] != 159) return 0; /* among, line 301 */
    if (!(find_among_b(z, a_22, 4))) return 0;
    {   int ret = r_mark_suffix_with_optional_y_consonant(z); /* call mark_suffix_with_optional_y_consonant, line 302 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_mark_yken(struct SN_env * z) { /* backwardmode */
    if (!(eq_s_b(z, 3, s_4))) return 0; /* literal, line 306 */
    {   int ret = r_mark_suffix_with_optional_y_consonant(z); /* call mark_suffix_with_optional_y_consonant, line 306 */
        if (ret <= 0) return ret;
    }
    return 1;
}

static int r_stem_nominal_verb_suffixes(struct SN_env * z) { /* backwardmode */
    z->ket = z->c; /* [, line 310 */
    z->B[0] = 1; /* set continue_stemming_noun_suffixes, line 311 */
    {   int m1 = z->l - z->c; (void)m1; /* or, line 313 */
        {   int m2 = z->l - z->c; (void)m2; /* or, line 312 */
            {   int ret = r_mark_ymUs_(z); /* call mark_ymUs_, line 312 */
                if (ret == 0) goto lab3;
                if (ret < 0) return ret;
            }
            goto lab2;
        lab3:
            z->c = z->l - m2;
            {   int ret = r_mark_yDU(z); /* call mark_yDU, line 312 */
                if (ret == 0) goto lab4;
                if (ret < 0) return ret;
            }
            goto lab2;
        lab4:
            z->c = z->l - m2;
            {   int ret = r_mark_ysA(z); /* call mark_ysA, line 312 */
                if (ret == 0) goto lab5;
                if (ret < 0) return ret;
            }
            goto lab2;
        lab5:
            z->c = z->l - m2;
            {   int ret = r_mark_yken(z); /* call mark_yken, line 312 */
                if (ret == 0) goto lab1;
                if (ret < 0) return ret;
            }
        }
    lab2:
        goto lab0;
    lab1:
        z->c = z->l - m1;
        {   int ret = r_mark_cAsInA(z); /* call mark_cAsInA, line 314 */
            if (ret == 0) goto lab6;
            if (ret < 0) return ret;
        }
        {   int m3 = z->l - z->c; (void)m3; /* or, line 314 */
            {   int ret = r_mark_sUnUz(z); /* call mark_sUnUz, line 314 */
                if (ret == 0) goto lab8;
                if (ret < 0) return ret;
            }
            goto lab7;
        lab8:
            z->c = z->l - m3;
            {   int ret = r_mark_lAr(z); /* call mark_lAr, line 314 */
                if (ret == 0) goto lab9;
                if (ret < 0) return ret;
            }
            goto lab7;
        lab9:
            z->c = z->l - m3;
            {   int ret = r_mark_yUm(z); /* call mark_yUm, line 314 */
                if (ret == 0) goto lab10;
                if (ret < 0) return ret;
            }
            goto lab7;
        lab10:
            z->c = z->l - m3;
            {   int ret = r_mark_sUn(z); /* call mark_sUn, line 314 */
                if (ret == 0) goto lab11;
                if (ret < 0) return ret;
            }
            goto lab7;
        lab11:
            z->c = z->l - m3;
            {   int ret = r_mark_yUz(z); /* call mark_yUz, line 314 */
                if (ret == 0) goto lab12;
                if (ret < 0) return ret;
            }
            goto lab7;
        lab12:
            z->c = z->l - m3;
        }
    lab7:
        {   int ret = r_mark_ymUs_(z); /* call mark_ymUs_, line 314 */
            if (ret == 0) goto lab6;
            if (ret < 0) return ret;
        }
        goto lab0;
    lab6:
        z->c = z->l - m1;
        {   int ret = r_mark_lAr(z); /* call mark_lAr, line 317 */
            if (ret == 0) goto lab13;
            if (ret < 0) return ret;
        }
        z->bra = z->c; /* ], line 317 */
        {   int ret = slice_del(z); /* delete, line 317 */
            if (ret < 0) return ret;
        }
        {   int m4 = z->l - z->c; (void)m4; /* try, line 317 */
            z->ket = z->c; /* [, line 317 */
            {   int m5 = z->l - z->c; (void)m5; /* or, line 317 */
                {   int ret = r_mark_DUr(z); /* call mark_DUr, line 317 */
                    if (ret == 0) goto lab16;
                    if (ret < 0) return ret;
                }
                goto lab15;
            lab16:
                z->c = z->l - m5;
                {   int ret = r_mark_yDU(z); /* call mark_yDU, line 317 */
                    if (ret == 0) goto lab17;
                    if (ret < 0) return ret;
                }
                goto lab15;
            lab17:
                z->c = z->l - m5;
                {   int ret = r_mark_ysA(z); /* call mark_ysA, line 317 */
                    if (ret == 0) goto lab18;
                    if (ret < 0) return ret;
                }
                goto lab15;
            lab18:
                z->c = z->l - m5;
                {   int ret = r_mark_ymUs_(z); /* call mark_ymUs_, line 317 */
                    if (ret == 0) { z->c = z->l - m4; goto lab14; }
                    if (ret < 0) return ret;
                }
            }
        lab15:
        lab14:
            ;
        }
        z->B[0] = 0; /* unset continue_stemming_noun_suffixes, line 318 */
        goto lab0;
    lab13:
        z->c = z->l - m1;
        {   int ret = r_mark_nUz(z); /* call mark_nUz, line 321 */
            if (ret == 0) goto lab19;
            if (ret < 0) return ret;
        }
        {   int m6 = z->l - z->c; (void)m6; /* or, line 321 */
            {   int ret = r_mark_yDU(z); /* call mark_yDU, line 321 */
                if (ret == 0) goto lab21;
                if (ret < 0) return ret;
            }
            goto lab20;
        lab21:
            z->c = z->l - m6;
            {   int ret = r_mark_ysA(z); /* call mark_ysA, line 321 */
                if (ret == 0) goto lab19;
                if (ret < 0) return ret;
            }
        }
    lab20:
        goto lab0;
    lab19:
        z->c = z->l - m1;
        {   int m7 = z->l - z->c; (void)m7; /* or, line 323 */
            {   int ret = r_mark_sUnUz(z); /* call mark_sUnUz, line 323 */
                if (ret == 0) goto lab24;
                if (ret < 0) return ret;
            }
            goto lab23;
        lab24:
            z->c = z->l - m7;
            {   int ret = r_mark_yUz(z); /* call mark_yUz, line 323 */
                if (ret == 0) goto lab25;
                if (ret < 0) return ret;
            }
            goto lab23;
        lab25:
            z->c = z->l - m7;
            {   int ret = r_mark_sUn(z); /* call mark_sUn, line 323 */
                if (ret == 0) goto lab26;
                if (ret < 0) return ret;
            }
            goto lab23;
        lab26:
            z->c = z->l - m7;
            {   int ret = r_mark_yUm(z); /* call mark_yUm, line 323 */
                if (ret == 0) goto lab22;
                if (ret < 0) return ret;
            }
        }
    lab23:
        z->bra = z->c; /* ], line 323 */
        {   int ret = slice_del(z); /* delete, line 323 */
            if (ret < 0) return ret;
        }
        {   int m8 = z->l - z->c; (void)m8; /* try, line 323 */
            z->ket = z->c; /* [, line 323 */
            {   int ret = r_mark_ymUs_(z); /* call mark_ymUs_, line 323 */
                if (ret == 0) { z->c = z->l - m8; goto lab27; }
                if (ret < 0) return ret;
            }
        lab27:
            ;
        }
        goto lab0;
    lab22:
        z->c = z->l - m1;
        {   int ret = r_mark_DUr(z); /* call mark_DUr, line 325 */
            if (ret <= 0) return ret;
        }
        z->bra = z->c; /* ], line 325 */
        {   int ret = slice_del(z); /* delete, line 325 */
            if (ret < 0) return ret;
        }
        {   int m9 = z->l - z->c; (void)m9; /* try, line 325 */
            z->ket = z->c; /* [, line 325 */
            {   int m10 = z->l - z->c; (void)m10; /* or, line 325 */
                {   int ret = r_mark_sUnUz(z); /* call mark_sUnUz, line 325 */
                    if (ret == 0) goto lab30;
                    if (ret < 0) return ret;
                }
                goto lab29;
            lab30:
                z->c = z->l - m10;
                {   int ret = r_mark_lAr(z); /* call mark_lAr, line 325 */
                    if (ret == 0) goto lab31;
                    if (ret < 0) return ret;
                }
                goto lab29;
            lab31:
                z->c = z->l - m10;
                {   int ret = r_mark_yUm(z); /* call mark_yUm, line 325 */
                    if (ret == 0) goto lab32;
                    if (ret < 0) return ret;
                }
                goto lab29;
            lab32:
                z->c = z->l - m10;
                {   int ret = r_mark_sUn(z); /* call mark_sUn, line 325 */
                    if (ret == 0) goto lab33;
                    if (ret < 0) return ret;
                }
                goto lab29;
            lab33:
                z->c = z->l - m10;
                {   int ret = r_mark_yUz(z); /* call mark_yUz, line 325 */
                    if (ret == 0) goto lab34;
                    if (ret < 0) return ret;
                }
                goto lab29;
            lab34:
                z->c = z->l - m10;
            }
        lab29:
            {   int ret = r_mark_ymUs_(z); /* call mark_ymUs_, line 325 */
                if (ret == 0) { z->c = z->l - m9; goto lab28; }
                if (ret < 0) return ret;
            }
        lab28:
            ;
        }
    }
lab0:
    z->bra = z->c; /* ], line 326 */
    {   int ret = slice_del(z); /* delete, line 326 */
        if (ret < 0) return ret;
    }
    return 1;
}

static int r_stem_suffix_chain_before_ki(struct SN_env * z) { /* backwardmode */
    z->ket = z->c; /* [, line 331 */
    {   int ret = r_mark_ki(z); /* call mark_ki, line 332 */
        if (ret <= 0) return ret;
    }
    {   int m1 = z->l - z->c; (void)m1; /* or, line 340 */
        {   int ret = r_mark_DA(z); /* call mark_DA, line 334 */
            if (ret == 0) goto lab1;
            if (ret < 0) return ret;
        }
        z->bra = z->c; /* ], line 334 */
        {   int ret = slice_del(z); /* delete, line 334 */
            if (ret < 0) return ret;
        }
        {   int m2 = z->l - z->c; (void)m2; /* try, line 334 */
            z->ket = z->c; /* [, line 334 */
            {   int m3 = z->l - z->c; (void)m3; /* or, line 336 */
                {   int ret = r_mark_lAr(z); /* call mark_lAr, line 335 */
                    if (ret == 0) goto lab4;
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 335 */
                {   int ret = slice_del(z); /* delete, line 335 */
                    if (ret < 0) return ret;
                }
                {   int m4 = z->l - z->c; (void)m4; /* try, line 335 */
                    {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 335 */
                        if (ret == 0) { z->c = z->l - m4; goto lab5; }
                        if (ret < 0) return ret;
                    }
                lab5:
                    ;
                }
                goto lab3;
            lab4:
                z->c = z->l - m3;
                {   int ret = r_mark_possessives(z); /* call mark_possessives, line 337 */
                    if (ret == 0) { z->c = z->l - m2; goto lab2; }
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 337 */
                {   int ret = slice_del(z); /* delete, line 337 */
                    if (ret < 0) return ret;
                }
                {   int m5 = z->l - z->c; (void)m5; /* try, line 337 */
                    z->ket = z->c; /* [, line 337 */
                    {   int ret = r_mark_lAr(z); /* call mark_lAr, line 337 */
                        if (ret == 0) { z->c = z->l - m5; goto lab6; }
                        if (ret < 0) return ret;
                    }
                    z->bra = z->c; /* ], line 337 */
                    {   int ret = slice_del(z); /* delete, line 337 */
                        if (ret < 0) return ret;
                    }
                    {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 337 */
                        if (ret == 0) { z->c = z->l - m5; goto lab6; }
                        if (ret < 0) return ret;
                    }
                lab6:
                    ;
                }
            }
        lab3:
        lab2:
            ;
        }
        goto lab0;
    lab1:
        z->c = z->l - m1;
        {   int ret = r_mark_nUn(z); /* call mark_nUn, line 341 */
            if (ret == 0) goto lab7;
            if (ret < 0) return ret;
        }
        z->bra = z->c; /* ], line 341 */
        {   int ret = slice_del(z); /* delete, line 341 */
            if (ret < 0) return ret;
        }
        {   int m6 = z->l - z->c; (void)m6; /* try, line 341 */
            z->ket = z->c; /* [, line 341 */
            {   int m7 = z->l - z->c; (void)m7; /* or, line 343 */
                {   int ret = r_mark_lArI(z); /* call mark_lArI, line 342 */
                    if (ret == 0) goto lab10;
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 342 */
                {   int ret = slice_del(z); /* delete, line 342 */
                    if (ret < 0) return ret;
                }
                goto lab9;
            lab10:
                z->c = z->l - m7;
                z->ket = z->c; /* [, line 344 */
                {   int m8 = z->l - z->c; (void)m8; /* or, line 344 */
                    {   int ret = r_mark_possessives(z); /* call mark_possessives, line 344 */
                        if (ret == 0) goto lab13;
                        if (ret < 0) return ret;
                    }
                    goto lab12;
                lab13:
                    z->c = z->l - m8;
                    {   int ret = r_mark_sU(z); /* call mark_sU, line 344 */
                        if (ret == 0) goto lab11;
                        if (ret < 0) return ret;
                    }
                }
            lab12:
                z->bra = z->c; /* ], line 344 */
                {   int ret = slice_del(z); /* delete, line 344 */
                    if (ret < 0) return ret;
                }
                {   int m9 = z->l - z->c; (void)m9; /* try, line 344 */
                    z->ket = z->c; /* [, line 344 */
                    {   int ret = r_mark_lAr(z); /* call mark_lAr, line 344 */
                        if (ret == 0) { z->c = z->l - m9; goto lab14; }
                        if (ret < 0) return ret;
                    }
                    z->bra = z->c; /* ], line 344 */
                    {   int ret = slice_del(z); /* delete, line 344 */
                        if (ret < 0) return ret;
                    }
                    {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 344 */
                        if (ret == 0) { z->c = z->l - m9; goto lab14; }
                        if (ret < 0) return ret;
                    }
                lab14:
                    ;
                }
                goto lab9;
            lab11:
                z->c = z->l - m7;
                {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 346 */
                    if (ret == 0) { z->c = z->l - m6; goto lab8; }
                    if (ret < 0) return ret;
                }
            }
        lab9:
        lab8:
            ;
        }
        goto lab0;
    lab7:
        z->c = z->l - m1;
        {   int ret = r_mark_ndA(z); /* call mark_ndA, line 349 */
            if (ret <= 0) return ret;
        }
        {   int m10 = z->l - z->c; (void)m10; /* or, line 351 */
            {   int ret = r_mark_lArI(z); /* call mark_lArI, line 350 */
                if (ret == 0) goto lab16;
                if (ret < 0) return ret;
            }
            z->bra = z->c; /* ], line 350 */
            {   int ret = slice_del(z); /* delete, line 350 */
                if (ret < 0) return ret;
            }
            goto lab15;
        lab16:
            z->c = z->l - m10;
            {   int ret = r_mark_sU(z); /* call mark_sU, line 352 */
                if (ret == 0) goto lab17;
                if (ret < 0) return ret;
            }
            z->bra = z->c; /* ], line 352 */
            {   int ret = slice_del(z); /* delete, line 352 */
                if (ret < 0) return ret;
            }
            {   int m11 = z->l - z->c; (void)m11; /* try, line 352 */
                z->ket = z->c; /* [, line 352 */
                {   int ret = r_mark_lAr(z); /* call mark_lAr, line 352 */
                    if (ret == 0) { z->c = z->l - m11; goto lab18; }
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 352 */
                {   int ret = slice_del(z); /* delete, line 352 */
                    if (ret < 0) return ret;
                }
                {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 352 */
                    if (ret == 0) { z->c = z->l - m11; goto lab18; }
                    if (ret < 0) return ret;
                }
            lab18:
                ;
            }
            goto lab15;
        lab17:
            z->c = z->l - m10;
            {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 354 */
                if (ret <= 0) return ret;
            }
        }
    lab15:
        ;
    }
lab0:
    return 1;
}

static int r_stem_noun_suffixes(struct SN_env * z) { /* backwardmode */
    {   int m1 = z->l - z->c; (void)m1; /* or, line 361 */
        z->ket = z->c; /* [, line 360 */
        {   int ret = r_mark_lAr(z); /* call mark_lAr, line 360 */
            if (ret == 0) goto lab1;
            if (ret < 0) return ret;
        }
        z->bra = z->c; /* ], line 360 */
        {   int ret = slice_del(z); /* delete, line 360 */
            if (ret < 0) return ret;
        }
        {   int m2 = z->l - z->c; (void)m2; /* try, line 360 */
            {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 360 */
                if (ret == 0) { z->c = z->l - m2; goto lab2; }
                if (ret < 0) return ret;
            }
        lab2:
            ;
        }
        goto lab0;
    lab1:
        z->c = z->l - m1;
        z->ket = z->c; /* [, line 362 */
        {   int ret = r_mark_ncA(z); /* call mark_ncA, line 362 */
            if (ret == 0) goto lab3;
            if (ret < 0) return ret;
        }
        z->bra = z->c; /* ], line 362 */
        {   int ret = slice_del(z); /* delete, line 362 */
            if (ret < 0) return ret;
        }
        {   int m3 = z->l - z->c; (void)m3; /* try, line 363 */
            {   int m4 = z->l - z->c; (void)m4; /* or, line 365 */
                z->ket = z->c; /* [, line 364 */
                {   int ret = r_mark_lArI(z); /* call mark_lArI, line 364 */
                    if (ret == 0) goto lab6;
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 364 */
                {   int ret = slice_del(z); /* delete, line 364 */
                    if (ret < 0) return ret;
                }
                goto lab5;
            lab6:
                z->c = z->l - m4;
                z->ket = z->c; /* [, line 366 */
                {   int m5 = z->l - z->c; (void)m5; /* or, line 366 */
                    {   int ret = r_mark_possessives(z); /* call mark_possessives, line 366 */
                        if (ret == 0) goto lab9;
                        if (ret < 0) return ret;
                    }
                    goto lab8;
                lab9:
                    z->c = z->l - m5;
                    {   int ret = r_mark_sU(z); /* call mark_sU, line 366 */
                        if (ret == 0) goto lab7;
                        if (ret < 0) return ret;
                    }
                }
            lab8:
                z->bra = z->c; /* ], line 366 */
                {   int ret = slice_del(z); /* delete, line 366 */
                    if (ret < 0) return ret;
                }
                {   int m6 = z->l - z->c; (void)m6; /* try, line 366 */
                    z->ket = z->c; /* [, line 366 */
                    {   int ret = r_mark_lAr(z); /* call mark_lAr, line 366 */
                        if (ret == 0) { z->c = z->l - m6; goto lab10; }
                        if (ret < 0) return ret;
                    }
                    z->bra = z->c; /* ], line 366 */
                    {   int ret = slice_del(z); /* delete, line 366 */
                        if (ret < 0) return ret;
                    }
                    {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 366 */
                        if (ret == 0) { z->c = z->l - m6; goto lab10; }
                        if (ret < 0) return ret;
                    }
                lab10:
                    ;
                }
                goto lab5;
            lab7:
                z->c = z->l - m4;
                z->ket = z->c; /* [, line 368 */
                {   int ret = r_mark_lAr(z); /* call mark_lAr, line 368 */
                    if (ret == 0) { z->c = z->l - m3; goto lab4; }
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 368 */
                {   int ret = slice_del(z); /* delete, line 368 */
                    if (ret < 0) return ret;
                }
                {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 368 */
                    if (ret == 0) { z->c = z->l - m3; goto lab4; }
                    if (ret < 0) return ret;
                }
            }
        lab5:
        lab4:
            ;
        }
        goto lab0;
    lab3:
        z->c = z->l - m1;
        z->ket = z->c; /* [, line 372 */
        {   int m7 = z->l - z->c; (void)m7; /* or, line 372 */
            {   int ret = r_mark_ndA(z); /* call mark_ndA, line 372 */
                if (ret == 0) goto lab13;
                if (ret < 0) return ret;
            }
            goto lab12;
        lab13:
            z->c = z->l - m7;
            {   int ret = r_mark_nA(z); /* call mark_nA, line 372 */
                if (ret == 0) goto lab11;
                if (ret < 0) return ret;
            }
        }
    lab12:
        {   int m8 = z->l - z->c; (void)m8; /* or, line 375 */
            {   int ret = r_mark_lArI(z); /* call mark_lArI, line 374 */
                if (ret == 0) goto lab15;
                if (ret < 0) return ret;
            }
            z->bra = z->c; /* ], line 374 */
            {   int ret = slice_del(z); /* delete, line 374 */
                if (ret < 0) return ret;
            }
            goto lab14;
        lab15:
            z->c = z->l - m8;
            {   int ret = r_mark_sU(z); /* call mark_sU, line 376 */
                if (ret == 0) goto lab16;
                if (ret < 0) return ret;
            }
            z->bra = z->c; /* ], line 376 */
            {   int ret = slice_del(z); /* delete, line 376 */
                if (ret < 0) return ret;
            }
            {   int m9 = z->l - z->c; (void)m9; /* try, line 376 */
                z->ket = z->c; /* [, line 376 */
                {   int ret = r_mark_lAr(z); /* call mark_lAr, line 376 */
                    if (ret == 0) { z->c = z->l - m9; goto lab17; }
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 376 */
                {   int ret = slice_del(z); /* delete, line 376 */
                    if (ret < 0) return ret;
                }
                {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 376 */
                    if (ret == 0) { z->c = z->l - m9; goto lab17; }
                    if (ret < 0) return ret;
                }
            lab17:
                ;
            }
            goto lab14;
        lab16:
            z->c = z->l - m8;
            {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 378 */
                if (ret == 0) goto lab11;
                if (ret < 0) return ret;
            }
        }
    lab14:
        goto lab0;
    lab11:
        z->c = z->l - m1;
        z->ket = z->c; /* [, line 382 */
        {   int m10 = z->l - z->c; (void)m10; /* or, line 382 */
            {   int ret = r_mark_ndAn(z); /* call mark_ndAn, line 382 */
                if (ret == 0) goto lab20;
                if (ret < 0) return ret;
            }
            goto lab19;
        lab20:
            z->c = z->l - m10;
            {   int ret = r_mark_nU(z); /* call mark_nU, line 382 */
                if (ret == 0) goto lab18;
                if (ret < 0) return ret;
            }
        }
    lab19:
        {   int m11 = z->l - z->c; (void)m11; /* or, line 382 */
            {   int ret = r_mark_sU(z); /* call mark_sU, line 382 */
                if (ret == 0) goto lab22;
                if (ret < 0) return ret;
            }
            z->bra = z->c; /* ], line 382 */
            {   int ret = slice_del(z); /* delete, line 382 */
                if (ret < 0) return ret;
            }
            {   int m12 = z->l - z->c; (void)m12; /* try, line 382 */
                z->ket = z->c; /* [, line 382 */
                {   int ret = r_mark_lAr(z); /* call mark_lAr, line 382 */
                    if (ret == 0) { z->c = z->l - m12; goto lab23; }
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 382 */
                {   int ret = slice_del(z); /* delete, line 382 */
                    if (ret < 0) return ret;
                }
                {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 382 */
                    if (ret == 0) { z->c = z->l - m12; goto lab23; }
                    if (ret < 0) return ret;
                }
            lab23:
                ;
            }
            goto lab21;
        lab22:
            z->c = z->l - m11;
            {   int ret = r_mark_lArI(z); /* call mark_lArI, line 382 */
                if (ret == 0) goto lab18;
                if (ret < 0) return ret;
            }
        }
    lab21:
        goto lab0;
    lab18:
        z->c = z->l - m1;
        z->ket = z->c; /* [, line 384 */
        {   int ret = r_mark_DAn(z); /* call mark_DAn, line 384 */
            if (ret == 0) goto lab24;
            if (ret < 0) return ret;
        }
        z->bra = z->c; /* ], line 384 */
        {   int ret = slice_del(z); /* delete, line 384 */
            if (ret < 0) return ret;
        }
        {   int m13 = z->l - z->c; (void)m13; /* try, line 384 */
            z->ket = z->c; /* [, line 384 */
            {   int m14 = z->l - z->c; (void)m14; /* or, line 387 */
                {   int ret = r_mark_possessives(z); /* call mark_possessives, line 386 */
                    if (ret == 0) goto lab27;
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 386 */
                {   int ret = slice_del(z); /* delete, line 386 */
                    if (ret < 0) return ret;
                }
                {   int m15 = z->l - z->c; (void)m15; /* try, line 386 */
                    z->ket = z->c; /* [, line 386 */
                    {   int ret = r_mark_lAr(z); /* call mark_lAr, line 386 */
                        if (ret == 0) { z->c = z->l - m15; goto lab28; }
                        if (ret < 0) return ret;
                    }
                    z->bra = z->c; /* ], line 386 */
                    {   int ret = slice_del(z); /* delete, line 386 */
                        if (ret < 0) return ret;
                    }
                    {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 386 */
                        if (ret == 0) { z->c = z->l - m15; goto lab28; }
                        if (ret < 0) return ret;
                    }
                lab28:
                    ;
                }
                goto lab26;
            lab27:
                z->c = z->l - m14;
                {   int ret = r_mark_lAr(z); /* call mark_lAr, line 388 */
                    if (ret == 0) goto lab29;
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 388 */
                {   int ret = slice_del(z); /* delete, line 388 */
                    if (ret < 0) return ret;
                }
                {   int m16 = z->l - z->c; (void)m16; /* try, line 388 */
                    {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 388 */
                        if (ret == 0) { z->c = z->l - m16; goto lab30; }
                        if (ret < 0) return ret;
                    }
                lab30:
                    ;
                }
                goto lab26;
            lab29:
                z->c = z->l - m14;
                {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 390 */
                    if (ret == 0) { z->c = z->l - m13; goto lab25; }
                    if (ret < 0) return ret;
                }
            }
        lab26:
        lab25:
            ;
        }
        goto lab0;
    lab24:
        z->c = z->l - m1;
        z->ket = z->c; /* [, line 394 */
        {   int m17 = z->l - z->c; (void)m17; /* or, line 394 */
            {   int ret = r_mark_nUn(z); /* call mark_nUn, line 394 */
                if (ret == 0) goto lab33;
                if (ret < 0) return ret;
            }
            goto lab32;
        lab33:
            z->c = z->l - m17;
            {   int ret = r_mark_ylA(z); /* call mark_ylA, line 394 */
                if (ret == 0) goto lab31;
                if (ret < 0) return ret;
            }
        }
    lab32:
        z->bra = z->c; /* ], line 394 */
        {   int ret = slice_del(z); /* delete, line 394 */
            if (ret < 0) return ret;
        }
        {   int m18 = z->l - z->c; (void)m18; /* try, line 395 */
            {   int m19 = z->l - z->c; (void)m19; /* or, line 397 */
                z->ket = z->c; /* [, line 396 */
                {   int ret = r_mark_lAr(z); /* call mark_lAr, line 396 */
                    if (ret == 0) goto lab36;
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 396 */
                {   int ret = slice_del(z); /* delete, line 396 */
                    if (ret < 0) return ret;
                }
                {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 396 */
                    if (ret == 0) goto lab36;
                    if (ret < 0) return ret;
                }
                goto lab35;
            lab36:
                z->c = z->l - m19;
                z->ket = z->c; /* [, line 398 */
                {   int m20 = z->l - z->c; (void)m20; /* or, line 398 */
                    {   int ret = r_mark_possessives(z); /* call mark_possessives, line 398 */
                        if (ret == 0) goto lab39;
                        if (ret < 0) return ret;
                    }
                    goto lab38;
                lab39:
                    z->c = z->l - m20;
                    {   int ret = r_mark_sU(z); /* call mark_sU, line 398 */
                        if (ret == 0) goto lab37;
                        if (ret < 0) return ret;
                    }
                }
            lab38:
                z->bra = z->c; /* ], line 398 */
                {   int ret = slice_del(z); /* delete, line 398 */
                    if (ret < 0) return ret;
                }
                {   int m21 = z->l - z->c; (void)m21; /* try, line 398 */
                    z->ket = z->c; /* [, line 398 */
                    {   int ret = r_mark_lAr(z); /* call mark_lAr, line 398 */
                        if (ret == 0) { z->c = z->l - m21; goto lab40; }
                        if (ret < 0) return ret;
                    }
                    z->bra = z->c; /* ], line 398 */
                    {   int ret = slice_del(z); /* delete, line 398 */
                        if (ret < 0) return ret;
                    }
                    {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 398 */
                        if (ret == 0) { z->c = z->l - m21; goto lab40; }
                        if (ret < 0) return ret;
                    }
                lab40:
                    ;
                }
                goto lab35;
            lab37:
                z->c = z->l - m19;
                {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 400 */
                    if (ret == 0) { z->c = z->l - m18; goto lab34; }
                    if (ret < 0) return ret;
                }
            }
        lab35:
        lab34:
            ;
        }
        goto lab0;
    lab31:
        z->c = z->l - m1;
        z->ket = z->c; /* [, line 404 */
        {   int ret = r_mark_lArI(z); /* call mark_lArI, line 404 */
            if (ret == 0) goto lab41;
            if (ret < 0) return ret;
        }
        z->bra = z->c; /* ], line 404 */
        {   int ret = slice_del(z); /* delete, line 404 */
            if (ret < 0) return ret;
        }
        goto lab0;
    lab41:
        z->c = z->l - m1;
        {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 406 */
            if (ret == 0) goto lab42;
            if (ret < 0) return ret;
        }
        goto lab0;
    lab42:
        z->c = z->l - m1;
        z->ket = z->c; /* [, line 408 */
        {   int m22 = z->l - z->c; (void)m22; /* or, line 408 */
            {   int ret = r_mark_DA(z); /* call mark_DA, line 408 */
                if (ret == 0) goto lab45;
                if (ret < 0) return ret;
            }
            goto lab44;
        lab45:
            z->c = z->l - m22;
            {   int ret = r_mark_yU(z); /* call mark_yU, line 408 */
                if (ret == 0) goto lab46;
                if (ret < 0) return ret;
            }
            goto lab44;
        lab46:
            z->c = z->l - m22;
            {   int ret = r_mark_yA(z); /* call mark_yA, line 408 */
                if (ret == 0) goto lab43;
                if (ret < 0) return ret;
            }
        }
    lab44:
        z->bra = z->c; /* ], line 408 */
        {   int ret = slice_del(z); /* delete, line 408 */
            if (ret < 0) return ret;
        }
        {   int m23 = z->l - z->c; (void)m23; /* try, line 408 */
            z->ket = z->c; /* [, line 408 */
            {   int m24 = z->l - z->c; (void)m24; /* or, line 408 */
                {   int ret = r_mark_possessives(z); /* call mark_possessives, line 408 */
                    if (ret == 0) goto lab49;
                    if (ret < 0) return ret;
                }
                z->bra = z->c; /* ], line 408 */
                {   int ret = slice_del(z); /* delete, line 408 */
                    if (ret < 0) return ret;
                }
                {   int m25 = z->l - z->c; (void)m25; /* try, line 408 */
                    z->ket = z->c; /* [, line 408 */
                    {   int ret = r_mark_lAr(z); /* call mark_lAr, line 408 */
                        if (ret == 0) { z->c = z->l - m25; goto lab50; }
                        if (ret < 0) return ret;
                    }
                lab50:
                    ;
                }
                goto lab48;
            lab49:
                z->c = z->l - m24;
                {   int ret = r_mark_lAr(z); /* call mark_lAr, line 408 */
                    if (ret == 0) { z->c = z->l - m23; goto lab47; }
                    if (ret < 0) return ret;
                }
            }
        lab48:
            z->bra = z->c; /* ], line 408 */
            {   int ret = slice_del(z); /* delete, line 408 */
                if (ret < 0) return ret;
            }
            z->ket = z->c; /* [, line 408 */
            {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 408 */
                if (ret == 0) { z->c = z->l - m23; goto lab47; }
                if (ret < 0) return ret;
            }
        lab47:
            ;
        }
        goto lab0;
    lab43:
        z->c = z->l - m1;
        z->ket = z->c; /* [, line 410 */
        {   int m26 = z->l - z->c; (void)m26; /* or, line 410 */
            {   int ret = r_mark_possessives(z); /* call mark_possessives, line 410 */
                if (ret == 0) goto lab52;
                if (ret < 0) return ret;
            }
            goto lab51;
        lab52:
            z->c = z->l - m26;
            {   int ret = r_mark_sU(z); /* call mark_sU, line 410 */
                if (ret <= 0) return ret;
            }
        }
    lab51:
        z->bra = z->c; /* ], line 410 */
        {   int ret = slice_del(z); /* delete, line 410 */
            if (ret < 0) return ret;
        }
        {   int m27 = z->l - z->c; (void)m27; /* try, line 410 */
            z->ket = z->c; /* [, line 410 */
            {   int ret = r_mark_lAr(z); /* call mark_lAr, line 410 */
                if (ret == 0) { z->c = z->l - m27; goto lab53; }
                if (ret < 0) return ret;
            }
            z->bra = z->c; /* ], line 410 */
            {   int ret = slice_del(z); /* delete, line 410 */
                if (ret < 0) return ret;
            }
            {   int ret = r_stem_suffix_chain_before_ki(z); /* call stem_suffix_chain_before_ki, line 410 */
                if (ret == 0) { z->c = z->l - m27; goto lab53; }
                if (ret < 0) return ret;
            }
        lab53:
            ;
        }
    }
lab0:
    return 1;
}

static int r_post_process_last_consonants(struct SN_env * z) { /* backwardmode */
    int among_var;
    z->ket = z->c; /* [, line 414 */
    among_var = find_among_b(z, a_23, 4); /* substring, line 414 */
    if (!(among_var)) return 0;
    z->bra = z->c; /* ], line 414 */
    switch (among_var) { /* among, line 414 */
        case 1:
            {   int ret = slice_from_s(z, 1, s_5); /* <-, line 415 */
                if (ret < 0) return ret;
            }
            break;
        case 2:
            {   int ret = slice_from_s(z, 2, s_6); /* <-, line 416 */
                if (ret < 0) return ret;
            }
            break;
        case 3:
            {   int ret = slice_from_s(z, 1, s_7); /* <-, line 417 */
                if (ret < 0) return ret;
            }
            break;
        case 4:
            {   int ret = slice_from_s(z, 1, s_8); /* <-, line 418 */
                if (ret < 0) return ret;
            }
            break;
    }
    return 1;
}

static int r_append_U_to_stems_ending_with_d_or_g(struct SN_env * z) { /* backwardmode */
    {   int m_test1 = z->l - z->c; /* test, line 429 */
        {   int m2 = z->l - z->c; (void)m2; /* or, line 429 */
            if (z->c <= z->lb || z->p[z->c - 1] != 'd') goto lab1; /* literal, line 429 */
            z->c--;
            goto lab0;
        lab1:
            z->c = z->l - m2;
            if (z->c <= z->lb || z->p[z->c - 1] != 'g') return 0; /* literal, line 429 */
            z->c--;
        }
    lab0:
        z->c = z->l - m_test1;
    }
    {   int m3 = z->l - z->c; (void)m3; /* or, line 431 */
        {   int m_test4 = z->l - z->c; /* test, line 430 */
            if (out_grouping_b_U(z, g_vowel, 97, 305, 1) < 0) goto lab3; /* goto */ /* grouping vowel, line 430 */
            {   int m5 = z->l - z->c; (void)m5; /* or, line 430 */
                if (z->c <= z->lb || z->p[z->c - 1] != 'a') goto lab5; /* literal, line 430 */
                z->c--;
                goto lab4;
            lab5:
                z->c = z->l - m5;
                if (!(eq_s_b(z, 2, s_9))) goto lab3; /* literal, line 430 */
            }
        lab4:
            z->c = z->l - m_test4;
        }
        {   int ret;
            {   int saved_c = z->c;
                ret = insert_s(z, z->c, z->c, 2, s_10); /* <+, line 430 */
                z->c = saved_c;
            }
            if (ret < 0) return ret;
        }
        goto lab2;
    lab3:
        z->c = z->l - m3;
        {   int m_test6 = z->l - z->c; /* test, line 432 */
            if (out_grouping_b_U(z, g_vowel, 97, 305, 1) < 0) goto lab6; /* goto */ /* grouping vowel, line 432 */
            {   int m7 = z->l - z->c; (void)m7; /* or, line 432 */
                if (z->c <= z->lb || z->p[z->c - 1] != 'e') goto lab8; /* literal, line 432 */
                z->c--;
                goto lab7;
            lab8:
                z->c = z->l - m7;
                if (z->c <= z->lb || z->p[z->c - 1] != 'i') goto lab6; /* literal, line 432 */
                z->c--;
            }
        lab7:
            z->c = z->l - m_test6;
        }
        {   int ret;
            {   int saved_c = z->c;
                ret = insert_s(z, z->c, z->c, 1, s_11); /* <+, line 432 */
                z->c = saved_c;
            }
            if (ret < 0) return ret;
        }
        goto lab2;
    lab6:
        z->c = z->l - m3;
        {   int m_test8 = z->l - z->c; /* test, line 434 */
            if (out_grouping_b_U(z, g_vowel, 97, 305, 1) < 0) goto lab9; /* goto */ /* grouping vowel, line 434 */
            {   int m9 = z->l - z->c; (void)m9; /* or, line 434 */
                if (z->c <= z->lb || z->p[z->c - 1] != 'o') goto lab11; /* literal, line 434 */
                z->c--;
                goto lab10;
            lab11:
                z->c = z->l - m9;
                if (z->c <= z->lb || z->p[z->c - 1] != 'u') goto lab9; /* literal, line 434 */
                z->c--;
            }
        lab10:
            z->c = z->l - m_test8;
        }
        {   int ret;
            {   int saved_c = z->c;
                ret = insert_s(z, z->c, z->c, 1, s_12); /* <+, line 434 */
                z->c = saved_c;
            }
            if (ret < 0) return ret;
        }
        goto lab2;
    lab9:
        z->c = z->l - m3;
        {   int m_test10 = z->l - z->c; /* test, line 436 */
            if (out_grouping_b_U(z, g_vowel, 97, 305, 1) < 0) return 0; /* goto */ /* grouping vowel, line 436 */
            {   int m11 = z->l - z->c; (void)m11; /* or, line 436 */
                if (!(eq_s_b(z, 2, s_13))) goto lab13; /* literal, line 436 */
                goto lab12;
            lab13:
                z->c = z->l - m11;
                if (!(eq_s_b(z, 2, s_14))) return 0; /* literal, line 436 */
            }
        lab12:
            z->c = z->l - m_test10;
        }
        {   int ret;
            {   int saved_c = z->c;
                ret = insert_s(z, z->c, z->c, 2, s_15); /* <+, line 436 */
                z->c = saved_c;
            }
            if (ret < 0) return ret;
        }
    }
lab2:
    return 1;
}

static int r_is_reserved_word(struct SN_env * z) { /* backwardmode */
    if (!(eq_s_b(z, 2, s_16))) return 0; /* literal, line 440 */
    {   int m1 = z->l - z->c; (void)m1; /* try, line 440 */
        if (!(eq_s_b(z, 3, s_17))) { z->c = z->l - m1; goto lab0; } /* literal, line 440 */
    lab0:
        ;
    }
    if (z->c > z->lb) return 0; /* atlimit, line 440 */
    return 1;
}

static int r_more_than_one_syllable_word(struct SN_env * z) { /* forwardmode */
    {   int c_test1 = z->c; /* test, line 447 */
        {   int i = 2;
            while(1) { /* atleast, line 447 */
                int c2 = z->c;
                {    /* gopast */ /* grouping vowel, line 447 */
                    int ret = out_grouping_U(z, g_vowel, 97, 305, 1);
                    if (ret < 0) goto lab0;
                    z->c += ret;
                }
                i--;
                continue;
            lab0:
                z->c = c2;
                break;
            }
            if (i > 0) return 0;
        }
        z->c = c_test1;
    }
    return 1;
}

static int r_postlude(struct SN_env * z) { /* forwardmode */
    z->lb = z->c; z->c = z->l; /* backwards, line 451 */

    {   int m1 = z->l - z->c; (void)m1; /* not, line 452 */
        {   int ret = r_is_reserved_word(z); /* call is_reserved_word, line 452 */
            if (ret == 0) goto lab0;
            if (ret < 0) return ret;
        }
        return 0;
    lab0:
        z->c = z->l - m1;
    }
    {   int m2 = z->l - z->c; (void)m2; /* do, line 453 */
        {   int ret = r_append_U_to_stems_ending_with_d_or_g(z); /* call append_U_to_stems_ending_with_d_or_g, line 453 */
            if (ret == 0) goto lab1;
            if (ret < 0) return ret;
        }
    lab1:
        z->c = z->l - m2;
    }
    {   int m3 = z->l - z->c; (void)m3; /* do, line 454 */
        {   int ret = r_post_process_last_consonants(z); /* call post_process_last_consonants, line 454 */
            if (ret == 0) goto lab2;
            if (ret < 0) return ret;
        }
    lab2:
        z->c = z->l - m3;
    }
    z->c = z->lb;
    return 1;
}

extern int turkish_UTF_8_stem(struct SN_env * z) { /* forwardmode */
    {   int ret = r_more_than_one_syllable_word(z); /* call more_than_one_syllable_word, line 460 */
        if (ret <= 0) return ret;
    }
    z->lb = z->c; z->c = z->l; /* backwards, line 462 */

    {   int m1 = z->l - z->c; (void)m1; /* do, line 463 */
        {   int ret = r_stem_nominal_verb_suffixes(z); /* call stem_nominal_verb_suffixes, line 463 */
            if (ret == 0) goto lab0;
            if (ret < 0) return ret;
        }
    lab0:
        z->c = z->l - m1;
    }
    if (!(z->B[0])) return 0; /* Boolean test continue_stemming_noun_suffixes, line 464 */
    {   int m2 = z->l - z->c; (void)m2; /* do, line 465 */
        {   int ret = r_stem_noun_suffixes(z); /* call stem_noun_suffixes, line 465 */
            if (ret == 0) goto lab1;
            if (ret < 0) return ret;
        }
    lab1:
        z->c = z->l - m2;
    }
    z->c = z->lb;
    {   int ret = r_postlude(z); /* call postlude, line 468 */
        if (ret <= 0) return ret;
    }
    return 1;
}

extern struct SN_env * turkish_UTF_8_create_env(void) { return SN_create_env(0, 0, 1); }

extern void turkish_UTF_8_close_env(struct SN_env * z) { SN_close_env(z, 0); }

