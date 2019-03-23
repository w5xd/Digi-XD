#include "RxAudio.h"

#include <vector>

/* This demostrates how to interface a native C++ object that
** processes the audio that is also handed to the FT8 decoder. This processor
** is delivered the audio on its ProcessAudio method. Returning zero or
** less ends the processing--XDft8.dll won't forward it to any clr client
** that registered a delegate.
**
** Returning greater than zero tells XDft8.dll to forward the samples
** at pTofoward (counted at pNumToForward).
**
** This example simply converts the incoming 16 bit signed to 32 bit float
** sample for sample.
**
** XDft8Test doesn't use the samples. Some other client may use them some
** other way. For example, if ProcessAudio calculates a Fourier transform,
** it can forward the transformed data. Your customization process as a
** developer involves two things:
** a) create your own native DLL that complies with the interface in
** NativeAudioProcessor.h
** b) create your own .NET application that calls, first, your
**    dll from (a) on its GetAudioProcessor and then
**    calls XDft8.Demodulator.SetAudioSamplesCallback with the result.
**
** There is no requirement that ProcessAudio return greater than zero.
** Returning zero or less means that the .NET application doesn't need
** the results. Presumably the results are used for something else
** in your application.
*/

class AudioProcessor : public XD::NativeAudioProcessor
{
public:
	// Inherited via NativeAudioProcessor
	int __stdcall Initialize(void * pDecData) override
	{
		return 1;
	}

	// all we do is convert to float
	int __stdcall ProcessAudio(const short * pSamples, unsigned numSamples, 
		const float ** pToforward, unsigned * pNumToForward) override
	{
		if (numSamples == 0)
			return 0;
		if (m_samples.size() < numSamples)
			m_samples.resize(numSamples);
		float *pOut = &m_samples[0];
		for (unsigned i = 0; i < numSamples; i++)
			*pOut++ = *pSamples++ * 1.f / 0x7FFF;
		*pNumToForward = numSamples;
		*pToforward = &m_samples[0];
		return 1;
	}

	void __stdcall ReleaseProcessor() override
	{
		delete this;
	}

private:
	std::vector<float> m_samples;
};

XD::NativeAudioProcessor *GetAudioProcessor()
{
	return new AudioProcessor();
}
