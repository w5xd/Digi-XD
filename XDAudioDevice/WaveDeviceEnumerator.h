#pragma once
namespace XD {
    public ref class WaveDeviceEnumerator
    {
    public:
        static System::Collections::Generic::List<System::String^> ^waveInDevices();
        static System::Collections::Generic::List<System::String^> ^waveOutDevices();
        static System::String^ waveInInstanceId(int waveInIdx);
        static System::String^ waveOutInstanceId(int waveOutIdx);
    };
}

