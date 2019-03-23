#pragma once

using namespace System;

namespace XD {
	namespace impl {
		class WaveDevicePlayer;
	}
	public ref class WaveDevicePlayer
	{
	public:
		WaveDevicePlayer();
		~WaveDevicePlayer();
		!WaveDevicePlayer();

		bool Open(unsigned deviceIndex, // devIndex is up to waveInGetNumDevs
			unsigned channel, // channel is 0 for left, 1 for right
			System::IntPtr demodulatorSink // native RxAudioSink*
		);
		void Close();
		// pause input wave reading
		bool Pause();
		// Open is in paused state. Resume to start input
		bool Resume();

		bool StartRecordingFile(System::String ^filePath);
		void StopRecording();
	private:
		impl::WaveDevicePlayer *m_impl;
	};

	public ref class WaveFilePlayer
	{
	public:
		static bool Play(System::String ^fileName, unsigned channel, System::IntPtr demodulator);
	};
}

