#ifndef __AUDIO_PLAYER__H___
#define __AUDIO_PLAYER__H___

int RK_Playback_setvolume(unsigned char f16Volume, void *userPtr);
int RK_Playback_pause( int pause, void *userPtr);
int RK_Playback_setvolume(unsigned char f16Volume, void *userPtr);
int RK_Playback_HardwareVolume(int i32Volume, int isSetVol);
int RK_Playback_write(unsigned char *buf, int size, void *userPtr);
int RK_Playback_state(void *userPtr);
int RK_Playback_close(void *userPtr);
int RK_Playback_open(S_AudioConf *pconf, void *userPtr);
int RK_Playback_Query_Playing_seeks(int expect_seeks, void *userPtr);
void RK_Playback_global_init(void);
int RK_Playback_create
( void **userPtr, const char* description, 
	int (*pbstart_callback)(void* hnd, void* usrpriv),    void *startusrprivdata,
	int (*pbfinished_callback)(void* hnd, void* usrpriv), void *finishusrprivdata
);

#endif	//__AUDIO_PLAYER__H___
