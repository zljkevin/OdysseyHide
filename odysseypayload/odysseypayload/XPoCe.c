#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
//#include <libproc.h>
#include <execinfo.h>
#include <time.h>
#include <pthread.h>
#include "colors.h"

#define fprintf(id, args...) printf(args)

//
// Interposing LibXPC - J Levin, http://NewOSXBook.com/
//
// Compile with gcc xpcsnoop -shared -o xpcsnoop.dylib
// 
// then shove forcefully with
//
// DYLD_INSERT_LIBRARIES=...
//
// Only the basic functionality, but the interesting one
// (i.e. snooping XPC messages!)
//
// Much more to be shown with MOXiI 2 Vol. I :-)
//
// License: Free to use and abuse, PROVIDED you :
//
// A) Give credit where credit is due 
// B) Don't remove this comment, especially if you GitHub this.
// C) Don't remove the const char *ver there, which is what what(1)
//    uses to identify binaries
// D) Don't complain this is "crap" or "shit" or any other vulgar word
//    or denigrating term - if you find a bug and/or have an improvement
//    submit it back to the source (j@ newosxbook) please.
// E) Spread good karma and/or help your fellow reversers and *OS enthusiasts
//


//
// This is the expected interpose structure
 typedef struct interpose_s { void *new_func;
			       void *orig_func; } interpose_t;

// Our prototypes - requires since we are putting them in 
//  the interposing_functions, below

// Don't remove this!
const char *ver[] = { "@(#) PROGRAM: XPoCe	PROJECT: J's OpenXPC 1.0",
		      "@(#) http://www.NewOSXBook.com/ - Free for non-commercial use, but please give credit where due." };

int g_backtrace = 0 ;
int g_hex = 0;
int g_myDesc = 1;
int g_color = 0;
int g_noIncoming = 0;
pthread_mutex_t	outputMutex =  PTHREAD_MUTEX_INITIALIZER;
int id = 1;

void *my_libxpc_initializer (void);
#include <xpc/xpc.h>

//void dumpDict (const char *DictName,  xpc_object_t    *dict,int indent);
void *xpc_dictionary_get_connection (xpc_object_t dict);
int xpc_describe (xpc_object_t	obj,  int indent);
void xpc_dictionary_apply_f(xpc_object_t dictionary, void *context, void *f);

xpc_object_t processedDict = NULL;

extern void *_libxpc_initializer(void);

FILE *output = NULL;


int g_indent = 0;
int g_reply  = 0; // unused


char *get_proc_name(int Pid)
{
	// proc_info makes for much easier API than sysctl..
	static char returned[1024];
	if (!Pid) return ("?");
	returned[0] ='\0';
//	int rc =  proc_name(Pid, returned, 1024);

	return (returned);

}

char *getTimeStamp()
{

	static char timeBuf[256]; // more than enough
	time_t now = time(NULL);
	ctime_r(&now,timeBuf);
	// ctime returns \n\0 - we dont want the \n
	char *newline = strchr(timeBuf,'\n');
	if (newline) *newline = '\0';
	return timeBuf;
}



int dictDumper(const char *key, xpc_object_t value) {
                   // Do iteration.
		int i = g_indent;
		while (i > 0) { fprintf(output,"  "); i--; }

		  if (0) {
		   fprintf(output,"Key: %s, Value: ", key);
			}
			else fprintf(output,"%s: ", key);

		xpc_object_t type = xpc_get_type(value);

		   if (type == XPC_TYPE_ACTIVITY) { fprintf(output,"Activity..."); }
		   if (type == XPC_TYPE_DATE) { fprintf(output,"DATE..."); }
		   if (type == XPC_TYPE_FD) { 

				char buf[4096];
				int fd= xpc_fd_dup(value);
				int rc = fcntl(fd,	F_GETPATH, buf);
				fprintf(output, "FD: %s", buf);

				 }

		   if (type == XPC_TYPE_UUID) { 

				char buf[256];
				uuid_unparse(xpc_uuid_get_bytes(value), buf);
			
				fprintf(output, "UUID: %s", buf);
			 }

		   if (type == XPC_TYPE_SHMEM) { fprintf(output,"Shared memory (not handled yet)..."); }
		   if (type ==XPC_TYPE_ENDPOINT) {  fprintf(output,"XPC Endpoint");}
		   if (type ==XPC_TYPE_BOOL) { 
			fprintf(output, xpc_bool_get_value(value) ? "true" : "false");
			}


		   if (type == XPC_TYPE_DATA)
		   {
			int len = xpc_data_get_length (value);
 		
			fprintf(output,"Data (%d bytes): ", len);

			char *bytes = (const char *) xpc_data_get_bytes_ptr(value);
			// print with nulls
			if (bytes) {
			int i = 0 ;
			for ( i = 0 ; i < len ; i++)
			{
			     if (g_hex) {
				fprintf(output,"\\x%02x", (unsigned char )bytes[i]);
				}
			     else
			     fputc(bytes[i], output);
		
			}
			} // bytes
			
		   }
		   if (xpc_get_type(value) == XPC_TYPE_ARRAY) {
			fprintf(output,"// Array (%zu values)\n", xpc_array_get_count(value));
			int i = 0;
			for (i = 0; i < xpc_array_get_count(value); i++)
				{
				   char elem[1024];
				   sprintf(elem, "%s[%d]", key, i);
				   void *arrayElem = xpc_array_get_value(value, i);
				   dictDumper (elem, arrayElem);
				}
			}
			
		   if (xpc_get_type(value) == XPC_TYPE_INT64) { 
			int64_t val  = xpc_int64_get_value(value);
				fprintf(output,"%c%lld",
					val >= 0 ? '+' : ' ',
					xpc_int64_get_value(value));  
					}
		   if (xpc_get_type(value) == XPC_TYPE_UINT64){ fprintf(output,"%lld",xpc_uint64_get_value(value)); }

		   if (xpc_get_type(value) == XPC_TYPE_DICTIONARY) { 
				fprintf(output,"// (dictionary %p)", value);
				xpc_describe(value,g_indent+1);
				}
		   if (xpc_get_type (value) == XPC_TYPE_STRING) { 
			fprintf(output, "\"%s\"" ,xpc_string_get_string_ptr(value));
			}
		   
		   fprintf(output,"\n");
                   return 1;
           };




int xpc_describe (xpc_object_t	obj,  int indent)
{
	if (!g_myDesc)
	{
		fprintf(output,"xpccd: %s", xpc_copy_description(obj));
		return 0;
	}

	g_indent ++;
	static char desc[1024];
	if (xpc_get_type (obj) == XPC_TYPE_CONNECTION)
	{
		int pid = xpc_connection_get_pid(obj);
		fprintf(output, "Peer: %s, PID: %d (%s) \n",
			xpc_connection_get_name(obj),
			pid,
			get_proc_name(pid)
			);
		return 0;

	}

	if (xpc_get_type(obj) == XPC_TYPE_ARRAY)
	{
		fprintf(output,"Array (@TODO)");
	
		return (0);

	}
	if (xpc_get_type(obj) == XPC_TYPE_DICTIONARY)
	{

	   fprintf(output,"--- Dictionary %p, %d values:\n", obj,xpc_dictionary_get_count(obj));
           xpc_dictionary_apply_f(obj, NULL, dictDumper);

	   g_indent --;
	   fprintf(output,"--- End Dictionary %p\n", obj);


	   return 0;
	}

		return (1);
} ;



void xpc_describe_by_dispatching(xpc_object_t obj, int indent)
{

 dispatch_async_f(dispatch_get_main_queue(), //dispatch_queue_t queue, 
		  obj,		// void *context,
         	 xpc_describe);

}

static inline void do_backtrace(void)
{
     static char backtraceBuf[16384];
     int num = backtrace(backtraceBuf, 16384);
     char **backtraceSyms = backtrace_symbols(backtraceBuf, num);

     if (backtraceSyms)
	{
	  int i = 0;
	  for (i = 0; i < num; i++)
		fprintf(output, "Frame %i: %s\n", i,backtraceSyms[i]);

	}

}



void   my_xpc_connection_send_message_with_reply(xpc_connection_t connection,
         xpc_object_t message, dispatch_queue_t targetq, xpc_handler_t handler)
{
	// On an outgoing message, we can clear processed Dict
	 processedDict = NULL;

	if (g_color) fprintf (output,RED);
	id++;
	fprintf (output,"XPCCSMWR %s (%d) Outgoing ==> ", getTimeStamp(), id);
	g_indent =0;
	xpc_describe(connection,0);
	fprintf (output, " queue: %s, ", dispatch_queue_get_label(targetq));
	fprintf(output,"\n");
	//pthread_mutex_lock(&outputMutex);
	xpc_describe(message,0);
	//pthread_mutex_unlock(&outputMutex);
	if (g_color) fprintf (output,NORMAL);
	fflush(output);

	if (g_backtrace) do_backtrace();

	 xpc_object_t reply = xpc_connection_send_message_with_reply_sync(connection, message);

	if (g_color) fprintf (output,CYAN);
	fprintf(output,"%s <== (reply sync)\n", getTimeStamp());
	g_indent = 0;
	if (g_color) fprintf (output,CYAN);
	xpc_describe(reply,0);
	if (g_color) fprintf (output,NORMAL);
       handler(reply);



}


xpc_object_t my_xpc_connection_send_message_with_reply_sync(xpc_connection_t connection, xpc_object_t message)
{
	// On an outgoing message, we can clear processed Dict
	 processedDict = NULL;
	xpc_object_t reply;
	if (g_color) fprintf (output,RED);
	fprintf (output,"XPCCSMWRS %s (%d) Outgoing ==> ", getTimeStamp(), id);
	id++;
	xpc_describe(connection,0);
	fprintf(output," (with reply sync)\n");
	xpc_describe(message,1);

	if (xpc_get_type (message) == XPC_TYPE_DICTIONARY)
	//dumpDict("XPC message", connection, 0); else { fprintf(output,"Message not a dictionary");}
	if (g_backtrace) do_backtrace();
	if (g_color) fprintf (output,NORMAL);
	fflush(output);
	if (g_color) fprintf (output,CYAN);
	reply = xpc_connection_send_message_with_reply_sync (connection, message);
	fprintf (output,"%s <== ", getTimeStamp());
	g_indent =0;
	xpc_describe(connection,0);
	fprintf(output,"\n");
	xpc_describe(reply,1);
	fflush(output);
	if (g_color) fprintf (output,NORMAL);

	return (reply);
	

}



void my_xpc_connection_send_message(xpc_connection_t connection, xpc_object_t message)
{
	if (g_color) fprintf (output,RED);
	//pthread_mutex_lock(&outputMutex);
	fprintf (output,"XPCCSM %s ==> ", getTimeStamp());
	g_indent =0;
	xpc_describe(connection,0);
	fprintf(output,"\n");
	xpc_describe(message,0);

// if (xpc_get_type (message) == XPC_TYPE_DICTIONARY)
	//dumpDict("XPC message", connection, 0); else { fprintf(output,"Message not a dictionary");}
	if (g_color) fprintf (output,NORMAL);
	fflush(output);
	//pthread_mutex_unlock(&outputMutex);
	if (g_backtrace) do_backtrace();
	xpc_connection_send_message (connection, message);

}

extern int xpc_pipe_routine (void *xpcPipe, xpc_object_t *inDict, xpc_object_t *out);

int my_xpc_pipe_routine (void *xpcPipe, xpc_object_t *inDict, xpc_object_t *outDict)
{
	if (output) {
	if (g_color) fprintf (output,RED);
	fprintf (output,"%s ==> %s\n", getTimeStamp(), xpc_copy_description(xpcPipe));
	xpc_describe(inDict,0);
	if (g_color) fprintf (output,NORMAL);

	if (g_backtrace) do_backtrace();

		}
	int returned= (xpc_pipe_routine (xpcPipe, inDict, outDict));
	if (*outDict && output) { 
	if (g_color) fprintf (output,CYAN);
	if (g_color) fprintf(output,"%s <== Reply: ", getTimeStamp());

	xpc_describe(*outDict,0);
	if (g_color) fprintf (output,NORMAL);
	}
	
	return (returned);

}


xpc_connection_t my_xpc_connection_create(const char *name, dispatch_queue_t targetq)
{

	fprintf (output,"xpc_connection_create(\"%s\", targetq=%p);\n", name, targetq);
	xpc_connection_t returned = xpc_connection_create (name, targetq);
	fprintf(output,"Returning %p\n", returned);
	return (returned);
}

void *my_libxpc_initializer (void)
{

	fprintf(output,"In XPC Initializer..\n");
	return (_libxpc_initializer());
} ;


#if 0
     void
     xpc_connection_send_barrier(xpc_connection_t connection,
         dispatch_block_t barrier);

     void
     xpc_connection_send_message_with_reply(xpc_connection_t connection,
         xpc_object_t message, dispatch_queue_t targetq, xpc_handler_t handler);

 xpc_object_t
     xpc_connection_send_message_with_reply_sync(xpc_connection_t connection,
         xpc_object_t message);


#endif

__attribute ((constructor)) void _init (void)
{
    printf("xpce init\n");
	
	if (getenv ("XPOCE_BACKTRACE")) g_backtrace++;
	if (getenv ("XPOCE_HEX")) g_hex++;
	// If we are being injected, we have to do interposing ourselves.
	
	if (getenv("XPOCE_NODESC")) g_myDesc = 0;
	if (getenv("XPOCE_COLOR")) g_color = 1;

	// @TODO: Move these into a config later
	if (access("/tmp/xpoce_color", R_OK) == 0) {g_color = 1;}
	if (access("/tmp/xpoce_backtrace", R_OK) == 0) {g_backtrace = 1;}
	if (access("/tmp/xpose_nodesc", R_OK) == 0) { g_myDesc = 0 ; }

	if (getenv("XPOCE_NOINC")) g_noIncoming=1;

	if (getenv("XPOCE_OUT")) output=stderr;
	else {
	char filename[1024];
	sprintf(filename,"/tmp/%s.%d.XPoCe", getprogname(),getpid());
	///strcpy(filename,"/tmp/xxx");
	output = fopen (filename, "w");
	}
	if (!output)
	{
		// we have a problem if we can't open.. just opt for stderr in this case
		// but stderr might be redirected to /dev/null..
		output = stderr;

	}
    fprintf(output, "fprintf test\n");
	// Use Apple's mach lib, since this will only run on iOS/OS X and thus
	// guaranteed to work

} // _init


void *my_xpc_get_type(xpc_object_t obj)
{
	void *returned = xpc_get_type(obj);
	
	fprintf(output, "Got type: %s\n", xpc_copy_description(obj));
	return (returned);

}


void * my_xpc_dictionary_get_string(xpc_object_t dictionary, const char *key)
{

	if (!g_noIncoming)
	{
	if (processedDict == dictionary) { 
		if (output)	fprintf(output,"Already processed message for key %s\n", key);     			}
	else {

		// Get mutex so we don't botch prints if already in print
	pthread_mutex_lock(&outputMutex);
	processedDict = dictionary;

	void *conn = xpc_dictionary_get_connection(dictionary);
	char *connDesc = NULL;
	id++;
	if (conn && output) { fprintf(output, "%s (%d) Incoming <==  ",getTimeStamp(), id);
		    xpc_describe(conn,0); }

	xpc_describe (dictionary,0);
	fflush(output);
	 pthread_mutex_unlock(&outputMutex);
	}
	}
	return (xpc_dictionary_get_string (dictionary,key));
}
int64_t my_xpc_dictionary_get_int64(xpc_object_t dictionary, const char *key)
{

	if (!g_noIncoming)
	{
	 //pthread_mutex_lock(&outputMutex);
	if (processedDict == dictionary) { 
		if (output)	fprintf(output,"Already processed message for key %s\n", key);     			}
	else {
		processedDict = dictionary;

	void *conn = xpc_dictionary_get_connection(dictionary);
	char *connDesc = NULL;
	if (conn && output) { fprintf(output, "%s <== Incoming Message: ",getTimeStamp()); 
		    xpc_describe(conn,0); }

	xpc_describe (dictionary,0);
	fflush(output);
	 //pthread_mutex_unlock(&outputMutex);

	}
	}
	return (xpc_dictionary_get_int64 (dictionary,key));

}


// Finally, interpose

static const interpose_t interposing_functions[] __attribute__ ((used,section("__DATA, __interpose"))) = {

 //{ (void *) my_xpc_get_type, (void *) xpc_get_type },
 { (void *) my_xpc_dictionary_get_int64, (void *)xpc_dictionary_get_int64 },
 { (void *) my_xpc_dictionary_get_string, (void *)xpc_dictionary_get_string },
 { (void *) my_xpc_connection_send_message_with_reply_sync, (void *) xpc_connection_send_message_with_reply_sync },

 { (void *) my_xpc_connection_send_message, (void *) xpc_connection_send_message },
 { (void *) my_xpc_connection_send_message_with_reply, (void *) xpc_connection_send_message_with_reply },
  // { (void *) my_libxpc_initializer, (void *) _libxpc_initializer },
 { (void *) my_xpc_pipe_routine, (void *) xpc_pipe_routine },
 { (void *) my_xpc_connection_create, (void *) xpc_connection_create },

};


