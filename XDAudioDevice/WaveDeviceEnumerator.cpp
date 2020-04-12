#include "stdafx.h"
#include "WaveDeviceEnumerator.h"
#include <Windows.h>
#include <mmsystem.h> //primitive old Windows wave interface...
namespace XD {

    System::Collections::Generic::List<System::String^> ^WaveDeviceEnumerator::waveInDevices()
    {
        
        System::Collections::Generic::List<System::String^> ^ret =
            gcnew System::Collections::Generic::List<System::String^>();

        const unsigned count = ::waveInGetNumDevs();
        for (unsigned i = 0; i < count; i++)
        {
            WAVEINCAPSW wc = { 0 };
            ::waveInGetDevCapsW(i, &wc, sizeof(wc));
            System::String ^name = msclr::interop::marshal_as<System::String ^>(wc.szPname);
            ret->Add(name);
        }
        return ret;
    }

    System::Collections::Generic::List<System::String^> ^WaveDeviceEnumerator::waveOutDevices()
    {
        System::Collections::Generic::List<System::String^> ^ret =
            gcnew System::Collections::Generic::List<System::String^>();
        const unsigned count = ::waveOutGetNumDevs();
        for (unsigned i = 0; i < count; i++)
        {
            WAVEOUTCAPSW wc = { 0 };
            ::waveOutGetDevCapsW(i, &wc, sizeof(wc));
            System::String ^name = msclr::interop::marshal_as<System::String ^>(wc.szPname);
            ret->Add(name);
        }
        return ret;
    }
    // from the WIndows 6 MMDDK.H
#define DRV_RESERVED                      0x0800
#define DRV_QUERYFUNCTIONINSTANCEID       (DRV_RESERVED + 17)
#define DRV_QUERYFUNCTIONINSTANCEIDSIZE   (DRV_RESERVED + 18)

    System::String^ WaveDeviceEnumerator::waveInInstanceId(int waveInIdx)
    {
        System::String ^ret = nullptr;
        size_t cbEndpointId(0);
        MMRESULT mmr = waveInMessage((HWAVEIN)IntToPtr(waveInIdx),
            DRV_QUERYFUNCTIONINSTANCEIDSIZE,
            (DWORD_PTR)&cbEndpointId, NULL);
        if (mmr == MMSYSERR_NOERROR)
        {
            std::unique_ptr<wchar_t[]>  RxInDeviceId;
            RxInDeviceId.reset(new wchar_t[cbEndpointId + 1]);
            // Get the endpoint ID string for this waveIn device.
            mmr = waveInMessage((HWAVEIN)IntToPtr(waveInIdx),
                DRV_QUERYFUNCTIONINSTANCEID,
                (DWORD_PTR)RxInDeviceId.get(),
                cbEndpointId);
            if (mmr == MMSYSERR_NOERROR)
                ret = msclr::interop::marshal_as<System::String ^>(&RxInDeviceId[0]);
        }
        return ret;
    }

    System::String^ WaveDeviceEnumerator::waveOutInstanceId(int waveOutIdx)
    {
        System::String ^ret = nullptr;
        size_t cbEndpointId(0);
        MMRESULT mmr = waveOutMessage((HWAVEOUT)IntToPtr(waveOutIdx),
            DRV_QUERYFUNCTIONINSTANCEIDSIZE,
            (DWORD_PTR)&cbEndpointId, NULL);
        if (mmr == MMSYSERR_NOERROR)
        {
            std::unique_ptr<wchar_t[]>  TxOutDeviceId;
            TxOutDeviceId.reset(new wchar_t[cbEndpointId + 1]);
            // Get the endpoint ID string for this waveOut device.
            mmr = waveOutMessage((HWAVEOUT)IntToPtr(waveOutIdx),
                DRV_QUERYFUNCTIONINSTANCEID,
                (DWORD_PTR)TxOutDeviceId.get(),
                cbEndpointId);
            if (mmr == MMSYSERR_NOERROR)
                ret = msclr::interop::marshal_as<System::String ^>(&TxOutDeviceId[0]);
        }
        return ret;
    }  
}