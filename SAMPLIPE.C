/*****************************************************************************
 *
 *  SAMPLIPE will create try to create a named pipe from argv[1],  start 
 *          IMHAPI.EXE and send commands to and receive responses from 
 *          IMHAPI.  Enter "closepipe" to stop both SAMPLIPE and IMHAPI.
 *          SAMPLIPE, it is hoped, will demonstrate how IMHAPI works
 *          with a named pipe.
 *
 *          IMHAPI is a program which allows a user to issue EHLLAPI
 *          commands either interactively (from command line prompt or
 *          via a pipe) or from a file of commands.
 *             
 *          IMHAPI.DOC should be included with this file set.  Please
 *          consult that for more information on IMHAPI.
 *
 *          PLEASE NOTE:  This program is offered with NO warranties
 *          or guarrantees of ANY kind, either expressed or implied.
 *          Use of this program and any accompanying programs or materials 
 *          is entirely at the risk of the user of those programs and 
 *          materials.  The set of archived files and programs, of which 
 *          this is a member, is offered as an means of sharing information 
 *          and techniques about aspects of OS/2 programming and usage.
 *          If there are any questions about this program or materials,
 *          please contact the author (address below).
 *             
 *          I really would appreciate your comments on this program and 
 *          its general level of usefulness.  I can be reached at:
 *             
 *                          Paul Firgens
 *                          461 Glenwood Heights
 *                          Wisconsin Rapids, WI 54494-6264
 *                          CompuServ Member ID: 73577,1234
 *             
 *          Thanks, in advance!
 *             
 *              Copyright (c) 1991, Paul Firgens, All Rights Reserved
 *
 *
 *****************************************************************************
 */
 
#define INCL_BASE

#define INCL_DOSNMPIPES
#define INCL_DOSFILEMGR
#define INCL_ERRORS
#define INCL_DOSPROCESS

#include <os2.h>
#include <dos.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <doscalls.h>
#include <ctype.h>

#define COUNT_OUTBUF    4096
#define COUNT_INBUF     4096
#define DEF_TIMEOUT     5000L
#define READ_BUFFER     4096

#define PIPE_ACCESS_DUPLEX  0x0002
#define PIPE_ACCESS_IN      0x0000
#define PIPE_ACCESS_OUT     0x0001
#define PIPE_WRITEBEHIND    0x0000
#define PIPE_NOWRITEBEHIND  0x4000
#define PIPE_INHERIT        0x0000
#define PIPE_NOINHERIT      0x0080
#define PIPE_WAIT           0x0000
#define PIPE_NOWAIT         0x8000
#define PIPE_BYTE_MODE      0x0000
#define PIPE_STREAM_MODE    0x0400
#define PIPE_BYTE_READ      0x0000
#define PIPE_STREAM_READ    0x0100


extern VOID ShowMyError(VOID);
extern int PipeErrorMessage(USHORT);

main(int argc, char *argv[])
{
 
    unsigned    rc, arglen, c; 
    PSZ         pszReadBuf, pszGotten, pszReceivedBuf;
    HPIPE       hp;
    SHORT       sBytesRead, sBytesReceived; 
    USHORT      cbBytesWritten;
    CHAR        chFailName[100];
    RESULTCODES rescResults;
    char        argbuf[256];

    rc = c = 0; 
    
    /* check to see that a pipe name parm is passed on the command line */
    if(argc != 2) 
    {
        puts("SAMPLIPE: Enter SAMPLIPE 'pipename', to start IMHAPI.EXE, connect\n\
        it to that named pipe and prompt for commands.  The form of a\n\
        named pipe 'pipename' is '\\pipe\\user_supplied_name'.");
        DosExit(EXIT_PROCESS,2);
    }

    /* Explain what's happening and then create a named pipe... */
    printf("SAMPLIPE creates the named pipe %s, starts IMHAPI.EXE and\n", argv[1]);
    puts(" sends commands and receives responses from IMHAPI.  Enter \"closepipe\" to");
    puts(" stop both SAMPLIPE and IMHAPI.  See IMHAPI.DOC for more information on");
    puts(" IMHAPI/EHLLAPI commands.");

    if((rc = DosMakeNmPipe(argv[1],
            &hp,
            PIPE_NOWRITEBEHIND | PIPE_NOINHERIT | PIPE_ACCESS_DUPLEX, 
            PIPE_NOWAIT | PIPE_STREAM_MODE | PIPE_STREAM_READ | 1,
            COUNT_OUTBUF, COUNT_INBUF, DEF_TIMEOUT)))
    {
        PipeErrorMessage(rc);
        free(argv[1]);
        DosExit(EXIT_PROCESS,2);
    }
    
    /* create the buffer that IMHAPI would expect to obtain if started from the 
     * the command line: argv[0] = 'IMHAPI' and argv[1] = pipename (which is
     * argv[1] from the start of SAMPLIPE) and then start IMHAPI...
     */
    arglen = strlen(argv[1]);
    memmove(argbuf, "IMHAPI", 6);
    memmove((char *)&(argbuf[7]), argv[1], arglen);
   
    rc = DosExecPgm(chFailName, 
        sizeof(chFailName), 
        2, 
        argbuf, 
        0, 
        &rescResults, 
        "imhapi.exe"); 

     if(rc)
     {
         printf("DosExecPgm failed with return code: %u\n", rc);
         DosClose(hp);
         DosExit(EXIT_PROCESS, 8);
     }
    
    /* keep trying to connect until IMHAPI is going, 
     * but only give it 20 chances to start up
     */
    do
    {
        rc = DosConnectNmPipe(hp);
        if (rc > 0)
        {
            PipeErrorMessage(rc);
            DosSleep(0500l); 
            c++;
        }
    }
    while ( (c<20) && rc ); 
    
    /* couldn't connect within the limit set.  We better quit */
    if(c >= 20)
    {
        fprintf(stderr, 
            "SAMPLIPE: Unable to Connect to Named Pipe %s.  Program aborted.\n",
            argv[1]);
        DosExit(EXIT_PROCESS,0);
    }

    /* let us know we made contact */
    puts("Connection made...");
    
    /* alloc space for the input and output buffers */    
    if(!(pszReadBuf = (PSZ)calloc(READ_BUFFER, 1)))
    {
        printf("Cannot allocate Read Buffer for STDIN");
        DosExit(EXIT_PROCESS,2);
    }
    if(!(pszReceivedBuf = (PSZ)calloc(READ_BUFFER, 1)))
    {
        printf("Cannot allocate Received Buffer for STDOUT");
        DosExit(EXIT_PROCESS,2);
    }
    
    /* issue prompt */
    VioWrtTTY("ToPipe >>", 9, 0);
    
    /* loop...getting commands, sending them the pipe, receiving the response,
     *        displaying the response until 'closepipe' command issued
     */
    for(pszGotten = gets(pszReadBuf); 
        pszGotten && (memcmp(pszReadBuf, "closepipe", 9) != 0);
        pszGotten = gets(pszReadBuf))
    { 
        sBytesRead = strlen(pszGotten);
        rc = DosTransactNmPipe(hp, 
            pszReadBuf, 
            sBytesRead, 
            pszReceivedBuf,
            4096,
            &sBytesReceived);
        if (rc)
            PipeErrorMessage(rc);
        VioWrtTTY("FromPipe << ", 12, 0);
        rc = DosWrite(1, pszReceivedBuf, sBytesReceived, &cbBytesWritten);
        VioWrtTTY("\r\nToPipe >>", 11, 0);
        
    }

    /* when we're done, release the buffers we've used, 
     * close the Named Pipe and exit the pgm
     */
    free(pszReadBuf);
    free(pszReceivedBuf);
    DosClose(hp);
    DosExit(EXIT_PROCESS,0);

}


int PipeErrorMessage(USHORT rc)
{
    switch(rc) 
     {
    case (ERROR_INVALID_PARAMETER):
        printf("*** Error *** Invalid Pipe Parameter!\n");
        break;
    case (ERROR_NOT_ENOUGH_MEMORY):
        printf("*** Error *** Insufficient Memory to Open Pipe!\n");
        break;
    case (ERROR_OUT_OF_STRUCTURES):
        printf("*** Error *** Pipe Not Opened - Out of Structures!\n");
        break;
    case (ERROR_PATH_NOT_FOUND):
        printf("*** Error *** Pipe Not Opened - Path Not Found!\n");
        break;
    case (ERROR_PIPE_BUSY):
        printf("*** Error *** Pipe Busy!\n");
        break;
    case(ERROR_ACCESS_DENIED):
        printf("*** Error *** Access Denied!\n");
        break;
    case(ERROR_BROKEN_PIPE):
        printf("*** Error *** Broken Pipe!\n");
        break;
    case(ERROR_INVALID_HANDLE):
        printf("*** Error *** Invalid Handle!\n");
        break;
    case(ERROR_LOCK_VIOLATION):
        printf("*** Error *** Lock Violation!\n");
        break;
    case(ERROR_NOT_DOS_DISK):
        printf("*** Error *** Not DOS Disk!\n");
        break;
    case(ERROR_BAD_PIPE):
        printf("*** Error *** Bad Pipe!\n");
        return(2);
    case(ERROR_INTERRUPT):
        printf("*** Error *** Interrupt Waiting for Pipe!\n");
        return(2);
    case(ERROR_SEM_TIMEOUT):
        printf("*** Error *** Time Out Waiting for Pipe!\n");
        return(2);
    case(ERROR_INVALID_FUNCTION):
        printf("*** Error *** Invalid Function!\n");
        break;
    case(ERROR_PIPE_NOT_CONNECTED):
        printf("*** Error *** Not Connected!\n");
        break;
    default:
        printf("*** Unknown Pipe Error ***!\n");
        printf("Error code: %u.\n", rc);
        break;
    }
    return(0);
}

