#pragma once
#include <memory>
#include <functional>
namespace XD {
    struct AudioSink;
    namespace impl {
        enum Transmit_Cycle { PLAY_NOW, PLAY_ODD_15S, PLAY_EVEN_15S, PLAY_ODD_8PER_MIN, PLAY_EVEN_8PER_MIN };
        // must match XDTxAudioSink.h
        class WaveDeviceTxImpl;
        typedef std::function<void(bool)> AudioBeginEndFcn_t;
        class WaveDeviceTx
        {
        public:
            WaveDeviceTx();
            ~WaveDeviceTx();
            bool Open(unsigned deviceIndex, 
                unsigned channel  );
            void Abort();
            Transmit_Cycle GetTransmitCycle() const;
            void SetTransmitCycle(Transmit_Cycle v);
            AudioSink *GetTxSink(const AudioBeginEndFcn_t &);
            void SetGain(float);
            float GetGain();
            void SetSamplesPerSecond(unsigned);
            unsigned GetSamplesPerSecond();
            void SetThrottleSource(bool);
            bool GetThrottleSource();
        protected:
            std::shared_ptr<WaveDeviceTxImpl> m_impl;
        };
    }
}

