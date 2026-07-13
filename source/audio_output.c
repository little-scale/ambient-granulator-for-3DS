#include "audio_output.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int16_t *buffer_samples(AudioOutput *output, int index)
{
    return output->samples + index * AUDIO_BUFFER_FRAMES * 2;
}

static void submit_buffer(AudioOutput *output, int index,
                          const AudioRenderConfig *config)
{
    int16_t *samples = buffer_samples(output, index);
    if (config->effects.freeze && config->effects.wet_percent >= 100)
        granular_engine_render_silent_wide(&output->engine,
                                           output->mix_samples,
                                           AUDIO_BUFFER_FRAMES,
                                           &config->granular);
    else
        granular_engine_render_wide(&output->engine, output->mix_samples,
                                    AUDIO_BUFFER_FRAMES,
                                    &config->granular);
    effects_chain_process_wide(&output->effects, output->mix_samples,
                               samples, AUDIO_BUFFER_FRAMES,
                               &config->effects);
    output_meter_process(&output->meter, samples, AUDIO_BUFFER_FRAMES);
    if (effects_chain_overloaded(&output->effects))
        output_meter_mark_clipped(&output->meter);
    DSP_FlushDataCache(samples,
        AUDIO_BUFFER_FRAMES * 2 * sizeof(*samples));
    ndspChnWaveBufAdd(0, &output->wave_buffers[index]);
    output->buffers_submitted++;
}

bool audio_output_init(AudioOutput *output,
                       const AudioRenderConfig *initial_config,
                       const int16_t *sample, uint32_t sample_count,
                       uint32_t sample_rate)
{
    memset(output, 0, sizeof(*output));
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

    output->ready = true;
    return true;
}

void audio_output_update(AudioOutput *output,
                         const AudioRenderConfig *config)
{
    if (!output->ready)
        return;

    int finished = 0;
    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++)
        finished += output->wave_buffers[i].status == NDSP_WBUF_DONE;
    if (finished == AUDIO_BUFFER_COUNT)
        output->underruns++;

    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
        if (output->wave_buffers[i].status == NDSP_WBUF_DONE)
            submit_buffer(output, i, config);
    }
}

void audio_output_set_sample(AudioOutput *output, const int16_t *sample,
                             uint32_t sample_count, uint32_t sample_rate)
{
    granular_engine_set_sample(&output->engine, sample, sample_count,
                               sample_rate);
}

void audio_output_trigger(AudioOutput *output, int center_x,
                          int grain_count)
{
    granular_engine_trigger(&output->engine, center_x, grain_count);
}

bool audio_output_pop_marker(AudioOutput *output, GranularMarker *marker)
{
    return granular_engine_pop_marker(&output->engine, marker);
}

int audio_output_active_voices(const AudioOutput *output)
{
    return granular_engine_active_voices(&output->engine);
}

uint32_t audio_output_grains_launched(const AudioOutput *output)
{
    return output->engine.grains_launched;
}

uint16_t audio_output_peak_left(const AudioOutput *output)
{
    return output->meter.peak_left;
}

uint16_t audio_output_peak_right(const AudioOutput *output)
{
    return output->meter.peak_right;
}

bool audio_output_clipping(const AudioOutput *output)
{
    return output_meter_clipping(&output->meter);
}

uint32_t audio_output_underruns(const AudioOutput *output)
{
    return output->underruns;
}

void audio_output_exit(AudioOutput *output)
{
    if (!output->ready)
        return;

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
