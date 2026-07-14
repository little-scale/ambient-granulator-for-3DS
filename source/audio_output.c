#include "audio_output.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_THREAD_STACK_SIZE (32 * 1024)

static int16_t *buffer_samples(AudioOutput *output, int index)
{
    return output->samples + index * AUDIO_BUFFER_FRAMES * 2;
}

static void submit_buffer(AudioOutput *output, int index,
                          const AudioRenderConfig *config)
{
    int16_t *samples = buffer_samples(output, index);
    int active_before = granular_engine_active_voices(&output->engine);
    if (config->effects.freeze && config->effects.wet_percent >= 100)
        granular_engine_render_silent_wide(&output->engine,
                                           output->mix_samples,
                                           AUDIO_BUFFER_FRAMES,
                                           &config->granular);
    else
        granular_engine_render_wide(&output->engine, output->mix_samples,
                                    AUDIO_BUFFER_FRAMES,
                                    &config->granular);
    bool any_grain_output = false;
    for (int frame = 0; frame < AUDIO_BUFFER_FRAMES; frame++)
        any_grain_output |= output->mix_samples[frame * 2] != 0
                         || output->mix_samples[frame * 2 + 1] != 0;
    int active_after = granular_engine_active_voices(&output->engine);
    bool intentionally_silent = config->effects.freeze
                             && config->effects.wet_percent >= 100;
    if (!any_grain_output && !intentionally_silent
            && (active_before > 0 || active_after > 0))
        __atomic_add_fetch(&output->silent_blocks, 1, __ATOMIC_RELEASE);
    effects_chain_process_wide(&output->effects, output->mix_samples,
                               samples, AUDIO_BUFFER_FRAMES,
                               &config->effects);
    output_meter_process(&output->meter, samples, AUDIO_BUFFER_FRAMES);
    if (effects_chain_overloaded(&output->effects))
        output_meter_mark_clipped(&output->meter);
    DSP_FlushDataCache(samples,
        AUDIO_BUFFER_FRAMES * 2 * sizeof(*samples));
    ndspChnWaveBufAdd(0, &output->wave_buffers[index]);
    __atomic_add_fetch(&output->buffers_submitted, 1, __ATOMIC_RELEASE);
}

static void publish_status(AudioOutput *output)
{
    __atomic_store_n(&output->active_voices_snapshot,
        (uint32_t)granular_engine_active_voices(&output->engine),
        __ATOMIC_RELEASE);
    __atomic_store_n(&output->grains_launched_snapshot,
        output->engine.grains_launched, __ATOMIC_RELEASE);
    __atomic_store_n(&output->peak_left_snapshot,
        output->meter.peak_left, __ATOMIC_RELEASE);
    __atomic_store_n(&output->peak_right_snapshot,
        output->meter.peak_right, __ATOMIC_RELEASE);
    __atomic_store_n(&output->clipping_snapshot,
        output_meter_clipping(&output->meter), __ATOMIC_RELEASE);
}

static void service_buffers(AudioOutput *output)
{
    int finished = 0;
    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++)
        finished += output->wave_buffers[i].status == NDSP_WBUF_DONE;
    if (finished == AUDIO_BUFFER_COUNT)
        __atomic_add_fetch(&output->underruns, 1, __ATOMIC_RELEASE);
    else if (finished >= AUDIO_BUFFER_COUNT - 1)
        __atomic_add_fetch(&output->late_refills, 1, __ATOMIC_RELEASE);

    LightLock_Lock(&output->config_lock);
    AudioRenderConfig config = output->config;
    LightLock_Unlock(&output->config_lock);

    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
        if (output->wave_buffers[i].status != NDSP_WBUF_DONE)
            continue;
        /* Release between blocks so control input can never sit behind a
           multi-buffer catch-up render. */
        LightLock_Lock(&output->state_lock);
        submit_buffer(output, i, &config);
        publish_status(output);
        LightLock_Unlock(&output->state_lock);
    }
}

static void audio_frame_callback(void *data)
{
    AudioOutput *output = data;
    if (!__atomic_load_n(&output->quit, __ATOMIC_ACQUIRE))
        LightEvent_Signal(&output->event);
}

static void audio_thread_main(void *data)
{
    AudioOutput *output = data;
    while (!__atomic_load_n(&output->quit, __ATOMIC_ACQUIRE)) {
        service_buffers(output);
        if (!__atomic_load_n(&output->quit, __ATOMIC_ACQUIRE))
            LightEvent_Wait(&output->event);
    }
}

bool audio_output_init(AudioOutput *output,
                       const AudioRenderConfig *initial_config,
                       const int16_t *sample, uint32_t sample_count,
                       uint32_t sample_rate)
{
    memset(output, 0, sizeof(*output));
    LightLock_Init(&output->config_lock);
    LightLock_Init(&output->state_lock);
    LightEvent_Init(&output->event, RESET_ONESHOT);
    output->config = *initial_config;
    granular_engine_init(&output->engine);
    output_meter_init(&output->meter);
    granular_engine_set_sample(&output->engine, sample, sample_count,
                               sample_rate);

    output->init_result = ndspInit();
    if (R_FAILED(output->init_result))
        return false;

    if (!effects_chain_init(&output->effects)) {
        ndspExit();
        output->init_result = -2;
        return false;
    }

    output->samples = linearAlloc(AUDIO_BUFFER_COUNT * AUDIO_BUFFER_FRAMES
                                  * 2 * sizeof(*output->samples));
    output->mix_samples = malloc(AUDIO_BUFFER_FRAMES * 2
                               * sizeof(*output->mix_samples));
    if (output->samples == NULL || output->mix_samples == NULL) {
        linearFree(output->samples);
        free(output->mix_samples);
        output->samples = NULL;
        output->mix_samples = NULL;
        effects_chain_exit(&output->effects);
        ndspExit();
        output->init_result = -1;
        return false;
    }

    memset(output->samples, 0, AUDIO_BUFFER_COUNT * AUDIO_BUFFER_FRAMES
                               * 2 * sizeof(*output->samples));
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, GRANULAR_OUTPUT_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12] = { 0.0f };
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(0, mix);

    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
        ndspWaveBuf *wave = &output->wave_buffers[i];
        memset(wave, 0, sizeof(*wave));
        wave->data_pcm16 = buffer_samples(output, i);
        wave->nsamples = AUDIO_BUFFER_FRAMES;
        submit_buffer(output, i, initial_config);
    }
    publish_status(output);

    output->ready = true;
    ndspSetCallback(audio_frame_callback, output);
    int32_t priority = 0x30;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    if (priority < 0x18)
        priority = 0x18;
    if (priority > 0x3F)
        priority = 0x3F;
    output->thread = threadCreate(audio_thread_main, output,
                                  AUDIO_THREAD_STACK_SIZE,
                                  priority, -1, false);
    if (output->thread == NULL) {
        ndspSetCallback(NULL, NULL);
        output->ready = false;
        ndspChnWaveBufClear(0);
        ndspChnReset(0);
        linearFree(output->samples);
        free(output->mix_samples);
        output->samples = NULL;
        output->mix_samples = NULL;
        effects_chain_exit(&output->effects);
        ndspExit();
        output->init_result = -3;
        return false;
    }
    return true;
}

void audio_output_update(AudioOutput *output,
                         const AudioRenderConfig *config)
{
    if (!output->ready)
        return;
    LightLock_Lock(&output->config_lock);
    output->config = *config;
    LightLock_Unlock(&output->config_lock);
}

void audio_output_set_sample(AudioOutput *output, const int16_t *sample,
                             uint32_t sample_count, uint32_t sample_rate)
{
    if (!output->ready)
        return;
    LightLock_Lock(&output->state_lock);
    granular_engine_set_sample(&output->engine, sample, sample_count,
                               sample_rate);
    publish_status(output);
    LightLock_Unlock(&output->state_lock);
}

void audio_output_stop_grains(AudioOutput *output)
{
    if (!output->ready)
        return;
    LightLock_Lock(&output->state_lock);
    granular_engine_stop(&output->engine);
    publish_status(output);
    LightLock_Unlock(&output->state_lock);
}

void audio_output_trigger(AudioOutput *output, int center_x,
                          int grain_count)
{
    if (!output->ready)
        return;
    LightLock_Lock(&output->state_lock);
    granular_engine_trigger(&output->engine, center_x, grain_count);
    LightLock_Unlock(&output->state_lock);
}

bool audio_output_pop_marker(AudioOutput *output, GranularMarker *marker)
{
    if (!output->ready)
        return false;
    if (LightLock_TryLock(&output->state_lock) != 0)
        return false;
    bool available = granular_engine_pop_marker(&output->engine, marker);
    LightLock_Unlock(&output->state_lock);
    return available;
}

int audio_output_active_voices(const AudioOutput *output)
{
    if (!output->ready)
        return 0;
    return (int)__atomic_load_n(&output->active_voices_snapshot,
                                __ATOMIC_ACQUIRE);
}

uint32_t audio_output_grains_launched(const AudioOutput *output)
{
    if (!output->ready)
        return 0;
    return __atomic_load_n(&output->grains_launched_snapshot,
                           __ATOMIC_ACQUIRE);
}

uint16_t audio_output_peak_left(const AudioOutput *output)
{
    if (!output->ready)
        return 0;
    return (uint16_t)__atomic_load_n(&output->peak_left_snapshot,
                                     __ATOMIC_ACQUIRE);
}

uint16_t audio_output_peak_right(const AudioOutput *output)
{
    if (!output->ready)
        return 0;
    return (uint16_t)__atomic_load_n(&output->peak_right_snapshot,
                                     __ATOMIC_ACQUIRE);
}

bool audio_output_clipping(const AudioOutput *output)
{
    if (!output->ready)
        return false;
    return __atomic_load_n(&output->clipping_snapshot,
                           __ATOMIC_ACQUIRE) != 0;
}

uint32_t audio_output_buffers_submitted(const AudioOutput *output)
{
    if (!output->ready)
        return 0;
    return __atomic_load_n(&output->buffers_submitted, __ATOMIC_ACQUIRE);
}

uint32_t audio_output_underruns(const AudioOutput *output)
{
    if (!output->ready)
        return 0;
    return __atomic_load_n(&output->underruns, __ATOMIC_ACQUIRE);
}

uint32_t audio_output_late_refills(const AudioOutput *output)
{
    if (!output->ready)
        return 0;
    return __atomic_load_n(&output->late_refills, __ATOMIC_ACQUIRE);
}

uint32_t audio_output_silent_blocks(const AudioOutput *output)
{
    if (!output->ready)
        return 0;
    return __atomic_load_n(&output->silent_blocks, __ATOMIC_ACQUIRE);
}

void audio_output_exit(AudioOutput *output)
{
    if (!output->ready)
        return;

    ndspSetCallback(NULL, NULL);
    __atomic_store_n(&output->quit, true, __ATOMIC_RELEASE);
    LightEvent_Signal(&output->event);
    threadJoin(output->thread, UINT64_MAX);
    threadFree(output->thread);
    output->thread = NULL;
    ndspChnWaveBufClear(0);
    ndspChnReset(0);
    ndspExit();
    effects_chain_exit(&output->effects);
    linearFree(output->samples);
    free(output->mix_samples);
    output->samples = NULL;
    output->mix_samples = NULL;
    output->ready = false;
}
