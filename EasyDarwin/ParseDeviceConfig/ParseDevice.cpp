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
#include <pthread.h>

using namespace std;

typedef map<std::string, DeviceInfo*> DeviceMap;

DeviceMap g_deviceMap;
int cms_fd = -1;

int rtspcount = 0;
pthread_mutex_t dmt; 

CParseDevice::CParseDevice()
{
	g_deviceMap.clear();
}

CParseDevice::~CParseDevice()
{
	Uninit();
}

int32_t CParseDevice::Init()
{
	pthread_mutex_init(&dmt, NULL);
	return success;
}

void CParseDevice::Uninit()
{
	pthread_mutex_destroy(&dmt);
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
		pthread_mutex_unlock(&dmt);
		return success;
	}

	//new
	DeviceInfo* pDeviceInfo = new DeviceInfo;
	*pDeviceInfo = deviceInfo;

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
	TiXmlElement *ser_addr = handle.FirstChild("Server_Addr").ToElement();
	int32_t nv = 0;
	strncpy(server_addr, ser_addr->Attribute("ip",&nv), sizeof(server_addr));

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
	if (NULL == xml)
	{
		printf("ERROR:Empty xml message...\n");
		return -1;
	}
	TiXmlDocument *handle = new TiXmlDocument();
	TiXmlPrinter *printer = new TiXmlPrinter();
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
					const char* szXmlValue = NULL;
					TiXmlNode* profile = profileNode->FirstChildElement("deviceip");
					if(profile != NULL){
						szXmlValue = profile->ToElement()->GetText();
						devInfo.m_nId = rtspcount++;
						int a[4];
						sscanf(szXmlValue, "%d.%d.%d.%d", &a[0], &a[1], &a[2], &a[3]);
						char ipbuffer[32];
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
						if(szXmlValue != NULL)
							strncpy(devInfo.m_szSourceUrl, szXmlValue, sizeof(devInfo.m_szSourceUrl));
					}

					//Add Device
					if(strcmp(devInfo.m_szSourceUrl,"") != 0){
						if (success != AddDevice(devInfo)){
							printf("ERROR:Add new ProxySession:%s Falied!",devInfo.m_szSourceUrl);
							TiXmlText *DestContent = new TiXmlText("None");
							TiXmlElement *DestElement = new TiXmlElement("desturi");
							DestElement->LinkEndChild(DestContent);
							profileNode->ToElement()->LinkEndChild(DestElement);
						}else{
							char desturl[500];
							memset(desturl,0,strlen(desturl));
							sprintf(desturl, "rtsp://%s:8554/%s", server_addr, devInfo.m_szIdname);
							printf("INFO:Add new ProxySession:%s\nProxy this Session from %s\n",devInfo.m_szSourceUrl,desturl);
							TiXmlText *DestContent = new TiXmlText(desturl);
							TiXmlElement *DestElement = new TiXmlElement("desturi");
							DestElement->LinkEndChild(DestContent);
							profileNode->ToElement()->LinkEndChild(DestElement);
						}
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
			pthread_mutex_unlock(&dmt);
			return pDeviceInfo;
		}
	}
	pthread_mutex_unlock(&dmt);
	return NULL;
}
