#include <vector>
#include "WaveDevicePlayerImpl.h"
#include <AudioSink.h>
#include "WaveWriterImpl.h"
namespace XD { namespace impl {

    const unsigned recordBufferLengthMsec = 64;

	struct RecordingBuffer : public WAVEHDR
	{
        typedef short Sample_t;
        // the Windows wave device wants buffers
        // described by WAVEHDR. it makes for
        // half as many trips through the memory
        // allocator if we put the WAVEHDR in the buffer.
		RecordingBuffer(HWAVEIN wi, unsigned sampleCount) 
			:m_w(wi)
            ,samples(sampleCount)
		{
			setup();
		}
		void setup()
		{
			lpData = reinterpret_cast<LPSTR>(&samples[0]);
			dwBufferLength = static_cast<DWORD>(sizeof(Sample_t) * samples.size());
			dwUser = 0;
			dwBytesRecorded = 0;
			dwFlags = 0;
			dwLoops = 0;
			lpNext = 0;
			reserved = 0;
			waveInPrepareHeader(m_w, this, sizeof(WAVEHDR));
		}
		~RecordingBuffer()
		{
			waveInUnprepareHeader(m_w, this, sizeof(WAVEHDR));
		}
		void reuse()
		{
			waveInUnprepareHeader(m_w, this, sizeof(WAVEHDR));
			setup();
		}
		const HWAVEIN m_w;
        const Sample_t *pSamples() { return &samples[0]; }
    protected:
		std::vector<Sample_t> samples;
	};

	WaveDevicePlayerImpl::WaveDevicePlayerImpl()
		: m_waveIn(0)
        , m_mixerControlId({})
		, m_threadId(0)
        , m_wf({})
		, m_deviceIndex(-1)
        , m_channel(-1)
        , m_gain(-1)
		, m_started(::CreateEvent(0, TRUE, FALSE, 0))
		, m_windowThread(std::bind(&WaveDevicePlayerImpl::threadHead, this))
	{
		::WaitForSingleObject(m_started, INFINITE);
		::CloseHandle(m_started);
		m_started = INVALID_HANDLE_VALUE;
	}

	WaveDevicePlayerImpl::~WaveDevicePlayerImpl()
	{
		if (IsWindow())
		{
			Close();
			::PostThreadMessage(m_threadId, WM_DESTROYWAVEWINDOW, 0, 0);
			::PostThreadMessage(m_threadId, WM_QUIT, 0, 0);
		}
		if (m_windowThread.joinable())
			m_windowThread.join();
	}

    
    // goop to put the wave in device on its own thread.
    // why? the wave device needs to live on a thread
    // that has a message pump (GetMessage/DispatchMessage).
    // But we're in Windows .net land, not knowing whether
    // our caller has one of those. 
    // so we make one.
	void WaveDevicePlayerImpl::threadHead()
	{
		m_threadId = ::GetCurrentThreadId();
		Create(0, 0, 0, WS_POPUP);
		::SetEvent(m_started);
		MSG msg;
		while (::GetMessage(&msg, 0, 0, 0))
		{
			if (msg.message == WM_DESTROYWAVEWINDOW)
				DestroyWindow();
			else
				::DispatchMessage(&msg);
		}
		if (m_waveIn)
			::waveInClose(m_waveIn);
		m_waveIn = 0;
	}

	// from external thread
	bool WaveDevicePlayerImpl::Open(unsigned deviceIndex, unsigned channel, void *demod)
	{
        if (IsWindow())
        {
            bool retval =  SendMessage(WM_STARTPLAYBACK, deviceIndex, channel) != 0;
            if (retval)
            {
				m_audioSink = std::shared_ptr<AudioSink>(reinterpret_cast<XD::AudioSink*>(demod),
					[](AudioSink*p) { p->ReleaseSink(); });
            }
            return retval;
        }
		return false;
	}
	
	// from external thread
	void WaveDevicePlayerImpl::Close()
	{
		if (IsWindow())
			SendMessage(WM_STOPPLAYBACK);
		m_audioSink.reset();
	}

	LRESULT WaveDevicePlayerImpl::OnOpenPlayback(UINT /*nMsg*/, WPARAM deviceIndex, LPARAM channel,
		BOOL& /*bHandled*/)
	{
		if (m_waveIn)
			return 0;
		if (channel > 1)
			return 0;
        // we handle just two variations: mono and stereo.
        m_wf = {};
        m_wf.wFormatTag = WAVE_FORMAT_PCM;
        m_wf.nSamplesPerSec = 12000;
        m_wf.wBitsPerSample = 16;
        m_wf.nChannels = 2;
        m_wf.nBlockAlign = m_wf.wBitsPerSample / 8 * m_wf.nChannels;
        m_wf.nAvgBytesPerSec = m_wf.nSamplesPerSec * m_wf.nBlockAlign;

		if (MMSYSERR_NOERROR == ::waveInOpen(&m_waveIn, static_cast<UINT>(deviceIndex), &m_wf,
			(DWORD_PTR)m_hWnd, 0, CALLBACK_WINDOW))
		{	// try stereo first
            // success...do nothing
		}
		else
		{	// try mono.
            m_wf.nChannels = 1;
            m_wf.nBlockAlign = m_wf.wBitsPerSample / 8 * m_wf.nChannels;
            m_wf.nAvgBytesPerSec = m_wf.nSamplesPerSec * m_wf.nBlockAlign;
			if (MMSYSERR_NOERROR == ::waveInOpen(&m_waveIn, static_cast<UINT>(deviceIndex), &m_wf,
				(DWORD_PTR)m_hWnd, 0, CALLBACK_WINDOW))
			{
                // success...do nothing
			}
		}

		if (m_waveIn != 0)
		{
			m_deviceIndex = static_cast<unsigned>(deviceIndex);
            m_channel = static_cast<unsigned>(channel);
            // one buffer is not enough. two is probably enough. so maybe 3 is better?
            unsigned sampleCount = (m_wf.nAvgBytesPerSec * recordBufferLengthMsec) / (1000 * sizeof(short));
            unsigned samplesPerBlock = m_wf.nBlockAlign / sizeof(short);
            sampleCount += samplesPerBlock-1; 
            sampleCount /= samplesPerBlock;
            sampleCount *= samplesPerBlock;
			waveInAddBuffer(m_waveIn, new RecordingBuffer(m_waveIn, sampleCount), sizeof(WAVEHDR));
			waveInAddBuffer(m_waveIn, new RecordingBuffer(m_waveIn, sampleCount), sizeof(WAVEHDR));
            waveInAddBuffer(m_waveIn, new RecordingBuffer(m_waveIn, sampleCount), sizeof(WAVEHDR));
            //waveInStart(m_waveIn); No. Instead, exit with device in paused state

            // find its volume control
            MIXERLINECONTROLSW mixerLineControls = {};
            mixerLineControls.cbStruct = sizeof(mixerLineControls);
            mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
            m_mixerControlId = {};
            m_mixerControlId.cbStruct = sizeof(m_mixerControlId);
            mixerLineControls.pamxctrl = &m_mixerControlId;
            mixerLineControls.cbmxctrl = sizeof(MIXERCONTROLW);
            mixerLineControls.cControls = 1;
            MMRESULT mmres = mixerGetLineControlsW(reinterpret_cast<HMIXEROBJ>(m_waveIn), &mixerLineControls,
                MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HWAVEIN);
            if (mmres == MMSYSERR_NOERROR)
            {
                MIXERCONTROLDETAILS	mxcd = {};
                mxcd.cbStruct = sizeof(mxcd);
                mxcd.dwControlID = m_mixerControlId.dwControlID;
                mxcd.cChannels = 1;
                MIXERCONTROLDETAILS_UNSIGNED		uValue;
                mxcd.cbDetails = sizeof(uValue);
                mxcd.paDetails = &uValue;
                mmres = mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(m_waveIn), &mxcd,
                    MIXER_OBJECTF_HWAVEIN);
                if (mmres == MMSYSERR_NOERROR)
                    m_gain = static_cast<float>(m_mixerControlId.Bounds.dwMinimum + uValue.dwValue) /
                            static_cast<float>(m_mixerControlId.Bounds.dwMaximum - m_mixerControlId.Bounds.dwMinimum);
            }
            return 1;
		}
		return 0;
	}

	LRESULT WaveDevicePlayerImpl::OnClosePlayback(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
		BOOL& /*bHandled*/)
	{
		if (!m_waveIn)
			return 0;
		waveInReset(m_waveIn);
		waveInClose(m_waveIn);
		m_waveIn = 0;
		return 0;
	}

	LRESULT WaveDevicePlayerImpl::OnWaveInData(UINT /*nMsg*/, WPARAM dev, LPARAM wave,
		BOOL& /*bHandled*/)
	{
		RecordingBuffer *pBuf = reinterpret_cast<RecordingBuffer*>(wave);
        if (pBuf)
        {
            if (m_waveIn)
            {        
                if (m_audioSink)
                {
                    const unsigned numFrames = pBuf->dwBytesRecorded / (sizeof(short) * m_wf.nChannels);
                    if (m_wf.nChannels == 1)
                    {   // if input is mono, no need to reformat
                        m_audioSink->AddMonoSoundFrames(pBuf->pSamples(), numFrames);
                        if (m_recordingFile)
                            m_recordingFile->Write(pBuf->pSamples(), numFrames );
                    }
                    else
                    {   // multi-channel input data has to be sorted through
                        std::vector<short> mono(numFrames);
                        short *p = &mono[0];
                        const short *q = pBuf->pSamples();
                        if (m_channel >= m_wf.nChannels)
                            return 0;
                        q += m_channel;
                        for (unsigned frames = 0; frames < numFrames; frames += 1)
                        {
                            *p++ = *q;
                            q += m_wf.nChannels;
                        }
                        m_audioSink->AddMonoSoundFrames(
                            reinterpret_cast<const short *>(&mono[0]), numFrames);
                        if (m_recordingFile)
                            m_recordingFile->Write(&mono[0], static_cast<unsigned>(mono.size()));
                    }
                }
                pBuf->reuse();
                waveInAddBuffer(m_waveIn, pBuf, sizeof(WAVEHDR));
            }
            else
                delete pBuf;
        }
		return 0;
	}

	bool WaveDevicePlayerImpl::Pause()
	{
		if (IsWindow())
			return SendMessage(WM_PAUSEPLAYBACK) != 0;
		return false;
	}

	LRESULT WaveDevicePlayerImpl::OnPause(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
		BOOL& /*bHandled*/)
	{
		if (m_waveIn)
		{
			waveInStop(m_waveIn);
			return 1;
		}
		return 0;
	}

	bool WaveDevicePlayerImpl::Resume()
	{
		if (IsWindow())
			return SendMessage(WM_CONTINUEPLAYBACK) != 0;
		return false;
	}

	LRESULT WaveDevicePlayerImpl::OnResume(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
		BOOL& /*bHandled*/)
	{
		if (m_waveIn)
            return MMSYSERR_NOERROR == waveInStart(m_waveIn) ? 1 : 0;
		return 0;
	}
    LRESULT WaveDevicePlayerImpl::OnStartRecording(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM fname,
        BOOL& /*bHandled*/)
    {
        m_recordingFile = std::make_shared<WaveWriterImpl>(reinterpret_cast<const wchar_t *>(fname));
        return 0;
    }

    LRESULT WaveDevicePlayerImpl::OnStopRecording(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
        BOOL& /*bHandled*/)
    {
        m_recordingFile.reset();
        return 0;
    }

    void WaveDevicePlayerImpl::RecordFile(const std::wstring &w)
    {
        if (IsWindow())
            SendMessage(WM_STARTRECORDING, 0, (LPARAM)w.c_str());
    }

    void  WaveDevicePlayerImpl::StopRecord()
    {
        if (IsWindow())
            SendMessage(WM_STOPRECORDING);
    }

    float WaveDevicePlayerImpl::GetGain()
    {
        return m_gain;
    }

    void WaveDevicePlayerImpl::SetGain(float v)
    {
        if (IsWindow())
            SendMessage(WM_SETGAIN, *reinterpret_cast<WPARAM*>(&v));
    }

    LRESULT WaveDevicePlayerImpl::OnSetGain(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/,
        BOOL& /*bHandled*/)
    {
        float gain = *reinterpret_cast<float *>(&wParam);
        if ((gain < 0) || (gain > 1))
            return 0;
        MIXERCONTROLDETAILS	mxcd = {};
        mxcd.cbStruct = sizeof(mxcd);
        mxcd.dwControlID = m_mixerControlId.dwControlID;
        mxcd.cChannels = 1;
        MIXERCONTROLDETAILS_UNSIGNED		uValue;
        mxcd.cbDetails = sizeof(uValue);
        mxcd.paDetails = &uValue;
        uValue.dwValue = static_cast<DWORD>(gain * (m_mixerControlId.Bounds.dwMaximum - m_mixerControlId.Bounds.dwMinimum));
        uValue.dwValue += m_mixerControlId.Bounds.dwMinimum;
        MMRESULT mmres = mixerSetControlDetails(reinterpret_cast<HMIXEROBJ>(m_waveIn), &mxcd,
            MIXER_OBJECTF_HWAVEIN);
        if (mmres == MMSYSERR_NOERROR)
            m_gain = gain;
        return 0;
    }

}}