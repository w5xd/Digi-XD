#include "WaveDevicePlayer.h"
#include "WaveDevicePlayerImpl.h"
/* Why this seemingly useless class that does nothing but forward calls? 
* Answer: it keeps the compile of the clr part of the app
* separate from the native stuff that calls std::thread (which
* the clr won't even allow us to compile) and mmsystem and
* other goop that you just don't want to try to force the
* C++ compiler to swallow in the same compilation unit.
*/
namespace XD { namespace impl {

	WaveDevicePlayer::WaveDevicePlayer() 
		:m_impl(std::make_shared<WaveDevicePlayerImpl>())
	{	}

	WaveDevicePlayer::~WaveDevicePlayer()
	{	}

	bool WaveDevicePlayer::Open(unsigned deviceIndex, unsigned channel, void *demod)
	{ return m_impl->Open(deviceIndex, channel, demod);	}
	void WaveDevicePlayer::Close()
	{ return m_impl->Close();	}
	bool WaveDevicePlayer::Pause()
	{ return m_impl->Pause();	}
	bool WaveDevicePlayer::Resume()
	{ return m_impl->Resume();}
    void WaveDevicePlayer::RecordFile(const std::wstring &w)
    {        m_impl->RecordFile(w);    }
    void WaveDevicePlayer::StopRecord()
    {        m_impl->StopRecord();    }
    void WaveDevicePlayer::SetGain(float f)
    { return m_impl->SetGain(f);}
    float WaveDevicePlayer::GetGain()
    { return m_impl->GetGain();}

}}