#pragma once
#include <string>
namespace XD {
    namespace impl {
        class WaveDevicePlayerImpl;
        class WaveDevicePlayer
        {
        public:
            WaveDevicePlayer();
            ~WaveDevicePlayer();
            bool Open(unsigned deviceIndex, unsigned channel, void *);
            void Close();
            bool Pause();
            bool Resume();

            void RecordFile(const std::wstring &w);
            void StopRecord();
        private:
            std::shared_ptr<WaveDevicePlayerImpl> m_impl;
        };

    }
}