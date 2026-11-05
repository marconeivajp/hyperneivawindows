#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>
#include <stdint.h>

void radio_init();
void radio_close();
void radio_play(const char* url);
void radio_stop();
int radio_read_pcm(int16_t* outSamples, int maxFrames);
bool radio_is_playing();
bool radio_is_buffering();
void radio_get_status(char* outMsg);
void radio_get_metadata(char* outTags, char* outCodec, int* outBitrate);

void radio_fetch_top50();
void radio_fetch_saopaulo();
void radio_fetch_podcast();
void radio_fetch_search(const char* query);
void radio_fetch_tag(const char* tag);
void radio_fetch_country(const char* country);
void radio_fetch_state(const char* state);
void radio_fetch_language(const char* language);
void exec_async_silent(const char* cmd);
void exec_silent(const char* cmd);

void grav_get_exe_dir(char* out, int maxLen);
bool grav_start_recording(const char* outputFile);
bool grav_screen_start_recording(const char* outputFile);
bool grav_video_start_recording(const char* outputFile);
bool grav_video_screen_start_recording(const char* outputFile);
bool grav_concat_mp4(const char* part1, const char* part2, const char* out);
bool grav_capture_screen_to_png(const char* outputPath, bool activeWindowOnly);
bool grav_capture_camera_to_png(const char* outputPath);
bool grav_print_preview_start(int source);
void grav_print_preview_stop(void);
void grav_stop_recording();
bool grav_is_recording();
long grav_temp_file_size(const char* path);
void grav_pause_recording();
void grav_resume_recording();
void grav_set_mic_mute(bool muted);
void grav_kill_orphans();
bool grav_finish_recording(const char* rawPath, const char* finalPath);
const char* grav_last_error_msg();
int  grav_video_preview_w(void);
int  grav_video_preview_h(void);
int  grav_video_preview_get(unsigned char* outBuf, int bufSize);
int  grav_video_preview_active(void);

extern char radioNames[500][128];
extern char radioUrls[500][256];
extern char radioFavicons[500][512];
extern char radioTags[500][256];
extern char radioCodecs[500][32];
extern int radioBitrates[500];
extern int radioCount;

#endif
