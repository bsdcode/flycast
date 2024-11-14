#ifndef USE_OSS
#include "audiostream.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>

class OSSAudioBackend : public AudioBackend
{
	int audioFD = -1;
	int recordFD = -1;

	static bool openDevice(const char* path, int flags)
	{

	}

public:
	OSSAudioBackend()
		: AudioBackend("oss", "Open Sound System") {}

	bool init() override
	{
		audioFD = open("/dev/dsp", O_WRONLY);
		if (audioFD < 0)
		{
			ERROR_LOG(AUDIO, "OSS: open(/dev/dsp) failed: %s", strerror(errno));
			term();
			return false;
		}

		const int rate = 44100;
		int tmp = rate;
		if (ioctl(audioFD, SNDCTL_DSP_SPEED, &tmp) < 0)
		{
			ERROR_LOG(AUDIO, "OSS: ioctl(SNDCTL_DSP_SPEED) failed: %s", strerror(errno));
			term();
			return false;
		}
		if (tmp != rate)
		{
			ERROR_LOG(AUDIO, "OSS: sample rate unsupported: %d => %d", rate, tmp);
			term();
			return false;
		}

		const int channels = 2;
		tmp = channels;
		if (ioctl(audioFD, SNDCTL_DSP_CHANNELS, &tmp) < 0)
		{
			ERROR_LOG(AUDIO, "OSS: ioctl(SNDCTL_DSP_CHANNELS) failed: %s", strerror(errno));
			term();
			return false;
		}
		if (tmp != channels)
		{
			ERROR_LOG(AUDIO, "OSS: channels unsupported: %d => %d", channels, tmp);
			term();
			return false;
		}

		const int format = AFMT_S16_LE;
		tmp = format;
		if (ioctl(audioFD, SNDCTL_DSP_SETFMT, &tmp) < 0)
		{
			ERROR_LOG(AUDIO, "OSS: ioctl(SNDCTL_DSP_SETFMT) failed: %s", strerror(errno));
			term();
			return false;
		}
		if (tmp != format)
		{
			ERROR_LOG(AUDIO, "OSS: sample format unsupported: s16le => %#.8x", tmp);
			term();
			return false;
		}

		return true;
	}

	u32 push(const void* frame, u32 samples, bool wait) override
	{
		write(audioFD, frame, samples * 4);
		return 1;
	}

	void term() override
	{
		if (audioFD >= 0)
			close(audioFD);
		audioFD = -1;
	}

	// recording untested

	bool initRecord(u32 sampling_freq) override
	{
		recordFD = open("/dev/dsp", O_RDONLY);
		if (recordFD < 0)
		{
			ERROR_LOG(AUDIO, "OSS: open(/dev/dsp) failed: %s", strerror(errno));
			return false;
		}

		tmp = sampling_freq;
		if (ioctl(recordFD, SNDCTL_DSP_SPEED, &tmp) == -1)
		{
			INFO_LOG(AUDIO, "OSS: can't set sample rate");
			close(recordFD);
			recordFD = -1;
			return false;
		}

		int tmp = AFMT_S16_NE;	// Native 16 bits
		if (ioctl(recordFD, SNDCTL_DSP_SETFMT, &tmp) == -1 || tmp != AFMT_S16_NE)
		{
			ERROR_LOG(AUDIO, "OSS: can't set sample format: %s", strerror(errno));
			close(recordFD);
			recordFD = -1;
			return false;
		}
		tmp = 1;
		if (ioctl(recordFD, SNDCTL_DSP_CHANNELS, &tmp) == -1)
		{
			INFO_LOG(AUDIO, "OSS: can't set channel count");
			close(recordFD);
			recordFD = -1;
			return false;
		}

		return true;
	}

	void termRecord() override
	{
		if (recordFD >= 0)
			close(recordFD);
		recordFD = -1;
	}

	u32 record(void *buffer, u32 samples) override
	{
		samples *= 2;
		int l = read(recordFD, buffer, samples);
		if (l < (int)samples)
		{
			if (l < 0)
			{
				INFO_LOG(AUDIO, "OSS: Recording error");
				l = 0;
			}
			memset((u8 *)buffer + l, 0, samples - l);
		}
		return l / 2;
	}
};
static OSSAudioBackend ossAudioBackend;

#endif
