#include "stdafx.h"

#include "XDRxAudioSource.h"
#include "WaveDevicePlayer.h"
#include "WaveFilePlayer.h"

namespace XD {

	WaveDevicePlayer::WaveDevicePlayer()
	{  }

	WaveDevicePlayer::~WaveDevicePlayer()
	{
		this->!WaveDevicePlayer();
	}

	WaveDevicePlayer::!WaveDevicePlayer()
	{Close();}

	bool WaveDevicePlayer::Open(unsigned deviceIndex, unsigned channel, System::IntPtr demodulatorSink)
	{
		Close();
		if (demodulatorSink.ToPointer() == 0)
			return false;
		m_impl = new impl::WaveDevicePlayer();
		return m_impl->Open(deviceIndex, channel, demodulatorSink.ToPointer());
	}

	void WaveDevicePlayer::Close()
	{
		if (m_impl)
			m_impl->Close();
		delete m_impl;
		m_impl = 0;
	}

	bool WaveDevicePlayer::Pause()
	{
		if (m_impl)
			return m_impl->Pause();
		return false;
	}

	bool WaveDevicePlayer::Resume()
	{
		if (m_impl)
			return m_impl->Resume();
		return false;
	}

	bool WaveDevicePlayer::StartRecordingFile(System::String ^filePath)
	{
		if (m_impl)
		{
			m_impl->RecordFile(msclr::interop::marshal_as<std::wstring>(filePath));
			return true;
		}
		return false;
	}

	void WaveDevicePlayer::StopRecording()
	{
		if (m_impl)
			m_impl->StopRecord();
	}


	bool WaveFilePlayer::Play(System::String ^fileName, unsigned channel, System::IntPtr demodulator)
	{
        try {
            return impl::WaveFilePlayer::Play(msclr::interop::marshal_as<std::wstring>(fileName),
                channel, demodulator.ToPointer());
        }
        catch (const std::exception &e)
        {   // convert any native exception to .net
            throw gcnew System::Exception(gcnew System::String(e.what()));
        }
	}

}