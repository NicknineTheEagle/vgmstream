#include "mixing_priv.h"


// TO-DO: some ops can be done with original PCM sbuf to avoid copying to the float sbuf
// when there are no actual float ops (ex. 'swap', if no ' volume' )
// Performance gain is probably fairly small, though.

void mixer_op_swap(mixer_data_t* data, int32_t sample_count, mix_op_t* op) {
    float* sbuf = data->mixbuf;

    for (int s = 0; s < sample_count; s++) {
        float temp_f = sbuf[op->ch_dst];
        sbuf[op->ch_dst] = sbuf[op->ch_src];
        sbuf[op->ch_src] = temp_f;

        sbuf += data->current_channels;
    }
}

void mixer_op_add(mixer_data_t* data, int32_t sample_count, mix_op_t* op) {
    float* sbuf = data->mixbuf;

    /* could optimize when vol == 1 to avoid one multiplication but whatevs (not common) */
    for (int s = 0; s < sample_count; s++) {
        sbuf[op->ch_dst] = sbuf[op->ch_dst] + sbuf[op->ch_src] * op->vol;

        sbuf += data->current_channels;
    }
}

void mixer_op_volume(mixer_data_t* data, int32_t sample_count, mix_op_t* op) {
    float* sbuf = data->mixbuf;
    
    if (op->ch_dst < 0) {
        /* "all channels", most common case */
        for (int s = 0; s < sample_count * data->current_channels; s++) {
            sbuf[s] = sbuf[s] * op->vol;
        }
    }
    else {
        for (int s = 0; s < sample_count; s++) {
            sbuf[op->ch_dst] = sbuf[op->ch_dst] * op->vol;

            sbuf += data->current_channels;
        }
    }
}

void mixer_op_limit(mixer_data_t* data, int32_t sample_count, mix_op_t* op) {
    float* sbuf = data->mixbuf;

    const float limiter_max = 32767.0f;
    const float limiter_min = -32768.0f;

    const float temp_max = limiter_max * op->vol;
    const float temp_min = limiter_min * op->vol;

    /* could optimize when vol == 1 to avoid one multiplication but whatevs (not common) */
    for (int s = 0; s < sample_count; s++) {

        if (op->ch_dst < 0) {
            for (int ch = 0; ch < data->current_channels; ch++) {
                if (sbuf[ch] > temp_max)
                    sbuf[ch] = temp_max;
                else if (sbuf[ch] < temp_min)
                    sbuf[ch] = temp_min;
            }
        }
        else {
            if (sbuf[op->ch_dst] > temp_max)
                sbuf[op->ch_dst] = temp_max;
            else if (sbuf[op->ch_dst] < temp_min)
                sbuf[op->ch_dst] = temp_min;
        }

        sbuf += data->current_channels;
    }
}

void mixer_op_upmix(mixer_data_t* data, int32_t sample_count, mix_op_t* op) {
    int max_channels = data->current_channels;
    data->current_channels += 1;

    float* sbuf_tmp = data->mixbuf + sample_count * data->current_channels;
    float* sbuf = data->mixbuf + sample_count * max_channels;

    /* copy 'backwards' as otherwise would overwrite samples before moving them forward */
    for (int s = 0; s < sample_count; s++) {
        sbuf_tmp -= data->current_channels;
        sbuf -= max_channels;

        int sbuf_ch = max_channels - 1;
        for (int ch = data->current_channels - 1; ch >= 0; ch--) {
            if (ch == op->ch_dst) {
                sbuf_tmp[ch] = 0; /* inserted as silent */
            }
            else {
                sbuf_tmp[ch] = sbuf[sbuf_ch]; /* 'pull' channels backward */
                sbuf_ch--;
            }
        }
    }
}

void mixer_op_downmix(mixer_data_t* data, int32_t sample_count, mix_op_t* op) {
    int max_channels = data->current_channels;
    data->current_channels -= 1;

    float* sbuf = data->mixbuf;
    float* sbuf_tmp = sbuf;

    for (int s = 0; s < sample_count; s++) {

        for (int ch = 0; ch < op->ch_dst; ch++) {
            sbuf_tmp[ch] = sbuf[ch]; /* copy untouched channels */
        }

        for (int ch = op->ch_dst; ch < max_channels; ch++) {
            sbuf_tmp[ch] = sbuf[ch + 1]; /* 'pull' dropped channels back */
        }

        sbuf_tmp += data->current_channels;
        sbuf += max_channels;
    }
}

void mixer_op_killmix(mixer_data_t* data, int32_t sample_count, mix_op_t* op) {
    int max_channels = data->current_channels;
    data->current_channels = op->ch_dst; /* clamp channels */

    float* sbuf = data->mixbuf;
    float* sbuf_tmp = sbuf;

    for (int s = 0; s < sample_count; s++) {
        for (int ch = 0; ch < data->current_channels; ch++) {
            sbuf_tmp[ch] = sbuf[ch];
        }

        sbuf_tmp += data->current_channels;
        sbuf += max_channels;
    }
}
