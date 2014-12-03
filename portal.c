
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include "libxml/xmlreader.h"
#include "libxml/xmlwriter.h"
#include "macro.h"

#ifndef MAXBUFFERLEN
#define MAXBUFFERLEN 1024*100
#endif

char * GetLocalTimeStr( char * timestr )
{
	time_t rawtime;
	struct tm * ptm;
	struct tm temp;

	time ( &rawtime );
	ptm = localtime_r ( &rawtime ,&temp);

	/* YYYYMMDDHHMMSS */
	sprintf( timestr, "%04d%02d%02d%02d%02d%02d",
		ptm->tm_year + 1900 ,
		ptm->tm_mon + 1,
		ptm->tm_mday,
		ptm->tm_hour,
		ptm->tm_min,
		ptm->tm_sec );

	return( timestr );
}

int packXmlToConfigServer(char *xmlbuffer,int msgtype, T_WEBREQUEST* inreq ) 
{

    int rc = 0;
    int ok = 0;
    char currenttime[30];
	
    xmlTextWriterPtr writer;
    xmlBufferPtr buf;
    GetLocalTimeStr( currenttime);
	
    buf = xmlBufferCreate();
    if (buf == NULL) {
        logNow(" Error creating the xml buffer\n");
        return(-1);
    }

    writer = xmlNewTextWriterMemory(buf, 0);
    if (writer == NULL) {
        logNow(" Error creating the xml writer\n");
        return(-1);
    }

    xmlTextWriterSetIndent(writer, 1);

    ok = xmlTextWriterStartDocument(writer, NULL, NULL, NULL);
    if (ok < 0) {
        logNow ( "Error at xmlTextWriterStartDocument\n");
        xmlFreeTextWriter(writer);
		xmlBufferFree(buf);
		return(-1);
    }
	
    T_WEBREQUEST *webreq = inreq;
    {
	// VX680 & VX6803G
	// Disabled 13/08/2014
	if( strncmp(webreq->model,"VX680",5) ==0) strcpy(webreq->model,"VX680");
    }

    switch( msgtype ) {
		case WEBREQUEST_MSGTYPE_HEARTBEAT: 
			ok = 
				StartElement(writer, "terminalConnectionRequest")
			&&  WriteAttribute(writer, "connectTime", currenttime)
			&& WriteElement(writer, "serialNumber", webreq->serialNumber ) //TODO
			&& WriteElement(writer, "model", webreq->model )
			&& WriteElement(writer, "appversion", webreq->appversion )
			&& WriteElement(writer, "returnACK", webreq->result )
			&& WriteElement(writer, "jsontext", webreq->jsontext );
			
			break;
		case WEBREQUEST_MSGTYPE_GETCONFIG:
			ok = 
				StartElement(writer, "terminalInitializationRequest")
			&& WriteElement(writer, "Tid", webreq->tid ) 
			&& WriteElement(writer, "serialNumber", webreq->serialNumber) ;
			if( webreq->appname[0] ) ok = ok && WriteElement(writer, "appName", webreq->appname ) ;
			if( webreq->objectname[0]) ok = ok && WriteElement(writer, "objectName", webreq->objectname );
			

			break;
		case WEBREQUEST_MSGTYPE_TRANSLOG:
				break;
		case WEBREQUEST_MSGTYPE_TRADEOFFER: 
			ok = 
				StartElement(writer, "terminalMsgRequest")
			&&  WriteAttribute(writer, "connectTime", currenttime)
			&& WriteElement(writer, "serialNumber", webreq->serialNumber ) //TODO
			&& WriteElement(writer, "jsontext", webreq->jsontext ) //TODO
			;
			break;
		case WEBREQUEST_MSGTYPE_IPAYTRANS: 
			ok = 
				StartElement(writer, "paymentTransactionRequest")
			&&  WriteAttribute(writer, "transTime", webreq->transTime)
			&& WriteElement(writer, "Tid", webreq->tid ) 
			&& WriteElement(writer, "serialNumber", webreq->serialNumber) 
			&& WriteElement(writer, "amount", webreq->amount) 
			&& WriteElement(writer, "last4Digits", webreq->last4Digits) 
			&& WriteElement(writer, "reponseCode", webreq->reponseCode) 
			&& WriteElement(writer, "authID", webreq->authID) 
			&& WriteElement(writer, "cardType", webreq->cardType) 
			;
			break;
		default:
			break;
	}
	
   
    ok = ok && ( xmlTextWriterEndDocument(writer) >= 0);
    if(!ok) 
		{
		logNow(" Error at generating xml\n");
		rc = -1;
		}
    else {
		strcpy( xmlbuffer, (const char *)buf->content );
		logNow( "WEBCONFIG request buffer=[%s]\n", xmlbuffer);
		rc = 0;
    }
    if(webreq->jsontext) {
	free(webreq->jsontext);
	webreq->jsontext=NULL;
    }
    xmlFreeTextWriter(writer);
    xmlBufferFree(buf);

    return(rc);

}

int unpackXmlHB(xmlTextReaderPtr  reader, char *xmlbuffer, T_WEBRESP *hb)
{
	char name[50]="";
	char value[200];
	int depth=0,rootdepth=0;
	char * ptr_start = NULL;
	char * ptr_end = NULL;
	char *jsonptr =NULL;

	logNow( "unpack xml \n");

	getNextElement(reader,name,&depth);
	if( depth == rootdepth &&
			(strcmp(name, "terminalConnectionResponse") ==0 ||
			strcmp(name, "terminalMsgResponse") ==0 ||
			strcmp(name, "paymentTransactionResponse") ==0 ||
			strcmp(name, "terminalInitializationResponse") ==0))
	{
		getNextElement(reader,name,&depth);
		if( depth==rootdepth+1 && strcmp(name,"serialNumber")==0)
		{
			getElementTextValue(reader,value);
			getNextElement(reader,name,&depth);
		}

		if( depth==rootdepth+1 && strcmp(name,"model")==0)
		{
			getElementTextValue(reader,value);
			getNextElement(reader,name,&depth);
		}

		if( depth==rootdepth+1 && strcmp(name,"NextMsg")==0)
		{
			getElementTextValue(reader,value);
			hb->nextmsg=value[0];			
			logNow( "unpack xml nextmsg[%c] \n", hb->nextmsg);
			getNextElement(reader,name,&depth);
		}

		ptr_start = strstr(xmlbuffer,"<jsontext>" );
		if(ptr_start) ptr_start += strlen( "<jsontext>");
		ptr_end = strstr( xmlbuffer,"</jsontext>" );
		if( ptr_start && ptr_end  ) {
			int length = ptr_end - ptr_start;
			hb->jsontext = (char*) calloc( length + 1,1);
			memcpy( hb->jsontext, ptr_start, length );
			*(hb->jsontext + length) = '\0';

			logNow( "unpack xml json[%s] \n", hb->jsontext);
		}

	}


	if(hb->jsontext && strlen(hb->jsontext)) {
		char *find=NULL,*end=NULL;
		char *p_start = hb->jsontext;
		char entity[10],entity_c;

		strcpy(entity, "&lt;");	entity_c = '<';
		while( (find = strstr(p_start,entity)) != NULL ) {
			end = find + strlen(entity);
			sprintf(find,"%c%s",entity_c,end);
		}
		strcpy(entity, "&gt;");	entity_c = '>';
		while( (find = strstr(p_start,entity)) != NULL ) {
			end = find + strlen(entity);
			sprintf(find,"%c%s",entity_c,end);
		}
		strcpy(entity, "&amp;");	entity_c = '&';
		while( (find = strstr(p_start,entity)) != NULL ) {
			end = find + strlen(entity);
			sprintf(find,"%c%s",entity_c,end);
		}
		strcpy(entity, "&apos;");	entity_c = '\'';
		while( (find = strstr(p_start,entity)) != NULL ) {
			end = find + strlen(entity);
			sprintf(find,"%c%s",entity_c,end);
		}
		strcpy(entity, "&quot;");	entity_c = '"';
		while( (find = strstr(p_start,entity)) != NULL ) {
			end = find + strlen(entity);
			sprintf(find,"%c%s",entity_c,end);
		}
	}

	return(0);
}

int unpackXmlFromPortal(char *xmlbuffer,int msgtype, T_WEBRESP* resp )
{

	xmlTextReaderPtr reader;
	int ok = 0;

	reader = xmlReaderForMemory( xmlbuffer,
			strlen(xmlbuffer), NULL /*URL*/, NULL/*encoding*/,0/*options*/);
	if(reader == NULL)
	{
		logNow(" Error: xmlReaderForMemory failed \n" );
		return(-1);
	}

	switch( msgtype ) {
		case WEBREQUEST_MSGTYPE_HEARTBEAT: 
		case WEBREQUEST_MSGTYPE_GETCONFIG:
		case WEBREQUEST_MSGTYPE_TRADEOFFER:
		case WEBREQUEST_MSGTYPE_IPAYTRANS:
		default:
			ok = unpackXmlHB( reader,xmlbuffer, resp);		
			break;
	}

	xmlFreeTextReader(reader);

	//One should call xmlCleanupParser() only when the process has finished using the library
	return(ok);

}

int tcp_connect(char *host , int port)
{

    struct sockaddr_in dst;
    int len; 
    int	sfd=0;
    struct hostent* pHE = gethostbyname(host);

    if (pHE == 0) {
            return (-1);
    }
    dst.sin_addr.s_addr = *((u_long*)pHE->h_addr_list[0]);
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    len = sizeof(dst);

    /*Create socket & err check*/
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
	logNow("ERROR tcp_connect >> socket()\n");
	return -1;
    }

    if ((connect(sfd, (struct sockaddr *)&dst, len)) < 0) {
	logNow("ERROR tcp_connect >> connect(),%s,%d\n",host,port);
	return -1;
    }

    return sfd;


}

int tcp_send (int sd,int nlen,char *str) 
{
    int nSentBytes = 0;
    while (nSentBytes < nlen) {
        int nTemp = send(sd, str + nSentBytes,nlen - nSentBytes, MSG_NOSIGNAL);
        if (nTemp > 0) nSentBytes += nTemp;
        else {
		break;
	}
    }

    return(nSentBytes);
}

int tcp_recv(int sd, int len, char *buff)
{
	int left = len;
	int nTotal = 0;
	char *ptr = buff;

	if(len <=0) return(0);

	do {
		int nReadBytes = recv(sd, ptr, left, 0);
		if(nReadBytes<=0) break;
		else {
			nTotal = nTotal + nReadBytes;
			ptr = ptr + nReadBytes;
			left = len - nTotal;
		}
	} while(left>0);

	return(nTotal);
}

int tcp_close(int sd)
{
	close(sd);
	return(0);
}

int sendXmlToPortal(int sd,int msgtype,T_WEBREQUEST* inreq, T_WEBRESP* xmlresp)
{
	char buff[10240];
	char acLength[6];
	int length ;
	int timeout= 10;
	int iret = 0;
	char *resp = NULL;

	memset(buff,0,sizeof(buff));

	if(inreq && inreq->serialNumber[0]!='\0') iret = packXmlToConfigServer( buff,msgtype,inreq );
	if(iret<0) {
		logNow("pack xml failed\n");
		return(-1);
	}

	{
		char temp[50];
		if(strlen(buff)) iret = tcp_send ( sd, strlen(buff)+1, buff ); // boyang TESTING
		if(iret<0) {
			logNow("ERROR socket_send to xml config server failed\n");
			return(-1);
		}
		logNow(	"\n xml SENT***\n");
	
		if(xmlresp == NULL) return (0);

		memset(acLength,0,sizeof(acLength));
		iret = tcp_recv( sd, 5, acLength);
		if(iret<0) {
			logNow("ERROR socket_recv header from xml config server failed\n");
			return(-1);
		} else {
			logNow("socket_recv header from xml[%s]\n", acLength);
		}


		length = atoi(acLength);
		resp = (char*) calloc( length + 1 ,1);
		iret = tcp_recv( sd, length, resp);
		if(iret<=0) {
			logNow("ERROR socket_recv from xml config server failed\n");
			free(resp);
			return(-1);
		} else {
			logNow("socket_recv from xml[%s]\n", resp);
		}

		logNow(	"\n xml RECV***");
	}

	if(resp && xmlresp) {
		if(strlen(resp)) iret = unpackXmlFromPortal( resp,msgtype,xmlresp );
		free(resp);
	}
	return(iret);
}
