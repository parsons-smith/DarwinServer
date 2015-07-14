/*
	date:2015 04 19
	author:luoo
	file:record.cpp
*/

#include <stdio.h>  
#include <string.h>  
#include <errno.h>  
#include <strings.h>  
#include <unistd.h>  
#include <sys/types.h>  
#include <sys/stat.h>  
#include <mysql/mysql.h>
#include <time.h>

#define MYSQL_SERVER_IP "192.168.101.149"
#define MYSQL_USERNAME "root"
#define MYSQL_PASSWORD "lunax"
#define MYSQL_DATABSE "lunaxweb"

class DbConnector{
	private:
		MYSQL sql_ins;  //handle
		MYSQL_RES *pRes; //result
		//MYSQL_ROW  sql_row; //row
	public:
		DbConnector();
		~DbConnector();
		int Init();
		int Close();
		int SQLExecute(char * sql);
		static DbConnector* createNew();
};

DbConnector::DbConnector(){

}

DbConnector::~DbConnector(){

}

DbConnector* DbConnector::createNew(){
	return new DbConnector();
}


int DbConnector::Init(){
	mysql_init(&sql_ins);
	if(!mysql_real_connect(&sql_ins,MYSQL_SERVER_IP,MYSQL_USERNAME,MYSQL_PASSWORD,MYSQL_DATABSE,0,NULL,0) ){
		fprintf(stderr,"ERROR:Connect failed...\n");
		return -1;
	}
	return 1;
}

int DbConnector::Close(){
	mysql_close(&sql_ins);
	return 1;
}

int DbConnector::SQLExecute(char * sql){
	Init();
	int res=mysql_query(&sql_ins,sql);
	if(res){
		fprintf(stderr,"ERROR:Excute failed...\n");
		Close();
		return -1;
	}
	Close();
	return 1;		
}

