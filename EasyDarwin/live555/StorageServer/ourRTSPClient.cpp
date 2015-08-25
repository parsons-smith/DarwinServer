#include "liveMedia.hh"
#include "GroupsockHelper.hh"
#include "BasicUsageEnvironment.hh"
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>  
#include <errno.h>  
#include <netdb.h>
#include <unistd.h> 
#include <strings.h>  
#include <unistd.h>  
#include <dirent.h>  
#include <sys/stat.h>  
#include <mysql/mysql.h>
#include <time.h>
#include "MutexQueue.h"
#include "tinyxml.h"
#include "tinystr.h"
using namespace std;

/*
#define MYSQL_SERVER_IP "192.168.1.252"
#define MYSQL_USERNAME "root"
#define MYSQL_PASSWORD "lunax"
#define MYSQL_DATABSE "lunaxweb"
#define CMS_SERVER_IP "127.0.0.1"
#define CMS_SERVER_PORT 8000

#define ROOTDIR  "/video/"
#define MAXDATASIZE 4096
#define REQUEST_STREAMING_OVER_TCP False 
*/


#define MODE (S_IRWXU | S_IRWXG | S_IRWXO) 
char MYSQL_SERVER_IP[16];
int  MYSQL_SERVER_PORT = 3306;
char MYSQL_USERNAME[50];
char MYSQL_PASSWORD[50];
char MYSQL_DATABASE[50];
char CMS_SERVER_IP[16];
int CMS_SERVER_PORT = 8000;
char ROOTDIR[50];
int MAXDATASIZE =4096;
int RTSP_CLIENT_VERBOSITY_LEVEL = 1;
bool REQUEST_STREAMING_OVER_TCP = False;


int cms_fd = -1;

int numbytes, sock_fd;
struct sockaddr_in server_addr;
MutexQueue<char *> smsgq;


const char * progName = "Lunax";
unsigned fileSinkBufferSize0 = 300000;
Boolean outPutAviFile = false;
int fileOutputIntervalset = 10;
HashTable * fRtspClient = HashTable::create(0); 
const char * filename_prefix = "";
const char * filename_suffix = "mp4";

struct ssession{
	char mac[100];
	char cfd[10];
	char rtspurl[300];
	char deviceip[20];
	int height;
	int width;
};


class StreamClientState {
public:
	StreamClientState();
	virtual ~StreamClientState();

public:
	MediaSubsessionIterator* iter;
	MediaSession* session;
	MediaSubsession* subsession;
	TaskToken streamTimerTask;
	double duration;
	bool flag;
};

StreamClientState::StreamClientState() :iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0), flag(true) {
}

StreamClientState::~StreamClientState() {
	delete iter;
	if (session != NULL) {  
		UsageEnvironment& env = session->envir(); // alias
		env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
		Medium::close(session);
	}
}

class ourRTSPClient : public RTSPClient {
public:
	static ourRTSPClient* createNew(struct ssession * ts, int verbosityLevel = 0, char const* applicationName = NULL, portNumBits tunnelOverHTTPPortNum = 0);
	void createPeriodicOutputFiles0();
	void createOutputFiles0(char const * periodicFilenameSuffix);
	int createVideoFile();
	void closeMediaSinks0();
	void startStorage();
	void stopStorage();
	int run();
	int stop();
	int sendstartreply(const char * msg);
	friend void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) ;
protected:
	ourRTSPClient(UsageEnvironment& env,struct ssession * ts, int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
	virtual ~ourRTSPClient();
	static void *run_thread(void * orc);
public:
	time_t lt;
	struct tm * starttime;
	pthread_t rpt;
	StreamClientState scs;
	struct ssession *ss;
	TaskToken periodicFileOutputTask0;
	unsigned fileOutputInterval; // ��
	unsigned fileOutputSecondsSoFar; // ��
	char eventLoopWatchVariable;
	Medium* Out0;
	double initialSeekTime0;
	float scale0;
	double endTime0;
	char date_t[20];
	//char start_t[20];
	int start_t[3];
	char end_t[20];
	char videoname[200];
	unsigned rtspClientCount ; 
};


int MkDir(char *dir);
bool LoadConfig(char * xmlFile);
ourRTSPClient * lookupClientByRTSPURL(const char * rtspurl);
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);
void subsessionAfterPlaying(void* clientData); 
void subsessionByeHandler(void* clientData); 
void streamTimerHandler(void* clientData); 
void periodicFileOutputTimerHandler0(RTSPClient * rtspClient);
void sessionAfterPlaying0(void * rtspClient);
void setupNextSubsession(RTSPClient* rtspClient); 
void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);
ourRTSPClient * lookupClientByRTSPURL(const char * rtspurl); 
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient);
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession);


ourRTSPClient::ourRTSPClient(UsageEnvironment& env, struct ssession * ts, int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) :
RTSPClient(env, ts->rtspurl, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
	time(&lt);
	starttime = localtime(&lt);
	start_t[0] = starttime->tm_hour;
	start_t[1] = starttime->tm_min;
	start_t[2] = starttime->tm_sec;
	strftime(date_t, 20, "%F",localtime(&lt));
	snprintf(this->videoname, sizeof this->videoname, "%d-%d-%d.%s", start_t[0], start_t[1], start_t[2], outPutAviFile ? "avi" : "mp4");
	ss = ts;
	fileOutputInterval = fileOutputIntervalset; // ��
	fileOutputSecondsSoFar = 0; // ��
	Out0 = NULL;
	initialSeekTime0 = 0.0f;
	scale0 = 1.0f;
	endTime0 = 0.0f;
	periodicFileOutputTask0 = NULL;
	fRtspClient->Add(this->url(), (void *)this);
	eventLoopWatchVariable = 0;
	rtspClientCount = 0;
}

ourRTSPClient::~ourRTSPClient() {
	//fRtspClient->Remove(this->url());
	delete ss;
}

ourRTSPClient* ourRTSPClient::createNew( struct ssession * ts, int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
	TaskScheduler *scheduler = BasicTaskScheduler::createNew();
	BasicUsageEnvironment *env = BasicUsageEnvironment::createNew(*scheduler);
	return  new ourRTSPClient(*env, ts, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

void ourRTSPClient::createPeriodicOutputFiles0() {
	char periodicFileNameSuffix[100];
	snprintf(periodicFileNameSuffix, sizeof periodicFileNameSuffix,
		"-%05d-%05d", fileOutputSecondsSoFar,
		fileOutputSecondsSoFar + fileOutputInterval);
	this->createOutputFiles0(periodicFileNameSuffix);

	periodicFileOutputTask0 = envir().taskScheduler().scheduleDelayedTask(
		fileOutputInterval * 1000000,
		(TaskFunc*)periodicFileOutputTimerHandler0, this);
}

void ourRTSPClient::createOutputFiles0(char const * periodicFileNameSuffix) {
	char outFileName[1000];
	snprintf(outFileName, sizeof outFileName, "%s%s.%s", filename_prefix, periodicFileNameSuffix, outPutAviFile ? "avi" : "mp4");

	if (strcmp(filename_suffix, "mp4") == 0){
		Out0 = QuickTimeFileSink::createNew(envir(), *scs.session, outFileName, fileSinkBufferSize0, ss->width, ss->height, 15, false, false, false, true);
		if (Out0 == NULL) {
			envir() << "Failed to create a \"QuickTimeFileSink\" for outputting to \""
				<< outFileName << "\": " << envir().getResultMsg() << "\n";
			closeMediaSinks0();
		}
		else {
			envir() << "Outputting to the file: \"" << outFileName << "\"\n";
		}
		((QuickTimeFileSink *)Out0)->startPlaying(sessionAfterPlaying0, this);
	}
	if (strcmp(filename_suffix, "avi") == 0){
		Out0 = AVIFileSink::createNew(envir(), *scs.session, outFileName, fileSinkBufferSize0, ss->width, ss->height, 25, false);
		if (Out0 == NULL) {
			envir() << "Failed to create a \"AVIFileSink\" for outputting to \""
				<< outFileName << "\": " << envir().getResultMsg() << "\n";
			closeMediaSinks0();
		}
		else {
			envir() << "Outputting to the file: \"" << outFileName << "\"\n";
		}
		((AVIFileSink *)Out0)->startPlaying(sessionAfterPlaying0, this);
	}
}

int ourRTSPClient::createVideoFile(){
	if (chdir(ROOTDIR) == -1){
		fprintf(stderr, "ERROR:Change working dir failed\n");
		return -1;
	}
	if (MkDir(this->ss->deviceip) == -1){
		fprintf(stderr, "ERROR:Create dir failed\n");
		return -1;
	}
	if (chdir(this->ss->deviceip) == -1){
		fprintf(stderr, "ERROR:Change working dir failed\n");
		return -1;
	}
	if (MkDir(date_t) == -1){
		fprintf(stderr, "ERROR:Create dir failed\n");
		return -1;
	}
	if (chdir(date_t) == -1){
		fprintf(stderr, "ERROR:Change working dir failed\n");
		return -1;
	}

	if (strcmp(filename_suffix, "mp4") == 0){
		//snprintf(this->videoname, sizeof this->videoname, "%s.%s", tmpbuf, "mp4");
		Out0 = QuickTimeFileSink::createNew(envir(), *scs.session, this->videoname, fileSinkBufferSize0, ss->width, ss->height, 25, false, false, false, true);
		if (Out0 == NULL) {
			envir() << "Failed to create a \"QuickTimeFileSink\" for outputting to \"" << this->videoname << "\": " << envir().getResultMsg() << "\n";
			closeMediaSinks0();
		}
		else {
			envir() << "Outputting to the file: \"" << this->videoname << "\"\n";
		}
		((QuickTimeFileSink *)Out0)->startPlaying(sessionAfterPlaying0, this);
	}
	if (strcmp(filename_suffix, "avi") == 0){
		//snprintf(this->videoname, sizeof this->videoname, "%s.%s", tmpbuf, "avi");
		Out0 = AVIFileSink::createNew(envir(), *scs.session, this->videoname, fileSinkBufferSize0, ss->width, ss->height, 25, false);
		if (Out0 == NULL) {
			envir() << "Failed to create a \"AVIFileSink\" for outputting to \""
				<< this->videoname << "\": " << envir().getResultMsg() << "\n";
			closeMediaSinks0();
		}
		else {
			envir() << "Outputting to the file: \"" << this->videoname << "\"\n";
		}
		((AVIFileSink *)Out0)->startPlaying(sessionAfterPlaying0, this);
	}
	if (strcmp(filename_suffix, "H264") == 0) {

		scs.iter = new MediaSubsessionIterator(*scs.session);
		FileSink* fs = NULL;
		while ((scs.subsession = scs.iter->next()) != NULL) {
			if (scs.subsession->readSource() == NULL) continue;

			Boolean createOggFileSink = False; // by default
			if (strcmp(scs.subsession->mediumName(), "video") == 0) {
				if (strcmp(scs.subsession->codecName(), "H264") == 0) {
					//snprintf(this->videoname, sizeof this->videoname, "%s.%s", tmpbuf, "264");
					fs = H264VideoFileSink::createNew(envir(), this->videoname, scs.subsession->fmtp_spropparametersets(), fileSinkBufferSize0, false);
				}
				else if (strcmp(scs.subsession->codecName(), "H265") == 0) {
					//snprintf(this->videoname, sizeof this->videoname, "%s.%s", tmpbuf, "265");
					fs = H265VideoFileSink::createNew(envir(), this->videoname, scs.subsession->fmtp_spropvps(), scs.subsession->fmtp_spropsps(), scs.subsession->fmtp_sproppps(), fileSinkBufferSize0, false);
				}
				else if (strcmp(scs.subsession->codecName(), "THEORA") == 0) {
					createOggFileSink = True;
				}
			}

			else if (strcmp(scs.subsession->mediumName(), "audio") == 0) {
				if (strcmp(scs.subsession->codecName(), "AMR") == 0 || strcmp(scs.subsession->codecName(), "AMR-WB") == 0) {
					//snprintf(this->videoname, sizeof this->videoname, "%s.%s", tmpbuf, "amr");
					fs = AMRAudioFileSink::createNew(envir(), this->videoname, fileSinkBufferSize0, false);
				}
				else if (strcmp(scs.subsession->codecName(), "VORBIS") == 0 || strcmp(scs.subsession->codecName(), "OPUS") == 0) {
					createOggFileSink = True;
				}
			}
			if (createOggFileSink) {
				//snprintf(this->videoname, sizeof this->videoname, "%s.%s", tmpbuf, "ogg");
				fs = OggFileSink::createNew(envir(), this->videoname, scs.subsession->rtpTimestampFrequency(), scs.subsession->fmtp_config());
			}
			else if (fs == NULL) {
				//snprintf(this->videoname, sizeof this->videoname, "%s.%s", tmpbuf, "unknown");
				fs = FileSink::createNew(envir(), this->videoname, fileSinkBufferSize0, false);
			}
			scs.subsession->sink = fs;
			if (scs.subsession->sink == NULL) {
				envir() << "Failed to create FileSink for \"" << this->videoname << "\": " << envir().getResultMsg() << "\n";
			}
			else{
				envir() << "Outputting data from the \"" << scs.subsession->mediumName() << "/" << scs.subsession->codecName()
					<< "\" subsession to \"" << this->videoname << "\"\n";
				if (strcmp(scs.subsession->mediumName(), "video") == 0 && strcmp(scs.subsession->codecName(), "MP4V-ES") == 0 &&
					scs.subsession->fmtp_config() != NULL) {
					unsigned configLen;
					unsigned char* configData = parseGeneralConfigStr(scs.subsession->fmtp_config(), configLen);
					struct timeval timeNow;
					gettimeofday(&timeNow, NULL);
					fs->addData(configData, configLen, timeNow);
					delete[] configData;
				}
				scs.subsession->sink->startPlaying(*(scs.subsession->readSource()), subsessionAfterPlaying, scs.subsession);
				if (scs.subsession->rtcpInstance() != NULL) {
					scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
				}
			}
		}
	}
	return 0;
}

void ourRTSPClient::closeMediaSinks0() {
	Medium::close(this->Out0);
	Out0 = NULL;
	if (scs.session == NULL)
		return;
	MediaSubsessionIterator iter(*(scs.session));
	MediaSubsession* subsession;
	while ((subsession = iter.next()) != NULL) {
		Medium::close(subsession->sink);
		subsession->sink = NULL;
	}
}

void ourRTSPClient::startStorage(){
	sendstartreply("success");
	envir().taskScheduler().doEventLoop(&eventLoopWatchVariable);
	// when exception happens......
	sendstartreply("exception");
	this->closeMediaSinks0();
	shutdownStream(this, 1);
	fRtspClient->Remove(ss->rtspurl);
}

void ourRTSPClient::stopStorage(){
	this->closeMediaSinks0();
	shutdownStream(this, 1);
	char path[200], sqlbuf[500];
	time(&lt);
	strftime(end_t, 128, "%X",localtime(&lt));
	struct sockaddr_in ourAddress;
	ourAddress.sin_addr.s_addr = ourIPAddress(envir());
	memset(path, 0, strlen(path));
	sprintf(path, "http://%s:80/%s/%s/%s", AddressString(ourAddress).val(), ss->deviceip, date_t, videoname);
	const char *sql = "insert into videoindex (deviceip,rtspuri,sdate,starttime,endtime,desturi)values('%s','%s','%s','%d:%d:%d','%s','%s');";
	sprintf(sqlbuf, sql, ss->deviceip, ss->rtspurl, date_t, start_t[0],start_t[1],start_t[2], end_t, path);
	MYSQL sql_ins;  
	mysql_init(&sql_ins);
	if(!mysql_real_connect(&sql_ins,MYSQL_SERVER_IP,MYSQL_USERNAME,MYSQL_PASSWORD,MYSQL_DATABASE,MYSQL_SERVER_PORT,NULL,0) ){
		fprintf(stderr,"ERROR:Connect failed...\n");
	}
	int res=mysql_query(&sql_ins,sqlbuf);
	if(res){
		printf("INFO:Insert record to mysql failed...\n");
	}else{
		printf("INFO:Insert record to mysql success...\n");
	}
	mysql_close(&sql_ins);
	printf("%s",ss->rtspurl);
	fRtspClient->Remove(ss->rtspurl);
}

int ourRTSPClient::run(){
	rtspClientCount++;
	int lthread = pthread_create(&rpt, NULL, run_thread, this);
	if (lthread < 0){
		printf("ERROR:Run_thread create failed!\n");
		return -1;
	}
	return 0;
}

int ourRTSPClient::stop(){
	try{
		this->stopStorage();
	}catch (exception &e){
		return -1;
	}
	return 0;
}

int ourRTSPClient::sendstartreply(const char * msg){
	const char * startreplystl="<?xml version=\"1.0\"?> \
                                <Envelope type=\"r_startstorage\"> \
                                        <profile> \
                                                <mac>%s</mac> \
                                                <cfd>%s</cfd> \
                                                <deviceip>%s</deviceip> \
                                                <rtspuri>%s</rtspuri> \
                                                <action>%s</action> \
                                        </profile> \
                                </Envelope>";
	char * replybuffer = (char *)malloc(sizeof(char) * MAXDATASIZE);
	sprintf(replybuffer, startreplystl, ss->mac, ss->cfd, ss->deviceip, ss->rtspurl, msg);
	smsgq.push(replybuffer);
	return 0;
}

void *ourRTSPClient::run_thread(void * orc){
	((ourRTSPClient *)orc)->sendDescribeCommand(continueAfterDESCRIBE);
	((ourRTSPClient *)orc)->startStorage();
	return NULL;
}

ourRTSPClient * lookupClientByRTSPURL(const char * rtspurl){
	return (ourRTSPClient *)fRtspClient->Lookup(rtspurl);
}

UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
	return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
	return env << subsession.mediumName() << "/" << subsession.codecName();
}

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to get a SDP description: "
				<< resultString << "\n";
			delete[] resultString;
			break;
		}
		char* const sdpDescription = resultString;
		env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";
		scs.session = MediaSession::createNew(env, sdpDescription);
		delete[] sdpDescription;
		if (scs.session == NULL) {
			env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
			break;
		}
		else if (!scs.session->hasSubsessions()) {
			env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
			break;
		}
		scs.iter = new MediaSubsessionIterator(*scs.session);
		setupNextSubsession(rtspClient);
		return;
	} while (0);
	shutdownStream(rtspClient);
}

void setupNextSubsession(RTSPClient* rtspClient) {
	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

	scs.subsession = scs.iter->next();
	if (scs.subsession != NULL) {
		if (!scs.subsession->initiate()) {
			env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
			setupNextSubsession(rtspClient); //���������ӻỰ��������һ���ӻỰ
		}
		else {
			env << *rtspClient << "Initiated the \"" << *scs.subsession << "\" subsession (";
			if (scs.subsession->rtcpIsMuxed()) {
				env << "client port " << scs.subsession->clientPortNum();
			}
			else {
				env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum() + 1;
			}
			env << ")\n";
			printf("Height : %d \t Width: %d \tFPS: %d\n", scs.subsession->videoHeight(), scs.subsession->videoWidth(), scs.subsession->videoFPS());

			rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
		}
		return;
	}

	//((ourRTSPClient *) rtspClient)->createPeriodicOutputFiles0();  //�ļ�����
	((ourRTSPClient *)rtspClient)->createVideoFile(); 

	if (scs.session->absStartTime() != NULL) {
		rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
	}
	else {
		scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
		rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
	}
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << resultString << "\n";
			break;
		}
		env << *rtspClient << "Set up the \"" << *scs.subsession << "\" subsession (";
		if (scs.subsession->rtcpIsMuxed()) {
			env << "client port " << scs.subsession->clientPortNum();
		}
		else {
			env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum() + 1;
		}
		env << ")\n";

	} while (0);
	delete[] resultString;
	//Ϊ��һ���ӻỰ�������ӣ�if any��
	setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
	Boolean success = False;
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias
		if (resultCode != 0) {
			env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
			break;
		}
		if (scs.duration > 0) {
			unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
			scs.duration += delaySlop;
			unsigned uSecsToDelay = (unsigned)(scs.duration * 1000000);
			scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
		}
		env << *rtspClient << "Started playing session";
		if (scs.duration > 0) {
			env << " (for up to " << scs.duration << " seconds)";
		}
		env << "...\n";
		success = True;
	} while (0);
	delete[] resultString;

	if (!success) {
		shutdownStream(rtspClient);
	}
}

void subsessionAfterPlaying(void* clientData) {
	MediaSubsession* subsession = (MediaSubsession*)clientData;
	RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

	Medium::close(subsession->sink);
	subsession->sink = NULL;

	MediaSession& session = subsession->parentSession();
	MediaSubsessionIterator iter(session);
	while ((subsession = iter.next()) != NULL) {
		if (subsession->sink != NULL)
			return; 
	}

	shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData) {
	MediaSubsession* subsession = (MediaSubsession*)clientData;
	RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
	UsageEnvironment& env = rtspClient->envir(); // alias

	env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";

	subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
	ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
	StreamClientState& scs = rtspClient->scs; // alias
	scs.streamTimerTask = NULL;
	//�ر���ý�崫��
	shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode) {
	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

	if (scs.session != NULL) {
		Boolean someSubsessionsWereActive = False;
		MediaSubsessionIterator iter(*scs.session);
		MediaSubsession* subsession;

		while ((subsession = iter.next()) != NULL) {
			if (subsession->sink != NULL) {
				Medium::close(subsession->sink);
				subsession->sink = NULL;
				if (subsession->rtcpInstance() != NULL) {
					subsession->rtcpInstance()->setByeHandler(NULL, NULL); // ��ֹ�ڴ���TEARDOMN����ʱ����������BYE����
				}
				someSubsessionsWereActive = True;
			}
		}
		if (someSubsessionsWereActive) {
			//����TEARDOWN������߷������ر���ý�崫�䣬���򲻻�Ӱ����TEARDOWN�����Ĵ����¼�
			rtspClient->sendTeardownCommand(*scs.session, NULL);
		}
	}

	env << *rtspClient << "Closing the stream.\n";
	Medium::close(rtspClient);
}

void periodicFileOutputTimerHandler0(RTSPClient *rtspClient) {
	((ourRTSPClient*)rtspClient)->fileOutputSecondsSoFar +=
		((ourRTSPClient*)rtspClient)->fileOutputInterval;
	((ourRTSPClient*)rtspClient)->closeMediaSinks0();
	if (((ourRTSPClient *)rtspClient)->scs.flag){
		((ourRTSPClient*)rtspClient)->createPeriodicOutputFiles0();
	}
	else{
		shutdownStream((ourRTSPClient *)rtspClient, 1);
	}
}

void sessionAfterPlaying0(void *rtspClient) {
	if (&(((ourRTSPClient *)rtspClient)->envir()) != NULL) {
		((ourRTSPClient *)rtspClient)->envir().taskScheduler().unscheduleDelayedTask(((ourRTSPClient*)rtspClient)->periodicFileOutputTask0);
		((ourRTSPClient *)rtspClient)->envir().taskScheduler().unscheduleDelayedTask(((ourRTSPClient*)rtspClient)->scs.streamTimerTask);
	}
	((ourRTSPClient *)rtspClient)->sendPlayCommand(
		*(((ourRTSPClient*)rtspClient)->scs.session), continueAfterPLAY,
		((ourRTSPClient*)rtspClient)->initialSeekTime0,
		((ourRTSPClient*)rtspClient)->endTime0,
		((ourRTSPClient*)rtspClient)->scale0, NULL);
}

int MkDir(char *dir){
	DIR *mydir = NULL;
	if ((mydir = opendir(dir)) == NULL){
		int ret = mkdir(dir, MODE);
		if (ret != 0){
			return -1;
		}
	}
	return 0;
}

bool LoadConfig(char * xmlFile){
	//cout<<"INFO:Loading Config File..."<<endl;
	if (NULL == xmlFile){
		return false;
	}
	TiXmlDocument config(xmlFile);
	if (!config.LoadFile(TIXML_ENCODING_UNKNOWN)){
		return false;
	}
	TiXmlHandle handle(&config);
	TiXmlElement *cms = handle.FirstChild("CMS_SERVER").ToElement();
	int32_t nv = 0;
	if(cms != NULL){
		strncpy(CMS_SERVER_IP, cms->Attribute("IP",&nv), sizeof(CMS_SERVER_IP));
		//cout<<"CMS_SERVER_IP:"<<CMS_SERVER_IP<<endl;
		CMS_SERVER_PORT = atoi(cms->Attribute("PORT",&nv));
		//cout<<"CMS_SERVER_PORT:"<<CMS_SERVER_PORT<<endl;
		//strncpy(CMS_SERVER_PORT, cms->Attribute("PORT",&nv), sizeof(CMS_SERVER_PORT));
	}

	TiXmlElement *mysql = handle.FirstChild("MYSQL_SERVER").ToElement();
	if(mysql != NULL){
		strncpy(MYSQL_SERVER_IP, mysql->Attribute("IP",&nv), sizeof(MYSQL_SERVER_IP));
		//cout<<"MYSQL_SERVER_IP:"<<MYSQL_SERVER_IP<<endl;
		//strncpy(MYSQL_PORT, mysql->Attribute("PORT",&nv), sizeof(MYSQL_PORT));
		MYSQL_SERVER_PORT = atoi(mysql->Attribute("PORT",&nv));
		//cout<<"MYSQL_SERVER_PORT:"<<MYSQL_SERVER_PORT<<endl;
		strncpy(MYSQL_DATABASE, mysql->Attribute("DATABASE",&nv), sizeof(MYSQL_DATABASE));
		//cout<<"MYSQL_DATABASE:"<<MYSQL_DATABASE<<endl;
		strncpy(MYSQL_USERNAME, mysql->Attribute("USERNAME",&nv), sizeof(MYSQL_USERNAME));
		//cout<<"MYSQL_USERNAME:"<<MYSQL_USERNAME<<endl;
		strncpy(MYSQL_PASSWORD, mysql->Attribute("PASSWORD",&nv), sizeof(MYSQL_PASSWORD));
		//cout<<"MYSQL_PASSWORD:"<<MYSQL_PASSWORD<<endl;
	}

	TiXmlElement *dir = handle.FirstChild("ROOTDIR").ToElement();
	if(dir != NULL){
		strncpy(ROOTDIR, dir->Attribute("dir",&nv), sizeof(ROOTDIR));
		//cout<<"ROOTDIR:"<<ROOTDIR<<endl;
	}

	TiXmlElement *size = handle.FirstChild("MAXDATASIZE").ToElement();
	if(size != NULL){
		MAXDATASIZE =  atoi(size->Attribute("size",&nv));
		//cout<<"MAXDATASIZE:"<<MAXDATASIZE<<endl;
	}
	TiXmlElement *overtcp = handle.FirstChild("REQUEST_STREAMING_OVER_TCP").ToElement();
	if(overtcp != NULL){
		if(strcmp(overtcp->Attribute("value", &nv), "False") == 0){
			REQUEST_STREAMING_OVER_TCP = False;
		}
		if(strcmp(overtcp->Attribute("value", &nv), "True") == 0){
			REQUEST_STREAMING_OVER_TCP = True;
		}
		//cout<<"REQUEST_STREAMING_OVER_TCP:"<<REQUEST_STREAMING_OVER_TCP<<endl;
	}
	//cout<<"INFO:Loading Config File success..."<<endl;
	return true;
}
