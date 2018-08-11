/* REXX */
/* CHEKMAIL.CMD - Check for unviewed electronic mail.
 * This sample REXX procedure demonstrates the use of IMHAPI with a
 * named pipe.  This procedure uses the two external functions supplied
 * with IMHAPI, MkHaPipe and ToHapi.  MkHaPipe creates a named pipe,
 * starts IMAPI as a background task and connects it to the new named
 * pipe.  From then on, the procedure uses the pipe handle returned by
 * MkHaPipe to send commands to IMHAPI with the ToHapi function.  The
 * results from IMHAPI are collected and checked and appropriate
 * actions taken.  Specifically, this procedure uses these functions
 * to check for electronic mail messages and pop up the appropirate
 * terminal window if mail is waiting.  Please see IMHAPI.DOC for more
 * information on the format of IMHAPI itself.  The source code for
 * MkHaPipe and ToHapi is included with this set of files.
 * 
 * This sample will have to be modified to work in your environment, of
 * course.  The references to a specific userid, user name, session
 * name, etc. are all unique to each user and each system.
 *     Paul Firgens, December 1991
 */

/* First, add the IMHAPI functions to the REXX session
 */
call RxFuncAdd 'MkHaPipe', 'HAPIREXX', 'MKHAPIPE'
call RxFuncAdd 'ToHapi',   'HAPIREXX', 'TOHAPI'

/* Start up IMHAPI, get it connected to a pipe and return the pipe's     
 * handle number.  We need that to know where to direct IMHAPI commands  
 */
phandle = MkHaPipe('\pipe\kahuna')

/* Get the user's password.  Send ANSI control codes to hide the entry   
 * and then restore the screen.                                          
 */
say "Please enter your password (the characters will be invisible):"
say "[8m"
pull psw
say "[0m"
say "Thank you.  Please be patient while your mail queue is checked."

/* Begin to make the connection....turn AutoWait on for IMHAPI.
 */
ignore = ToHapi(phandle, '-a') 

/* Attempt to connect with one of the terminal sessions, 'd' here
 */
rc = ToHapi(phandle, '1 d') 

/* Pick off the return code.  Check that the CONNECT_PS was successful.  
 * If it wasn't, quit the procedure, otherwise continue.
 */
firstch = substr(rc,1,1)
if firstch <> '0'
    then
    do
        say 'Cannot connect to Terminal session.  Program terminated.'
        call GBYE  /* go, clean-up and leave */
    end
else
    nop

/* The session is available, assume it is on the initial VTAM panel and  
 * login through CICS to the mail software.
 */
ignore = ToHapi(phandle, '3 cics@E') 
ignore = ToHapi(phandle, '3 @C * * MMMM')
ignore = ToHapi(phandle, '3 cesn@E')
ignore = ToHapi(phandle, '3 UFFDA@T'psw'@E * * PASSWORD:')

/* The CICS logon is complete so login to the electronic mail system,
 * after waiting for specific phrases to appear before continuing.
 */
ignore = ToHapi(phandle, '3 mail@E * * COMPLETE')
ignore = ToHapi(phandle, '3 "Ann E. Mailuser@T'psw'@E" * * MAIL')
ignore = ToHapi(phandle, '3 @0 * * MEETINGS')

/* SEARCH the PSP (function code 6) for the key word 'UNVIEWED'.
 */
msgloc = ToHapi(phandle, '6 UNVIEWED * 1000')
firstch = substr(msgloc,1,1)

/* If SEARCH doesn't find the key word, tell the user and quit.
 */
if firstch <> '0'
    then
    do
        say 'No UNVIEWED messages are in your mail queue.'
        beep(220,60)
        ignore = ToHapi(phandle, '3 off@E')
        ignore = ToHapi(phandle, '3 "cssf logoff" * * QUIT')
        ignore = ToHapi(phandle, '2')
        call GBYE
    end
else
/* The key word was found, so bring up the terminal window and make     
 * it the active window and beep.  Then, quit this procedure.
 */
    do
        ignore = ToHapi(phandle, '3 in@E')
        ignore = ToHapi(phandle, '2')
        ignore = ToHapi(phandle, '101 d')
        ignore = ToHapi(phandle, '104 d\c1\\s1080')
        beep(440,60)
        say "Mail is waiting.  Please check the active terminal window."
        call GBYE
    end
exit
    
GBYE:
/* BE CERTAIN to clean-up!  Otherwise, restarting REXX procedures       
 * with calls to these external functions in the same OS/2 session      
 * is likely to crash the session, as they try to allocate resources    
 * (esp. the named pipe) that are already hanging around                
 * from the previous execution of the procedure.  This can happen       
 * during debugging of REXX programs using these functions as the       
 * programs don't get to close down properly before terminating.  One   
 * way around the debugging problem is to EXIT the session with the     
 * crashed, buggy REXX procedure before re-trying it in a new session.
 */

/* First, the 'exit' command to IMHAPI has it terminate.  The           
 * 'closepipe' command has ToHapi close the pipe.
 */
    ignore = ToHapi(phandle, 'exit') 
    ignore = ToHapi(phandle, 'closepipe')
    call RxFuncDrop 'MkHaPipe'
    call RxFuncDrop 'ToHapi'
exit
