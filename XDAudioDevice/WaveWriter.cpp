#include "stdafx.h"
#include "AudioSink.h"
#include "WaveWriter.h"
#include "WaveWriterImpl.h"
namespace XD {
    namespace impl {
        struct FileAudioSink : public XD::AudioSink
        {
            FileAudioSink(std::shared_ptr<WaveWriterImpl> impl)
                : m_impl(impl)
            {}

            bool __stdcall AddMonoSoundFrames(const short *p, unsigned count) override
            {
                return m_impl->Write(p, count);
            }

            void __stdcall AudioComplete() override
            { }

            void __stdcall ReleaseSink() override
            {   delete this;    }

        private:
            std::shared_ptr<WaveWriterImpl> m_impl;
        };

        AudioSink * WaveWriter::Open(const std::wstring &filePath)
        {
            return new FileAudioSink(std::make_shared<WaveWriterImpl>(filePath, 48000));
        }
    }
}