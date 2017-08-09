#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cJSON/cJSON.h>
#include "alexa_alert.h"

char * RK_AlertStruct_to_Json(S_SetAlertDirective* const arrary, int num_directives)//if return value not eq NULL, it need to be free by user, when it is useless.
{
	char * pJsonData = NULL;
	switch((arrary && 1)){
		case 0:{
			//ALEXA_INFO("---->>>> S_SetAlertDirectiveSet* p_struct == NULL ---->>>>by yan");
			pJsonData = NULL;
		}
		break;
		case 1:{
			cJSON * cJsonRoot = cJSON_CreateArray();
			cJSON *sAlertDirective = NULL;
			int iIndex;
			for( iIndex = 0; iIndex < num_directives; iIndex++){
				if(arrary[iIndex].eScheduledState == eSCH_STATE_IDLE)break;
				cJSON_AddItemToArray(cJsonRoot, sAlertDirective = cJSON_CreateObject() );
				char *eScheduledState = NULL;
				switch(arrary[iIndex].eScheduledState){
					case eSCH_STATE_PREPARE:       eScheduledState = "eSCH_STATE_PREPARE";       break;
					case eSCH_STATE_WAIT_SUBMIT:   eScheduledState = "eSCH_STATE_WAIT_SUBMIT";   break;
					case eSCH_STATE_SUBMIT:        eScheduledState = "eSCH_STATE_SUBMIT";        break;
					case eSCH_STATE_ALERT_REACH:   eScheduledState = "eSCH_STATE_ALERT_REACH";   break;
					case eSCH_STATE_ALERT_REACHED: eScheduledState = "eSCH_STATE_ALERT_REACHED"; break;
					case eSCH_STATE_ALERTING:      eScheduledState = "eSCH_STATE_ALERTING";      break;
					case eSCH_STATE_STOP:          eScheduledState = "eSCH_STATE_STOP";          break;
					default: break;
				}
				cJSON_AddStringToObject(sAlertDirective, "eScheduledState",  eScheduledState);
				cJSON_AddStringToObject(sAlertDirective, "strNamespace",     arrary[iIndex].strNamespace);
				cJSON_AddStringToObject(sAlertDirective, "strName",          arrary[iIndex].strName);
				cJSON_AddStringToObject(sAlertDirective, "strMessageId",     arrary[iIndex].strMessageId);
				cJSON_AddStringToObject(sAlertDirective, "strToken",         arrary[iIndex].strToken);
				cJSON_AddStringToObject(sAlertDirective, "strType",          arrary[iIndex].strType);
				cJSON_AddStringToObject(sAlertDirective, "strScheduledTime", arrary[iIndex].strScheduledTime);
			}
			pJsonData = cJSON_Print(cJsonRoot);
			cJSON_Delete(cJsonRoot);
		}
		break;
	}
	return pJsonData;
	
}

int RK_AlertJson_Append_to_StructFile(char* cJsonData, char* filename)
{
	if(cJsonData == NULL || filename == NULL){
		//ALEXA_INFO("---->>>>Alert---cJsonData || configfilename== NULL----by yan\n");
		return -1;
	}
	int ret = 0;
	FILE* fd = fopen(filename, "ab");
	switch((int)(fd && 1)){
		case 0: ret = -1; break;
		default :
			ret = (fprintf(fd, "\n%s\n", cJsonData) >= 0 ) ? fflush(fd) : -1;
	}
	fclose(fd);
	return ret;
}

void RK_Alert_ConfigFile_Initial(char * filename)
{
	char cmd[128]={0};
	sprintf(cmd, "rm -rf %s", filename);
	system(cmd);
}

void RK_Alert_Clean_SetDirctives(S_SetAlertDirective* p, int len)
{
	if(p)
		memset(p, 0, sizeof(S_SetAlertDirective)*len);
	else 
		printf("---->>>>SetAlertDirective pointer is NULL ---->>>>by yan");
}


void RK_Alert_Clean_DelDirctives(S_DeleteAlertDirective* p, int len)
{
	if(p)
		memset(p, 0, sizeof(S_DeleteAlertDirective)*len);
	else 
		printf("---->>>>DelAlertdirective pointer is NULL ---->>>>by yan");
}


int RK_AlertDirective_Find_State(const S_SetAlertDirective* p_AlertDirectives, int const State, int const iNumDirectives)
{
	if(p_AlertDirectives == NULL){
		printf("---->>>>Alert---S_SetAlertDirective* p_AlertDirectives == NULL----by yan\n");
		return -1;
	}
	int iIndex;
	for(iIndex = 0; iIndex < iNumDirectives; iIndex++)
	{
		if(p_AlertDirectives[iIndex].eScheduledState == eSCH_STATE_IDLE)break;
		if(p_AlertDirectives[iIndex].eScheduledState == State){
			return iIndex;
		}
	}
	printf("---->>>>Alert----RK_AlertDirective_Find_State not find the state---by yan\n");
	return -1;
}

int RK_AlertDirective_Delete_Index(S_SetAlertDirective* p_AlertDirectives, int iIndex, int iNumDirectives)
{
	if(p_AlertDirectives == NULL){
		printf("---->>>>Alert---S_AVS_scheduledTime* p_scheduleds == NULL----by yan\n");
		return -1;
	}
	memmove(p_AlertDirectives + iIndex, p_AlertDirectives + iIndex + 1, ( iNumDirectives - iIndex -1 ) * sizeof(S_SetAlertDirective));
	memset(p_AlertDirectives + iNumDirectives -1, 0, sizeof(S_SetAlertDirective));
	return 0;
}

int RK_AlertDirective_Delete_State(S_SetAlertDirective* p_AlertDirectives, const int State, const int iNumDirectives)
{
	if(p_AlertDirectives == NULL){
		printf("---->>>>Alert---S_SetAlertDirective* p_AlertDirectives == NULL----by yan\n");
		return -1;
	}
	int iIndex, iSucceeded = -1;
	for(iIndex = 0; iIndex < iNumDirectives; iIndex++){
		if(p_AlertDirectives[iIndex].eScheduledState == eSCH_STATE_IDLE)break;
		if(p_AlertDirectives[iIndex].eScheduledState == State){
			int tmpSucceeded = RK_AlertDirective_Delete_Index(p_AlertDirectives, iIndex, iNumDirectives);
			iSucceeded = (iSucceeded == 0)? 0: tmpSucceeded;
		}
	}
	printf("---->>>>Alert----RK_AlertDirective_Find_State not find the state---by yan\n");
	return iSucceeded;
}

void* RK_AlertDirective_Vacate_TimeSpace(S_SetAlertDirective* const p_AlertDirectives, S_SetAlertDirective* const p_AlertInput, int const iNumDirectives)
{
	if((p_AlertDirectives == NULL) || (p_AlertInput == NULL)){
		printf("Alert---S_AVS_scheduledTime* p_scheduleds or S_SetAlertDirective* p_AlertInput == NULL----by yan\n");
		return NULL;
	}
	int iInsertIndex = -1, iLen = iNumDirectives, iSameTokenIndex = -1;
	int iIdx;
	int iDiff = -1;//two alert is diffrent, !=0 : diffrent
	p_AlertInput->bSetAlertSucceed = 0;
	
	printf("%s %d Expected ScheduledTime be set: %s \n", __func__, __LINE__, p_AlertInput->strScheduledTime);
	for(iIdx = 0; iIdx < iNumDirectives; iIdx++){
		if(p_AlertDirectives[iIdx].eScheduledState != eSCH_STATE_IDLE){
			if(iDiff != 0){
				if(!strcmp(p_AlertInput->strToken, p_AlertDirectives[iIdx].strToken)){
					iDiff = 0;
					iSameTokenIndex = iIdx;
					p_AlertInput->bSetAlertSucceed = 2;
					return (p_AlertDirectives + iSameTokenIndex);
				}
			}
			if(iInsertIndex == -1){
				if(strcmp(p_AlertInput->strScheduledTime, p_AlertDirectives[iIdx].strScheduledTime) < 0){
					iInsertIndex = iIdx;
				}
			}
//			iInsertIndex = (iInsertIndex == -1)?((strcmp(p_AlertInput->strScheduledTime, p_AlertDirectives[iIdx].strScheduledTime) < 0)? iIdx : -1): iInsertIndex;
		}else{
			printf("%s %d alerts numbers - %d\n",__func__, __LINE__, iIdx);
			break;
		}
	}
	iLen = iIdx;
	(iInsertIndex == -1) ? iInsertIndex = iIdx : 0;
	if(iLen >= iNumDirectives){
		p_AlertInput->bSetAlertSucceed = 0;
		printf("The schedule timer is fully!\n");//SetAlert failed
		return NULL;
	}
	p_AlertInput->bSetAlertSucceed = 1;
	S_SetAlertDirective * psDirectiveSetAlert = p_AlertDirectives + iInsertIndex;
	memmove(psDirectiveSetAlert + 1, psDirectiveSetAlert, (iLen - iInsertIndex) * sizeof(S_SetAlertDirective));
	memset(psDirectiveSetAlert, 0, sizeof(S_SetAlertDirective));
	psDirectiveSetAlert->eScheduledState = eSCH_STATE_PREPARE;
	return psDirectiveSetAlert;
}

int RK_Schedule_Delete_Index(S_AVS_scheduledTime* p_scheduleds, int iIndex, int iNumScheduleds)
{
	if(p_scheduleds == NULL){
		printf("---->>>>Alert---S_AVS_scheduledTime* p_scheduleds == NULL----by yan\n");
		return -1;
	}
	memmove(p_scheduleds + iIndex, p_scheduleds + iIndex + 1, ( iNumScheduleds - iIndex -1 ) * sizeof(S_AVS_scheduledTime));
	memset(p_scheduleds + iNumScheduleds -1, 0, sizeof(S_AVS_scheduledTime));
	return 0;
}

int RK_Schedule_Set(S_AVS_scheduledTime *schTm, char *isotime)//0
{
	if((isotime == NULL) || (schTm == NULL))
		return -1;
	
	printf("%s %d ISO Time:%s\n", __func__, __LINE__, isotime);
	
	struct tm *stm =schTm;
	time_t t_AlertingTime;
	
	if( sscanf (isotime, "%d-%d-%dT%d:%d:%d", &stm->tm_year, &stm->tm_mon, &stm->tm_mday, \
											&stm->tm_hour, &stm->tm_min, &stm->tm_sec) == 6)
	{
		stm->tm_year -= 1900;
		stm->tm_mon  -= 1;
		stm->tm_isdst	= 0;
		t_AlertingTime = timegm(stm);//it can adjust .tm_wday, .tm_yday
	}else{
		printf("%s %d:Schedule_set date is failed!\n", __func__, __LINE__);
		return -1;
	}
	printf("endtime(GM):%lds\n", t_AlertingTime);
	return 0;
}

int RK_Schedule_Get(S_AVS_scheduledTime *schTm)//return >=0,time is up; otherwise, time is not up.
{
	if(!schTm){
		printf("---->>>> schTm is NULL error ---->>>>by yan\n");
		return -1;
	}
	time_t tNowTime = time(NULL);
	time_t tSchTime = timegm(schTm);
	int    iTimeOut = tNowTime - tSchTime;
	iTimeOut = (tSchTime ? iTimeOut : -iTimeOut);//when tSchTime == 0, s_alertTime may be occure error
	return iTimeOut;
}
