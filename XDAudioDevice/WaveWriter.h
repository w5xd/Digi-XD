#pragma once
namespace XD {
    struct AudioSink;
    namespace impl {
        class WaveWriterImpl;
        class WaveWriter
        {
        public:
            static AudioSink * Open(const std::wstring &filePath);
        };

    }
}