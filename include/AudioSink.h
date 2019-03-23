#pragma once
namespace XD {
	struct AudioSink {
		virtual bool __stdcall AddMonoSoundFrames(const short *p, unsigned frameCount) = 0;
		virtual void __stdcall AudioComplete() = 0; // end of wav file playback
		virtual void __stdcall ReleaseSink() = 0; // free memory. do not access after this.
	};

}
