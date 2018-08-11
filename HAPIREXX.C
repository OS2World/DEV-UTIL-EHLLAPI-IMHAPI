/*****************************************************************************
 *
 *  HAPIREXX.C creates a DLL for use with REXX.  Specifically, HapiRexx 
 *             contains two external REXX functions, MkHaPipe and ToHapi
 *             to create a named pipe, start IMHAPI and commnunicate to
 *             IMHAPI over that pipe.
 *
 *             While REXX can work directly with named pipes (via 'stream()',
 *             ' linein()' and 'lineout()' functions), it can't (as far as 
 *             I know) create a pipe.  If that's possible I'd appreciate
 *             learning how.  Otherwise, since IMHAPI (in version 1.0)
 *             can't create one either, we're forced to have an external
 *             function (MkHaPipe) create a pipe and then use
 *             another function use it.  If it were possible to have REXX 
 *             functions use the pipe created by MkHaPipe, it would be great
 *             but I couldn't get that to work either.  So, again, another
 *             external function is needed, ToHapi.  If there is a means
 *             to get around these REXX limitations, please share it with
 *             me.  My electronic and physical addresses are listed below. 
 *             
 *             MkHaPipe creates a named pipe (from the parameter it is passed), 
 *             then starts IMHAPI.EXE, pointing IMHAPI to the newly created pipe
 *             and, if successful, exits, passing back the pipe handle number 
 *             for that named pipe.  
 *             
 *             ToHapi engages in a converation with IMHAPI via the
 *             pipe created by MkHaPipe.  ToHapi expects two parameters, 
 *             the pipe handle number passed back by MkHapiRexx and the 
 *             command intended for IMHAPI.  It waits for a response and 
 *             returns the result to the REXX program which called it.
 *             
 *             IMHAPI is a program which allows a user to issue EHLLAPI
 *             commands either interactively (from command line prompt or
 *             via a pipe) or from a file of commands.
 *             
 *             IMHAPI.DOC should be included with this file set.  Please
 *             consult that for more information on IMHAPI.
 *             
 *             The HAPIREXX.DLL file must be copied into a file that is
 *             in the LIBPATH concatenation defined in the CONFIG.SYS file 
 *             of a user's machine.
 *             These functions must also be registered with the REXX program
 *             which intends to use them.  That is accomplished with the 
 *             REXX statements:
 *                  CALL RxFuncAdd 'MkHaPipe',     'HAPIREXX', 'MKHAPIPE'
 *                  CALL RxFuncAdd 'ToHapi',       'HAPIREXX', 'TOHAPI'  
 *             MkHaPipe must be called before ToHapi is used and ToHapi 
 *             must use a handle suppiled by MkHaPipe or errors will result!
 *             The pipe and IMHAPI process can be closed by calling ToHapi with 
 *             the command 'closepipe' as the second parameter.
 *
 *             MkHaPipe can return the following messages:
 *               MKPIPERR: Invalid Pipe Parameter.
 *               MKPIPERR: Not Enough Memory to Open Pipe.
 *               MKPIPERR: Pipe Unopened - Out of Structures.
 *               MKPIPERR: Pipe Unopened - Path Not Found.
 *               MKPIPERR: Pipe Busy.
 *               MKPIPERR: Unknown Pipe Error: <OS/2 Error Code>
 *               CREPGMER: Unable to start IMHAPI.EXE, request failed with 
 *                         return code: <OS/2 Error Code>
 *               NOCONECT: Unable to establish connection to IMHAPI via the pipe.
 *
 *             ToHapi can return the following messages:
 *               INCOPARM: ToHapi requires two parameters.
 *               NOTNUMER: ToHapi requires a numeric pipe handle for its first parameter.
 *               TRPIPERR: Bad Pipe
 *               TRPIPERR: Interrupt Waiting for Pipe
 *               TRPIPERR: Time Out Waiting for Pipe
 *               TRPIPERR: Invalid Function
 *               TRPIPERR: Unknown Pipe Error: <OS/2 Error Code>
 *             
 *
 *             PLEASE NOTE:  This program is offered with NO warranties
 *             or guarrantees of ANY kind, either expressed or implied.
 *             Use of this program and any accompanying programs or materials 
 *             is entirely at the risk of the user of those programs and 
 *             materials.  The set of archived files and programs, of which 
 *             this is a member, is offered as an means of sharing information 
 *             and techniques about aspects of OS/2 programming and usage.
 *             If there are any questions about this program or materials,
 *             please contact the author (address below).
 *             
 *             I really would appreciate your comments on this program,
 *             its operation with REXX and its general level of usefulness.
 *             I can be reached at:
 *             
 *                          Paul Firgens
 *                          461 Glenwood Heights
 *                          Wisconsin Rapids, WI 54494-6264
 *                          CompuServ Member ID: 73577,1234
 *             
 *             Thanks, in advance!
 *             
 *              Copyright (c) 1991, Paul Firgens, All Rights Reserved
 *
 *
 *****************************************************************************
 *             
 *  This program was developed using the Lattice C compiler, ver. 6.05.
 *  I know that's more than a little unusual, but it's all I had access to.
 *  Basically, I found it worked fine, although with no other OS/2 C 
 *  compiler to compare it to, a complete judgement is hard to provide.
 *  At any rate, Lattice provides versions of the STRING.H functions
 *  which specifically require far pointers as arguments and I chose to use
 *  some of those (_fmemcpy, _fmemmove and _fstrcpy) for this program.
 *  The '__private' keyword helps us follow Microsoft conventions
 *  for OS/2 programs, apparently.  According to the Lattice docs, 
 *  __private tells the compiler to initialize a DLL function's DS 
 *  register to the DLL's DGROUP.
 *  Since REXX and DLLs need these features, I used them here, yet used a 
 *  '#define' to help state the function calls in forms that other C 
 *  compilers could also use.  Hope it helps...
 *             
 *****************************************************************************
 */

#if LATTICE
#define memcpy  _fmemcpy
#define memmove _fmemmove
#define strcpy  _fstrcpy
#define __APIENTRY __private APIENTRY 
#else
#define __APIENTRY APIENTRY 
#endif
   
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
#include <_string.h>
#include <ctype.h>
#include <rexxsaa.h>

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


/*****************************************************************************
 *
 * MkHaPipe: create a pipe, start an async process and return the handle
 *           which points to the pipe.  The handle is returned as    
 *           a numeric character rather than as a binary integer.       
 *
 *****************************************************************************
 */

SHORT __APIENTRY  MkHaPipe(
      PSZ       name,             /* name of function       */
      USHORT    argc,             /* argument count         */
      PRXSTRING argv,             /* argument list          */
      PSZ       QueueName,        /* current active queue   */
      PRXSTRING retval)           /* return value           */

{

    short   rc, c; 
    HPIPE   hp;    
    char    pdream[255];
    char    smsg[8];
    CHAR        chFailName[100];
    RESULTCODES rescResults;
    
    rc = c = 0;
    memset(retval->strptr, '\0', 249);  /* NULL out the area from REXX */

    /* string together the command line that IMHAPI expects to find */
    memset(pdream, '\0', 255);
    memmove(pdream, "IMHAPI", 6);
    memmove((PSZ)&pdream[7], argv[0].strptr, (USHORT)argv[0].strlength); 
        
    /* Create a named pipe... */
    if((rc = DosMakeNmPipe((PSZ)&pdream[7],
            &hp,
            PIPE_NOWRITEBEHIND | PIPE_NOINHERIT | PIPE_ACCESS_DUPLEX, 
            PIPE_NOWAIT | PIPE_STREAM_MODE | PIPE_STREAM_READ | 1,
            COUNT_OUTBUF, COUNT_INBUF, DEF_TIMEOUT)))
    {   
        switch(rc) 
        {
        case (ERROR_INVALID_PARAMETER):
            sprintf(retval->strptr, "MKPIPERR: Invalid Pipe Parameter.");
            break;
        case (ERROR_NOT_ENOUGH_MEMORY):
            sprintf(retval->strptr, "MKPIPERR: Not Enough Memory to Open Pipe.");
            break;
        case (ERROR_OUT_OF_STRUCTURES):
            sprintf(retval->strptr, "MKPIPERR: Pipe Unopened - Out of Structures.");
            break;
        case (ERROR_PATH_NOT_FOUND):
            sprintf(retval->strptr, "MKPIPERR: Pipe Unopened - Path Not Found.");
            break;
        case (ERROR_PIPE_BUSY):
            sprintf(retval->strptr, "MKPIPERR: Pipe Busy.");
            break;
        default:
            sprintf(retval->strptr, "MKPIPERR: Unknown Pipe Error: %u.", rc);
            break;
        }
    }
    else  
    {
        rc = 0;
        /* start IMHAPI to process messages from the pipe */
        rc = DosExecPgm(chFailName, 
            sizeof(chFailName), 
            2, 
            pdream, 
            0, 
            &rescResults, 
            "imhapi.exe"); 

        /* if the DosExecPgm call fails tell the REXX pgm about it and close the
         * pipe
         */
        if(rc)
        {
            sprintf(retval->strptr, "CREPGMER: Unable to start IMHAPI.EXE, request failed with return code: %u\n", rc);
            retval->strlength = strlen(retval->strptr);
            DosClose(hp);
            return(0);
        }
        
        /* once IMHAPI is going, attempt to connect via the pipe - try 25 times */
        do 
        {
            rc = DosConnectNmPipe(hp);
            if (rc) 
            {
               /* sprintf(smsg, "  waiting for connection: %u.\r\n\0", rc);
                * VioWrtTTY(smsg,strlen(smsg),0);
                */
                DosSleep(250L);
                c++;
            }
        }
        while ( (c<25) && rc ); 
        
        /* if the connection can't be made, close the pipe and tell REXX */
        if (c>=25) 
        {
            sprintf(retval->strptr, "NOCONECT: Unable to establish connection to IMHAPI via the pipe.\n");
            retval->strlength = strlen(retval->strptr);
            DosClose(hp);
            return(0);
        }
  
        /* everything looks OK so make a numeric digit out of the pipe handle */
        memset(smsg, '\0', 8);
        memset(retval->strptr, '\0', 249);
        sprintf(smsg, "%u " , hp);
        memmove(retval->strptr, smsg, strlen(smsg)); 
       
    }

    retval->strlength = strlen(retval->strptr);
    return(0);
 }

/*****************************************************************************
 *
 * ToHapi: receive command from REXX, send it down the pipe, wait for
 *         answer and return result to REXX
 *
 *****************************************************************************
 */

SHORT __APIENTRY ToHapi(
      PSZ       name,             /* name of function       */
      USHORT    argc,             /* argument count         */
      PRXSTRING argv,             /* argument list          */
      PSZ       QueueName,        /* current active queue   */
      PRXSTRING retval)           /* return value           */
      
{
    unsigned    rc, c;
    char        parm1[8];
    char        parm2[256];
    HPIPE       hp;
    SHORT       sBytesReceived;
    PCH         psReceivedBuf;
    SEL         ReceivedSel;
    
    /* get some space to hold the results returned from the pipe */
    DosAllocSeg(READ_BUFFER, &ReceivedSel, 0); 
    psReceivedBuf = MAKEP(ReceivedSel,0);

    hp = c = 0;
    
    if (argc < 2)
    {
        sprintf(retval->strptr, "INCOPARM: ToHapi requires two parameters.\n");
        retval->strlength = strlen(retval->strptr);
        return(0);
    }
        
    memset(parm1, '\0', 8);
    memset(parm2, '\0', 256);
    memset(psReceivedBuf, '\0', READ_BUFFER);
    
    /* collect the parms, and convert the pipe handle digit to a binary no. */
    memmove(parm1, argv[0].strptr, (USHORT)argv[0].strlength); 
    
    if (!(isdigit(parm1[0])))
    {
        sprintf(retval->strptr, (PSZ)"NOTNUMER: ToHapi requires a numeric pipe handle for its first parameter.\n");
        retval->strlength = strlen(retval->strptr);
        return(0);
    }
    
    hp = atoi(parm1); 
    memmove(parm2, argv[1].strptr, (USHORT)argv[1].strlength);

    /* check the second parm, if it is "closepipe", shut the pipe down (this 
     * will also terminate IMHAPI) and return to REXX....
     */
    if(!(strcmp(parm2, "closepipe")) )
    {
        DosClose(hp);
        return(0);
    }
    /* otherwise, send the second parm off to IMHAPI and wait for a 
     * response
     */
    else
    {
        memset(psReceivedBuf, '\0', READ_BUFFER);
        rc = DosTransactNmPipe(hp, 
            argv[1].strptr, 
            (USHORT)argv[1].strlength, 
            (PSZ)psReceivedBuf,
            4096,
            &sBytesReceived);
        /* if there is an error, tell the REXX pgm about it */
        if (rc)
        {
            switch(rc) 
            {
                case(ERROR_BAD_PIPE):
                    sprintf(retval->strptr, "TRPIPERR: Bad Pipe");
                    break;
                case(ERROR_INTERRUPT):
                    sprintf(retval->strptr, "TRPIPERR: Interrupt Waiting for Pipe");
                    break;
                case(ERROR_SEM_TIMEOUT):
                    sprintf(retval->strptr, "TRPIPERR: Time Out Waiting for Pipe");
                    break;
                case(ERROR_INVALID_FUNCTION):
                    sprintf(retval->strptr, "TRPIPERR: Invalid Function");
                    break;
                default:
                    sprintf(retval->strptr, "TRPIPERR: Unknown Pipe Error: %u", rc);
                    break;
            }
        }
        /* transaction completed and now return result to REXX */
        else
            retval->strptr = psReceivedBuf; 
        retval->strlength = sBytesReceived;
        return 0;
    }
}


