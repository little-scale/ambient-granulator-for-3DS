#include <citro2d.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "audio_output.h"
#include "build_info.h"
#include "edit_repeat.h"
#include "microphone_input.h"
#include "ram_sample.h"
#include "sample_bank.h"
#include "sample_library.h"
#include "waveform.h"

#define SCREEN_HEIGHT 240
#define BOTTOM_WIDTH 320
#define CELL_WIDTH 8
#define CELL_HEIGHT 8
#define TOP_ROW_OFFSET 1
#define MARKER_COUNT 16
#define MARKER_LIFETIME 18
#define CIRCLE_NAV_PRESS 70
#define CIRCLE_NAV_RELEASE 45

typedef struct {
    const char *name;
    int value;
    int minimum;
    int maximum;
    int step;
    int coarse_step;
    const char *unit;
    int column;
    int row;
    int value_column;
} Parameter;

enum {
    PARAM_RANGE,
    PARAM_PITCH,
    PARAM_FINE,
    PARAM_PITCH_DEVIATION,
    PARAM_FINE_DEVIATION,
    PARAM_CLOCK,
    PARAM_BPM,
    PARAM_DIVISION,
    PARAM_INTERVAL,
    PARAM_JITTER,
    PARAM_GRAINS,
    PARAM_POLYPHONY,
    PARAM_LENGTH,
    PARAM_ATTACK,
    PARAM_RELEASE,
    PARAM_GAIN,
    PARAM_VOLUME,
    PARAM_PAN,
    PARAM_PAN_DIVERGENCE,
    PARAM_REVERB,
    PARAM_REVERB_FEEDBACK,
    PARAM_REVERB_SIZE,
    PARAM_REVERB_DAMPING,
    PARAM_REVERB_FREEZE,
    PARAM_PHASER_DEPTH,
    PARAM_PHASER_SPEED,
    PARAM_HIGHPASS,
    PARAM_LOWPASS,
    PARAM_MIC_GAIN,
    PARAM_SAMPLE,
    PARAM_COUNT,
};

static Parameter parameters[PARAM_COUNT] = {
    { "RANGE",    24, -128, 128,  1,  16, "PX", 0,  2,  8 },
    { "PITCH",     0,  -24,  24,  1,  12, "ST", 0,  3,  8 },
    { "FINE",      0, -100, 100,  1,  10, "CT", 0,  4,  8 },
    { "P DEV",     0,    0,  12,  1,   4, "ST", 0,  5,  8 },
    { "F DEV",     0,    0, 100,  1,  10, "CT", 0,  6,  8 },
    { "MODE",      1,    0,   1,  1,   1,   "", 0,  9,  8 },
    { "BPM",      96,   40, 240,  1,  10,   "", 0, 10,  8 },
    { "DIV",       1,    0,   3,  1,   1,   "", 0, 11,  8 },
    { "INTERVAL",120,   20,1000, 10, 100, "MS", 0, 12,  8 },
    { "JITTER",   20,    0, 100,  5,  20,  "%", 0, 13,  8 },
    { "GRAINS",    8,    1,  32,  1,   4,   "", 0, 14,  8 },
    { "POLY",      16,    1,  16,  1,   4,   "", 0, 18,  8 },
    { "LENGTH",  120,   20, 500, 10, 100, "MS",25,  2, 34 },
    { "ATTACK",   15,    0,  50,  5,  20,  "%",25,  3, 34 },
    { "RELEASE",  25,    0,  50,  5,  20,  "%",25,  4, 34 },
    { "GAIN",      0,  -24,  18,  1,   6, "DB",25,  5, 34 },
    { "VOL",     100,    0, 100,  1,  10,  "%",25,  6, 34 },
    { "PAN",       0, -100, 100,  1,  10,  "%",25,  7, 34 },
    { "P DEV",   100,    0, 100,  1,  10,  "%",25,  8, 34 },
    { "REV",     100,    0, 100,  1,  10,  "%",25, 13, 34 },
    { "FEEDBACK",900,    0, 999,  1,  10,  "%",25, 14, 34 },
    { "SIZE",    100,    0, 100,  1,  10,  "%",25, 15, 34 },
    { "DAMP",      5,    0, 100,  1,  10,  "%",25, 16, 34 },
    { "FREEZE",    0,    0,   1,  1,   1,   "",25, 17, 34 },
    { "PHASE",   100,    0, 100,  1,  10,  "%",25, 11, 34 },
    { "P SPD",    10,    1, 100,  1,  10,   "",25, 12, 34 },
    { "HPF",       0,    0,4000, 10, 500, "HZ",25, 20, 34 },
    { "LPF",    8000,  200,8000, 10, 500, "HZ",25, 21, 34 },
    { "MIC GAIN", MICROPHONE_INPUT_DEFAULT_GAIN,
        MICROPHONE_INPUT_MIN_GAIN, MICROPHONE_INPUT_MAX_GAIN,
        1, 12, "", 0, 21, 9 },
    { "SAMPLE",    0,    0,   0,  1,   1,   "", 0, 17,  8 },
};

static const int divisions[] = { 8, 16, 32, 64 };
static int16_t waveform_minimum[BOTTOM_WIDTH];
static int16_t waveform_maximum[BOTTOM_WIDTH];

static const uint8_t digit_font[12][7] = {
    { 14, 17, 19, 21, 25, 17, 14 },
    {  4, 12,  4,  4,  4,  4, 14 },
    { 14, 17,  1,  2,  4,  8, 31 },
    { 30,  1,  1, 14,  1,  1, 30 },
    {  2,  6, 10, 18, 31,  2,  2 },
    { 31, 16, 16, 30,  1,  1, 30 },
    { 14, 16, 16, 30, 17, 17, 14 },
    { 31,  1,  2,  4,  8,  8,  8 },
    { 14, 17, 17, 14, 17, 17, 14 },
    { 14, 17, 17, 15,  1,  1, 14 },
    {  0,  4,  4, 31,  4,  4,  0 },
    {  0,  0,  0, 31,  0,  0,  0 },
};

static const uint8_t alphabet_font[26][7] = {
    { 14, 17, 17, 31, 17, 17, 17 }, { 30, 17, 17, 30, 17, 17, 30 },
    { 14, 17, 16, 16, 16, 17, 14 }, { 30, 17, 17, 17, 17, 17, 30 },
    { 31, 16, 16, 30, 16, 16, 31 }, { 31, 16, 16, 30, 16, 16, 16 },
    { 14, 17, 16, 23, 17, 17, 15 }, { 17, 17, 17, 31, 17, 17, 17 },
    { 14,  4,  4,  4,  4,  4, 14 }, {  7,  2,  2,  2, 18, 18, 12 },
    { 17, 18, 20, 24, 20, 18, 17 }, { 16, 16, 16, 16, 16, 16, 31 },
    { 17, 27, 21, 21, 17, 17, 17 }, { 17, 25, 21, 19, 17, 17, 17 },
    { 14, 17, 17, 17, 17, 17, 14 }, { 30, 17, 17, 30, 16, 16, 16 },
    { 14, 17, 17, 17, 21, 18, 13 }, { 30, 17, 17, 30, 20, 18, 17 },
    { 15, 16, 16, 14,  1,  1, 30 }, { 31,  4,  4,  4,  4,  4,  4 },
    { 17, 17, 17, 17, 17, 17, 14 }, { 17, 17, 17, 17, 10, 10,  4 },
    { 17, 17, 17, 21, 21, 21, 10 }, { 17, 17, 10,  4, 10, 17, 17 },
    { 17, 17, 10,  4,  4,  4,  4 }, { 31,  1,  2,  4,  8, 16, 31 },
};

static const uint8_t percent_glyph[7] = { 17, 2, 4, 4, 8, 16, 17 };
static const uint8_t slash_glyph[7] = { 1, 2, 2, 4, 8, 8, 16 };
static const uint8_t colon_glyph[7] = { 0, 12, 12, 0, 12, 12, 0 };
static const uint8_t dot_glyph[7] = { 0, 0, 0, 0, 0, 12, 12 };

typedef struct {
    int selected_parameter;
    int playhead_x;
    bool b_pressed;
    bool b_used;
    bool touch_active;
    bool sample_is_ram;
    bool ram_allocation_failed;
    uint32_t recording_start_sample;
    int recording_start_x;
    touchPosition touch;
    circlePosition circle;
    circlePosition cstick;
    uint32_t keys_held;
    EditRepeatState edit_repeat;
    EditRepeatState navigation_repeat;
    EditRepeatState playhead_repeat;
    uint32_t previous_circle_directions;
    uint32_t previous_cstick_directions;
    int marker_x[MARKER_COUNT];
    int marker_ttl[MARKER_COUNT];
    int next_marker;
} AppState;

static const uint8_t *font_glyph(char character)
{
    if (character >= '0' && character <= '9')
        return digit_font[character - '0'];
    if (character >= 'A' && character <= 'Z')
        return alphabet_font[character - 'A'];
    if (character >= 'a' && character <= 'z')
        return alphabet_font[character - 'a'];
    if (character == '+')
        return digit_font[10];
    if (character == '-')
        return digit_font[11];
    if (character == '%')
        return percent_glyph;
    if (character == '/')
        return slash_glyph;
    if (character == ':')
        return colon_glyph;
    if (character == '.')
        return dot_glyph;
    return NULL;
}

static void draw_text(int column, int row, const char *text, bool inverted)
{
    uint32_t white = C2D_Color32(255, 255, 255, 255);
    uint32_t black = C2D_Color32(0, 0, 0, 255);
    for (int index = 0; text[index] != '\0'; index++) {
        float origin_x = (float)((column + index) * CELL_WIDTH);
        float origin_y = (float)(row * CELL_HEIGHT);
        if (inverted)
            C2D_DrawRectSolid(origin_x, origin_y, 0.1f,
                              CELL_WIDTH, CELL_HEIGHT, black);
        const uint8_t *glyph = font_glyph(text[index]);
        if (glyph == NULL)
            continue;
        uint32_t ink = inverted ? white : black;
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 5; x++) {
                if (glyph[y] & (1U << (4 - x)))
                    C2D_DrawRectSolid(origin_x + 1 + x, origin_y + y,
                                      0.5f, 1.0f, 1.0f, ink);
            }
        }
    }
}

static void draw_rule(float x, float y, float width)
{
    C2D_DrawRectSolid(x, y, 0.3f, width, 1.0f,
                      C2D_Color32(0, 0, 0, 255));
}

static void draw_section_header(int column, int row, const char *text)
{
    draw_text(column, row, text, false);
    float start = (float)((column + (int)strlen(text) + 1) * CELL_WIDTH);
    float end = (float)((column + 23) * CELL_WIDTH);
    if (end > start)
        draw_rule(start, (float)(row * CELL_HEIGHT + 7), end - start);
}

static void draw_peak_bar(int row, const char *label, uint16_t peak)
{
    const float x = 2.0f * CELL_WIDTH;
    const float y = (float)(row * CELL_HEIGHT + 1);
    const float width = 48.0f * CELL_WIDTH;
    const float height = 6.0f;
    uint32_t black = C2D_Color32(0, 0, 0, 255);
    draw_text(0, row, label, false);
    C2D_DrawRectSolid(x, y, 0.2f, width, 1.0f, black);
    C2D_DrawRectSolid(x, y + height - 1.0f, 0.2f, width, 1.0f, black);
    C2D_DrawRectSolid(x, y, 0.2f, 1.0f, height, black);
    C2D_DrawRectSolid(x + width - 1.0f, y, 0.2f, 1.0f, height, black);
    float fill = (width - 2.0f) * peak / 32768.0f;
    if (fill > 0.0f)
        C2D_DrawRectSolid(x + 1.0f, y + 1.0f, 0.3f,
                          fill, height - 2.0f, black);
}

static void draw_input_peak_bar(int row, uint16_t peak)
{
    const float x = 2.0f * CELL_WIDTH;
    const float y = (float)(row * CELL_HEIGHT + 1);
    const float width = 21.0f * CELL_WIDTH;
    const float height = 6.0f;
    uint32_t black = C2D_Color32(0, 0, 0, 255);
    draw_text(0, row, "IN", false);
    C2D_DrawRectSolid(x, y, 0.2f, width, 1.0f, black);
    C2D_DrawRectSolid(x, y + height - 1.0f, 0.2f, width, 1.0f, black);
    C2D_DrawRectSolid(x, y, 0.2f, 1.0f, height, black);
    C2D_DrawRectSolid(x + width - 1.0f, y, 0.2f, 1.0f, height, black);

    float level = 0.0f;
    if (peak > 0) {
        double db = 20.0 * log10((double)peak / 32768.0);
        level = (float)((db + 60.0) / 60.0);
        if (level < 0.0f)
            level = 0.0f;
        if (level > 1.0f)
            level = 1.0f;
    }
    float fill = (width - 2.0f) * level;
    if (fill > 0.0f)
        C2D_DrawRectSolid(x + 1.0f, y + 1.0f, 0.3f,
                          fill, height - 2.0f, black);
}

static void format_output_peak(const AudioOutput *audio, char *text,
                               size_t size)
{
    uint16_t peak = audio_output_peak_left(audio);
    if (audio_output_peak_right(audio) > peak)
        peak = audio_output_peak_right(audio);
    if (peak == 0) {
        snprintf(text, size, "PEAK -INF");
        return;
    }
    double ratio = (double)peak / 32768.0;
    int tenths = (int)lround(200.0 * log10(ratio));
    if (tenths > 0)
        tenths = 0;
    int magnitude = tenths < 0 ? -tenths : tenths;
    snprintf(text, size, "PEAK %s%d.%dDB", tenths < 0 ? "-" : "",
             magnitude / 10, magnitude % 10);
}

static void format_parameter(const AppState *state, int index,
                             char *text, size_t size)
{
    int value = parameters[index].value;
    switch (index) {
    case PARAM_RANGE:
        if (value > 0)
            snprintf(text, size, "+-%d", value);
        else
            snprintf(text, size, "%d", value);
        break;
    case PARAM_PITCH:
    case PARAM_FINE:
        snprintf(text, size, "%+d", value);
        break;
    case PARAM_CLOCK:
        snprintf(text, size, "%s", value ? "SYNC" : "FREE");
        break;
    case PARAM_DIVISION:
        snprintf(text, size, "1/%d", divisions[value]);
        break;
    case PARAM_GAIN:
        snprintf(text, size, "%+dDB", value);
        break;
    case PARAM_MIC_GAIN: {
        int tenths = 105 + value * 5;
        snprintf(text, size, "%d.%dDB", tenths / 10, tenths % 10);
        break;
    }
    case PARAM_PAN:
        snprintf(text, size, "%+d%%", value);
        break;
    case PARAM_JITTER:
    case PARAM_ATTACK:
    case PARAM_RELEASE:
    case PARAM_VOLUME:
    case PARAM_PAN_DIVERGENCE:
    case PARAM_REVERB:
    case PARAM_REVERB_SIZE:
    case PARAM_REVERB_DAMPING:
    case PARAM_PHASER_DEPTH:
        snprintf(text, size, "%d%%", value);
        break;
    case PARAM_PHASER_SPEED:
        snprintf(text, size, "%d.%02dHZ", value / 100, value % 100);
        break;
    case PARAM_REVERB_FEEDBACK:
        snprintf(text, size, "%d.%d%%", value / 10, value % 10);
        break;
    case PARAM_REVERB_FREEZE:
        snprintf(text, size, "%s", value ? "ON" : "OFF");
        break;
    case PARAM_HIGHPASS:
        if (value == 0)
            snprintf(text, size, "OFF");
        else
            snprintf(text, size, "%dHZ", value);
        break;
    case PARAM_LOWPASS:
        if (value >= 8000)
            snprintf(text, size, "OFF");
        else
            snprintf(text, size, "%dHZ", value);
        break;
    case PARAM_SAMPLE:
        if (state->sample_is_ram)
            snprintf(text, size, "RAM");
        else
            snprintf(text, size, "%02d/%02d", value + 1,
                     parameters[PARAM_SAMPLE].maximum + 1);
        break;
    default:
        snprintf(text, size, "%d", value);
        break;
    }
}

static void draw_top_screen(const AppState *state, const AudioOutput *audio,
                            const SampleBank *bank,
                            const LoadedSample *sample,
                            const MicrophoneInput *microphone)
{
    char text[64];
    draw_text(0, TOP_ROW_OFFSET, "GRAIN", false);
    if (state->sample_is_ram)
        snprintf(text, sizeof(text), "RAM");
    else
        snprintf(text, sizeof(text), "S%02d",
                 parameters[PARAM_SAMPLE].value + 1);
    draw_text(8, TOP_ROW_OFFSET, text, false);
    draw_text(25, TOP_ROW_OFFSET, "VOICE", false);
    draw_text(35, TOP_ROW_OFFSET, APP_BUILD_ID, false);
    draw_rule(0, TOP_ROW_OFFSET * CELL_HEIGHT + 7, 184);
    draw_rule(200, TOP_ROW_OFFSET * CELL_HEIGHT + 7, 184);
    draw_section_header(0, 8 + TOP_ROW_OFFSET, "CLOCK");
    draw_section_header(0, 16 + TOP_ROW_OFFSET, "SOURCE");
    draw_section_header(25, 10 + TOP_ROW_OFFSET, "SPACE");
    draw_section_header(25, 19 + TOP_ROW_OFFSET, "OUTPUT");

    for (int index = 0; index < PARAM_COUNT; index++) {
        const Parameter *parameter = &parameters[index];
        draw_text(parameter->column, parameter->row + TOP_ROW_OFFSET,
                  parameter->name, false);
        format_parameter(state, index, text, sizeof(text));
        draw_text(parameter->value_column,
                  parameter->row + TOP_ROW_OFFSET, text,
                  index == state->selected_parameter);
        if (parameter->unit[0] != '\0' && parameter->column == 0)
            draw_text(14, parameter->row + TOP_ROW_OFFSET,
                      parameter->unit, false);
    }

    snprintf(text, sizeof(text), "POSITION %03d", state->playhead_x);
    draw_text(0, 19 + TOP_ROW_OFFSET, text, false);

    if (bank->sample_count > 0 && sample->samples != NULL) {
        snprintf(text, sizeof(text), "BANK %02lu NDSGRN01",
                 (unsigned long)bank->sample_count);
        draw_text(0, 20 + TOP_ROW_OFFSET, text, false);
        snprintf(text, sizeof(text), "SAMPLE %.31s", sample->name);
        draw_text(0, 25 + TOP_ROW_OFFSET, text, false);
    } else {
        draw_text(0, 20 + TOP_ROW_OFFSET, "BANK MISSING", false);
        draw_text(0, 25 + TOP_ROW_OFFSET, bank->error, false);
    }

    if (audio->ready) {
        snprintf(text, sizeof(text), "NDSP V%02d/%02d",
                 audio_output_active_voices(audio),
                 parameters[PARAM_POLYPHONY].value);
        draw_text(0, 23 + TOP_ROW_OFFSET, text, false);
        snprintf(text, sizeof(text), "BUF %08lX",
                 (unsigned long)audio_output_buffers_submitted(audio));
        draw_text(0, 24 + TOP_ROW_OFFSET, text, false);
    } else {
        snprintf(text, sizeof(text), "ERR %08lX",
                 (unsigned long)(uint32_t)audio->init_result);
        draw_text(0, 23 + TOP_ROW_OFFSET, text, false);
        if (R_MODULE(audio->init_result) == RM_DSP
                && R_SUMMARY(audio->init_result) == RS_NOTFOUND
                && R_DESCRIPTION(audio->init_result) == RD_NOT_FOUND)
            draw_text(0, 24 + TOP_ROW_OFFSET,
                      "DSP FIRM MISSING", false);
        else
            draw_text(0, 24 + TOP_ROW_OFFSET,
                      "NDSP INIT FAILED", false);
    }

    if (microphone_input_recording(microphone)) {
        uint32_t samples = microphone_input_sample_count(microphone);
        uint32_t hundredths = samples * 100U
                            / MICROPHONE_INPUT_SAMPLE_RATE;
        snprintf(text, sizeof(text), "REC @%03d %lu.%02luS",
                 state->recording_start_x,
                 (unsigned long)(hundredths / 100U),
                 (unsigned long)(hundredths % 100U));
        draw_text(0, 22 + TOP_ROW_OFFSET, text, true);
    } else if (!microphone_input_available(microphone)) {
        snprintf(text, sizeof(text), "MIC ERR %08lX",
                 (unsigned long)(uint32_t)
                    microphone_input_result(microphone));
        draw_text(0, 22 + TOP_ROW_OFFSET, text, false);
    } else if (state->ram_allocation_failed) {
        draw_text(0, 22 + TOP_ROW_OFFSET, "RAM ALLOC FAILED", false);
    } else if (microphone_input_take_too_short(microphone)) {
        draw_text(0, 22 + TOP_ROW_OFFSET, "MIC TAP IGNORED", false);
    } else if (R_FAILED((Result)microphone_input_result(microphone))) {
        snprintf(text, sizeof(text), "MIC ERR %08lX",
                 (unsigned long)(uint32_t)
                    microphone_input_result(microphone));
        draw_text(0, 22 + TOP_ROW_OFFSET, text, false);
    }

    format_output_peak(audio, text, sizeof(text));
    draw_text(25, 23 + TOP_ROW_OFFSET, text, false);
    if (audio_output_clipping(audio))
        draw_text(44, 23 + TOP_ROW_OFFSET, "CLIP", true);

    draw_input_peak_bar(26 + TOP_ROW_OFFSET,
                        microphone_input_peak(microphone));
    draw_peak_bar(27 + TOP_ROW_OFFSET, "L", audio_output_peak_left(audio));
    draw_peak_bar(28 + TOP_ROW_OFFSET, "R", audio_output_peak_right(audio));
}

static bool marker_covers_column(const AppState *state, int x)
{
    for (int index = 0; index < MARKER_COUNT; index++) {
        if (state->marker_ttl[index] > 0
                && x >= state->marker_x[index] - 1
                && x <= state->marker_x[index] + 2)
            return true;
    }
    return false;
}

static void draw_boundary(int x, uint32_t color)
{
    for (int y = 0; y < SCREEN_HEIGHT; y += 8)
        C2D_DrawRectSolid((float)x, (float)y, 0.2f, 2.0f, 4.0f, color);
}

static void draw_bottom_screen(const AppState *state)
{
    uint32_t white = C2D_Color32(255, 255, 255, 255);
    uint32_t black = C2D_Color32(0, 0, 0, 255);
    const int zero_y = SCREEN_HEIGHT / 2;
    for (int x = 0; x < BOTTOM_WIDTH; x++) {
        int top = zero_y - (int)waveform_maximum[x] * 108 / 32768;
        int bottom = zero_y - (int)waveform_minimum[x] * 108 / 32768;
        if (top < 0)
            top = 0;
        if (bottom >= SCREEN_HEIGHT)
            bottom = SCREEN_HEIGHT - 1;
        if (bottom < top)
            bottom = top;
        uint32_t color = marker_covers_column(state, x) ? black : white;
        C2D_DrawRectSolid((float)x, (float)top, 0.5f, 1.0f,
                          (float)(bottom - top + 1), color);
    }

    int range = parameters[PARAM_RANGE].value;
    int amount = range < 0 ? -range : range;
    if (amount > 0) {
        int right = state->playhead_x + amount;
        if (right >= BOTTOM_WIDTH)
            right = BOTTOM_WIDTH - 1;
        draw_boundary(right - 1, white);
        if (range >= 0) {
            int left = state->playhead_x - amount;
            if (left < 0)
                left = 0;
            draw_boundary(left, white);
        }
    }
    C2D_DrawRectSolid((float)state->playhead_x, 0.0f, 0.1f,
                      1.0f, SCREEN_HEIGHT, white);
}

static void reset_parameters(void)
{
    parameters[PARAM_RANGE].value = 24;
    parameters[PARAM_PITCH].value = 0;
    parameters[PARAM_FINE].value = 0;
    parameters[PARAM_PITCH_DEVIATION].value = 0;
    parameters[PARAM_FINE_DEVIATION].value = 0;
    parameters[PARAM_CLOCK].value = 1;
    parameters[PARAM_BPM].value = 96;
    parameters[PARAM_DIVISION].value = 1;
    parameters[PARAM_INTERVAL].value = 120;
    parameters[PARAM_JITTER].value = 20;
    parameters[PARAM_GRAINS].value = 8;
    parameters[PARAM_POLYPHONY].value = 16;
    parameters[PARAM_LENGTH].value = 120;
    parameters[PARAM_ATTACK].value = 15;
    parameters[PARAM_RELEASE].value = 25;
    parameters[PARAM_GAIN].value = 0;
    parameters[PARAM_VOLUME].value = 100;
    parameters[PARAM_PAN].value = 0;
    parameters[PARAM_PAN_DIVERGENCE].value = 100;
    parameters[PARAM_REVERB].value = 100;
    parameters[PARAM_REVERB_FEEDBACK].value = 900;
    parameters[PARAM_REVERB_SIZE].value = 100;
    parameters[PARAM_REVERB_DAMPING].value = 5;
    parameters[PARAM_REVERB_FREEZE].value = 0;
    parameters[PARAM_PHASER_DEPTH].value = 100;
    parameters[PARAM_PHASER_SPEED].value = 10;
    parameters[PARAM_HIGHPASS].value = 0;
    parameters[PARAM_LOWPASS].value = 8000;
    parameters[PARAM_MIC_GAIN].value = MICROPHONE_INPUT_DEFAULT_GAIN;
}

static void add_marker(AppState *state, int x)
{
    state->marker_x[state->next_marker] = x;
    state->marker_ttl[state->next_marker] = MARKER_LIFETIME;
    state->next_marker = (state->next_marker + 1) % MARKER_COUNT;
}

static void animate_markers(AppState *state)
{
    for (int index = 0; index < MARKER_COUNT; index++) {
        if (state->marker_ttl[index] > 0)
            state->marker_ttl[index]--;
    }
}

static void move_cursor(AppState *state, int horizontal, int vertical)
{
    int selected = state->selected_parameter;
    if (horizontal != 0) {
        int target_column = parameters[selected].column == 0 ? 25 : 0;
        int best = -1;
        int distance = 100;
        for (int index = 0; index < PARAM_COUNT; index++) {
            if (parameters[index].column != target_column)
                continue;
            int candidate = abs(parameters[index].row
                              - parameters[selected].row);
            if (candidate < distance) {
                best = index;
                distance = candidate;
            }
        }
        if (best >= 0)
            state->selected_parameter = best;
    }

    if (vertical != 0) {
        selected = state->selected_parameter;
        int column = parameters[selected].column;
        int row = parameters[selected].row;
        int best = -1;
        int best_row = vertical > 0 ? 100 : -1;
        for (int index = 0; index < PARAM_COUNT; index++) {
            int candidate = parameters[index].row;
            if (parameters[index].column != column)
                continue;
            if (vertical > 0 && candidate > row && candidate < best_row) {
                best = index;
                best_row = candidate;
            }
            if (vertical < 0 && candidate < row && candidate > best_row) {
                best = index;
                best_row = candidate;
            }
        }
        if (best < 0) {
            best_row = vertical > 0 ? 100 : -1;
            for (int index = 0; index < PARAM_COUNT; index++) {
                int candidate = parameters[index].row;
                if (parameters[index].column != column)
                    continue;
                if (vertical > 0 && candidate < best_row) {
                    best = index;
                    best_row = candidate;
                }
                if (vertical < 0 && candidate > best_row) {
                    best = index;
                    best_row = candidate;
                }
            }
        }
        if (best >= 0)
            state->selected_parameter = best;
    }
}

static void nudge_selected(AppState *state, int amount)
{
    Parameter *parameter = &parameters[state->selected_parameter];
    parameter->value += amount;
    if (parameter->value < parameter->minimum)
        parameter->value = parameter->minimum;
    if (parameter->value > parameter->maximum)
        parameter->value = parameter->maximum;
}

static void nudge_playhead(AppState *state, int amount)
{
    state->playhead_x += amount;
    if (state->playhead_x < 0)
        state->playhead_x = 0;
    if (state->playhead_x >= BOTTOM_WIDTH)
        state->playhead_x = BOTTOM_WIDTH - 1;
}

static uint32_t circle_navigation_directions(
    const circlePosition *circle, uint32_t previous)
{
    uint32_t directions = 0;
    if (circle->dx >= CIRCLE_NAV_PRESS
            || ((previous & KEY_DRIGHT)
                && circle->dx > CIRCLE_NAV_RELEASE))
        directions |= KEY_DRIGHT;
    else if (circle->dx <= -CIRCLE_NAV_PRESS
            || ((previous & KEY_DLEFT)
                && circle->dx < -CIRCLE_NAV_RELEASE))
        directions |= KEY_DLEFT;

    if (circle->dy >= CIRCLE_NAV_PRESS
            || ((previous & KEY_DUP)
                && circle->dy > CIRCLE_NAV_RELEASE))
        directions |= KEY_DUP;
    else if (circle->dy <= -CIRCLE_NAV_PRESS
            || ((previous & KEY_DDOWN)
                && circle->dy < -CIRCLE_NAV_RELEASE))
        directions |= KEY_DDOWN;
    return directions;
}

static AudioRenderConfig render_config(const AppState *state)
{
    AudioRenderConfig config = {
        .granular = {
            .center_x = state->playhead_x,
            .range = parameters[PARAM_RANGE].value,
            .pitch_semitones = parameters[PARAM_PITCH].value,
            .fine_cents = parameters[PARAM_FINE].value,
            .pitch_deviation = parameters[PARAM_PITCH_DEVIATION].value,
            .fine_deviation_cents
                = parameters[PARAM_FINE_DEVIATION].value,
            .clock_sync = parameters[PARAM_CLOCK].value != 0,
            .bpm = parameters[PARAM_BPM].value,
            .division = parameters[PARAM_DIVISION].value,
            .interval_ms = parameters[PARAM_INTERVAL].value,
            .jitter_percent = parameters[PARAM_JITTER].value,
            .grain_count = parameters[PARAM_GRAINS].value,
            .voice_limit = parameters[PARAM_POLYPHONY].value,
            .length_ms = parameters[PARAM_LENGTH].value,
            .attack_percent = parameters[PARAM_ATTACK].value,
            .release_percent = parameters[PARAM_RELEASE].value,
            .gain_db = parameters[PARAM_GAIN].value,
            .volume_percent = parameters[PARAM_VOLUME].value,
            .pan_percent = parameters[PARAM_PAN].value,
            .pan_divergence = parameters[PARAM_PAN_DIVERGENCE].value,
            .gate = state->touch_active || (state->keys_held & KEY_A) != 0,
        },
        .effects = {
            .wet_percent = parameters[PARAM_REVERB].value,
            .feedback_tenths_percent
                = parameters[PARAM_REVERB_FEEDBACK].value,
            .size_percent = parameters[PARAM_REVERB_SIZE].value,
            .damping_percent = parameters[PARAM_REVERB_DAMPING].value,
            .freeze = parameters[PARAM_REVERB_FREEZE].value != 0,
            .phaser_depth_percent = parameters[PARAM_PHASER_DEPTH].value,
            .phaser_speed_hundredths_hz
                = parameters[PARAM_PHASER_SPEED].value,
            .highpass_hz = parameters[PARAM_HIGHPASS].value,
            .lowpass_hz = parameters[PARAM_LOWPASS].value,
        },
    };
    return config;
}

static bool select_sample(const SampleLibrary *library,
                          const LoadedSample **sample, AudioOutput *audio,
                          AppState *state, int index)
{
    const LoadedSample *replacement = sample_library_get(
        library, (uint32_t)index);
    if (replacement == NULL
            || !sample_library_copy_waveform(
                library, (uint32_t)index, waveform_minimum,
                waveform_maximum, BOTTOM_WIDTH))
        return false;
    if (audio != NULL)
        audio_output_set_sample(audio, replacement->samples,
                                replacement->sample_count,
                                replacement->sample_rate);
    *sample = replacement;
    state->sample_is_ram = false;
    state->ram_allocation_failed = false;
    memset(state->marker_ttl, 0, sizeof(state->marker_ttl));
    state->playhead_x = BOTTOM_WIDTH / 2;
    return true;
}

static bool prepare_ram_recording(RamSample *ram_sample,
                                  const LoadedSample *current_sample,
                                  AppState *state)
{
    state->ram_allocation_failed = false;
    if (current_sample == NULL || current_sample->samples == NULL
            || !ram_sample_clone(ram_sample, current_sample->samples,
                                 current_sample->sample_count,
                                 current_sample->sample_rate)) {
        state->ram_allocation_failed = true;
        return false;
    }
    state->recording_start_x = state->playhead_x;
    state->recording_start_sample = ram_sample_position(
        ram_sample->sample_count, state->playhead_x, BOTTOM_WIDTH);
    return true;
}

static bool commit_ram_recording(MicrophoneInput *microphone,
                                 RamSample *ram_sample,
                                 LoadedSample *ram_view,
                                 const LoadedSample **current_sample,
                                 AudioOutput *audio, AppState *state)
{
    if (!microphone_input_finish(microphone))
        return false;
    uint32_t written = ram_sample_punch_in(
        ram_sample, state->recording_start_sample,
        microphone_input_samples(microphone),
        microphone_input_sample_count(microphone),
        MICROPHONE_INPUT_SAMPLE_RATE);
    if (written == 0)
        return false;

    if (!state->sample_is_ram)
        snprintf(ram_view->name, sizeof(ram_view->name), "RAM %.28s",
                 (*current_sample)->name);
    ram_view->samples = ram_sample->samples;
    ram_view->sample_count = ram_sample->sample_count;
    ram_view->sample_rate = ram_sample->sample_rate;
    waveform_analyze_changed(ram_view->samples, ram_view->sample_count,
                             waveform_minimum, waveform_maximum,
                             BOTTOM_WIDTH, state->recording_start_sample,
                             written);
    audio_output_set_sample(audio, ram_view->samples,
                            ram_view->sample_count, ram_view->sample_rate);
    *current_sample = ram_view;
    state->sample_is_ram = true;
    memset(state->marker_ttl, 0, sizeof(state->marker_ttl));
    return true;
}

int main(void)
{
    AppState state;
    SampleBank bank;
    SampleLibrary sample_library;
    LoadedSample empty_sample;
    LoadedSample ram_view;
    RamSample ram_sample;
    MicrophoneInput microphone;
    const LoadedSample *loaded_sample = &empty_sample;
    memset(&state, 0, sizeof(state));
    memset(&bank, 0, sizeof(bank));
    memset(&sample_library, 0, sizeof(sample_library));
    memset(&empty_sample, 0, sizeof(empty_sample));
    memset(&ram_view, 0, sizeof(ram_view));
    memset(&ram_sample, 0, sizeof(ram_sample));
    memset(&microphone, 0, sizeof(microphone));
    state.playhead_x = BOTTOM_WIDTH / 2;
    reset_parameters();

    gfxInitDefault();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxExit();
        return 1;
    }
    if (!C2D_Init(8192)) {
        C3D_Fini();
        gfxExit();
        return 1;
    }
    C2D_Prepare();
    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if (top == NULL || bottom == NULL) {
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 1;
    }

    bool romfs_mounted = R_SUCCEEDED(romfsInit());
    if (!sample_bank_open(&bank,
            "sdmc:/3ds/3ds-granulator/sample_bank.bin"))
        sample_bank_open(&bank, "romfs:/sample_bank.bin");
    if (bank.file != NULL) {
        if (sample_library_load(&bank, BOTTOM_WIDTH, &sample_library)) {
            parameters[PARAM_SAMPLE].maximum
                = (int)sample_library.sample_count - 1;
            select_sample(&sample_library, &loaded_sample, NULL, &state, 0);
        } else {
            snprintf(bank.error, sizeof(bank.error), "PRELOAD FAILED");
        }
        sample_bank_close(&bank);
    }
    if (loaded_sample->samples == NULL)
        waveform_analyze(NULL, 0, waveform_minimum, waveform_maximum,
                         BOTTOM_WIDTH);

    AudioRenderConfig config = render_config(&state);
    AudioOutput audio;
    audio_output_init(&audio, &config, loaded_sample->samples,
                      loaded_sample->sample_count, loaded_sample->sample_rate);
    microphone_input_init(&microphone);
    bool cstick_available = R_SUCCEEDED(irrstInit());
    int applied_microphone_gain = MICROPHONE_INPUT_DEFAULT_GAIN;

    bool running = true;
    while (running && aptMainLoop()) {
        hidScanInput();
        uint32_t down = hidKeysDown();
        uint32_t up = hidKeysUp();
        state.keys_held = hidKeysHeld();
        hidCircleRead(&state.circle);
        if (cstick_available) {
            irrstScanInput();
            hidCstickRead(&state.cstick);
        }
        microphone_input_update(&microphone);
        bool recording = microphone_input_recording(&microphone);
        state.touch_active = !recording
                          && (state.keys_held & KEY_TOUCH) != 0;
        if (state.touch_active) {
            hidTouchRead(&state.touch);
            state.playhead_x = state.touch.px;
            if (state.playhead_x < 0)
                state.playhead_x = 0;
            if (state.playhead_x >= BOTTOM_WIDTH)
                state.playhead_x = BOTTOM_WIDTH - 1;
        }

        if (down & KEY_START)
            running = false;

        if ((down & KEY_R) && !recording) {
            AudioRenderConfig quiet_config = render_config(&state);
            quiet_config.granular.gate = false;
            audio_output_update(&audio, &quiet_config);
            if (prepare_ram_recording(&ram_sample, loaded_sample, &state)
                    && microphone_input_start(&microphone)) {
                audio_output_stop_grains(&audio);
                memset(state.marker_ttl, 0, sizeof(state.marker_ttl));
                state.b_pressed = false;
                state.b_used = false;
                state.touch_active = false;
            }
            recording = microphone_input_recording(&microphone);
        }

        if (recording && ((up & KEY_R)
                          || microphone_input_full(&microphone))) {
            AudioRenderConfig quiet_config = render_config(&state);
            quiet_config.granular.gate = false;
            audio_output_update(&audio, &quiet_config);
            commit_ram_recording(&microphone, &ram_sample, &ram_view,
                                 &loaded_sample, &audio, &state);
            recording = false;
        }

        if (!recording && (down & KEY_B)) {
            state.b_pressed = true;
            state.b_used = false;
        }

        const uint32_t direction_mask = KEY_DUP | KEY_DDOWN
                                      | KEY_DLEFT | KEY_DRIGHT;
        uint32_t directions_down = down & direction_mask;
        uint32_t directions_held = state.keys_held & direction_mask;
        uint32_t circle_directions = circle_navigation_directions(
            &state.circle, state.previous_circle_directions);
        uint32_t circle_directions_down = circle_directions
                                        & ~state.previous_circle_directions;
        state.previous_circle_directions = circle_directions;
        uint32_t cstick_directions = cstick_available
            ? circle_navigation_directions(
                &state.cstick, state.previous_cstick_directions)
            : 0;
        uint32_t cstick_directions_down = cstick_directions
                                        & ~state.previous_cstick_directions;
        state.previous_cstick_directions = cstick_directions;
        uint32_t analog_directions = circle_directions | cstick_directions;
        uint32_t analog_directions_down = circle_directions_down
                                        | cstick_directions_down;
        bool editing = (state.keys_held & KEY_B) != 0;
        bool moving_playhead = !editing
                            && (state.keys_held & KEY_Y) != 0;
        uint32_t edit_directions = edit_repeat_update(
            &state.edit_repeat,
            !recording && editing,
            recording ? 0 : directions_down | analog_directions_down,
            recording ? 0 : directions_held | analog_directions);
        uint32_t playhead_directions = edit_repeat_update(
            &state.playhead_repeat,
            !recording && moving_playhead,
            recording ? 0 : (directions_down | analog_directions_down)
                              & (KEY_DLEFT | KEY_DRIGHT),
            recording ? 0 : (directions_held | analog_directions)
                              & (KEY_DLEFT | KEY_DRIGHT));
        uint32_t navigation_directions = edit_repeat_update(
            &state.navigation_repeat,
            !recording && !editing && !moving_playhead,
            recording ? 0 : directions_down | analog_directions_down,
            recording ? 0 : directions_held | analog_directions);
        if (edit_directions != 0) {
            Parameter *parameter = &parameters[state.selected_parameter];
            int previous_sample = parameters[PARAM_SAMPLE].value;
            bool editing_sample = state.selected_parameter == PARAM_SAMPLE;
            if (edit_directions & KEY_DLEFT)
                nudge_selected(&state, -parameter->step);
            if (edit_directions & KEY_DRIGHT)
                nudge_selected(&state, parameter->step);
            if (edit_directions & KEY_DUP)
                nudge_selected(&state, parameter->coarse_step);
            if (edit_directions & KEY_DDOWN)
                nudge_selected(&state, -parameter->coarse_step);
            if ((parameters[PARAM_SAMPLE].value != previous_sample
                    || (editing_sample && state.sample_is_ram))
                    && !select_sample(&sample_library, &loaded_sample, &audio,
                                      &state,
                                      parameters[PARAM_SAMPLE].value))
                parameters[PARAM_SAMPLE].value = previous_sample;
            state.b_used = true;
        } else if (playhead_directions != 0) {
            int horizontal = ((playhead_directions & KEY_DRIGHT) ? 1 : 0)
                           - ((playhead_directions & KEY_DLEFT) ? 1 : 0);
            nudge_playhead(&state, horizontal);
        } else if (navigation_directions != 0) {
            int horizontal = ((navigation_directions & KEY_DRIGHT) ? 1 : 0)
                           - ((navigation_directions & KEY_DLEFT) ? 1 : 0);
            int vertical = ((navigation_directions & KEY_DDOWN) ? 1 : 0)
                         - ((navigation_directions & KEY_DUP) ? 1 : 0);
            move_cursor(&state, horizontal, vertical);
        }

        if (!recording && (up & KEY_B)) {
            if (state.b_pressed && !state.b_used)
                audio_output_trigger(&audio, state.playhead_x,
                                     parameters[PARAM_GRAINS].value);
            state.b_pressed = false;
        }
        if (down & KEY_L)
            parameters[PARAM_REVERB_FREEZE].value ^= 1;
        if (down & KEY_X)
            reset_parameters();
        if (!recording && microphone_input_available(&microphone)
                && parameters[PARAM_MIC_GAIN].value
                    != applied_microphone_gain) {
            if (microphone_input_set_gain(
                    &microphone,
                    (uint8_t)parameters[PARAM_MIC_GAIN].value))
                applied_microphone_gain = parameters[PARAM_MIC_GAIN].value;
            else
                parameters[PARAM_MIC_GAIN].value = applied_microphone_gain;
        }

        config = render_config(&state);
        if (recording)
            config.granular.gate = false;
        audio_output_update(&audio, &config);
        GranularMarker marker;
        while (audio_output_pop_marker(&audio, &marker))
            add_marker(&state, marker.x);
        animate_markers(&state);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, C2D_Color32(255, 255, 255, 255));
        C2D_SceneBegin(top);
        draw_top_screen(&state, &audio, &bank, loaded_sample, &microphone);
        C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(bottom);
        draw_bottom_screen(&state);
        C3D_FrameEnd(0);
    }

    audio_output_exit(&audio);
    microphone_input_exit(&microphone);
    if (cstick_available)
        irrstExit();
    ram_sample_free(&ram_sample);
    sample_library_free(&sample_library);
    sample_bank_close(&bank);
    if (romfs_mounted)
        romfsExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
