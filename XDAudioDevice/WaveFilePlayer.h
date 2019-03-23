#pragma once
#include <string>
namespace XD {
    namespace impl {
        class WaveFilePlayer
        {
        public:
            static bool Play(const std::wstring &fileName, unsigned channel, void *);
        };

    }
}