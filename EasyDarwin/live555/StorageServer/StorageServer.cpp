/*��·�洢������
 ���ڣ�20150125
 */
#include "ourRTSPClient.cpp"
using namespace std;

#define CMS_SERVER_IP "127.0.0.1"
#define CMS_SERVER_PORT 8000
const char * registermsg = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?><Envelope type=\"sregister\"></Envelope>";
int DecodeXml(char * buffer);

int cms_fd = -1;
char buf[MAXDATASIZE];


int main()
{
	printf("INFO:StorageServer  started...\n");
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("ERROR:Socket error\n");
		exit(-1);
	}
	memset(&server_addr, 0, sizeof(struct sockaddr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(CMS_SERVER_PORT);
	server_addr.sin_addr.s_addr = inet_addr(CMS_SERVER_IP);
	if (connect(sock_fd, (struct sockaddr *) & server_addr, sizeof(struct sockaddr)) == -1) {
		perror("ERROR:Cannot connect to a CMSServer...\n");
		exit(-1);
	}
	if (send(sock_fd, registermsg, strlen(registermsg), 0) == -1) {
		perror("ERROR:Send error\n");
	}

	fd_set fdsr;
	int ret;
	struct timeval tv;
	tv.tv_sec = 30;
	tv.tv_usec = 0;

	while (1){
		FD_ZERO(&fdsr);
		FD_SET(sock_fd, &fdsr);
		if (pthread_mutex_lock(&mutex) != 0){
			printf("ERROR:Thread lock failed!\n");
			continue;
		}
		ret = select(sock_fd + 1, &fdsr, NULL, NULL, &tv);
		pthread_mutex_unlock(&mutex);
		if (ret < 0) {
			perror("ERROR:Select...\n");
			continue;
		}
		if (ret == 0) {
			sleep(0.1);
			continue;
		}
		if (FD_ISSET(sock_fd, &fdsr)) {
			if (pthread_mutex_lock(&mutex) != 0){
				printf("ERROR:Thread lock failed!\n");
				continue;
			}
			ret = recv(sock_fd, buf, MAXDATASIZE, 0);
			if (ret <= 0) {
				printf("ERROR:Socket closed...\n");
				FD_CLR(sock_fd, &fdsr);
				pthread_mutex_unlock(&mutex);
				break;
			}
			else {        // receive data
				pthread_mutex_unlock(&mutex);
				buf[ret] = '\0';
				DecodeXml(buf);
			}
		}
	}
	printf("ERROR:CMSServer is down, please restart it ...\n");
	close(sock_fd);
}

int DecodeXml(char * buffer){
	xmlDocPtr doc = xmlParseMemory(buffer, strlen(buffer));
	if (doc == NULL){
		return -1;
	}
	xmlNodePtr curNode = xmlDocGetRootElement(doc); //get root element
	if (curNode == NULL){
		xmlFreeDoc(doc);
		return -2;
	}
	if (xmlStrcmp(curNode->name, BAD_CAST "Envelope")){  //ƥ��Envelope
		xmlFreeDoc(doc);
		return -3;
	}
	if (xmlHasProp(curNode, BAD_CAST "type")){
		xmlChar * szAttr = xmlGetProp(curNode, BAD_CAST "type");
		if (!xmlStrcmp(szAttr, BAD_CAST "r_sregister")){  //ƥ��cmsregister
			cms_fd = sock_fd;
			cout << "INFO:StorageServer registered to a CMSServer...\n" << endl;
		}

		if (sock_fd != cms_fd){
			cout << "WARNING:got a unregistered message..." << endl;
		}
		else{

			if (!xmlStrcmp(szAttr, BAD_CAST "startstorage")){ 
				xmlNodePtr sNode = curNode->xmlChildrenNode;
				struct ssession *ss = (struct ssession *)malloc(sizeof(struct ssession));
				while (sNode != NULL){
					if (!xmlStrcmp(sNode->name, BAD_CAST "profile")){
						xmlNodePtr cNode = sNode->xmlChildrenNode;
						//cout << sNode->name <<endl;
						while (cNode != NULL){
							if (!xmlStrcmp(cNode->name, BAD_CAST "deviceip")){
								strcpy(ss->deviceip, (char *)xmlNodeGetContent(cNode));
							}
							if (!xmlStrcmp(cNode->name, BAD_CAST "mac")){
								strcpy(ss->mac, (char *)xmlNodeGetContent(cNode));
							}
							if (!xmlStrcmp(cNode->name, BAD_CAST "cfd")){
								strcpy(ss->cfd, (char *)xmlNodeGetContent(cNode));
							}
							if (!xmlStrcmp(cNode->name, BAD_CAST "rtspuri")){
								strcpy(ss->rtspurl, (char *)xmlNodeGetContent(cNode));
							}
							if (!xmlStrcmp(cNode->name, BAD_CAST "height")){
								ss->height = atoi((char *)xmlNodeGetContent(cNode));
							}
							if (!xmlStrcmp(cNode->name, BAD_CAST "width")){
								ss->width = atoi((char *)xmlNodeGetContent(cNode));
							}
							if (!xmlStrcmp(cNode->name, BAD_CAST "split")){
								fileOutputIntervalset = atoi((char *)xmlNodeGetContent(cNode));
							}
							if (!xmlStrcmp(cNode->name, BAD_CAST "format")){
								filename_suffix = (char *)xmlNodeGetContent(cNode);
							}
							cNode = cNode->next;
						}
						if (lookupClientByRTSPURL(ss->rtspurl) == NULL){
							ourRTSPClient* rtspClient = ourRTSPClient::createNew(ss, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
							rtspClient->run();
						}
					}
					sNode = sNode->next;
				}

			}
			if (!xmlStrcmp(szAttr, BAD_CAST "stopstorage")){  
				xmlSetProp(curNode, (const xmlChar*)"type", (const xmlChar*)"r_stopstorage"); 
				xmlNodePtr sNode = curNode->xmlChildrenNode;
				while (sNode != NULL){
					if (!xmlStrcmp(sNode->name, BAD_CAST "profile")){
						xmlNodePtr cNode = sNode->xmlChildrenNode;
						//cout << sNode->name <<endl;
						while (cNode != NULL){
							if (!xmlStrcmp(cNode->name, BAD_CAST "rtspuri")){
								//cout <<"\t"<< cNode->name << " : "<<xmlNodeGetContent(cNode) <<endl;
								ourRTSPClient* rtspClient = lookupClientByRTSPURL((char *)xmlNodeGetContent(cNode));
								if (rtspClient != NULL){
									if (rtspClient->stop() >= 0){
										printf("INFO:Stop storage success...\n");
										xmlNewChild(sNode,NULL,(xmlChar *) "action",(xmlChar *) "success");
									}
									else{
										printf("INFO:Stop storage failed...\n");
										xmlNewChild(sNode,NULL,(xmlChar *) "action",(xmlChar *) "fail");
									}
								}
								else{
									printf("WARNING: %s didn't exist...\n", (char *)xmlNodeGetContent(cNode));
									xmlNewChild(sNode,NULL,(xmlChar *) "action",(xmlChar *) "exception");
								}
							}
							cNode = cNode->next;
						}
					}
					sNode = sNode->next;
				}
				xmlFree(sNode);
			           xmlChar *xml_buff;
			           int size;
			           xmlDocDumpMemory(doc,&xml_buff,&size);
			           //printf("%s-----%d\n",(char *)xml_buff,size);
			           if (send ( sock_fd, (char*)xml_buff , strlen((char*)xml_buff), 0) == - 1) { 
			                    perror ( "send error" ); 
			           } 
			}

		}
		xmlFree(szAttr);
	}
	xmlFreeDoc(doc);
	return 0;
}


