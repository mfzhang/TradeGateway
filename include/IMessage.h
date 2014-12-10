#ifndef _IMESSAGE_H_
#define _IMESSAGE_H_

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "tradelog.pb.h"

#include "sslsession.h"
#include "tcpsession.h"



#define MSG_TYPE_HTTP 0
#define MSG_TYPE_TCP_OLD 1
#define MSG_TYPE_SSL_PB 2
#define MSG_TYPE_TCP_NEW 3
#define MSG_TYPE_SSL_NEW 4




class IMessage
{
public:
	IMessage();
	~IMessage(void);

	// 业务处理生成的日志
	// 日志的处理
	void SetRecvTime();
	void SetSendTime();

	Trade::TradeLog log;
	void Log(Trade::TradeLog::LogLevel logLevel, std::string sysNo, std::string sysVer, std::string busiType, std::string funcId, std::string account, std::string clientIp, std::string request, std::string response, int status, std::string errCode, std::string errMsg, std::string beginTime, int runtime, std::string gatewayIp, std::string gatewayPort, std::string counterIp, std::string counterPort, std::string counterType);

	


	
	size_t m_MsgHeaderSize;
	std::vector<char> m_MsgHeader;
	size_t GetMsgHeaderSize();
	std::vector<char>& GetMsgHeader();
	void SetMsgHeader(std::vector<char> header);
	virtual bool DecoderMsgHeader()=0;


	std::vector<char> m_MsgContent;
	size_t GetMsgContentSize();
	std::vector<char>& GetMsgContent();
	std::string GetMsgContentString();
	void SetMsgContent(std::string content);
	void SetMsgContent(std::vector<char> content);
	


	int m_msgType;
	
	

	// 删除消息指针
	void destroy();	


	// 以下信息需要重构
	TcpSessionPtr tcpSession;
	void SetTcpSession(TcpSessionPtr session);
	TcpSessionPtr& GetTcpSession();

	SSLSessionPtr sslSession;
	void SetSslSession(SSLSessionPtr session);
	SSLSessionPtr& GetSslSession();
};

#endif
