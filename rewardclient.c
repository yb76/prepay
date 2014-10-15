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
#include "rewardmessage.h"

int CallRewardservice(MessageReward *rewmsg, char *host,char *port)
{
  // call service
  char req[1024];
  char resp[1024];

  memset( req, 0, sizeof(req));
  memset( resp, 0, sizeof(resp));

  int pkglen = sizeof( MessageReward );
  memcpy( req,  rewmsg , pkglen );

  int iret = RequestCallStruct( pkglen, req, resp ,host, port);
  if(iret < 0) {
	rewmsg->msgcode = 99;
	strcpy( rewmsg->msgtext, "*connection failed" );
  } else {
	memcpy( rewmsg, resp, pkglen );
  }

  return(iret);
	
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

//only used for fixed length package
int RequestCallStruct(int plen,char *req, char *resp,char* host,char* port)
{
  struct sockaddr_in remote;
  int sock;
  int tmpres;
  char ip[16];
  char *get;
  char buf[4096];
  char content[4096];

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

  //printf( "------------------------------\n" );
  //printf( "prepare message ...");

  int packagelen = plen;

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

  //printf( "sent \n");

  memset(buf, 0, sizeof(buf));
  memset(content, 0, sizeof(content));

  struct timeval tv;
  tv.tv_sec = 10;  /* 10 Secs Timeout */
  tv.tv_usec = 0; 
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
		(struct timeval *)&tv,sizeof(struct timeval));

  int datalen = 0;
  while((tmpres = recv(sock,buf,plen,0))>0)
	{
		memcpy( content + datalen, buf, tmpres);
		datalen += tmpres;
		if(datalen >= plen) break;
	}

  if(datalen < plen) {
  	 printf("Error receiving data, datalen[%d]",datalen);
	 return(-1);
  }
  //printf( "received message, len [%d]\n" , datalen);

  close(sock);

  memcpy( resp, content ,plen);
  return 0;
}

