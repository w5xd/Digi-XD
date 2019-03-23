#include "WaveFilePlayer.h"
#include <AudioSink.h>
#include <vector>
#include <Windows.h>
#include <mmsystem.h>
#include <memory>
namespace XD { namespace impl {
	// Class to manage mmio file
	class MmioFile {
	public:
		MmioFile(const std::wstring &wavFileName)
			: m_MmFile(mmioOpenW(const_cast<wchar_t *>(wavFileName.c_str()), 0, MMIO_READ))
			, m_pWf(0)
			, m_pos(0)
		{
			PositionToFirstDataChunk();
		}

		~MmioFile()
		{
			close();
		}

		void close()
		{
			if (m_MmFile)
				mmioClose(m_MmFile, 0);
			m_MmFile = 0;
		}

		bool is_open() const { return m_MmFile != 0; }

		unsigned Read(char *pBuf, unsigned numBytes)
		{
			if (!is_open())
				return 0;
			long maxToRead = m_infoData.cksize;
			maxToRead -= m_pos;
			if (static_cast<long>(numBytes) > maxToRead)
				numBytes = maxToRead;
			long r = 0;
			if (numBytes > 0)
				r = mmioRead(m_MmFile, pBuf, numBytes);
			m_pos += r;
			return r;
		};

		const WAVEFORMATEX *GetFormat() const { return m_pWf; }

		unsigned BytesRemaining() const
		{
			if (m_infoData.cksize == 0)
				return 0;
			return m_infoData.cksize - m_pos;
		}
		unsigned SamplesRemaining() const
		{
			return BytesRemaining() / (m_pWf->nBlockAlign);
		}
	protected:
		bool PositionToFirstDataChunk()
		{
			m_pWf = 0;
			if (!is_open())
				return false;
			MMCKINFO		infoSubchunk;
			memset(&m_infoParent, 0, sizeof(m_infoParent));
			memset(&m_infoData, 0, sizeof(m_infoData));
			m_infoData.ckid = mmioFOURCC('d', 'a', 't', 'a');
			/* position file to first data chunk */
			m_infoParent.fccType = mmioFOURCC('W', 'A', 'V', 'E');
			infoSubchunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
			if (mmioDescend(m_MmFile, &m_infoParent,
				NULL, MMIO_FINDRIFF) == MMSYSERR_NOERROR &&
				mmioDescend(m_MmFile,
					&infoSubchunk, &m_infoParent, MMIO_FINDCHUNK) == MMSYSERR_NOERROR)
			{
				m_pFormat.reset(new char[infoSubchunk.cksize]);
				if (((long)mmioRead(m_MmFile, m_pFormat.get(), infoSubchunk.cksize) ==
					(long)infoSubchunk.cksize) &&
					(mmioAscend(m_MmFile, &infoSubchunk, 0) == MMSYSERR_NOERROR) &&
					(mmioDescend(m_MmFile, &m_infoData, &m_infoParent, MMIO_FINDCHUNK) ==
						MMSYSERR_NOERROR))
				{
					m_pWf = reinterpret_cast<const WAVEFORMATEX*>(m_pFormat.get());
					return true;
				}
			}
			close();
			return false;
		}
		HMMIO			m_MmFile;
		MMCKINFO		m_infoParent;
		MMCKINFO		m_infoData;
		const WAVEFORMATEX *m_pWf;
		DWORD m_pos;
		std::unique_ptr<char[]> m_pFormat;
	};

	bool WaveFilePlayer::Play(const std::wstring &fileName, unsigned channel, void *demodulator)
	{
		if (!demodulator)
			return false;

		std::shared_ptr<AudioSink> demo = std::shared_ptr<AudioSink>(
			reinterpret_cast<AudioSink*>(demodulator),
			[](AudioSink *p) {p->ReleaseSink(); });

		MmioFile file(fileName);
		if (file.is_open())
		{
			auto fmt = file.GetFormat();
			if ((fmt->nSamplesPerSec == 12000) &&
				(fmt->wFormatTag == WAVE_FORMAT_PCM) &&
				(fmt->wBitsPerSample == 16))
			{
				// TODO format conversions ?
				static const unsigned AUDIO_FRAMES = 1024;
				std::vector<char> buf(AUDIO_FRAMES);
				for (;;)
				{
					unsigned byteCount = file.Read(&buf[0], static_cast<unsigned>(buf.size()));
					if (byteCount == 0)
						break;
					const unsigned numFrames = byteCount / (sizeof(short) * fmt->nChannels);
					if (fmt->nChannels == 1)
					{
						demo->AddMonoSoundFrames(reinterpret_cast<const short *>(&buf[0]), numFrames);
					}
					else
					{
						std::vector<short> mono(numFrames);
						short *p = &mono[0];
						const short *q = reinterpret_cast<const short *>(&buf[0]);
						if (channel >= fmt->nChannels)
							return false;
						q += channel;
						for (unsigned frames = 0; frames < numFrames; frames += 1)
						{
							*p++ = *q;
							q += fmt->nChannels;
						}
						demo->AddMonoSoundFrames(&mono[0], numFrames);
					}
				}
				demo->AudioComplete();
				return true;
			}
		}
		return false;		
	}

}}