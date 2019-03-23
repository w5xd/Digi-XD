#include "stdafx.h"
#include "XDftRxAudioProcess.h"
#include "RxAudio.h"
/* In XDft8Test project, this assembly does nothing but
** pass audio buffers around without processing them.
** Not useful for the end users, but your app might need
** to use the audio stream.
*/
namespace XDft8RxAudioProcess {
	System::IntPtr RxAudio::GetAudioProcessor()
	{
		return System::IntPtr(::GetAudioProcessor());
	}
}