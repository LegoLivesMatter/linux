/*
 * linux/sound/soc/pxa/mmp-map-card.c
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/mfd/mmp-map.h>
#include "mmp-tdm.h"

/* I2S1/I2S4/I2S3 use 44.1k sample rate by default */
#define MAP_SR_HIFI SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000
#define MAP_SR_FM SNDRV_PCM_RATE_48000
/* I2S2/I2S3 use 8K or 16K sample rate */
#define MAP_SR_LOFI (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000)

/* SSPA and sysclk pll sources */
#define SSPA_AUDIO_PLL                          0
#define SSPA_I2S_PLL                            1
#define SSPA_VCXO_PLL                           2
#define AUDIO_PLL                               3

static int map_startup_hifi(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);

	cpu_dai->driver->playback.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->capture.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->playback.rates = MAP_SR_HIFI;
	cpu_dai->driver->capture.rates = MAP_SR_HIFI;

	return 0;
}

static int map_fe_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = snd_soc_rtd_to_codec(rtd, 0);
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	u32 freq_in, freq_out, sspa_mclk, sysclk, sspa_div;
	u32 srate = params_rate(params);

	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

	/* For i2s2(voice call) and i2s3(bt-audio), the dai format is pcm */
	if (codec_dai->id == 2) {
		snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
		snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	} else if (codec_dai->id == 5) {
		snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
			    SND_SOC_DAIFMT_CBM_CFM);
		snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
			    SND_SOC_DAIFMT_CBM_CFM);
	} else {
		snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_CBM_CFM);
		snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_CBM_CFM);
	}

	/* SSPA clock ctrl register changes, and can't use previous API */
	snd_soc_dai_set_sysclk(cpu_dai, AUDIO_PLL, freq_out, 0);

	/* set i2s1/2/3/4 sysclk */
	snd_soc_dai_set_sysclk(codec_dai, APLL_32K, srate, 0);

	return 0;
}

static int map_tdm_spkr_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = snd_soc_rtd_to_codec(rtd, 0);
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int freq_in, freq_out, sspa_mclk, sysclk, sspa_div;
	int channel;
	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);

	snd_soc_dai_set_sysclk(cpu_dai, APLL_32K, freq_out, 0);

	/*allocate slot*/
	channel = params_channels(params);
	mmp_tdm_request_slot(substream, channel);
	return 0;
}

void map_tdm_spkr_shutdown(struct snd_pcm_substream *substream)
{

	mmp_tdm_free_slot(substream);
}

void map_tdm_hs_shutdown(struct snd_pcm_substream *substream)
{
#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int tx[2] = {0, 0};
	int tx_num = 2;
#endif

#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	snd_soc_dai_set_channel_map(codec_dai, 0, NULL, tx_num, tx);
	snd_soc_dai_set_channel_map(cpu_dai, tx_num, tx, 0, NULL);
	mmp_tdm_static_slot_free(substream);
#else
	mmp_tdm_free_slot(substream);
#endif
}

static int map_tdm_hs_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = snd_soc_rtd_to_codec(rtd, 0);
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int freq_in, freq_out, sspa_mclk, sysclk, sspa_div;
#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	int tx[2] = {3, 4};
	int tx_num = 2;
#else
	int channel;
#endif
	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);

	snd_soc_dai_set_sysclk(cpu_dai, APLL_32K, freq_out, 0);

#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	mmp_tdm_static_slot_alloc(substream, tx, tx_num, NULL, 0);
	snd_soc_dai_set_channel_map(cpu_dai, tx_num, tx, 0, NULL);
	snd_soc_dai_set_channel_map(codec_dai, 0, NULL, tx_num, tx);
#else
	/* allocate slot */
	channel = params_channels(params);
	mmp_tdm_request_slot(substream, channel);
#endif
	return 0;
}

static struct snd_soc_ops map_hifi_ops = {
	.startup = map_startup_hifi,
	.hw_params = map_fe_hw_params,
};

static struct snd_soc_ops map_spkr_ops = {
	.startup = map_startup_hifi,
	.hw_params = map_tdm_spkr_hw_params,
	.shutdown = map_tdm_spkr_shutdown,
};

static struct snd_soc_ops map_hs_ops = {
	.startup = map_startup_hifi,
	.hw_params = map_tdm_hs_hw_params,
	.shutdown = map_tdm_hs_shutdown,
};

static struct snd_soc_dai_link_component sspa_dlc = {
	.name = "d128dc00.audio-controller",
};

static struct snd_soc_dai_link_component codec_speaker_dlc = {
	.name = "88pm860-codec",
	.dai_name = "88pm860-tdm-out2",
};

static struct snd_soc_dai_link_component codec_hs_dlc = {
	.name = "88pm860-codec",
	.dai_name = "88pm860-tdm-out1",
};

static struct snd_soc_dai_link_component map_speaker_dlc = {
	.name = "mmp-map-be-tdm",
	.dai_name = "tdm-out2",
};

static struct snd_soc_dai_link_component map_hs_dlc = {
	.name = "mmp-map-be-tdm",
	.dai_name = "tdm-out1",
};

static struct snd_soc_dai_link_component map_codec_dlc = {
	.name = "mmp-map-codec",
	.dai_name = "map-i2s4-dai",
};

static struct snd_soc_dai_link map_dai_links[] = {
	{
		.name = "PCM0 SSPA",
		.stream_name = "System Playback",
		.cpus = &sspa_dlc,
		.num_cpus = 1,
		.platforms = &sspa_dlc,
		.num_platforms = 1,
		.codecs = &map_codec_dlc,
		.num_codecs = 1,
		.dynamic = 1,
		.dpcm_merged_format = 1,
		.dpcm_merged_chan = 1,
		.dpcm_merged_rate = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &map_hifi_ops,
	},
	{
		.name = "Codec Speaker",
		.cpus = &map_speaker_dlc,
		.num_cpus = 1,
		.codecs = &codec_speaker_dlc,
		.num_codecs = 1,
		.platforms = &snd_soc_dummy_dlc,
		.num_platforms = 1,
		.ops = &map_spkr_ops,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		.name = "Codec Headset",
		.cpus = &map_hs_dlc,
		.num_cpus = 1,
		.codecs = &codec_hs_dlc,
		.num_codecs = 1,
		.platforms = &snd_soc_dummy_dlc,
		.num_platforms = 1,
		.ops = &map_hs_ops,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
};

static const struct snd_soc_dapm_route map_routes[] = {
	{"MM_DL2", NULL, "sspa-playback"},
	{"TDM_OUT2_PLAYBACK", NULL, "out1_spkr_en"},
	{"TDM_OUT1_PLAYBACK", NULL, "out1_hs_en"},
};

/* audio machine driver */
static struct snd_soc_card snd_soc_map = {
	.name = "map asoc",
	.dai_link = map_dai_links,
	.num_links = ARRAY_SIZE(map_dai_links),
	.dapm_routes = map_routes,
	.num_dapm_routes = ARRAY_SIZE(map_routes),
};

static struct of_device_id mmp_map_dt_ids[] = {
	{.compatible = "marvell,map-card",},
	{}
};
MODULE_DEVICE_TABLE(of, mmp_map_dt_ids);

static int map_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_map;

	card->dev = &pdev->dev;

	return devm_snd_soc_register_card(&pdev->dev, card);
}

static struct platform_driver map_audio_driver = {
	.driver		= {
		.name	= "marvell-map-audio",
		.of_match_table = mmp_map_dt_ids,
	},
	.probe		= map_audio_probe,
};

module_platform_driver(map_audio_driver);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC Audio MAP");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:audio-map");
