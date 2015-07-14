/*多路存储服务器
 日期：20150125
 */
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
#include <libxml/parser.h>
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
#include "dbConnector.cpp"
using namespace std;

// Listening thread var
#define MAXDATASIZE 4096
#define CMS_SERVER_IP "127.0.0.1"
#define CMS_SERVER_PORT 8000
#define MAXCONN_NUM 10
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)  
#define ROOTDIR  "/video/"
char const* singleMedium = NULL;
const char * registermsg = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
                            <Envelope type=\"sregister\"> \
                            </Envelope>";
pthread_t lpt;
int cms_fd = -1;
char buf[MAXDATASIZE]; 
int numbytes,sock_fd;
struct sockaddr_in server_addr; 
xmlNodePtr tmpNode;
//......................

void *ListeningThread(void *arg);
int DecodeXml(char * buffer);
//创建目录
int MkDir(char *dir);
void *run_thread(void * orc);

// 环境参数
char * progName="Lunax";
unsigned fileSinkBufferSize0 = 200000;
Boolean outPutAviFile = false;
unsigned rtspClientCount = 0; // 记录有多少RTSPClient正在使用
int fileOutputIntervalset = 10; //输出文件时间间隔
#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // 默认输出调试信息
#define REQUEST_STREAMING_OVER_TCP False //是否使用TCP连接
HashTable * fRtspClient = HashTable::create(0); //用来存储RTSPClient的Hash表
char * filename_prefix="";
char * filename_suffix="mp4";
char * rtspuri="";
char * tdeviceip="";
int height;
int width;
char datebuf[50],timebuf[50];


//定义一个StreamClientState类来维护每个RTSPClient的生命周期
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

StreamClientState::StreamClientState() :iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0),flag(true) {
}

StreamClientState::~StreamClientState() {
	delete iter;
	if (session != NULL) {
		//删除session并清空streamTimerTask
		UsageEnvironment& env = session->envir(); // alias
		env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
		Medium::close(session);
	}
}

//继承RTSPClient，定制自己的RTSPClient，实现多种方法
class ourRTSPClient: public RTSPClient {
public:
	static ourRTSPClient* createNew(char const * dip, char const* rtspURL,int verbosityLevel = 0, char const* applicationName = NULL,portNumBits tunnelOverHTTPPortNum = 0);
	void createPeriodicOutputFiles0();
	void createOutputFiles0(char const * periodicFilenameSuffix);
	int createVideoFile();
	void closeMediaSinks0();
	void startStorage();
	void stopStorage();
	int run();
	int stop();

protected:
	ourRTSPClient(UsageEnvironment& env, char const * dip, char const* rtspURL,int verbosityLevel, char const* applicationName,portNumBits tunnelOverHTTPPortNum);
	virtual ~ourRTSPClient();

public:
	//TaskScheduler *scheduler;
	//BasicUsageEnvironment *env;
	time_t lt; 
	pthread_t rpt;
	DbConnector *dbc;
	StreamClientState scs;
	TaskToken periodicFileOutputTask0;
	unsigned fileOutputInterval; // 秒
	unsigned fileOutputSecondsSoFar; // 秒
	char eventLoopWatchVariable = 0;
	int height;
	int width;
	Medium* Out0;
	double initialSeekTime0;
	float scale0;
	double endTime0;
	char deviceip[20];
	char rtspurl[200];
	struct tm *starttime;
	char videoname[200];
};


// RTSP响应处理事件
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

// 其他事件句柄
void subsessionAfterPlaying(void* clientData); // 当子媒体流停止时调用
void subsessionByeHandler(void* clientData); // 当子媒体流接收到RTCP（BYE）时调用
void streamTimerHandler(void* clientData); // 处理流媒体结束时的持续时间（未接收到BYE命令）
void periodicFileOutputTimerHandler0(RTSPClient * rtspClient); //写文件事件
void sessionAfterPlaying0(void * rtspClient);

//void openURL(UsageEnvironment& env, char const* progName, char const* rtspURL, char const* dip);

//用迭代器为每个子媒体会话建立连接
void setupNextSubsession(RTSPClient* rtspClient);
//关闭一个流媒体连接（包括RTSPClient对象）
void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

//根据rtspurl查找已存在的RTSPClient
ourRTSPClient * lookupClientByRTSPURL(const char * rtspurl);

UsageEnvironment& operator<<(UsageEnvironment& env,const RTSPClient& rtspClient);
UsageEnvironment& operator<<(UsageEnvironment& env,const MediaSubsession& subsession);


ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const * dip, char const* rtspURL,int verbosityLevel, char const* applicationName,portNumBits tunnelOverHTTPPortNum) :
		RTSPClient(env, rtspURL, verbosityLevel, applicationName,tunnelOverHTTPPortNum, -1) {
	time(&lt);
	starttime = localtime(&lt);
	strcpy(deviceip,dip);
	strcpy(rtspurl,rtspURL);
	dbc = DbConnector::createNew();
	fileOutputInterval = fileOutputIntervalset; // 秒
	fileOutputSecondsSoFar = 0; // 秒
	Out0 = NULL;
	initialSeekTime0 = 0.0f;
	scale0 = 1.0f;
	endTime0 = 0.0f;
	periodicFileOutputTask0 = NULL;
	fRtspClient->Add(this->url(),(void *)this);
	++rtspClientCount;
}

ourRTSPClient::~ourRTSPClient() {
	//fRtspClient->Remove(this->url());
	delete dbc;
}

ourRTSPClient* ourRTSPClient::createNew(char const *dip, char const* rtspURL, int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
	//printf("deviceip: %s\n", dip);
	TaskScheduler *scheduler = BasicTaskScheduler::createNew();
	BasicUsageEnvironment *env = BasicUsageEnvironment::createNew(*scheduler);
	return  new ourRTSPClient(*env, dip, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

void ourRTSPClient::createPeriodicOutputFiles0() {
	char periodicFileNameSuffix[100];
	snprintf(periodicFileNameSuffix, sizeof periodicFileNameSuffix,
			"-%05d-%05d", fileOutputSecondsSoFar,
			fileOutputSecondsSoFar + fileOutputInterval);
	this->createOutputFiles0(periodicFileNameSuffix);

	periodicFileOutputTask0 = envir().taskScheduler().scheduleDelayedTask(
			fileOutputInterval * 1000000,
			(TaskFunc*) periodicFileOutputTimerHandler0, this);
}

void ourRTSPClient::createOutputFiles0(char const * periodicFileNameSuffix) {
	char outFileName[1000];
	//char const* prefix = this->name();
	snprintf(outFileName, sizeof outFileName, "%s%s.%s", filename_prefix,periodicFileNameSuffix, outPutAviFile?"avi":"mp4");

	if(strcmp(filename_suffix,"mp4")==0){
		Out0 = QuickTimeFileSink::createNew(envir(), *scs.session, outFileName,fileSinkBufferSize0, width, height, 15, false, false, false, true);
		if (Out0 == NULL) {
			envir()<< "Failed to create a \"QuickTimeFileSink\" for outputting to \""
				<< outFileName << "\": " << envir().getResultMsg() << "\n";
			closeMediaSinks0();
		} else {
			envir() << "Outputting to the file: \"" << outFileName << "\"\n";
		}
		((QuickTimeFileSink *)Out0)->startPlaying(sessionAfterPlaying0, this);
	}
	if(strcmp(filename_suffix,"avi")==0){
		Out0 = AVIFileSink::createNew(envir(), *scs.session, outFileName,fileSinkBufferSize0, width, height, 25, false);
		if (Out0 == NULL) {
			envir()<< "Failed to create a \"AVIFileSink\" for outputting to \""
				<< outFileName << "\": " << envir().getResultMsg() << "\n";
			closeMediaSinks0();
		} else {
			envir() << "Outputting to the file: \"" << outFileName << "\"\n";
		}
		((AVIFileSink *)Out0)->startPlaying(sessionAfterPlaying0, this);
	}
}


int ourRTSPClient::createVideoFile(){
	if(chdir(ROOTDIR)==-1){
		fprintf(stderr,"ERROR:Change working dir failed\n");
		return -1;
	}
	struct tm *newtime;
	char tmpbuf[128];
	time_t lt1; 
	time(&lt1);
	newtime=localtime(&lt1);
	strftime(tmpbuf,128,"%F",newtime);
	if(MkDir(this->deviceip)==-1){
		fprintf(stderr,"ERROR:Create dir failed\n");
		return -1;
	}
	if(chdir(this->deviceip)==-1){
		fprintf(stderr,"ERROR:Change working dir failed\n");
		return -1;
	}
	if(MkDir(tmpbuf)==-1){
		fprintf(stderr,"ERROR:Create dir failed\n");
		return -1;
	}
	if(chdir(tmpbuf)==-1){
		fprintf(stderr,"ERROR:Change working dir failed\n");
		return -1;
	}
	//strftime(tmpbuf,128,"%F-%T",newtime);
	snprintf(tmpbuf,128,"%d-%d-%d-%d-%d-%d",(1900+newtime->tm_year),( 1+newtime->tm_mon), newtime->tm_mday,newtime->tm_hour, newtime->tm_min, newtime->tm_sec);
	//printf (“%d%d%d ”, (1900+p->tm_year),( l+p->tm_mon), p->tm_mday);
	//printf(“%s%d:%d:%d\n”, wday[p->tm_wday],p->tm_hour, p->tm_min, p->tm_sec);
	snprintf(this->videoname, sizeof this->videoname, "%s.%s",tmpbuf, outPutAviFile?"avi":"mp4");
	printf("videoname: %s\n",this->videoname);

	if(strcmp(filename_suffix,"mp4")==0){
		snprintf(this->videoname, sizeof this->videoname, "%s.%s",tmpbuf, "mp4");
		Out0 = QuickTimeFileSink::createNew(envir(), *scs.session, this->videoname,fileSinkBufferSize0, width, height, 15, false, false, false, true);
		if (Out0 == NULL) {
			envir()<< "Failed to create a \"QuickTimeFileSink\" for outputting to \""
				<< this->videoname << "\": " << envir().getResultMsg() << "\n";
			closeMediaSinks0();
		} else {
			envir() << "Outputting to the file: \"" << this->videoname << "\"\n";
		}
		((QuickTimeFileSink *)Out0)->startPlaying(sessionAfterPlaying0, this);
	}
	if(strcmp(filename_suffix,"avi")==0){
		snprintf(this->videoname, sizeof this->videoname, "%s.%s",tmpbuf, "avi");
		Out0 = AVIFileSink::createNew(envir(), *scs.session, this->videoname,fileSinkBufferSize0, width, height, 25, false);
		if (Out0 == NULL) {
			envir()<< "Failed to create a \"AVIFileSink\" for outputting to \""
				<< this->videoname << "\": " << envir().getResultMsg() << "\n";
			closeMediaSinks0();
		} else {
			envir() << "Outputting to the file: \"" << this->videoname << "\"\n";
		}
		((AVIFileSink *)Out0)->startPlaying(sessionAfterPlaying0, this);
	}
	if (strcmp(filename_suffix, "H264") == 0) {

		scs.iter =  new MediaSubsessionIterator(*scs.session);
		FileSink* fs;
	    	while ((scs.subsession = scs.iter->next()) != NULL) {
	      		if (scs.subsession->readSource() == NULL) continue;

	     		Boolean createOggFileSink = False; // by default
	      		if (strcmp(scs.subsession->mediumName(), "video") == 0) {
					if (strcmp(scs.subsession->codecName(), "H264") == 0) {
						snprintf(this->videoname, sizeof this->videoname, "%s.%s",tmpbuf, "264");
		  				fs = H264VideoFileSink::createNew(envir(), this->videoname,scs.subsession->fmtp_spropparametersets(),fileSinkBufferSize0, false);
					}else if(strcmp(scs.subsession->codecName(), "H265") == 0) {
						snprintf(this->videoname, sizeof this->videoname, "%s.%s",tmpbuf, "265");
		  				fs = H265VideoFileSink::createNew(envir(), this->videoname,scs.subsession->fmtp_spropvps(),scs.subsession->fmtp_spropsps(),scs.subsession->fmtp_sproppps(),fileSinkBufferSize0, false);
					}else if(strcmp(scs.subsession->codecName(), "THEORA") == 0) {
		  				createOggFileSink = True;
					}
	      		}

	      		else if(strcmp(scs.subsession->mediumName(), "audio") == 0) {
					if (strcmp(scs.subsession->codecName(), "AMR") == 0 || strcmp(scs.subsession->codecName(), "AMR-WB") == 0) {
						snprintf(this->videoname, sizeof this->videoname, "%s.%s",tmpbuf, "amr");
		  				fs = AMRAudioFileSink::createNew(envir(), this->videoname,fileSinkBufferSize0, false);
					}else if(strcmp(scs.subsession->codecName(), "VORBIS") == 0 || strcmp(scs.subsession->codecName(), "OPUS") == 0) {
		  				createOggFileSink = True;
					}
	      		}
	      		if (createOggFileSink) {
	      			snprintf(this->videoname, sizeof this->videoname, "%s.%s",tmpbuf, "ogg");
					fs = OggFileSink::createNew(envir(), this->videoname,scs.subsession->rtpTimestampFrequency(), scs.subsession->fmtp_config());
	      		}else if(fs == NULL) {
	      			snprintf(this->videoname, sizeof this->videoname, "%s.%s",tmpbuf, "unknown");
					fs = FileSink::createNew(envir(), this->videoname,fileSinkBufferSize0, false);
	      		}
	      		scs.subsession->sink = fs;
	      		if (scs.subsession->sink == NULL) {
					envir() << "Failed to create FileSink for \"" << this->videoname<< "\": " << envir().getResultMsg() << "\n";
	      		}else{
				if (singleMedium == NULL) {
	  				envir()<< "Created output file: \"" << this->videoname << "\"\n";
				}else{
	  				envir() << "Outputting data from the \"" << scs.subsession->mediumName()<< "/" << scs.subsession->codecName()
	       				<< "\" subsession to \"" << this->videoname << "\"\n";
				}
				if (strcmp(scs.subsession->mediumName(), "video") == 0 && strcmp(scs.subsession->codecName(), "MP4V-ES") == 0 &&
	    				scs.subsession->fmtp_config() != NULL) {
	  				unsigned configLen;
	  				unsigned char* configData = parseGeneralConfigStr(scs.subsession->fmtp_config(), configLen);
					struct timeval timeNow;
					gettimeofday(&timeNow, NULL);
					fs->addData(configData, configLen, timeNow);
					delete[] configData;
				}
				scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),subsessionAfterPlaying,scs.subsession);
				if (scs.subsession->rtcpInstance() != NULL) {
  					scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
				}
	    		}
		}
	}
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
	envir().taskScheduler().doEventLoop(&eventLoopWatchVariable);
	//this->createVideoFile();
}

void ourRTSPClient::stopStorage(){
	this->closeMediaSinks0();
	shutdownStream(this, 1);
	strftime(datebuf,50,"%F",this->starttime);
	strftime(timebuf,50,"%X",this->starttime); 

   	struct tm *endtime;
   	char path[200],sqlbuf[500],cmd[200],ip[20],vn[200], url[200], endtimebuf[128];
	time_t lt; 
	time(&lt);
	endtime=localtime(&lt);
	strftime(endtimebuf,128,"%X",endtime);
	strcpy(ip, this->deviceip);
	strcpy(vn, this->videoname);
	strcpy(url, this->rtspurl);

  	struct sockaddr_in ourAddress;
  	ourAddress.sin_addr.s_addr = ourIPAddress(envir());
	memset(path,0,strlen(path));
	sprintf(path,"rtsp://%s:8554/%s/%s/%s", AddressString(ourAddress).val(), ip, datebuf, vn);
	char *sql="insert into videoindex (deviceip,rtspuri,sdate,starttime,endtime,desturi)values('%s','%s','%s','%s','%s','%s');";
	sprintf(sqlbuf,sql,ip,url,datebuf,timebuf,endtimebuf,path);
	//printf("4.deviceip:%s\nrtspurl:%s\nvideoname:%s\n",this->deviceip,this->rtspurl,this->videoname);

	if(dbc->SQLExecute(sqlbuf)){
		printf("INFO:Insert record to mysql success...\n");
	}else{
		printf("INFO:Insert record to mysql failed...\n");
	}

	//printf("5.deviceip:%s\nrtspurl:%s\nvideoname:%s\n",this->deviceip,this->rtspurl,this->videoname);
	//为文件添加hint track
	memset(cmd,0,strlen(cmd));
	sprintf(cmd,"mp4creator -hint=1 /video/%s/%s/%s", ip, datebuf, vn);
	//printf("Hint CMD:%s\n",cmd);
	if(system(cmd) == -1){
		perror("ERROR:Hint file failed...\n");
	}else{
		printf("INFO:Hint mp4 file success...\n");
	}
	fRtspClient->Remove(url);
}

int ourRTSPClient::run(){
	int lthread = pthread_create(&rpt, NULL, run_thread, this);
	if(lthread < 0){
		printf("run_thread create failed!\n");
		return -1;
	}
	return 0;
}


int ourRTSPClient::stop(){
	this->stopStorage();
	pthread_cancel(rpt);
	return 0;
}

void *run_thread(void * orc){
	((ourRTSPClient *)orc)->sendDescribeCommand(continueAfterDESCRIBE);
	((ourRTSPClient *)orc)->startStorage();
}

ourRTSPClient * lookupClientByRTSPURL(const char * rtspurl){
	return (ourRTSPClient *)fRtspClient->Lookup(rtspurl);
}

UsageEnvironment& operator<<(UsageEnvironment& env,const RTSPClient& rtspClient) {
	return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

UsageEnvironment& operator<<(UsageEnvironment& env,const MediaSubsession& subsession) {
	return env << subsession.mediumName() << "/" << subsession.codecName();
}

//RTSP响应事件的实现
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode,char* resultString) {
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*) rtspClient)->scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to get a SDP description: "
					<< resultString << "\n";
			delete[] resultString;
			break;
		}
		char* const sdpDescription = resultString;
		env << *rtspClient << "Got a SDP description:\n" << sdpDescription<< "\n";
		//根据SDP信息创建一个会话对象
		scs.session = MediaSession::createNew(env, sdpDescription);
		delete[] sdpDescription;
		if (scs.session == NULL) {
			env << *rtspClient<< "Failed to create a MediaSession object from the SDP description: "<< env.getResultMsg() << "\n";
			break;
		} else if (!scs.session->hasSubsessions()) {
			env << *rtspClient<< "This session has no media subsessions (i.e., no \"m=\" lines)\n";
			break;
		}
		//为会话创建数据源并建立连接，在每个子会话上迭代操作
		//调用MediaSubsession::initiate()方法，然后为每个子会话发送SETUP命令，每个子会话拥有自己的数据源
		scs.iter = new MediaSubsessionIterator(*scs.session);
		setupNextSubsession(rtspClient);
		return;
	} while (0);

	//发生不可逆转的错误
	shutdownStream(rtspClient);
}

void setupNextSubsession(RTSPClient* rtspClient) {
	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((ourRTSPClient*) rtspClient)->scs; // alias

	scs.subsession = scs.iter->next();
	if (scs.subsession != NULL) {
		if (!scs.subsession->initiate()) {
			env << *rtspClient << "Failed to initiate the \"" << *scs.subsession<< "\" subsession: " << env.getResultMsg() << "\n";
			setupNextSubsession(rtspClient); //放弃这个子会话，进入下一个子会话
		} else {
			env << *rtspClient << "Initiated the \"" << *scs.subsession<< "\" subsession (";
			if (scs.subsession->rtcpIsMuxed()) {
				env << "client port " << scs.subsession->clientPortNum();
			} else {
				env << "client ports " << scs.subsession->clientPortNum() << "-"<< scs.subsession->clientPortNum() + 1;
			}
			env << ")\n";
			//继续为这个子会话建立连接，发送SETUP命令
			printf("Height : %d \t Width: %d \tFPS: %d\n",scs.subsession->videoHeight(),scs.subsession->videoWidth(),scs.subsession->videoFPS());

			rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP,False, REQUEST_STREAMING_OVER_TCP);
		}
		return;
	}

	//((ourRTSPClient *) rtspClient)->createPeriodicOutputFiles0();  //文件输出
	((ourRTSPClient *) rtspClient)->createVideoFile();  //文件输出

	//为所有子会话建立连接成功，发送PLAY命令，开始传送流媒体
	if (scs.session->absStartTime() != NULL) {
		//流媒体使用绝对时间
		rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY,scs.session->absStartTime(), scs.session->absEndTime());
	} else {
		//流媒体使用相对时间
		scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
		rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
	}
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode,char* resultString) {
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*) rtspClient)->scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to set up the \"" << *scs.subsession<< "\" subsession: " << resultString << "\n";
			break;
		}
		env << *rtspClient << "Set up the \"" << *scs.subsession<< "\" subsession (";
		if (scs.subsession->rtcpIsMuxed()) {
			env << "client port " << scs.subsession->clientPortNum();
		} else {
			env << "client ports " << scs.subsession->clientPortNum() << "-"<< scs.subsession->clientPortNum() + 1;
		}
		env << ")\n";

	} while (0);
	delete[] resultString;
	//为下一个子会话建立连接（if any）
	setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode,char* resultString) {
	Boolean success = False;

	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*) rtspClient)->scs; // alias

		if (resultCode != 0) {
			env << *rtspClient << "Failed to start playing session: "<< resultString << "\n";
			break;
		}
		//设置一个计时器，处理在流媒体结束时的持续时间
		if (scs.duration > 0) {
			unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
			scs.duration += delaySlop;
			unsigned uSecsToDelay = (unsigned) (scs.duration * 1000000);
			scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*) streamTimerHandler, rtspClient);
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
		//发生不可逆转的错误
		shutdownStream(rtspClient);
	}
}

void subsessionAfterPlaying(void* clientData) {
	MediaSubsession* subsession = (MediaSubsession*) clientData;
	RTSPClient* rtspClient = (RTSPClient*) (subsession->miscPtr);

	//开始关闭子会话
	Medium::close(subsession->sink);
	subsession->sink = NULL;

	//检查是否所有子会话已关闭
	MediaSession& session = subsession->parentSession();
	MediaSubsessionIterator iter(session);
	while ((subsession = iter.next()) != NULL) {
		if (subsession->sink != NULL)
			return; // 这个子会话还未关闭
	}

	//所有子会话已关闭，然后停止RTSPClient
	shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData) {
	MediaSubsession* subsession = (MediaSubsession*) clientData;
	RTSPClient* rtspClient = (RTSPClient*) subsession->miscPtr;
	UsageEnvironment& env = rtspClient->envir(); // alias

	env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession<< "\" subsession\n";

	//当做子会话已关闭来处理
	subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
	ourRTSPClient* rtspClient = (ourRTSPClient*) clientData;
	StreamClientState& scs = rtspClient->scs; // alias
	scs.streamTimerTask = NULL;
	//关闭流媒体传输
	shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode) {
	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((ourRTSPClient*) rtspClient)->scs; // alias

	//首先检查是否还存在子会话需要关闭
	if (scs.session != NULL) {
		Boolean someSubsessionsWereActive = False;
		MediaSubsessionIterator iter(*scs.session);
		MediaSubsession* subsession;

		while ((subsession = iter.next()) != NULL) {
			if (subsession->sink != NULL) {
				Medium::close(subsession->sink);
				subsession->sink = NULL;
				if (subsession->rtcpInstance() != NULL) {
					subsession->rtcpInstance()->setByeHandler(NULL, NULL); // 防止在处理TEARDOMN命令时服务器发送BYE命令
				}
				someSubsessionsWereActive = True;
			}
		}
		if (someSubsessionsWereActive) {
			//发送TEARDOWN命令，告诉服务器关闭流媒体传输，这个不会影响对TEARDOWN命令的处理事件
			rtspClient->sendTeardownCommand(*scs.session, NULL);
		}
	}

	env << *rtspClient << "Closing the stream.\n";
	Medium::close(rtspClient);
	//这个地方也会释放StreamClientState

	if (--rtspClientCount == 0) {
		//当没有连接时退出程序，或者用eventLoopWatchVariable = 1来替换，控制eventloop是否继续。
		//eventLoopWatchVariable = 1;
		//exit(exitCode);
	}
}


void periodicFileOutputTimerHandler0(RTSPClient *rtspClient) {
	((ourRTSPClient*) rtspClient)->fileOutputSecondsSoFar +=
			((ourRTSPClient*) rtspClient)->fileOutputInterval;
	((ourRTSPClient*) rtspClient)->closeMediaSinks0();
	if(((ourRTSPClient *) rtspClient)->scs.flag){
		((ourRTSPClient*) rtspClient)->createPeriodicOutputFiles0();
	}else{
		shutdownStream((ourRTSPClient *) rtspClient, 1);
	}
}

void sessionAfterPlaying0(void *rtspClient) {
	if (&(((ourRTSPClient *) rtspClient)->envir()) != NULL) {
		((ourRTSPClient *) rtspClient)->envir().taskScheduler().unscheduleDelayedTask(
				((ourRTSPClient*) rtspClient)->periodicFileOutputTask0);
		((ourRTSPClient *) rtspClient)->envir().taskScheduler().unscheduleDelayedTask(
				((ourRTSPClient*) rtspClient)->scs.streamTimerTask);
	}
	((ourRTSPClient *) rtspClient)->sendPlayCommand(
			*(((ourRTSPClient*) rtspClient)->scs.session), continueAfterPLAY,
			((ourRTSPClient*) rtspClient)->initialSeekTime0,
			((ourRTSPClient*) rtspClient)->endTime0,
			((ourRTSPClient*) rtspClient)->scale0, NULL);
}


int main()
{
        printf("INFO:VideoServer  started...\n");
        if ((sock_fd = socket ( AF_INET , SOCK_STREAM , 0)) == - 1) { 
                perror ("ERROR:Socket error\n"); 
                pthread_exit((void *) -1);
        } 
        memset ( &server_addr, 0, sizeof(struct sockaddr)); 
        server_addr.sin_family = AF_INET; 
        server_addr.sin_port = htons (CMS_SERVER_PORT); 
        server_addr.sin_addr.s_addr = inet_addr(CMS_SERVER_IP); 
        if ( connect ( sock_fd, ( struct sockaddr * ) & server_addr, sizeof( struct sockaddr ) ) == -1) { 
                perror ("ERROR:Cannot connect to a CMSServer...\n"); 
                pthread_exit((void *) -1);
        }
        if ( send ( sock_fd, registermsg , strlen(registermsg), 0) == - 1) { 
                perror ( "ERROR:Send error\n" ); 
        }

        fd_set fdsr;
        int ret;
        struct timeval tv;
        tv.tv_sec = 30;
        tv.tv_usec = 0;

        while(1){
                FD_ZERO(&fdsr);
                FD_SET(sock_fd, &fdsr);
                ret = select(sock_fd + 1, &fdsr, NULL, NULL, &tv);
                if (ret < 0) {
                    perror("ERROR:Select...\n");
                    continue;
                } 
                if (ret == 0) {
                        sleep(0.1);
                        continue;
                }
                if (FD_ISSET(sock_fd, &fdsr)) {
                        ret = recv(sock_fd, buf, MAXDATASIZE, 0);
                        if (ret <= 0) { 
                                printf("ERROR:Socket closed...\n");
                                FD_CLR(sock_fd, &fdsr);
                                break;
                        } else {        // receive data
                                buf[ret] = '\0';
                                DecodeXml(buf);
                        }
                }
        }
        printf("ERROR:CMSServer is down, please restart it ...\n");
        close ( sock_fd); 
}

int DecodeXml(char * buffer){
    xmlDocPtr doc = xmlParseMemory(buffer,strlen(buffer));
    if (doc == NULL){
        return -1;
    }
    xmlNodePtr curNode = xmlDocGetRootElement(doc); //get root element
    if (curNode == NULL){
        xmlFreeDoc(doc);
        return -2;
    }
    if (xmlStrcmp(curNode->name, BAD_CAST "Envelope")){  //匹配Envelope
        xmlFreeDoc(doc);
        return -3;
    }
    if (xmlHasProp(curNode,BAD_CAST "type")){
        xmlChar * szAttr = xmlGetProp(curNode,BAD_CAST "type");
        //cout <<"XMLDecoded:" << szAttr << endl;

        if(!xmlStrcmp(szAttr,BAD_CAST "r_sregister")){  //匹配cmsregister
            cms_fd = sock_fd;
            cout << "INFO:StorageServer registered to a CMSServer...\n"<<endl;
        }

        if(sock_fd != cms_fd ){  
            cout << "WARNING:got a unregistered message..." <<endl;
        }else{

            if(!xmlStrcmp(szAttr,BAD_CAST "startstorage")){  //匹配startstorage
            	   xmlSetProp(curNode, (const xmlChar*)"type", (const xmlChar*)"r_startstorage"); 
                xmlNodePtr sNode = curNode->xmlChildrenNode;
                bool flag = false;
                while (sNode != NULL){
                    if(!xmlStrcmp(sNode->name,BAD_CAST "profile")){
                        xmlNodePtr cNode = sNode->xmlChildrenNode;
                        //cout << sNode->name <<endl;
                        while(cNode != NULL){
                           if(!xmlStrcmp(cNode->name,BAD_CAST "deviceip")){
                            	tdeviceip = (char *)xmlNodeGetContent(cNode);
                                //cout <<"\t"<< cNode->name << " : "<<xmlNodeGetContent(cNode) <<endl;
                                flag = false;
                            }
                            if (!xmlStrcmp(cNode->name,BAD_CAST "rtspuri")){
                            	rtspuri = (char *)xmlNodeGetContent(cNode);
                                //cout <<"\t"<< cNode->name << " : "<<xmlNodeGetContent(cNode) <<endl;
                                flag = false;
                            }
                            if(!xmlStrcmp(cNode->name,BAD_CAST "height")){
                            	height = atoi((char *)xmlNodeGetContent(cNode));
                                //cout <<"\t"<< cNode->name << " : "<<xmlNodeGetContent(cNode) <<endl;
                                flag = true;
                            }
                            if(!xmlStrcmp(cNode->name,BAD_CAST "width")){
                            	width = atoi((char *)xmlNodeGetContent(cNode));
                                //cout <<"\t"<< cNode->name << " : "<<xmlNodeGetContent(cNode) <<endl;
                                flag = true;
                            }
                            if(!xmlStrcmp(cNode->name,BAD_CAST "split")){
                            	fileOutputIntervalset = atoi((char *)xmlNodeGetContent(cNode));
                                //cout <<"\t"<< cNode->name << " : "<<xmlNodeGetContent(cNode) <<endl;
                                flag = true;
                            }
                            if(!xmlStrcmp(cNode->name,BAD_CAST "format")){
                            	filename_suffix = (char *)xmlNodeGetContent(cNode);
                                //cout <<"\t"<< cNode->name << " : "<<xmlNodeGetContent(cNode) <<endl;
                                flag = true;
                            }
                            tmpNode = cNode->next;
                            if(flag){
                            	xmlUnlinkNode(cNode);
                                xmlFreeNode(cNode);
                             }
                             cNode = tmpNode;
                        }
                        //openURL(progName, rtspuri , deviceip);
                        if( lookupClientByRTSPURL(rtspuri) == NULL){
			ourRTSPClient* rtspClient = ourRTSPClient::createNew(tdeviceip, rtspuri, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
			if(rtspClient->run() >= 0){
				printf("INFO:Start storage success...\n");
				xmlNewChild(sNode,NULL,(xmlChar *) "action",(xmlChar *) "success");
			}else{
				printf("INFO:Start storage failed...\n");
				xmlNewChild(sNode,NULL,(xmlChar *) "action",(xmlChar *) "fail");
			}
                        }else{
                        		printf("WARNING: %s  existed...\n", rtspuri);
                        		xmlNewChild(sNode,NULL,(xmlChar *) "action",(xmlChar *) "exist");
                        }

                    }
                    sNode = sNode->next;
                }

                xmlChar *xml_buff;
                int size;
                xmlDocDumpMemory(doc,&xml_buff,&size);
                //printf("%s-----%d\n",(char *)xml_buff,size);
                if (send ( sock_fd, (char*)xml_buff , strlen((char*)xml_buff), 0) == - 1) { 
                    perror ( "ERROR:Send error\n" ); 
                } 
            }
            if(!xmlStrcmp(szAttr,BAD_CAST "stopstorage")){  //匹配stopstorage
            	   xmlSetProp(curNode, (const xmlChar*)"type", (const xmlChar*)"r_stopstorage"); 
                xmlNodePtr sNode = curNode->xmlChildrenNode;
                while (sNode != NULL){
                    if(!xmlStrcmp(sNode->name,BAD_CAST "profile")){
                        xmlNodePtr cNode = sNode->xmlChildrenNode;
                        //cout << sNode->name <<endl;
                        while(cNode != NULL){
                            if (!xmlStrcmp(cNode->name,BAD_CAST "rtspuri")){
                            	//cout <<"\t"<< cNode->name << " : "<<xmlNodeGetContent(cNode) <<endl;
                            	ourRTSPClient* rtspClient = lookupClientByRTSPURL((char *)xmlNodeGetContent(cNode));
                            	if(rtspClient != NULL ){
				if( rtspClient->stop() >= 0){
	                            		printf("INFO:Stop storage success...\n");
					xmlNewChild(sNode,NULL,(xmlChar *) "action",(xmlChar *) "success");
	                            	}else{
	                            		printf("INFO:Stop storage failed...\n");
	                            		xmlNewChild(sNode,NULL,(xmlChar *) "action",(xmlChar *) "fail");
	                            	}
                            	}else{
                            		printf("WARNING: %s didn't exist...\n",(char *)xmlNodeGetContent(cNode));
                            		xmlNewChild(sNode,NULL,(xmlChar *) "action",(xmlChar *) "notexist");
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
                    perror ( "ERROR:Send error\n" ); 
                } 
            }

        }
        xmlFree(szAttr);
    }
    xmlFreeDoc(doc);
}


int MkDir(char *dir)  
{  
    DIR *mydir = NULL;  
    if((mydir= opendir(dir))==NULL)//判断目录   
    {  
    	int ret = mkdir(dir, MODE);//创建目录  
      	if (ret != 0)  
      	{  
          	return -1;  
      	}  
      	//printf("INFO: folder %s created sucess!\n", dir);  
    }  
    else  
    {  
        //printf("INFO: folder %s exist!\n", dir);  
    }  
    return 0;  
}

