#include <sstream>
#include "WaveDeviceTxImpl.h"

namespace XD {
    struct AudioSink;
    namespace impl {

        namespace {
            const unsigned CYCLE_TIMERID = 2;
            const unsigned DEFAULT_SAMPLES_PER_SECOND = 48000;
            const unsigned MAX_BUFFERS_OUTSTANDING = 10;
        }

        struct PlaybackBuffer : public WAVEHDR
        {
            // the Windows wave device wants buffers
            // described by WAVEHDR. it makes for
            // half as many trips through the memory
            // allocator if we put the WAVEHDR in the buffer.
            PlaybackBuffer(HWAVEOUT wo, unsigned sze)
                :m_w(wo)
                ,samples(sze)
            {   setup();      }

            void setup()
            {
                lpData = (LPSTR)&samples[0];
                dwBufferLength = static_cast<DWORD>(samples.size() * sizeof(samples[0]));
                dwUser = 0;
                dwBytesRecorded = 0;
                dwFlags = 0;
                dwLoops = 0;
                lpNext = 0;
                reserved = 0;
                waveOutPrepareHeader(m_w, this, sizeof(WAVEHDR));
            }
            ~PlaybackBuffer()
            { waveOutUnprepareHeader(m_w, this, sizeof(WAVEHDR)); }

            std::vector<short> samples;
            const HWAVEOUT m_w;
        };

        WaveDeviceTxImpl::WaveDeviceTxImpl()
            : m_waveOut(0)
            , m_mixerControlId({})
            , m_threadId(0)
            , m_wf({})
            , m_deviceIndex(-1)
            , m_channel(-1)
            , m_gain(-1)
            , m_completionSet(false)
            , m_playing(false)
            , m_buffersOutstanding(0)
            , m_throttleSourceOnBuffersOutstanding(false)
            , m_transmitCycle(PLAY_NOW)
            , m_started(::CreateEvent(0, TRUE, FALSE, 0))
            , m_timerActive(false)
            , m_waveOutWriteError(MMSYSERR_NOERROR)
            , m_SamplesPerSecond(DEFAULT_SAMPLES_PER_SECOND)
            , m_windowThread(std::bind(&WaveDeviceTxImpl::threadHead, this))
        {
            ::WaitForSingleObject(m_started, INFINITE);
            ::CloseHandle(m_started);
            m_started = INVALID_HANDLE_VALUE;
        }

        WaveDeviceTxImpl::~WaveDeviceTxImpl()
        {
            if (IsWindow())
            {
                ::PostThreadMessage(m_threadId, WM_DESTROYWAVEOUTWINDOW, 0, 0);
                ::PostThreadMessage(m_threadId, WM_QUIT, 0, 0);
            }
            if (m_windowThread.joinable())
                m_windowThread.join();
        }

        void WaveDeviceTxImpl::threadHead()
        {
            m_threadId = ::GetCurrentThreadId();
            Create(0, 0, 0, WS_POPUP);
            ::SetEvent(m_started);
            MSG msg;
            while (::GetMessage(&msg, 0, 0, 0))
            {
                if (msg.message == WM_DESTROYWAVEOUTWINDOW)
                    DestroyWindow();
                else
                    ::DispatchMessage(&msg);
            }
            if (m_waveOut)
                ::waveOutClose(m_waveOut);
            m_waveOut = 0;
        }

        void  WaveDeviceTxImpl::AudioComplete()
        {   // on different thread
            {
                lock_t l(m_mutex);
                m_completionSet = true;
            }
            PostMessage(WM_WAVECHECKYOURMESSAGES);
        }

        void WaveDeviceTxImpl::SetAudioBeginEndCb(const AudioBeginEndFcn_t&f)
        {      // on different thread
            lock_t l(m_mutex);
            m_complete = f;
        }

        bool  WaveDeviceTxImpl::AddMonoSoundFrames(const short *p, unsigned count)
        {      // on different thread
            if (IsWindow())
            {
                {
                    lock_t l(m_mutex);
                    if (m_channel <= 1)
                    {   // one of two stereo channels
                        std::unique_ptr<PlaybackBuffer> buf (new PlaybackBuffer(m_waveOut, count * 2));
                        short *q = &buf->samples[0];
                        if (m_channel==1)
                            q += 1;
                        for (unsigned i = 0; i < count; i++)
                        {
                            *q++ = *p++;
                            q += 1;
                        }
                        m_toSend.push_back(std::move(buf));
                    }
                    else
                    {   // mono output
                        std::unique_ptr<PlaybackBuffer> buf(new PlaybackBuffer(m_waveOut,count));
                        memcpy(&buf->samples[0], p, count * sizeof(*p));
                        m_toSend.push_back(std::move(buf));
                    }
                    m_completionSet = false;
                    while (m_throttleSourceOnBuffersOutstanding && (m_buffersOutstanding >= MAX_BUFFERS_OUTSTANDING - 1))
                        m_notifyBuffersOutstanding.wait(l);
                }
                PostMessage(WM_WAVECHECKYOURMESSAGES);
                return true;
            }
            return false;
        }

        // on window thread
        void WaveDeviceTxImpl::StartPlayIfTime()
        {
            if (!m_playing)
            {
                if (m_transmitCycle != PLAY_NOW)
                {
                    bool playNow = false;
                    unsigned cycleTenthSeconds = 0;
                    bool doOdd = false;
                    switch (m_transmitCycle)
                    {
                    case PLAY_ODD_15S:
                        doOdd = true;
                        cycleTenthSeconds = 150;
                        break;
                    case PLAY_EVEN_15S:
                        cycleTenthSeconds = 150;
                        break;
                    case PLAY_ODD_8PER_MIN:
                        cycleTenthSeconds = 75;
                        doOdd = true;
                        break;
                    case PLAY_EVEN_8PER_MIN:
                        cycleTenthSeconds = 75;
                        break;
                    case PLAY_NOW:
                        playNow = true;
                        break;
                    }

                    if (!playNow)
                    {
                        SYSTEMTIME st;
                        GetSystemTime(&st);
                        WORD tenthSeconds = st.wSecond * 10 + st.wMilliseconds / 100;
                        unsigned ftSecondNumberByTen = tenthSeconds % cycleTenthSeconds;
                        static const unsigned MAX_TENTHS_LATE = 2;
                        if (ftSecondNumberByTen <= MAX_TENTHS_LATE)
                        {
                            bool cycleIsOdd = (1 & (tenthSeconds / cycleTenthSeconds)) != 0;
                            playNow = !(cycleIsOdd ^ doOdd);
                        }
                    }

                    if (!playNow)
                    {   // cannot start in current second
                        if (!m_timerActive)
                        {
                            m_timerActive = true;
                            SetTimer(CYCLE_TIMERID, 50);
                        }
                        return; // start later
                    }
                }
                // the order here is intentional:
                // If the callback delays us, then this window thread
                // is blocked and the audio does NOT start until the callback
                // returns.
                AudioBeginEndFcn_t f;
                {
                    lock_t l(m_mutex);
                    f = m_complete;
                }
                if (f)
                    f(true);
                if (MMSYSERR_NOERROR == ::waveOutRestart(m_waveOut))
                    m_playing = true;
            }
            KillTimer(CYCLE_TIMERID);
            m_timerActive = false;
        }

        LRESULT  WaveDeviceTxImpl::OnTimer(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
            BOOL& /*bHandled*/)
        {
            StartPlayIfTime();
            return 0;
        }

        LRESULT WaveDeviceTxImpl::OnNewData(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
            BOOL& /*bHandled*/)
        {
            if (m_waveOut == 0)
                return 0;
            if (m_buffersOutstanding > MAX_BUFFERS_OUTSTANDING)
                return 0;
            std::unique_ptr<PlaybackBuffer> readyToSend;
            {
                lock_t l(m_mutex);
                if (!m_toSend.empty())
                {
                    readyToSend = std::move(m_toSend.front());
                    m_toSend.pop_front();
                }
            }
            if (readyToSend)
            {
                MMRESULT mmr = ::waveOutWrite(m_waveOut, readyToSend.release(), sizeof(WAVEHDR));
                if (MMSYSERR_NOERROR == mmr)
                {
                    m_buffersOutstanding += 1;
                    if (!m_playing)
                        StartPlayIfTime();
                }
                else
                {
                    m_waveOutWriteError = mmr;
                    waveOutClose(m_waveOut);
                    m_waveOut = 0;
                }
            }
            return 0;
        }

        bool WaveDeviceTxImpl::Open(unsigned deviceIndex,
            unsigned channel)
        {
            if (IsWindow())
                return SendMessage(WM_WAVEDEVICEOUTONOPEN, deviceIndex, channel) != 0;
            return false;
        }

        LRESULT WaveDeviceTxImpl::OnOpenPlayback(UINT /*nMsg*/, WPARAM deviceIndex, LPARAM channel,
            BOOL& /*bHandled*/)
        {
            if (m_waveOut)
                return 0;
            if ((channel > 2) || (channel < 0))
                return 0;
            // we handle just two variations: mono and stereo.
            m_wf = {};
            m_wf.wFormatTag = WAVE_FORMAT_PCM;
            m_wf.nSamplesPerSec = m_SamplesPerSecond;
            m_wf.wBitsPerSample = 16;
            m_wf.nChannels = channel <= 1 ? 2 : 1;
            m_wf.nBlockAlign = m_wf.wBitsPerSample / 8 * m_wf.nChannels;
            m_wf.nAvgBytesPerSec = m_wf.nSamplesPerSec * m_wf.nBlockAlign;

            if (MMSYSERR_NOERROR == ::waveOutOpen(&m_waveOut, static_cast<UINT>(deviceIndex), &m_wf,
                (DWORD_PTR)m_hWnd, 0, CALLBACK_WINDOW))
            {	
                ::waveOutPause(m_waveOut);
                m_playing = false;
                m_deviceIndex = static_cast<unsigned>(deviceIndex);
                m_channel = static_cast<unsigned>(channel);

                // find its volume control
                MIXERLINECONTROLSW mixerLineControls = {};
                mixerLineControls.cbStruct = sizeof(mixerLineControls);
                mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
                m_mixerControlId = {};
                m_mixerControlId.cbStruct = sizeof(m_mixerControlId);
                mixerLineControls.pamxctrl = &m_mixerControlId;
                mixerLineControls.cbmxctrl = sizeof(MIXERCONTROLW);
                mixerLineControls.cControls = 1;
                MMRESULT mmres = mixerGetLineControlsW(reinterpret_cast<HMIXEROBJ>(m_waveOut), &mixerLineControls,
                    MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HWAVEOUT);
                if (mmres == MMSYSERR_NOERROR)
                {
                    MIXERCONTROLDETAILS	mxcd = {};
                    mxcd.cbStruct = sizeof(mxcd);
                    mxcd.dwControlID = m_mixerControlId.dwControlID;
                    mxcd.cChannels = m_wf.nChannels;
                    std::vector<MIXERCONTROLDETAILS_UNSIGNED>		uValue(m_wf.nChannels);
                    mxcd.cbDetails = sizeof(uValue[0]);
                    mxcd.paDetails = &uValue[0];
                    mmres = mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(m_waveOut), &mxcd,
                        MIXER_OBJECTF_HWAVEOUT);
                    if (mmres == MMSYSERR_NOERROR)
                    {
                        unsigned idx = m_wf.nChannels > 1 ? static_cast<unsigned>(channel) : 0;
                        m_gain = static_cast<float>(m_mixerControlId.Bounds.dwMinimum + uValue[idx].dwValue) /
                            static_cast<float>(m_mixerControlId.Bounds.dwMaximum - m_mixerControlId.Bounds.dwMinimum);
                    }
                    else if (m_wf.nChannels == 2)
                    {
                        uValue.resize(1);
                        mxcd.cChannels = 1;
                        mmres = mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(m_waveOut), &mxcd,
                            MIXER_OBJECTF_HWAVEOUT);
                        if (mmres == MMSYSERR_NOERROR)
                            m_gain = static_cast<float>(m_mixerControlId.Bounds.dwMinimum + uValue[0].dwValue) /
                                static_cast<float>(m_mixerControlId.Bounds.dwMaximum - m_mixerControlId.Bounds.dwMinimum);
                    }
                    
                }
                return 1;
            }
            return 0;
        }

        void WaveDeviceTxImpl::Abort()
        {
            if (IsWindow())
                SendMessage(WM_WAVEDEVICEOUTABORT);
            m_notifyBuffersOutstanding.notify_all();
        }

        bool WaveDeviceTxImpl::OkToStart()
        {
            if (!IsWindow())
                throw std::runtime_error("WaveTxDevice no HWND");
            if (!m_waveOut)
            {
                if (m_waveOutWriteError == MMSYSERR_NOERROR)
                    throw std::runtime_error("WaveTxDevice no HWAVEOUT");
                else
                {
                    std::stringstream oss;
                    oss << "WaveTxDevice no HWAVEOUT, err=" << m_waveOutWriteError;
                    throw std::runtime_error(oss.str().c_str());
                }
            }
            return true;
        }

        LRESULT WaveDeviceTxImpl::OnAbortPlayback(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
            BOOL& /*bHandled*/)
        {
            if (m_waveOut)
            {
                ::waveOutReset(m_waveOut);
                ::waveOutPause(m_waveOut);
                KillTimer(CYCLE_TIMERID);
                m_timerActive = false;
                lock_t l(m_mutex);
                m_toSend.clear();
                m_playing = false;
            }
            m_notifyBuffersOutstanding.notify_all();
            return 0;
        }

        LRESULT WaveDeviceTxImpl::OnWomDone(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM pBuf,
            BOOL& bHandled)
        {
            PlaybackBuffer *pPlayback = reinterpret_cast<PlaybackBuffer*>(pBuf);
            delete pPlayback;
            m_buffersOutstanding -= 1;
            m_notifyBuffersOutstanding.notify_all();
            if (m_completionSet && (m_buffersOutstanding == 0) && m_toSend.empty())
            {
                ::waveOutPause(m_waveOut);
                m_playing = false;
                AudioBeginEndFcn_t f;
                {
                    lock_t l(m_mutex);
                    f = m_complete;
                    // only callback one time (once true, one false)
                    m_complete = AudioBeginEndFcn_t();
                    m_completionSet = false;
                }
                if (f)
                    f(false);
            }
            else
                OnNewData(0, 0, 0, bHandled);

            return 0;
        }

        LRESULT WaveDeviceTxImpl::OnSetGain(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/,
            BOOL& /*bHandled*/)
        {
            float gain = *reinterpret_cast<float *>(&wParam);
            if ((gain < 0) || (gain > 1))
                return 0;
            MIXERCONTROLDETAILS	mxcd = {};
            mxcd.cbStruct = sizeof(mxcd);
            mxcd.dwControlID = m_mixerControlId.dwControlID;
            mxcd.cChannels = m_wf.nChannels;
            std::vector<MIXERCONTROLDETAILS_UNSIGNED>		uValue(m_wf.nChannels);
            mxcd.cbDetails = sizeof(uValue[0]);
            mxcd.paDetails = &uValue[0];
            unsigned idx = m_wf.nChannels > 1 ? m_channel : 0;
            if ((MMSYSERR_NOERROR != mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(m_waveOut), &mxcd, MIXER_OBJECTF_HWAVEOUT))
                && m_wf.nChannels != 1)
            {
                mxcd.cChannels = 1;
                idx = 0;
            }
            uValue[idx].dwValue = static_cast<DWORD>(gain * (m_mixerControlId.Bounds.dwMaximum - m_mixerControlId.Bounds.dwMinimum));
            uValue[idx].dwValue += m_mixerControlId.Bounds.dwMinimum;
            MMRESULT mmres = mixerSetControlDetails(reinterpret_cast<HMIXEROBJ>(m_waveOut), &mxcd,
                MIXER_OBJECTF_HWAVEOUT);
            if (mmres == MMSYSERR_NOERROR)
                m_gain = gain;
            return 0;
        }

        void WaveDeviceTxImpl::SetGain(float g)
        {
            if (IsWindow())
                SendMessage(WM_SETGAIN, *reinterpret_cast<WPARAM*>(&g));
        }

        float WaveDeviceTxImpl::GetGain()
        {  return m_gain;   }

        void WaveDeviceTxImpl::SetSamplesPerSecond(unsigned g)
        {
            m_SamplesPerSecond = g;
        }

        unsigned WaveDeviceTxImpl::GetSamplesPerSecond()
        {
            return m_SamplesPerSecond;
        }

        void WaveDeviceTxImpl::SetThrottleSource(bool v)
        {
            m_throttleSourceOnBuffersOutstanding = v;
            m_notifyBuffersOutstanding.notify_all();
        }

        bool WaveDeviceTxImpl::GetThrottleSource()
        {
            return m_throttleSourceOnBuffersOutstanding;
        }
    }
}