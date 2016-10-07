#ifndef _ADTS_UTIL_H_
#define _ADTS_UTIL_H_

#if defined(__cplusplus)
extern "C"
{
#endif

#define ADTS_HEADER_SIZE	7

typedef struct ADTSContext{
	int write_adts;
	int objecttype;
	int sample_rate_index;
	int channel_conf;
} ADTSContext;

extern int aac_decode_extradata(ADTSContext *adts, unsigned char *pbuf, int bufsize);

extern int aac_set_adts_head(ADTSContext *acfg, unsigned char *buf, int frame_length);

#if defined(__cplusplus)
}
#endif

#endif