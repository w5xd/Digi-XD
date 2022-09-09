#pragma once

using namespace System;

namespace XD {
    public enum class Transmit_Cycle { PLAY_NOW, PLAY_ODD_15S, PLAY_EVEN_15S, PLAY_ODD_8PER_MIN, PLAY_EVEN_8PER_MIN };
    // must match WaveDeviceTx.h
    namespace impl {
        class WaveDeviceTx;
        class WaveWriter;
    }
    public delegate void SoundBeginEnd(bool isBeginning);
    public ref class WaveDeviceTx
    {
    public:
        WaveDeviceTx();
        ~WaveDeviceTx();
        !WaveDeviceTx();

        bool Open(unsigned deviceIndex, // devIndex is up to waveOutGetNumDevs
            unsigned channel // channel is 0 for left, 1 for right
        );
        void Abort();
        
        property Transmit_Cycle  TransmitCycle { Transmit_Cycle get(); void set(Transmit_Cycle); }

        System::IntPtr GetRealTimeAudioSink();
        property SoundBeginEnd^ SoundSyncCallback { SoundBeginEnd^ get(); void set(SoundBeginEnd^); }
        property float Gain { float get(); void set(float); }
        property unsigned SamplesPerSecond { unsigned get(); void set(unsigned); }
        property bool ThrottleSource { bool get(); void set(bool); }

    private:
        impl::WaveDeviceTx *m_impl;
        SoundBeginEnd ^m_soundCallback;
    };

    public ref class FileDeviceTx
    {
    public:
       static System::IntPtr Open(System::String ^filePath  );
    };

}
