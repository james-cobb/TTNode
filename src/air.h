// Combined current air measurements

#ifndef AIR_H__
#define AIR_H__

bool s_air_upload_needed(void *);
void s_air_measure(void *);
void s_air_done_settling();
void s_air_poll(void *g);
bool s_air_init();
bool s_air_term();

#endif // OPC_H__
