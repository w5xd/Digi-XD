#pragma once
#include <string>
#include <thread>
#include <memory>
#include <windows.h>
#include <mmsystem.h> //primitive old Windows wave interface...
#include <atlbase.h>
#include <atlwin.h>
//...but it shares access and does rate conversions for us...
namespace XD {
	struct AudioSink;
    namespace impl {
        const UINT WM_DESTROYWAVEWINDOW = WM_USER + 1;
        const UINT WM_STARTPLAYBACK = WM_USER + 2;
        const UINT WM_STOPPLAYBACK = WM_USER + 3;
        const UINT WM_PAUSEPLAYBACK = WM_USER + 4;
        const UINT WM_CONTINUEPLAYBACK = WM_USER + 5;
        const UINT WM_STARTRECORDING = WM_USER + 6;
        const UINT WM_STOPRECORDING = WM_USER + 7;
        const UINT WM_SETGAIN = WM_USER + 8;

        class WaveWriterImpl;

        class WaveDevicePlayerImpl : public ATL::CWindowImpl<WaveDevicePlayerImpl>
        {
        public:
            WaveDevicePlayerImpl();
            ~WaveDevicePlayerImpl();
            bool Open(unsigned deviceIndex, unsigned channel, void *demod);
            bool Pause();
            bool Resume();
            void RecordFile(const std::wstring &w);
            void StopRecord();
            float GetGain();
            void SetGain(float);
            DECLARE_WND_CLASS(_T("XDft8WaveDevicePlayer"))
        protected:
            BEGIN_MSG_MAP(WaveDevicePlayerImpl)
                MESSAGE_HANDLER(WM_STARTPLAYBACK, OnOpenPlayback)
                MESSAGE_HANDLER(WM_STOPPLAYBACK, OnClosePlayback)
                MESSAGE_HANDLER(MM_WIM_DATA, OnWaveInData)
                MESSAGE_HANDLER(WM_PAUSEPLAYBACK, OnPause)
                MESSAGE_HANDLER(WM_CONTINUEPLAYBACK, OnResume)
                MESSAGE_HANDLER(WM_STARTRECORDING, OnStartRecording)
                MESSAGE_HANDLER(WM_STOPRECORDING, OnStopRecording)
                MESSAGE_HANDLER(WM_SETGAIN, OnSetGain)
            END_MSG_MAP()
            // Handler prototypes:
            //  LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

            LRESULT OnOpenPlayback(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnClosePlayback(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnWaveInData(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnPause(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnResume(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnStartRecording(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnStopRecording(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);
            LRESULT OnSetGain(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                BOOL& /*bHandled*/);

            void Close();
            void threadHead();

			std::shared_ptr<AudioSink> m_audioSink;
            WAVEFORMATEX m_wf;
            HWAVEIN m_waveIn;
            MIXERCONTROLW m_mixerControlId;
            HANDLE m_started;
            DWORD m_threadId;
            unsigned m_deviceIndex;
            unsigned m_channel;
            float m_gain;
            std::shared_ptr<WaveWriterImpl> m_recordingFile;
            std::thread m_windowThread;// initialize last
        };
    }
}