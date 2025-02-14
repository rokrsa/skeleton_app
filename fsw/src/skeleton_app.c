/*******************************************************************************
**
**      GSC-18128-1, "Core Flight Executive Version 6.7"
**
**      Copyright (c) 2006-2019 United States Government as represented by
**      the Administrator of the National Aeronautics and Space Administration.
**      All Rights Reserved.
**
**      Licensed under the Apache License, Version 2.0 (the "License");
**      you may not use this file except in compliance with the License.
**      You may obtain a copy of the License at
**
**        http://www.apache.org/licenses/LICENSE-2.0
**
**      Unless required by applicable law or agreed to in writing, software
**      distributed under the License is distributed on an "AS IS" BASIS,
**      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**      See the License for the specific language governing permissions and
**      limitations under the License.
**
** File: skeleton_app.c
**
** Purpose:
**   This file contains the source code for a skeleton application that does
**   nothing and is the minimum required for a valid cFS application.
**
*******************************************************************************/

/*
** Include Files:
*/
#include "skeleton_app_events.h"
#include "skeleton_app_version.h"
#include "skeleton_app.h"

#include <string.h>

/*
** global data
*/
SKELETON_AppData_t SKELETON_AppData;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/* SKELETON_AppMain() -- Application entry point and main process loop        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void SKELETON_AppMain( void )
{
    int32  status;

    /*
    ** Perform application specific initialization
    ** If the Initialization fails, set the RunStatus to
    ** CFE_ES_RunStatus_APP_ERROR and the App will not enter the RunLoop
    */
    status = SKELETON_AppInit();
    if (status != CFE_SUCCESS)
    {
        SKELETON_AppData.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** SKELETON Runloop
    */
    while (CFE_ES_RunLoop(&SKELETON_AppData.RunStatus) == true)
    {
        /* Pend on receipt of command packet */
        status = CFE_SB_ReceiveBuffer(&SKELETON_AppData.SBBufPtr, SKELETON_AppData.CommandPipe, CFE_SB_PEND_FOREVER);

        if (status == CFE_SUCCESS)
        {
            SKELETON_ProcessCommandPacket(SKELETON_AppData.SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(SKELETON_PIPE_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "SKELETON APP: SB Pipe Read Error, App Will Exit");

            SKELETON_AppData.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }

    }

    CFE_ES_ExitApp(SKELETON_AppData.RunStatus);

} /* End of SKELETON_AppMain() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/*                                                                            */
/* SKELETON_AppInit() --  initialization                                      */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
int32 SKELETON_AppInit( void )
{
    int32    status;

    SKELETON_AppData.RunStatus = CFE_ES_RunStatus_APP_RUN;

    /*
    ** Initialize app command execution counters
    */
    SKELETON_AppData.CmdCounter = 0;
    SKELETON_AppData.ErrCounter = 0;

    /*
    ** Initialize app configuration data
    */
    SKELETON_AppData.PipeDepth = SKELETON_PIPE_DEPTH;

    strncpy(SKELETON_AppData.PipeName, "SKELETON_CMD_PIPE", sizeof(SKELETON_AppData.PipeName));
    SKELETON_AppData.PipeName[sizeof(SKELETON_AppData.PipeName) - 1] = 0;

    /*
    ** Register the events
    */
    if ((status = CFE_EVS_Register(NULL, 0, 0)) != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Skeleton App: Error Registering Events, RC = %lu\n",
                             (unsigned long)status);
        return ( status );
    }

    /*
    ** Initialize housekeeping packet (clear user data area).
    */
    CFE_MSG_Init(&SKELETON_AppData.HkBuf.TlmHeader.Msg, 
                 SKELETON_APP_HK_TLM_MID, 
                 sizeof(SKELETON_AppData.HkBuf));

    /*
    ** Create Software Bus message pipe.
    */
    status = CFE_SB_CreatePipe(&SKELETON_AppData.CommandPipe,
                               SKELETON_AppData.PipeDepth,
                               SKELETON_AppData.PipeName);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Skeleton App: Error creating pipe, RC = 0x%08lX\n",
                             (unsigned long)status);
        return ( status );
    }

    /*
    ** Subscribe to Housekeeping request commands
    */
    status = CFE_SB_Subscribe(SKELETON_APP_SEND_HK_MID,
                              SKELETON_AppData.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Skeleton App: Error Subscribing to HK request, RC = 0x%08lX\n",
                             (unsigned long)status);
        return ( status );
    }

    /*
    ** Subscribe to ground command packets
    */
    status = CFE_SB_Subscribe(SKELETON_APP_CMD_MID,
                              SKELETON_AppData.CommandPipe);
    if (status != CFE_SUCCESS )
    {
        CFE_ES_WriteToSysLog("Skeleton App: Error Subscribing to Command, RC = 0x%08lX\n",
                             (unsigned long)status);

        return ( status );
    }

    CFE_EVS_SendEvent (SKELETON_STARTUP_INF_EID,
                       CFE_EVS_EventType_INFORMATION,
                       "SKELETON App Initialized. Version %d.%d.%d.%d",
                       SKELETON_APP_MAJOR_VERSION,
                       SKELETON_APP_MINOR_VERSION,
                       SKELETON_APP_REVISION,
                       SKELETON_APP_MISSION_REV);

    return ( CFE_SUCCESS );

} /* End of SKELETON_AppInit() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  SKELETON_ProcessCommandPacket                                        */
/*                                                                            */
/*  Purpose:                                                                  */
/*     This routine will process any packet that is received on the SKELETON    */
/*     command pipe.                                                          */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
void SKELETON_ProcessCommandPacket( CFE_SB_Buffer_t *SBBufPtr )
{
    CFE_SB_MsgId_t  MsgId;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    switch (MsgId)
    {
        case SKELETON_APP_CMD_MID:
            SKELETON_ProcessGroundCommand(SBBufPtr);
            break;

        case SKELETON_APP_SEND_HK_MID:
            SKELETON_ReportHousekeeping((CFE_MSG_CommandHeader_t *)SBBufPtr);
            break;

        default:
            CFE_EVS_SendEvent(SKELETON_INVALID_MSGID_ERR_EID,
                              CFE_EVS_EventType_ERROR,
         	                  "SKELETON: invalid command packet,MID = 0x%x",
                              (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            break;
    }

    return;

} /* End SKELETON_ProcessCommandPacket */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* SKELETON_ProcessGroundCommand() -- SKELETON ground commands                */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
void SKELETON_ProcessGroundCommand( CFE_SB_Buffer_t *SBBufPtr )
{
    CFE_MSG_FcnCode_t CommandCode;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    /*
    ** Process "known" SKELETON app ground commands
    */
    switch (CommandCode)
    {
        case SKELETON_APP_NOOP_CC:
            if (SKELETON_VerifyCmdLength(&SBBufPtr->Msg, sizeof(SKELETON_Noop_t)))
            {
                SKELETON_Noop((SKELETON_Noop_t *)SBBufPtr);
            }

            break;

        case SKELETON_APP_RESET_COUNTERS_CC:
            if (SKELETON_VerifyCmdLength(&SBBufPtr->Msg, sizeof(SKELETON_ResetCounters_t)))
            {
                SKELETON_ResetCounters((SKELETON_ResetCounters_t *)SBBufPtr);
            }

            break;

        case SKELETON_APP_PROCESS_CC:
            if (SKELETON_VerifyCmdLength(&SBBufPtr->Msg, sizeof(SKELETON_Process_t)))
            {
                SKELETON_Process((SKELETON_Process_t *)SBBufPtr);
            }

            break;

        /* default case already found during FC vs length test */
        default:
            CFE_EVS_SendEvent(SKELETON_COMMAND_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "Invalid ground command code: CC = %d",
                              CommandCode);
            break;
    }

    return;

} /* End of SKELETON_ProcessGroundCommand() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  SKELETON_ReportHousekeeping                                        */
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function is triggered in response to a task telemetry request */
/*         from the housekeeping task. This function will gather the Apps     */
/*         telemetry, packetize it and send it to the housekeeping task via   */
/*         the software bus                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
int32 SKELETON_ReportHousekeeping( const CFE_MSG_CommandHeader_t *Msg )
{
    /*
    ** Get command execution counters...
    */
    SKELETON_AppData.HkBuf.Payload.CommandErrorCounter = SKELETON_AppData.ErrCounter;
    SKELETON_AppData.HkBuf.Payload.CommandCounter      = SKELETON_AppData.CmdCounter;

    /*
    ** Send housekeeping telemetry packet...
    */
    CFE_SB_TimeStampMsg(&SKELETON_AppData.HkBuf.TlmHeader.Msg);
    CFE_SB_TransmitMsg(&SKELETON_AppData.HkBuf.TlmHeader.Msg, true);

    return CFE_SUCCESS;

} /* End of SKELETON_ReportHousekeeping() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* SKELETON_Noop -- SKELETON NOOP commands                                    */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
int32 SKELETON_Noop( const SKELETON_Noop_t *Msg )
{

    SKELETON_AppData.CmdCounter++;

    CFE_EVS_SendEvent(SKELETON_COMMANDNOP_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "SKELETON: NOOP command  Version %d.%d.%d.%d",
                      SKELETON_APP_MAJOR_VERSION,
                      SKELETON_APP_MINOR_VERSION,
                      SKELETON_APP_REVISION,
                      SKELETON_APP_MISSION_REV);

    return CFE_SUCCESS;

} /* End of SKELETON_Noop */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  SKELETON_ResetCounters                                             */
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function resets all the global counter variables that are     */
/*         part of the task telemetry.                                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
int32 SKELETON_ResetCounters( const SKELETON_ResetCounters_t *Msg )
{

    SKELETON_AppData.CmdCounter = 0;
    SKELETON_AppData.ErrCounter = 0;

    CFE_EVS_SendEvent(SKELETON_COMMANDRST_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "SKELETON: RESET command");

    return CFE_SUCCESS;

} /* End of SKELETON_ResetCounters() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  SKELETON_Process                                                   */
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function Process Ground Station Command                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
int32  SKELETON_Process( const SKELETON_Process_t *Msg )
{
    return CFE_SUCCESS;

} /* End of SKELETON_ProcessCC */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* SKELETON_VerifyCmdLength() -- Verify command packet length                 */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
bool SKELETON_VerifyCmdLength( CFE_MSG_Message_t *MsgPtr, CFE_MSG_Size_t ExpectedLength )
{
    bool result = true;
    CFE_MSG_Size_t ActualLength;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);

    /*
    ** Verify the command packet length.
    */
    if (ExpectedLength != ActualLength)
    {
        CFE_SB_MsgId_t      MessageID;
        CFE_MSG_FcnCode_t   CommandCode;

        CFE_MSG_GetMsgId(MsgPtr, &MessageID);
        CFE_MSG_GetFcnCode(MsgPtr, &CommandCode);

        CFE_EVS_SendEvent(SKELETON_LEN_ERR_EID,
                          CFE_EVS_EventType_ERROR,
                          "Invalid Msg length: ID = 0x%X,  CC = %d, Len = %d, Expected = %d",
                          MessageID,
                          CommandCode,
                          ActualLength,
                          ExpectedLength);

        result = false;

        SKELETON_AppData.ErrCounter++;
    }

    return( result );

} /* End of SKELETON_VerifyCmdLength() */
