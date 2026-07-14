#include "microphone_input.h"

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#define MICROPHONE_SHARED_BUFFER_SIZE UINT32_C(0x10000)
#define MICROPHONE_SHARED_ALIGNMENT UINT32_C(0x1000)

static void sample_shared_buffer(MicrophoneInput *input)
{
    uint32_t write_offset = micGetLastSampleOffset();
    uint16_t peak = recording_buffer_ring_peak(
        input->shared_buffer, input->shared_data_size,
        &input->monitor_read_offset, write_offset);
    if (peak > input->input_peak)
        input->input_peak = peak;
    if (input->recording)
        recording_buffer_drain(
            &input->capture, input->shared_buffer,
            input->shared_data_size, write_offset);
}

bool microphone_input_init(MicrophoneInput *input)
{
    memset(input, 0, sizeof(*input));
    input->shared_buffer_size = MICROPHONE_SHARED_BUFFER_SIZE;
    input->shared_buffer = memalign(MICROPHONE_SHARED_ALIGNMENT,
                                    input->shared_buffer_size);
    input->capture_samples = malloc(
        (size_t)MICROPHONE_INPUT_MAX_SAMPLES
        * sizeof(*input->capture_samples));
    if (input->shared_buffer == NULL || input->capture_samples == NULL) {
        input->init_result = -1;
        free(input->shared_buffer);
        free(input->capture_samples);
        input->shared_buffer = NULL;
        input->capture_samples = NULL;
        return false;
    }

    memset(input->shared_buffer, 0, input->shared_buffer_size);
    Result result = micInit(input->shared_buffer, input->shared_buffer_size);
    input->init_result = (int32_t)result;
    if (R_FAILED(result)) {
        free(input->shared_buffer);
        free(input->capture_samples);
        input->shared_buffer = NULL;
        input->capture_samples = NULL;
        return false;
    }

    input->shared_data_size = micGetSampleDataSize() & ~UINT32_C(1);
    if (input->shared_data_size < sizeof(int16_t)) {
        input->init_result = -2;
        micExit();
        free(input->shared_buffer);
        free(input->capture_samples);
        input->shared_buffer = NULL;
        input->capture_samples = NULL;
        return false;
    }
    Result gain_result = MICU_SetGain(MICROPHONE_INPUT_DEFAULT_GAIN);
    if (R_FAILED(gain_result)) {
        input->init_result = (int32_t)gain_result;
        micExit();
        free(input->shared_buffer);
        free(input->capture_samples);
        input->shared_buffer = NULL;
        input->capture_samples = NULL;
        return false;
    }
    Result clamp_result = MICU_SetClamp(true);
    if (R_FAILED(clamp_result)) {
        input->init_result = (int32_t)clamp_result;
        micExit();
        free(input->shared_buffer);
        free(input->capture_samples);
        input->shared_buffer = NULL;
        input->capture_samples = NULL;
        return false;
    }
    Result sampling_result = MICU_StartSampling(
        MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_32730,
        0, input->shared_data_size, true);
    input->last_result = (int32_t)sampling_result;
    if (R_FAILED(sampling_result)) {
        input->init_result = (int32_t)sampling_result;
        micExit();
        free(input->shared_buffer);
        free(input->capture_samples);
        input->shared_buffer = NULL;
        input->capture_samples = NULL;
        return false;
    }
    input->monitor_read_offset = micGetLastSampleOffset() & ~UINT32_C(1);
    input->initialized = true;
    return true;
}

bool microphone_input_start(MicrophoneInput *input)
{
    if (input == NULL || !input->initialized || input->recording)
        return false;
    recording_buffer_begin(&input->capture, input->capture_samples,
                           MICROPHONE_INPUT_MAX_SAMPLES);
    input->capture.ring_read_offset
        = micGetLastSampleOffset() & ~UINT32_C(1);
    input->take_too_short = false;
    input->recording = true;
    return true;
}

void microphone_input_update(MicrophoneInput *input)
{
    if (input == NULL)
        return;
    input->input_peak = (uint16_t)(input->input_peak * 15U / 16U);
    if (input->input_peak < 8)
        input->input_peak = 0;
    if (input->initialized)
        sample_shared_buffer(input);
}

bool microphone_input_finish(MicrophoneInput *input)
{
    if (input == NULL || !input->recording)
        return false;
    sample_shared_buffer(input);
    input->recording = false;
    if (microphone_input_sample_count(input) < MICROPHONE_INPUT_MIN_SAMPLES) {
        input->take_too_short = true;
        return false;
    }
    return true;
}

bool microphone_input_set_gain(MicrophoneInput *input, uint8_t gain)
{
    if (input == NULL || !input->initialized
            || gain > MICROPHONE_INPUT_MAX_GAIN)
        return false;
    Result result = MICU_SetGain(gain);
    input->last_result = (int32_t)result;
    return R_SUCCEEDED(result);
}

bool microphone_input_available(const MicrophoneInput *input)
{
    return input != NULL && input->initialized;
}

bool microphone_input_recording(const MicrophoneInput *input)
{
    return input != NULL && input->recording;
}

bool microphone_input_full(const MicrophoneInput *input)
{
    return input != NULL && input->capture.full;
}

bool microphone_input_take_too_short(const MicrophoneInput *input)
{
    return input != NULL && input->take_too_short;
}

uint32_t microphone_input_sample_count(const MicrophoneInput *input)
{
    size_t count = input == NULL ? 0
        : recording_buffer_sample_count(&input->capture);
    return (uint32_t)count;
}

const int16_t *microphone_input_samples(const MicrophoneInput *input)
{
    return input == NULL ? NULL : input->capture_samples;
}

uint16_t microphone_input_peak(const MicrophoneInput *input)
{
    return input == NULL ? 0 : input->input_peak;
}

int32_t microphone_input_result(const MicrophoneInput *input)
{
    if (input == NULL)
        return -1;
    return input->initialized ? input->last_result : input->init_result;
}

void microphone_input_exit(MicrophoneInput *input)
{
    if (input == NULL)
        return;
    if (input->initialized) {
        MICU_StopSampling();
        micExit();
    }
    free(input->shared_buffer);
    free(input->capture_samples);
    memset(input, 0, sizeof(*input));
}
