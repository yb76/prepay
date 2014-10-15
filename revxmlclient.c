/*
 * =============================================================================
 *
 *       Filename: 
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/2009
 *       Revision: 
 *       Compiler:  
 *         Author:  
 *        Company:  
 * =============================================================================
 */


#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include "libxml/xmlreader.h"
#include "libxml/xmlwriter.h"
#include "revprepaymessage.h"

static int RequestCall(char *req, char *resp,char* host, char *port);
static int getNextElement( xmlTextReaderPtr reader, char* name, int*dep);
static int getAttribute( xmlTextReaderPtr reader, 
		const char* name, char* value);
static int getElementTextValue( xmlTextReaderPtr reader, char* value);
static int StartElement( xmlTextWriterPtr writer, char *name);
static int WriteElement( xmlTextWriterPtr writer,char *name, char* value);
static int EndElement( xmlTextWriterPtr writer);
static int WriteAttribute( xmlTextWriterPtr writer, char *name, char* value);

int CallRevCardservice(MessageRev *revmsg, char *revhost,char *revport)
{
  xmlBufferPtr xbuf = xmlBufferCreate();
  xmlTextWriterPtr writer = xmlNewTextWriterMemory(xbuf, 0);

  char *transrc = "T" ;	//TERMINAL request
  char svalue[20] ="";
  sprintf( svalue , "%ld", revmsg->value );

  int ok = StartElement(writer, "revCardRequest");
  ok = ok
      &&  WriteAttribute(writer, "storeId", revmsg->storeid)
      &&  WriteAttribute(writer, "operatorId", revmsg->opid)
      &&  WriteAttribute(writer, "transType", revmsg->transtype)
      &&  WriteAttribute(writer, "transSource", transrc )
		&& WriteElement(writer, "deviceTime",revmsg->devicetime)
		&& WriteElement(writer, "CustomerReference",revmsg->customref) 
		&& WriteElement(writer, "cardTrack1",revmsg->cardtr1)
		&& WriteElement(writer, "value",svalue)
		&& WriteElement(writer, "password",revmsg->oppass)
	&& EndElement(writer);

  xmlTextWriterEndDocument(writer);

  //-----------------------------------------------------
  // call rev service
  char req[4*BUFSIZ+1];
  char resp[4*BUFSIZ+1];

  memset( req, 0, sizeof(req));
  memset( resp, 0, sizeof(resp));

  strcpy( req,  (const char *)xbuf->content);
  RequestCall( req, resp ,revhost, revport);

  xmlBufferFree(xbuf);

  //-----------------------------------------------------

  xmlTextReaderPtr reader;
  reader = xmlReaderForMemory( resp,
            strlen(resp), NULL /*URL*/, NULL/*encoding*/,0/*options*/);

  if(reader == NULL)
    {
  		printf("parse xml error");
        return(-1);
    }

  char name[50]="";
  int depth=0;
  int rootdepth = 0;

  getNextElement(reader,name,&depth);
    
  if( depth == rootdepth &&
            strcmp(name, "revCardResponse") ==0)
    {
		getAttribute(reader,"storeId",revmsg->storeid);
        getAttribute(reader,"operatorId",revmsg->opid);

		getNextElement(reader,name,&depth);

		if( depth==rootdepth+1 && strcmp(name,"systemTime")==0) {
			getElementTextValue(reader,revmsg->systemtime);
			getNextElement(reader,name,&depth);
		}

		if( depth==rootdepth+1 && strcmp(name,"returnCode")==0) {
			getElementTextValue(reader,revmsg->msgcode);
			getNextElement(reader,name,&depth);
		}

		if( depth==rootdepth+1 && strcmp(name,"returnMessage")==0) {
			getElementTextValue(reader,revmsg->msgtext);
			getNextElement(reader,name,&depth);
		}
	}

	return(0);
	
}

static int create_tcp_socket();
static char *get_ip(char *host, char* outip);

static int RequestCall(char *req, char *resp,char* revhost,char* revport)
{
  struct sockaddr_in remote;
  int sock;
  int tmpres;
  char ip[16];
  char *get;
  char buf[BUFSIZ+1];
  char content[4*BUFSIZ+1];
  char *host;
  char *port;

  host = revhost;
  port = revport;

  sock = create_tcp_socket();
  get_ip(host,ip);

  remote.sin_family = AF_INET;
  tmpres = inet_pton(AF_INET, ip, (void *)(&(remote.sin_addr.s_addr)));

  if( tmpres < 0)  
  {
    printf("Error: Can't set remote.sin_addr.s_addr");
    return(-1);
  }else if(tmpres == 0)
  {
    printf( "Error: %s is not a valid IP address", ip);
    return(-1);
  }

  remote.sin_port = htons(atoi(port));
  if(connect(sock, (struct sockaddr *)&remote, sizeof(struct sockaddr)) < 0){
    printf("Error: Could not connect");
    return(-1);
  }

  get = req;

  printf( "-----------------" );
  printf( "Send message :\n%s",get );
  printf( "-----------------" );

  int packagelen = strlen(get) + 1;

  //Send the query to the server
  int sent = 0;
  while(sent < packagelen )
  {
    tmpres = send(sock, get+sent, packagelen-sent, 0);
    if(tmpres == -1){
      printf("Error:Can't send message to REV Service");
      return(-1);
    }
    sent += (int)tmpres;
  }

  printf( "receiving message:" );
  //now it is time to receive the page
  memset(buf, 0, sizeof(buf));
  memset(content, 0, sizeof(content));

  struct timeval tv;
  tv.tv_sec = 20;  /* 10 Secs Timeout */
  tv.tv_usec = 0; 
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
		(struct timeval *)&tv,sizeof(struct timeval));

	int datalen = 0;
    while((tmpres = recv(sock,buf,BUFSIZ,0))>0)
	{
		memcpy( content + datalen, buf, tmpres);
		datalen += tmpres;
		if(content[datalen] == '\0' ) break;
	}

  	if(tmpres < 0)
   		 printf("Error receiving data");
  	printf( "%s",content );
  printf( "-----------------" );

  close(sock);

  strcpy( resp, content );
  return 0;
}

static int create_tcp_socket()
{
  int sock;
  if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
    printf("Error: Can't create TCP socket");
    return(-1);
  }
  return sock;
}

static char *get_ip(char *host, char *outip)
{
  struct hostent *hent;
  char ip[16];

  if((hent = gethostbyname(host)) == NULL)
  {
    printf("Error:Can't get IP");
    return(NULL);
  }

  if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip,sizeof(ip)) == NULL)
  {
    printf("Error:Can't resolve host");
    return(NULL);
  }

  strcpy(outip,ip);
  return (outip);
}

static int getNextElement( xmlTextReaderPtr reader, char* name, int*dep)
{
	const char *tmpname=NULL;
	int ret = 0;
	strcpy( name, "");
	*dep = -1;

	while( (ret = xmlTextReaderRead(reader)) == 1)
	{
		int nodetype = xmlTextReaderNodeType(reader);
		if( nodetype == XML_READER_TYPE_ELEMENT )
		{
			tmpname = (const char *)xmlTextReaderConstName(reader);
			*dep = xmlTextReaderDepth(reader);
			strcpy( name, tmpname);
			return(1);
		}
	}

	return(0);
}

static int getAttribute( xmlTextReaderPtr reader, 
		const char* name, char* value)
{
	char *tmpvalue = NULL;
	tmpvalue = (char *)xmlTextReaderGetAttribute(reader, (const xmlChar *)name);
	strcpy( value , "");

	if(tmpvalue == NULL) return(0);
	strcpy( value, tmpvalue );
	xmlFree(tmpvalue);
	return(1);
}

static int getElementTextValue( xmlTextReaderPtr reader, char* value)
{

	int ret = 0;
	const char *tmpvalue ;

	while( (ret = xmlTextReaderRead(reader)) == 1)
	{
		int nodetype = xmlTextReaderNodeType(reader);
		if( nodetype == XML_READER_TYPE_END_ELEMENT ) break;
		if( nodetype == XML_READER_TYPE_TEXT )
		{
			tmpvalue = (const char *)xmlTextReaderConstValue(reader);
			strcpy( value, tmpvalue );
			return(1);
		}
	}
	return(0);
}

static int StartElement( xmlTextWriterPtr writer, char *name)
{
	int rc = xmlTextWriterStartElement(writer, BAD_CAST name);
    if (rc < 0) 
		{
		return(0);
		}
    return(1);
}

static int WriteElement( xmlTextWriterPtr writer,char *name, char* value)
{
	if(value == NULL || strlen(value) == 0) return(1) ; //Nomal exit
	int rc = xmlTextWriterWriteElement(writer, BAD_CAST name, BAD_CAST value);
    if (rc < 0) 
	{
		return(0);
	}
    return(1);
}

static int EndElement( xmlTextWriterPtr writer)
{
	int rc = xmlTextWriterEndElement(writer);
    if (rc < 0) 
	{
		return(0);
	}
    return(1);
}

static int WriteAttribute( xmlTextWriterPtr writer, char *name, char* value)
{
	int rc =xmlTextWriterWriteAttribute(writer, BAD_CAST name, BAD_CAST value);
    if (rc < 0) 
	{
    	return(0);
	}
    return(1);
}

