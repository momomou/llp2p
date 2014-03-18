/*
 * Copyright (C) 2004-2009 Georgy Yunaev gyunaev@ulduzsoft.com
 *
 * This example is free, and not covered by LGPL license. There is no 
 * restriction applied to their modification, redistribution, using and so on.
 * You can study them, modify them, use them in your own program - either 
 * completely or partially. By using it you may give me some credits in your
 * program, but you don't have to.
 *
 *
 * This example tests most features of libirc. It can join the specific
 * channel, welcoming all the people there, and react on some messages -
 * 'help', 'quit', 'dcc chat', 'dcc send', 'ctcp'. Also it can reply to 
 * CTCP requests, receive DCC files and accept DCC chats.
 *
 * Features used:
 * - nickname parsing;
 * - handling 'channel' event to track the messages;
 * - handling dcc and ctcp events;
 * - using internal ctcp rely procedure;
 * - generating channel messages;
 * - handling dcc send and dcc chat events;
 * - initiating dcc send and dcc chat.
 *
 * $Id: irctest.c 109 2012-01-24 03:06:42Z gyunaev $
 */

#include "libircclient.h"

typedef struct
{
	unsigned short port;
	char ip[16];
	char nick[20];
	char channel[20];

} irc_cli_thread;

typedef struct
{
	char 	* channel;
	char 	* nick;

} irc_ctx_t;

void addlog (const char * fmt, ...);


void dump_event (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count);


void event_join (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count);


void event_connect (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count);


void event_privmsg (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count);


void dcc_recv_callback (irc_session_t * session, irc_dcc_t id, int status, void * ctx, const char * data, unsigned int length);


void dcc_file_recv_callback (irc_session_t * session, irc_dcc_t id, int status, void * ctx, const char * data, unsigned int length);


void event_channel (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count);


void irc_event_dcc_chat (irc_session_t * session, const char * nick, const char * addr, irc_dcc_t dccid);


void irc_event_dcc_send (irc_session_t * session, const char * nick, const char * addr, const char * filename, unsigned long size, irc_dcc_t dccid);

void event_numeric (irc_session_t * session, unsigned int event, const char * origin, const char ** params, unsigned int count);


int irc_client_thread (void *arc);
