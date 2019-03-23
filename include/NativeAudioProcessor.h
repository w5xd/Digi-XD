#pragma once

namespace XD {
	// Optionally call a native C++ class to process
	// and present native audio to a clr client. The incoming format
	// is 16 bit integers and the outgoing format is 32 bit float.
    struct NativeAudioProcessor {
        virtual int __stdcall Initialize(void *pDecData) = 0; // returns > 0 on success
        virtual int __stdcall ProcessAudio(const short *pSamples, unsigned numSamples, const float **pToforward, unsigned *pNumToForward)=0; // return >0 to forward to clr host
        virtual void __stdcall ReleaseProcessor()=0; // free memory. do not access after this
    };
}

