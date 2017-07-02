#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>	/* for fprintf */
#include <string.h>	/* for memcpy */
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
//check for possible errors
void sys_err(char *err)
{
  perror(err);
  exit(2);
}

//taken from previous project
void getHost(char *buffer,int offset,struct sockaddr_in *cliaddr,int port)
{
  unsigned long host;
  unsigned char a,b,c,d;
  host = ntohl(cliaddr->sin_addr.s_addr);
  a = host >> 24;
  b = (host >> 16) & 0xff;
  c = (host >> 8) & 0xff;
  d = host & 0xff;
  sprintf(buffer+offset,"from %d.%d.%d.%d:%d",a,b,c,d,port);
}

//taken from class lecture
void pack_err(char *buf,uint16_t opcode,uint16_t errcode, char *errmsg)
{
  *buf++ = opcode >> 8;
  *buf++ = opcode; 
  *buf++ = errcode >> 8; 
  *buf++= errcode;
  int i=0;
  //concatenate file not found.
  for(i=0;i<16;i++)
    *buf++=*errmsg++;
  *buf=0;
  printf("sending err packet\n");
}

void pack_data(char *packet,uint16_t opcode, uint16_t blockNum, char *data)
{
  char *temp;
  temp=packet;
  *temp++ = opcode >> 8;
  *temp++ = opcode; 
  *temp++ = blockNum >> 8; 
  *temp++= blockNum;
  int i=0;
  //concatenate file contents
  strcpy(temp,data);
  printf("sending file contents packet\n");
}

void pack_ack(char *packet,uint16_t opcode, uint16_t blockNum)
{
  char *temp;
  temp=packet;
  *temp++ = opcode >> 8;
  *temp++ = opcode; 
  *temp++ = blockNum >> 8; 
  *temp++= blockNum;
  printf("sending ack packet\n");
}
struct packet_info
{
  char *req;
  char parsed[512];
  int recvlen;
  char fileName[512];
  char mode[512];
  int fileSize;
  uint16_t opcode;
  int packlen;
  struct sockaddr_in *cliaddr;
};

void packInfo(struct packet_info *info,char *buf, int recvlen)
{
  int k;
  //get opcode/request
  int i=*buf++;
  int j=*buf++;
  
  printf("%d%d\n",i,j);
  //i should always b 0, j is the opcode/request
  if(i==0)
	{
	  switch(j)
	    {
	    case 1:
	      info->req="RRQ";
	      break;
	    case 2:
	      info->req="WRQ";
	      break;
	    case 3:
	      info->req="DATA";
	      break;
	    case 4:
	      info->req="ACK";
	      break;
	    case 5:
	      info->req="ERROR";
	      break;
	    default:
	      sys_err("not a request");
	    }
	  info->opcode=j;
	}
      else
	sys_err("not a request");
  //add request to parsed string
  k=snprintf(info->parsed,strlen(info->req)+2,"%s ",info->req);
  printf("%s\n",info->parsed);

  //get fileName
  for(i=0;*buf!=0;i++)
    snprintf(info->fileName+i,2,"%c",*buf++);
  printf("%s\n",info->fileName);
  
  //add fileName to parsed string
  k+=snprintf(info->parsed+k,i+2,"%s ",info->fileName);
  printf("%s\n",info->parsed);
  
  //0 byte
  *buf++;
  
  //get mode
  for(i=0;*buf!=0;i++)
    snprintf(info->mode+i,2,"%c",*buf++);
  printf("%s\n",info->mode);

  //add mode to parsed string
  k+=snprintf(info->parsed+k,i+8,"%s ",info->mode);
  printf("%s\n",info->parsed);
  info->packlen=k;
}


int main(int argc, char **argv)
{
  int sockfd,recvlen,port;
  socklen_t addrlen;
  struct sockaddr_in servaddr;
  struct sockaddr_storage cliaddr;
  addrlen=sizeof(cliaddr);
  
  //socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    sys_err("socket");
  
  //bind
  bzero(&servaddr,sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  port=atoi(argv[1]);
  servaddr.sin_port = htons(port);
  if (bind(sockfd,(struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    sys_err("bind");


  //io multiplexing
  fd_set rset;
  FD_ZERO(&rset);
  for(;;)
    {
      FD_SET(sockfd,&rset);
      if (select(sockfd+1,&rset,NULL,NULL,NULL)<0)
	sys_err("select");
      
      char buf[512];
      printf("new request\n\n");
      
      //receive
      if(FD_ISSET(sockfd,&rset))
	{
	  if((recvlen = recvfrom(sockfd, buf, 512, 0, (struct sockaddr*)&cliaddr, &addrlen))<0)
	    sys_err("recv");
	}
      else
	printf("\n\nNOT SET\n\n");
      
      
      struct packet_info packetInfo;
      packInfo(&packetInfo,buf,recvlen);
      char *req=packetInfo.req;
      char *parsed=packetInfo.parsed;
      int recvlen=packetInfo.recvlen;
      char *fileName=packetInfo.fileName;
      char *mode=packetInfo.mode;
      int fileSize=packetInfo.fileSize;
      uint16_t opcode=packetInfo.opcode;
      int packlen=packetInfo.packlen;
      getHost(packetInfo.parsed,packetInfo.packlen,&cliaddr,port);
      printf("%s\n",packetInfo.parsed);
      
      uint16_t BLOCK_NUM=0;
      if(req=="RRQ")
	{
	  FILE *filefp;
	  //check if file exists
	  filefp=fopen(fileName,"r");
	  
	  //if file doesn't exist, send error packet
	  if(filefp==NULL)
	    {
	      //make error packet
	      char buf2[512];
	      uint16_t OP_CODE=5;
	      uint16_t ERR_CODE=1;
	      pack_err(buf2,OP_CODE,ERR_CODE,"File not found.");
	      if(sendto(sockfd,buf2,512,0,(struct sockaddr*) &cliaddr,sizeof(cliaddr))<0)
		sys_err("send");
	      sleep(1);
	      sys_err("fopen");
	      continue;
	    }

	  uint16_t OP_CODE=4;
	  char buf3[512];
	  pack_ack(buf3,OP_CODE,BLOCK_NUM);

	  //send packet
	  if(sendto(sockfd,buf3,512,0,(struct sockaddr*) &cliaddr,sizeof(cliaddr))<0)
	    sys_err("sendWRQ");


	  //wait for ack
	  while((buf3[1]!=4)&&(buf3[3]!=BLOCK_NUM))
	    if(recvlen = recvfrom(sockfd, buf3, 512, 0, (struct sockaddr*)&cliaddr, &addrlen)<0)
	      sys_err("recv");
	  printf("received ack: %d\n",BLOCK_NUM);
	  BLOCK_NUM++;
	  //if file exists, determine fileSize
	  fseek(filefp,0,SEEK_END);
	  long fileSize=ftell(filefp);
	  rewind(filefp);
	  printf("\nfileSize:%ld\n",fileSize);
	  
	  //store file's content
	  char readBuf[fileSize];
	  if(fread(readBuf,fileSize,1,filefp)<0)
	    sys_err("fread");
	  printf("file contents:\n%s\n",readBuf);

	  //if fileSize>512, must send multiple packets
	  int nloops=0;
	  if(fileSize>512)
	    nloops+=(fileSize/512);
	  printf("number of loops:%d\n",nloops);

	  
	  //send file's content by 512 bytes
	  int j;
	  
	  for(j=0;j<nloops;j++)
	    {
	      struct sockaddr_in newservaddr;
	      int newsockfd;
	      //socket
	      if ((newsockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		sys_err("socket");
  
	      //bind
	      bzero(&newservaddr,sizeof(newservaddr));
	      newservaddr.sin_family = AF_INET;
	      newservaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	      srand(time(NULL));
	      int portNew=10000+rand()%10000;
	      printf("new port: %d\n",portNew);
	      newservaddr.sin_port = htons(portNew);
	  
	      //initialize packet
	      char readBuf2[516];
	      char buf2[516];

	      //inc. the readBuf holding the contents of the file when j>0 b/c we already sent 508 bytes.
	      strncpy(readBuf2,readBuf+j*512,512);
	      printf("sending file contents:\n%s\n\n",readBuf2);
	      uint16_t OP_CODE=3;
	      pack_data(buf2,OP_CODE,BLOCK_NUM,readBuf2);

	      //send packet
	      if(sendto(newsockfd,buf2,516,0,(struct sockaddr*) &cliaddr,sizeof(cliaddr))<0)
		sys_err("sendRRQ");
	      //wait for ack
	      while((buf2[1]!=4)&&(buf2[3]!=BLOCK_NUM))
		{
		if(recvlen = recvfrom(sockfd, buf2, 512, 0, (struct sockaddr*)&cliaddr, &addrlen)<0)
		  sys_err("recv");
		printf("%d,%d",buf2[1],buf2[3]);
		}
	      printf("received ack: %d\n",BLOCK_NUM);
	      //this is bad if we need fileSize later, but we don't so it is ok
	      fileSize-=512;
	      printf("rest of fileSize:%lu\n",fileSize);
	      BLOCK_NUM++;
	      if(close(newsockfd)<0)
		sys_err("close newsock1");
	    }
	  
	  sleep(1);
	  struct sockaddr_in newservaddr;
	  int newsockfd;
	  //socket
	  if ((newsockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	    sys_err("socket");
  
	  //bind
	  bzero(&newservaddr,sizeof(newservaddr));
	  newservaddr.sin_family = AF_INET;
	  newservaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	  srand(time(NULL));
	  int portNew=10000+rand()%10000;
	  printf("new port: %d\n",portNew);
	  newservaddr.sin_port = htons(portNew);
	  
	  printf("%lu\n",fileSize);
	  //send rest of file's content
	  char buf2[fileSize+4];
	  OP_CODE=3;
	  pack_data(buf2,OP_CODE,BLOCK_NUM,readBuf+j*512);
	  //send packet
	  if(sendto(newsockfd,buf2,sizeof(buf2),0,(struct sockaddr*) &cliaddr,sizeof(cliaddr))<0)
	    sys_err("sendRRQ");
	  //wait for ack
	  while((buf2[1]!=4)&&(buf2[3]!=BLOCK_NUM))
	    if(recvlen = recvfrom(sockfd, buf2, 512, 0, (struct sockaddr*)&cliaddr, &addrlen)<0)
	      sys_err("recv");
	  printf("received ack: %d\n",BLOCK_NUM);
	  //close
	  if(close(newsockfd)<0)
		sys_err("close newsock1");
	  
	  if(fclose(filefp)!=0)
	    sys_err("fclose");
	}
      else if(req=="WRQ")
	{
	  //check if file exists
	  FILE *filefp;
	  filefp=fopen(fileName,"w");
	  
	  //if file doesn't exist, send and error packet
	  if(filefp==NULL)
	    {
	      //make error packet
	      char buf2[512];
	      uint16_t OP_CODE=5;
	      uint16_t ERR_CODE=1;
	      pack_err(buf2,OP_CODE,ERR_CODE,"File not found.");
	      if(sendto(sockfd,buf2,512,0,(struct sockaddr*) &cliaddr,sizeof(cliaddr))<0)
		sys_err("send");
	      sys_err("fopen");
	      continue;
	    }
	  
	  char buf[512];
	  //initialize ack packet
	  uint16_t OP_CODE=4;
	  char buf3[512];
	  pack_ack(buf3,OP_CODE,BLOCK_NUM);

	  //send packet
	  if(sendto(sockfd,buf3,512,0,(struct sockaddr*) &cliaddr,sizeof(cliaddr))<0)
	    sys_err("sendWRQ");


	  //loop for files larger than 512 bytes
	  while(1)
	    {
	      char buf4[516];
	      int recvlen2;
	      if((recvlen2 = recvfrom(sockfd, buf4, 516, 0, (struct sockaddr*)&cliaddr, &addrlen))<0)
		sys_err("recvWRQ");
	      while(buf4[3]!=48+BLOCK_NUM)
		if((recvlen2 = recvfrom(sockfd, buf4, 516, 0, (struct sockaddr*)&cliaddr, &addrlen))<0)
		sys_err("recvWRQ");
	      printf("recvlen: %d\n",recvlen2);
	      int i;
	      for(i=4;i<recvlen2;i++)
		{
		  printf("%c",buf4[i]);
		}
	      i++;
	      buf[i]=0;
	      int write_err;

	      printf("\nwriting to file\n");
	      //write what was received
	      write_err=fwrite(buf4+4,1,recvlen2,filefp);
	      if(write_err<=0)
		sys_err("write");

	      //send ack packet
	      char buf5[512];
	      BLOCK_NUM++;
	      printf("\n2:\n");
	      pack_ack(buf5,OP_CODE,BLOCK_NUM);
	      if(sendto(sockfd,buf5,512,0,(struct sockaddr*) &cliaddr,sizeof(cliaddr))<0)
		sys_err("sendWRQ2");
	      //if what was received was not greater than 511 bytes, that was the only packet
	      printf("\nrecvlen:%d\n",recvlen2);
	      if(recvlen2<512)
		break;
	    }
	  
	  if(fclose(filefp)!=0)
	    sys_err("fclose");
	}
      printf("closing\n");
      BLOCK_NUM=0;
      FD_CLR(sockfd,&rset);
    }
  if(close(sockfd)<0)
    sys_err("close");
}
