#include <AudioSink.h>
#include "WaveDeviceTx.h"
#include "WaveDeviceTxImpl.h"
namespace XD {
    namespace impl {

        WaveDeviceTx::WaveDeviceTx()
            : m_impl(std::make_shared<WaveDeviceTxImpl>())
        {        }

        WaveDeviceTx::~WaveDeviceTx()
        {        }

        bool WaveDeviceTx::Open(unsigned deviceIndex, 
            unsigned channel   )
        {            return m_impl->Open(deviceIndex, channel);        }

        void WaveDeviceTx::Abort()
        {            m_impl->Abort();        }

        struct TxAudioSink : public XD::AudioSink
        {
            TxAudioSink(std::shared_ptr<WaveDeviceTxImpl> impl)
                : m_weak(impl)
            {}

            bool __stdcall AddMonoSoundFrames(const short *p, unsigned count) override
            {
                auto device = m_weak.lock();
                if (device)
                    return device->AddMonoSoundFrames(p, count);
                return false;
            }

            void __stdcall AudioComplete() override
            {
                auto device = m_weak.lock();
                if (device)
                    device->AudioComplete();
            }

            void __stdcall ReleaseSink() override
            {
                delete this;
            }
        private:
            std::weak_ptr<WaveDeviceTxImpl> m_weak;
        };

        AudioSink *WaveDeviceTx::GetTxSink(const AudioBeginEndFcn_t &complete)
        {
            if (!m_impl)
                return false;
            if (!m_impl->OkToStart())
                return false;
            m_impl->SetAudioBeginEndCb(complete);
            return new TxAudioSink(m_impl);
        }
        Transmit_Cycle WaveDeviceTx::GetTransmitCycle() const
        {  return m_impl->GetTransmitCycle(); }
        void WaveDeviceTx::SetTransmitCycle(Transmit_Cycle v)
        {  return m_impl->SetTransmitCycle(v);  }
        void WaveDeviceTx::SetGain(float g)
        { return m_impl->SetGain(g);}
        float WaveDeviceTx::GetGain()
        { return m_impl->GetGain();}
    }
}
