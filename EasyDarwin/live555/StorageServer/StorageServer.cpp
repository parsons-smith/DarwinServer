#include "ourRTSPClient.cpp"
#include <sys/statfs.h>
using namespace std;

int DecodeXml(char * buffer);
void *send_thread(void * st);
bool available(char * freesize);

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		cout << "usage: "<<argv[0] << " xmlfile" << endl;
		return 1;
	}
	if(!LoadConfig(argv[1])){
		cout<<"ERROR:LoadConfigFile failed"<<endl;
	}
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
	const char * registermsg = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?><Envelope type=\"sregister\"></Envelope>";
	if (send(sock_fd, registermsg, strlen(registermsg), 0) == -1) {
		perror("ERROR:Send error\n");
	}

	pthread_t st;
	int sthread = pthread_create(&st, NULL, send_thread, NULL);
	if(sthread < 0){
		printf("ERROR:send_thread create failed!\n");
	}

	fd_set fdsr;
	int ret;
	struct timeval tv;
	char buf[MAXDATASIZE];

	while (1){
		tv.tv_sec = 30;
		tv.tv_usec = 0;
		FD_ZERO(&fdsr);
		FD_SET(sock_fd, &fdsr);
		ret = select(sock_fd + 1, &fdsr, NULL, NULL, &tv);
		if (ret < 0) {
			perror("ERROR:Select...\n");
			continue;
		}
		if (ret == 0) {
			continue;
		}
		if (FD_ISSET(sock_fd, &fdsr)) {
			ret = recv(sock_fd, buf, MAXDATASIZE, 0);
			if (ret <= 0) {
				printf("ERROR:Socket closed...\n");
				FD_CLR(sock_fd, &fdsr);
				break;
			}
			else {        // receive data
				buf[ret] = '\0';
				DecodeXml(buf);
			}
		}
	}
	printf("ERROR:CMSServer is down, please restart it ...\n");
	close(sock_fd);
}

int DecodeXml(char * buffer){
	TiXmlDocument *handle = new TiXmlDocument();
	TiXmlPrinter *printer = new TiXmlPrinter();
	handle->Parse(buffer);
	TiXmlNode* EnvelopeNode = handle->FirstChild("Envelope");
	const char * EnvelopeType = EnvelopeNode->ToElement()->Attribute("type");
	if(EnvelopeType != NULL){
		if(!strcmp(EnvelopeType,"r_sregister")){
			cms_fd = sock_fd;
			cout << "INFO:StorageServer registered to a CMSServer...\n"<<endl;
		 }
		if(cms_fd != sock_fd){
			cout << "WARNING:got a unregistered message..." <<endl;
		}else{
			if(!strcmp(EnvelopeType, "startstorage")){
				TiXmlNode* ProfileNode = EnvelopeNode->FirstChild("profile");
				//struct ssession *ss = (struct ssession *)malloc(sizeof(struct ssession));
				struct ssession * ss = new ssession;
				memset(ss,0,sizeof(struct ssession));
				strcpy(ss->deviceip,ProfileNode->FirstChildElement("deviceip")->GetText());
				strcpy(ss->mac , ProfileNode->FirstChildElement("mac")->GetText());
				strcpy(ss->cfd , ProfileNode->FirstChildElement("cfd")->GetText());
				strcpy(ss->rtspurl , ProfileNode->FirstChildElement("rtspuri")->GetText());
				ss->height = (int )atof(ProfileNode->FirstChildElement("height")->GetText());
				ss->width = (int )atof(ProfileNode->FirstChildElement("width")->GetText());
				//fileOutputIntervalset = (int )atof(ProfileNode->FirstChildElement("split")->GetText());
				//filename_suffix = ProfileNode->FirstChildElement("format")->GetText();
				ourRTSPClient* rtspClient = lookupClientByRTSPURL(ss->rtspurl);
				if (rtspClient == NULL){
					ourRTSPClient* rtspClient = ourRTSPClient::createNew(ss, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
					rtspClient->run();
				}else{
					cout<<"INFO:"<<ss->rtspurl<<" exist, stop it first..."<<endl;
					rtspClient->sendstartreply("exist");
				}
			}
			if(!strcmp(EnvelopeType, "stopstorage")){
				EnvelopeNode->ToElement()->SetAttribute("type", "r_stopstorage");
				TiXmlNode* ProfileNode = EnvelopeNode->FirstChild("profile");
				const char * rtsp = ProfileNode->FirstChildElement("rtspuri")->GetText();
				TiXmlElement *action = new TiXmlElement("action");  
				ProfileNode->LinkEndChild(action);  
				ourRTSPClient* rtspClient = lookupClientByRTSPURL(rtsp);
				if (rtspClient != NULL){
					if (rtspClient->stop() >= 0){
						printf("INFO:Stop storage success...\n");
						TiXmlText *actionvalue = new TiXmlText("success");  
						action->LinkEndChild(actionvalue); 
					}else{
						printf("INFO:Stop storage failed...\n");
						TiXmlText *actionvalue = new TiXmlText("fail");  
						action->LinkEndChild(actionvalue); 
					}
				}else{
					printf("WARNING: %s didn't exist...\n", rtsp);
					TiXmlText *actionvalue = new TiXmlText("exception");  
					action->LinkEndChild(actionvalue); 
				}
				handle->Accept( printer ); 
				char * replybuffer = (char *)malloc(sizeof(char) * MAXDATASIZE);
				memset(replybuffer,0,sizeof(char) * MAXDATASIZE);
				strcpy(replybuffer, const_cast<char *>(printer->CStr()));
				smsgq.push(replybuffer);
			}
			if(!strcmp(EnvelopeType, "available")){
				EnvelopeNode->ToElement()->SetAttribute("type", "r_available");
				TiXmlNode* ProfileNode = EnvelopeNode->FirstChild("profile");
				TiXmlElement *action = new TiXmlElement("avail");  
				ProfileNode->LinkEndChild(action);
				char freesize[30];
				available(freesize);
				TiXmlText *actionvalue = new TiXmlText(freesize);  
				action->LinkEndChild(actionvalue); 
				handle->Accept( printer ); 
				char * replybuffer = (char *)malloc(sizeof(char) * MAXDATASIZE);
				memset(replybuffer,0,sizeof(char) * MAXDATASIZE);
				strcpy(replybuffer, const_cast<char *>(printer->CStr()));
				smsgq.push(replybuffer);
			}
		}
	}
	delete handle;
	delete printer;
	return 0;
}



void *send_thread(void * st){
	while(true){
		sleep(1);
		while(!smsgq.empty()){
			usleep(10);
			char *msg = smsgq.pop();
			if(send(sock_fd, msg, strlen(msg), 0) == -1){
				perror("ERROR:Send error\n");
			}
			delete [] msg;
		}
	}
	return (void *)NULL;
}

bool available(char * freesize){
	struct statfs diskInfo;
	statfs(ROOTDIR,&diskInfo);
	unsigned long long totalBlocks = diskInfo.f_bsize;
	unsigned long long freeDisk = diskInfo.f_bavail*totalBlocks;
	sprintf(freesize,"%llu",freeDisk>>30);
	return true;
}