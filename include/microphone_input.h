#ifndef GRANULATOR_MICROPHONE_INPUT_H
#define GRANULATOR_MICROPHONE_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "recording_buffer.h"

#define MICROPHONE_INPUT_SAMPLE_RATE 32728
#define MICROPHONE_INPUT_GAIN 67
#define MICROPHONE_INPUT_MAX_SECONDS 4
#define MICROPHONE_INPUT_MAX_SAMPLES \
    (MICROPHONE_INPUT_SAMPLE_RATE * MICROPHONE_INPUT_MAX_SECONDS)
#define MICROPHONE_INPUT_MIN_SAMPLES 512

typedef struct {
    uint8_t *shared_buffer;
    uint32_t shared_buffer_size;
    uint32_t shared_data_size;
    int16_t *capture_samples;
    RecordingBuffer capture;
    int32_t init_result;
    int32_t last_result;
    bool initialized;
    bool recording;
    bool take_too_short;
} MicrophoneInput;

bool microphone_input_init(MicrophoneInput *input);
bool microphone_input_start(MicrophoneInput *input);
void microphone_input_update(MicrophoneInput *input);
bool microphone_input_finish(MicrophoneInput *input);
bool microphone_input_available(const MicrophoneInput *input);
bool microphone_input_recording(const MicrophoneInput *input);
bool microphone_input_full(const MicrophoneInput *input);
bool microphone_input_take_too_short(const MicrophoneInput *input);
uint32_t microphone_input_sample_count(const MicrophoneInput *input);
const int16_t *microphone_input_samples(const MicrophoneInput *input);
int32_t microphone_input_result(const MicrophoneInput *input);
void microphone_input_exit(MicrophoneInput *input);

#endif
