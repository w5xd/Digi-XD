#pragma once
#include <mmsystem.h> //primitive old Windows wave interface...
namespace XD {
    namespace impl {

        class WaveWriterImpl {
        public:
            WaveWriterImpl(const std::wstring &fn, unsigned rate = 12000)
                :m_fileHandle(mmioOpenW(
                    const_cast<wchar_t *>(fn.c_str()),
                    0, MMIO_CREATE | MMIO_WRITE | MMIO_EXCLUSIVE))
                , m_info1({})
                , m_info2({})
            {
                m_info1.fccType = mmioFOURCC('W', 'A', 'V', 'E');
                if (mmioCreateChunk(m_fileHandle, &m_info1, MMIO_CREATERIFF) ==
                    MMSYSERR_NOERROR)
                {
                    MMCKINFO infoFmt = { 0 };
                    infoFmt.ckid = mmioFOURCC('f', 'm', 't', ' ');
                    if (mmioCreateChunk(m_fileHandle, &infoFmt, 0) ==
                        MMSYSERR_NOERROR)
                    {
                        WAVEFORMATEX wfmt = { 0 };
                        wfmt.wFormatTag = WAVE_FORMAT_PCM;
                        wfmt.nSamplesPerSec = rate;
                        wfmt.wBitsPerSample = 16;
                        wfmt.nChannels = 1;
                        wfmt.nBlockAlign = wfmt.wBitsPerSample / 8 * wfmt.nChannels;
                        wfmt.nAvgBytesPerSec = wfmt.nSamplesPerSec * wfmt.nBlockAlign;

                        mmioWrite(m_fileHandle, (const char *)&wfmt, sizeof(wfmt) + wfmt.cbSize);
                        mmioAscend(m_fileHandle, &infoFmt, 0);

                        m_info2.ckid = mmioFOURCC('d', 'a', 't', 'a');
                        if (MMSYSERR_NOERROR == ::mmioCreateChunk(m_fileHandle, &m_info2, 0))
                            return;
                    }
                }
                mmioClose(m_fileHandle, 0);
                m_fileHandle = 0;

            }
            ~WaveWriterImpl()
            {
                if (m_fileHandle)
                {
                    mmioAscend(m_fileHandle, &m_info2, 0);
                    mmioAscend(m_fileHandle, &m_info1, 0);
                    mmioClose(m_fileHandle, 0);
                }
            }

            bool Write(const short *p, unsigned samples)
            {
                unsigned byteCount = samples * sizeof(*p);
                return byteCount ==
                    mmioWrite(m_fileHandle, reinterpret_cast<const char*>(p), byteCount);
            }
        protected:
            HMMIO m_fileHandle;
            MMCKINFO m_info1;
            MMCKINFO m_info2;
        };
    }
}