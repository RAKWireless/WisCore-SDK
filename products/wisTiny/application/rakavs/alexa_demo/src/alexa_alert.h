#ifndef __ALEXA_ALERT_H_
#define __ALEXA_ALERT_H_
#include <time.h>
#include "avs_private.h"

typedef struct tm S_AVS_scheduledTime;////	struct tm s_alertTime;

int RK_Schedule_Delete_Index(S_AVS_scheduledTime* p_scheduleds, int iIndex, int iNumScheduleds);
int RK_Schedule_Set(S_AVS_scheduledTime *schTm, char *isotime);
int RK_Schedule_Get(S_AVS_scheduledTime *schTm);
void* RK_AlertDirective_Vacate_TimeSpace(S_SetAlertDirective* const p_AlertDirectives, S_SetAlertDirective* const p_AlertInput, int const iNumDirectives);
char * RK_AlertStruct_to_Json(S_SetAlertDirective* const arrary, int num_directives);//if return value not eq NULL, it need to be free by user, when it is useless.
int RK_AlertJson_Append_to_StructFile(char* cJsonData, char* filename);
void RK_Alert_Clean_DelDirctives(S_DeleteAlertDirective* p, int len);
int RK_AlertDirective_Find_State(const S_SetAlertDirective* p_AlertDirectives, int const State, int const iNumDirectives);
void RK_Alert_ConfigFile_Initial(char * filename);
int RK_AlertDirective_Delete_Index(S_SetAlertDirective* p_AlertDirectives, int iIndex, int iNumDirectives);

#endif
