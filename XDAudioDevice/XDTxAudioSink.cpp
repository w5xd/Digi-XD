#include "stdafx.h"
#include "XDTxAudioSink.h"
#include "WaveDeviceTx.h"
#include "WaveWriter.h"
namespace XD {

    WaveDeviceTx::WaveDeviceTx()
        : m_impl(new  impl::WaveDeviceTx())
    {    }

    WaveDeviceTx::~WaveDeviceTx()
    {  this->!WaveDeviceTx();  }

    WaveDeviceTx::!WaveDeviceTx()
    {
        delete m_impl;
        m_impl = 0;
    }

    bool WaveDeviceTx::Open(unsigned deviceIndex, // devIndex is up to waveOutGetNumDevs
        unsigned channel // channel is 0 for left, 1 for right
    )
    {
        try {
            return m_impl->Open(deviceIndex, channel);
        }
        catch (const std::exception &e)
        {   //convert c++ to clr exceptions
            throw gcnew System::Exception(gcnew System::String(e.what()));
        }
    }

    void WaveDeviceTx::Abort()
    {   m_impl->Abort();   }

    void ForwardAudioComplete(long long cbId, bool isBeginning)
    {
        SoundBeginEnd^ client =
            System::Runtime::InteropServices::Marshal::GetDelegateForFunctionPointer<SoundBeginEnd^>
            (System::IntPtr(cbId));
        if (nullptr == client)
            return;
        try {
            client(isBeginning);
        }
        catch (System::Exception ^)
        { /* ignore clr exceptions if client throws them*/
        }
    }

    System::IntPtr WaveDeviceTx::GetRealTimeAudioSink()
    {
        impl::AudioBeginEndFcn_t
            completeFcn;
        if (nullptr != m_soundCallback)
            completeFcn =
            std::bind(&ForwardAudioComplete,
                System::Runtime::InteropServices::Marshal::GetFunctionPointerForDelegate<SoundBeginEnd^>(m_soundCallback).ToInt64(),
                std::placeholders::_1);
        return System::IntPtr(m_impl->GetTxSink(completeFcn));
    }

    SoundBeginEnd^  WaveDeviceTx::SoundSyncCallback::get()
    {   return m_soundCallback;   }

    void WaveDeviceTx::SoundSyncCallback::set(SoundBeginEnd^v)
    {   m_soundCallback = v;   }

    Transmit_Cycle  WaveDeviceTx::TransmitCycle::get()
    { return static_cast<Transmit_Cycle>((int)m_impl->GetTransmitCycle());    }

    void   WaveDeviceTx::TransmitCycle::set(Transmit_Cycle when)
    { m_impl->SetTransmitCycle(static_cast<impl::Transmit_Cycle>((int)when));  }

    System::IntPtr FileDeviceTx::Open(System::String ^filePath)
    {
        try {
            impl::WaveWriter toOpen;
            return System::IntPtr(toOpen.Open(msclr::interop::marshal_as<std::wstring>(filePath)));
        }
        catch (const std::exception &e)
        {
            throw gcnew System::Exception(gcnew System::String(e.what()));
        }
    }
}