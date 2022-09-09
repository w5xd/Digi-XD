#pragma once
#include <thread>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <deque>
#include <vector>
#include <windows.h>
#include <mmsystem.h> //primitive old Windows wave interface...
#include <atlbase.h>
#include <atlwin.h>
#include "WaveDeviceTx.h"
namespace XD {
    struct AudioSink;
    namespace impl {
        const UINT WM_DESTROYWAVEOUTWINDOW = WM_USER + 1;
        const UINT WM_WAVEDEVICEOUTONOPEN = WM_USER + 2;
        const UINT WM_WAVEDEVICEOUTABORT = WM_USER + 3;
        const UINT WM_WAVECHECKYOURMESSAGES = WM_USER + 4;
        const UINT WM_SETGAIN = WM_USER + 5;
        struct PlaybackBuffer;
        class WaveDeviceTxImpl : public ATL::CWindowImpl<WaveDeviceTxImpl>
        {
        public:
            WaveDeviceTxImpl();
            ~WaveDeviceTxImpl();
            bool Open(unsigned deviceIndex,  unsigned channel);
            void Abort();
            void AudioComplete();
            bool AddMonoSoundFrames(const short *p, unsigned count);
            Transmit_Cycle GetTransmitCycle() const { return m_transmitCycle;   }
            void SetTransmitCycle(Transmit_Cycle v) { m_transmitCycle = v; }
            void SetAudioBeginEndCb(const AudioBeginEndFcn_t&);
            bool OkToStart();
            void SetGain(float);
            float GetGain();
            void SetSamplesPerSecond(unsigned);
            unsigned GetSamplesPerSecond();
            void SetThrottleSource(bool);
            bool GetThrottleSource();
            DECLARE_WND_CLASS(_T("XDft8WaveDeviceRecorder"))
        protected:
            typedef std::unique_lock<std::mutex> lock_t;
            BEGIN_MSG_MAP(WaveDeviceTxImpl)
                MESSAGE_HANDLER(WM_WAVEDEVICEOUTONOPEN, OnOpenPlayback)
                MESSAGE_HANDLER(WM_WAVEDEVICEOUTABORT, OnAbortPlayback)
                MESSAGE_HANDLER(WM_WAVECHECKYOURMESSAGES, OnNewData)
                MESSAGE_HANDLER(WM_SETGAIN, OnSetGain)
                MESSAGE_HANDLER(MM_WOM_DONE, OnWomDone)
                MESSAGE_HANDLER(WM_TIMER, OnTimer)
            END_MSG_MAP()

            LRESULT OnOpenPlayback(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnAbortPlayback(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnNewData(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnWomDone(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnTimer(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnSetGain(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);

            void threadHead();

            void StartPlayIfTime();

            Transmit_Cycle m_transmitCycle;
            WAVEFORMATEX m_wf;
            unsigned m_SamplesPerSecond;
            HWAVEOUT m_waveOut;
            MMRESULT m_waveOutWriteError;
            MIXERCONTROLW m_mixerControlId;
            float m_gain;
            HANDLE m_started;
            DWORD m_threadId;
            unsigned m_deviceIndex;
            unsigned m_channel;
            AudioBeginEndFcn_t m_complete;
            bool m_completionSet;
            bool m_playing;
            int m_buffersOutstanding;
            bool m_timerActive;
            bool m_throttleSourceOnBuffersOutstanding;
            std::deque<std::unique_ptr<PlaybackBuffer>> m_toSend;
            std::condition_variable m_notifyBuffersOutstanding;
            std::mutex m_mutex;
            std::thread m_windowThread;// initialize last
        };
    }
}

