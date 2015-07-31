#include <map>
#include <string>

#include "tinyxml.h"
#include "ParseDevice.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <exception>

using namespace std;

typedef map<std::string, DeviceInfo*> DeviceMap;

DeviceMap g_deviceMap;
pthread_mutex_t dmt = PTHREAD_MUTEX_INITIALIZER;
int cms_fd = -1;
TiXmlElement* spElement = NULL;
const char* szXmlValue = NULL;
int rtspcount = 0;
char ipbuffer[100];
TiXmlDocument* handle;
TiXmlPrinter *printer;
socklen_t len; 
struct sockaddr_in addr; 
char desturl[500];

CParseDevice::CParseDevice()
{
	pthread_mutex_lock(&dmt);
	g_deviceMap.clear();
	pthread_mutex_unlock(&dmt);
}

CParseDevice::~CParseDevice()
{
	Uninit();
}

int32_t CParseDevice::Init()
{
	return success;
}

void CParseDevice::Uninit()
{
	Clear();
}

void CParseDevice::Clear()
{
	pthread_mutex_lock(&dmt);
	DeviceMap::iterator deviceIter = g_deviceMap.begin();
	for (; deviceIter != g_deviceMap.end(); deviceIter++)
	{
		delete (*deviceIter).second;
	}

	g_deviceMap.clear();
	pthread_mutex_unlock(&dmt);
}

int32_t CParseDevice::AddDevice(DeviceInfo& deviceInfo)
{
	//check Id
	pthread_mutex_lock(&dmt);
	DeviceMap::iterator deviceIter = g_deviceMap.find(deviceInfo.m_szSourceUrl);
	if (deviceIter != g_deviceMap.end())
	{
		printf("INFO::%s exsit\n",deviceInfo.m_szSourceUrl);
		return success;
	}

	//new
	DeviceInfo* pDeviceInfo = new DeviceInfo;
	*pDeviceInfo = deviceInfo;

	//bool bHasUserInfo = false;
	//if ((0 != strlen(deviceInfo.m_szUser)) && (0 != strlen(deviceInfo.m_szPassword)))
	//{
	//	bHasUserInfo = true;
	//}

	//if (0 == strcmp("DH", deviceInfo.m_szIdentifier))
	//{
	//	if (deviceInfo.m_nRate)
	//	{
	//		sprintf(pDeviceInfo->m_szSourceUrl, "rtsp://%s:%d/cam/realmonitor?channel=2&subtype=1", deviceInfo.m_szIP, deviceInfo.m_nPort);
	//	} 
	//	else
	//	{
	//		sprintf(pDeviceInfo->m_szSourceUrl, "rtsp://%s:%d/cam/realmonitor?channel=2&subtype=0", deviceInfo.m_szIP, deviceInfo.m_nPort);
	//	}
	//}
	//else if (0 == strcmp("HK", deviceInfo.m_szIdentifier))
	//{
	//	if (deviceInfo.m_nRate)
	//	{
	//		sprintf(pDeviceInfo->m_szSourceUrl, "rtsp://%s:%d/h264/ch1/main/av_stream", deviceInfo.m_szIP, deviceInfo.m_nPort);
	//	} 
	//	else
	//	{
	//		sprintf(pDeviceInfo->m_szSourceUrl, "rtsp://%s:%d/h264/ch1/sub/av_stream", deviceInfo.m_szIP, deviceInfo.m_nPort);
	//	}
	//}
	//else 
	//{
	//	delete pDeviceInfo;

	//	return fail;
	//}

	//±êÊ¶Ãû
	//sprintf(pDeviceInfo->m_szIdname, "%s%s%d", deviceInfo.m_szIdentifier, deviceInfo.m_szModel, deviceInfo.m_nId);

	//insert
	g_deviceMap.insert(make_pair(pDeviceInfo->m_szSourceUrl, pDeviceInfo));
	pthread_mutex_unlock(&dmt);
	return success;
}
/*
int32_t CParseDevice::DelDevice(uint32_t nId)
{
	//check Id
	DeviceMap::iterator deviceIter =  g_deviceMap.find(nId);
	if (deviceIter == g_deviceMap.end())
	{
		return fail;
	}

	//delete
	delete (*deviceIter).second;
	g_deviceMap.erase(deviceIter);

	return success;
}
*/
//device xml
int32_t CParseDevice::LoadDeviceXml(const char *pXmlFile)
{
	if (NULL == pXmlFile)
	{
		return fail;
	}

	TiXmlDocument config(pXmlFile);
	if (!config.LoadFile(TIXML_ENCODING_UNKNOWN))
	{
		return fail;
	}

	//clear
	Clear();

	TiXmlHandle handle(&config);

	TiXmlElement* pDevElement = handle.FirstChild("Devices").FirstChild("Device").ToElement();
	while (NULL != pDevElement)
	{
		int32_t nValue = 0;
		const char* pszvalue = NULL;

		DeviceInfo devInfo;
		memset(&devInfo, 0, sizeof(devInfo));

		//id
		pszvalue = pDevElement->Attribute("id", &nValue);
		if (NULL == pszvalue) 
		{
			return fail;
		}
		else
		{
			devInfo.m_nId = nValue;
		}

		//streamName
		pszvalue = pDevElement->Attribute("name", &nValue);
		if (NULL == pszvalue) 
		{
			return fail;
		}
		else
		{
			strncpy(devInfo.m_szIdname, pszvalue, sizeof(devInfo.m_szIdname));
		}

		//url
		pszvalue = pDevElement->Attribute("url", &nValue);
		if (NULL == pszvalue) 
		{
			return fail;
		}
		else
		{
			strncpy(devInfo.m_szSourceUrl, pszvalue, sizeof(devInfo.m_szSourceUrl));
		}


		if (success != AddDevice(devInfo))
		{
			return fail;
		}
		rtspcount++;
		//next element
		pDevElement = pDevElement->NextSiblingElement("Device");
	}

	return success;
}



//DecodeXml
int CParseDevice::DecodeXml(const char *xml, int sock_fd)
{
	try{
		if (NULL == xml)
		{
			printf("ERROR:Empty xml message...\n");
			return -1;
		}
		handle = new TiXmlDocument();
		printer = new TiXmlPrinter();
		handle->Parse(xml);
		TiXmlNode* EnvelopeNode = handle->FirstChild("Envelope");
		const char * EnvelopeType = EnvelopeNode->ToElement()->Attribute("type");
		if(EnvelopeType != NULL){
			if(!strcmp(EnvelopeType,"r_pregister")){
				cms_fd = sock_fd;
				return  0;
			}
			if (cms_fd == sock_fd){
				if(!strcmp(EnvelopeType,"getrtspuri")){
					EnvelopeNode->ToElement()->SetAttribute("type","r_getrtspuri");
					TiXmlNode* profileNode = EnvelopeNode->FirstChildElement("profile"); 
					int count = 0;
					while(profileNode){

						DeviceInfo devInfo;
						memset(&devInfo, 0, sizeof(devInfo));

						TiXmlNode* profile = profileNode->FirstChildElement("deviceip");
						if(profile != NULL){
							szXmlValue = profile->ToElement()->GetText();
							devInfo.m_nId = rtspcount++;
							int a[4];
							sscanf(szXmlValue, "%d.%d.%d.%d", &a[0], &a[1], &a[2], &a[3]);
							sprintf(ipbuffer, "%xx%xx%xx%x%c",a[0],     a[1], a[2], a[3], (char)(count +103 ));
							strncpy(devInfo.m_szIdname, ipbuffer, strlen(ipbuffer));
						}
						profile = profileNode->FirstChildElement("token");
						if(profile != NULL){
							szXmlValue = profile->ToElement()->GetText();
						}
						profile = profileNode->FirstChildElement("sourceuri");
						if(profile != NULL){
							szXmlValue = profile->ToElement()->GetText();
							strncpy(devInfo.m_szSourceUrl, szXmlValue, sizeof(devInfo.m_szSourceUrl));
						}

						//Add Device
						if (success != AddDevice(devInfo))
						{
							printf("ERROR:Add new ProxySession:%s Falied!",devInfo.m_szSourceUrl);
							TiXmlText *DestContent = new TiXmlText("None");
							TiXmlElement *DestElement = new TiXmlElement("desturi");
							DestElement->LinkEndChild(DestContent);
							profileNode->ToElement()->LinkEndChild(DestElement);
						}else{
							memset(desturl,0,strlen(desturl));
							strcat(desturl,"rtsp://");
							//getsockname(sock_fd, (struct sockaddr *)&addr, &len);
							//strcat(desturl,inet_ntoa(addr.sin_addr));
							strcat(desturl,"192.168.101.151");
							strcat(desturl,":8554/");
							strcat(desturl, devInfo.m_szIdname);
							printf("INFO:Add new ProxySession:%s\nProxy this Session from %s\n",devInfo.m_szSourceUrl,desturl);
							TiXmlText *DestContent = new TiXmlText(desturl);
							TiXmlElement *DestElement = new TiXmlElement("desturi");
							DestElement->LinkEndChild(DestContent);
							profileNode->ToElement()->LinkEndChild(DestElement);
						}
						profileNode = profileNode->NextSiblingElement("profile");
						count++;
					}   
					handle->Accept( printer );  
					if (send ( sock_fd, printer->CStr() , strlen(const_cast<char *>(printer->CStr())), 0) == - 1) { 
	                    				perror ( "ERROR:Send error\n" ); 
	                    				return -1;
	                			} 
	                			delete handle;
	                			delete printer;
					return 0; 
				}
			}

		}
		return -1;
	}catch(exception &e){
		perror("ERROR:error while decoding xml\n");
		return -1;
	}

 }



DeviceInfo* CParseDevice::GetDeviceInfoByIdName(const char *pszIdName)
{
	pthread_mutex_lock(&dmt);
	DeviceMap::iterator deviceIter = g_deviceMap.begin();
	for (; deviceIter != g_deviceMap.end(); deviceIter++)
	{
		DeviceInfo* pDeviceInfo = (*deviceIter).second;
		if (NULL == pDeviceInfo)
		{
			continue;
		}

		if (0 == strcmp(pszIdName, pDeviceInfo->m_szIdname))
		{
			return pDeviceInfo;
		}
	}
	pthread_mutex_unlock(&dmt);

	return NULL;
}
