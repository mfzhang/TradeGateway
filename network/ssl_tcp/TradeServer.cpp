#include "stdafx.h"
//#define BOOST_FILESYSTEM_VERSION 2
// mongodb�õĻ����ϰ汾��ֻ֧��2��δ�����Դ�boost1.49

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/functional/factory.hpp>

#include "TradeServer.h"

#include "common.h"
#include "./config/configmanager.h"


#include "./business/szkingdom/tradebusiness.h"
#include "./business/hundsun_t2/tradebusinesst2.h"
#include "./business/hundsun_com/TradeBusinessComm.h"
#include "./business/apexsoft/TradeBusinessDingDian.h"
#include "./business/apexsoft/DingDian.h"



#include "./output/FileLog.h"

#include "connectpool/connectmanager.h"

// ����
#include "business/xinyi/TCPClientSync.h"

// AGC
#include "business/SunGuard/SywgConnect.h"


#include "log/FileLogManager.h"
#include "errcode.h"

#include "network/ssl_tcp/TcpSession.h"
#include "network/ssl_tcp/SSLSession.h"

#include "network/http/http_message.h"
#include "network/tcp/tcp_message_old.h"
#include "network/ssl/ssl_message.h"
#include "network/ssl_tcp/custommessage.h"


TradeServer::TradeServer()
	:req_worker_(recvq_, boost::bind(&TradeServer::ProcessRequest, this, _1), gConfigManager::instance().m_nTcpWorkerThreadPool)
	,resp_worker_(sendq_, boost::bind(&TradeServer::ProcessResponse, this, _1), gConfigManager::instance().m_nTcpSendThreadPool)
	
{
}





void TradeServer::start()
{
	req_worker_.start();
	resp_worker_.start();
}

void TradeServer::stop()
{
	req_worker_.stop();
	resp_worker_.stop();
}

TradeServer::req_queue_type& TradeServer::recv_queue()
{
	return recvq_;
}

// ����Ӧ��
bool TradeServer::ProcessResponse(IMessage* resp)
{
	resp->GetSession()->write(resp);
	return true;
}

// ���������������̺߳�������Ҫʹ�ù�������
bool TradeServer::ProcessRequest(IMessage* req)
{
	std::string SOH = "\x01";

	Trade::TradeLog::LogLevel logLevel = Trade::TradeLog::INFO_LEVEL;

	MSG_HEADER binMsgHeader;
	memcpy(&binMsgHeader, req->GetMsgHeader().data(), req->GetMsgHeaderSize());

	std::string sysNo = "";
	std::string busiType = "";
	int nBusiType;
	std::string sysVer = "";
	std::string funcId = "";
	int nFuncId = binMsgHeader.FunctionNo;
	std::string account = "";
	std::string clientIp = "";

	std::string gatewayIp = "";
	std::string gatewayPort = "";
	std::string counterIp = "";
	std::string counterPort = "";
	std::string counterType = "";

	
	boost::posix_time::ptime ptBeginTime;
	std::string beginTime = "";
	int runtime = 0;


	std::string request = req->GetMsgContentString();
	std::string response = "";
	int status = 0;
	std::string errCode = "";
	std::string errMsg = "";


	

	// �����Ϣ���Ͳ�����������
	if (binMsgHeader.MsgType != MSG_TYPE_REQUEST)
	{
		logLevel = Trade::TradeLog::ERROR_LEVEL;

		errCode = boost::lexical_cast<std::string>(MSG_HEADER_ERROR);
		errMsg = gError::instance().GetErrMsg(MSG_HEADER_ERROR);

		response = "1" + SOH + "2" + SOH;
		response += "cssweb_code";
		response += SOH;
		response += "cssweb_msg";
		response += SOH;
		response += errCode;
		response += SOH;
		response += errMsg;
		response += SOH;

		
		goto finish;
	}

	// �ͻ�����������
	if (nFuncId == FUNCTION_HEARTBEAT)
	{
		response = "heartbeat";

		goto finish;
	}

	

	
	if (!GetSysNoAndBusiType(request, sysNo, busiType, sysVer, account, funcId, clientIp))
	{
		logLevel = Trade::TradeLog::ERROR_LEVEL;

		errCode = boost::lexical_cast<std::string>(PARAM_ERROR);
		errMsg = gError::instance().GetErrMsg(PARAM_ERROR);

		response = "1" + SOH + "2" + SOH;
		response += "cssweb_code";
		response += SOH;
		response += "cssweb_msg";
		response += SOH;
		response += errCode;
		response += SOH;
		response += errMsg;
		response += SOH;

		goto finish;
	}

	nBusiType = boost::lexical_cast<int>(busiType);

	// ��ʼ����̨����
	if (req->GetSession()->counterConnect == NULL)
	{
		Counter * counter = NULL;
		counter = g_ConnectManager.GetServer(sysNo, gConfigManager::instance().ConvertIntToBusiType(nBusiType), "0000");
		if (counter == NULL)
		{
			logLevel = Trade::TradeLog::ERROR_LEVEL;

			errCode = boost::lexical_cast<std::string>(CONFIG_ERROR);
			errMsg = gError::instance().GetErrMsg(CONFIG_ERROR);

			response = "1" + SOH + "2" + SOH;
			response += "cssweb_code";
			response += SOH;
			response += "cssweb_msg";
			response += SOH;
			response += errCode;
			response += SOH;
			response += errMsg;
			response += SOH;


			goto finish;
		}
				
		switch(counter->m_eCounterType)
		{
		case CT_HS_T2:
			req->GetSession()->counterConnect = new TradeBusinessT2();
			break;
		case CT_HS_COM:
			break;
		case CT_JZ_WIN:
			req->GetSession()->counterConnect = new TradeBusiness();
			break;
		case CT_JZ_LINUX:
			break;
		case CT_DINGDIAN:
			req->GetSession()->counterConnect = new TradeBusinessDingDian();
			break;
		case CT_JSD:
			{
			req->GetSession()->counterConnect = new CSywgConnect();
			
			break;
			}
		case CT_XINYI:
			{
			req->GetSession()->counterConnect = new CTCPClientSync();
			
			break;
			}
		default:
			break;
		}

		req->GetSession()->counterConnect->SetCounterServer(counter);

		counterIp = counter->m_sIP;
		counterPort = boost::lexical_cast<std::string>(counter->m_nPort);
		counterType = GetCounterType(counter->m_eCounterType);
	}
	else
	{
		counterIp = req->GetSession()->counterConnect->m_Counter->m_sIP;
		counterPort = boost::lexical_cast<std::string>(req->GetSession()->counterConnect->m_Counter->m_nPort);
		counterType = GetCounterType(req->GetSession()->counterConnect->m_Counter->m_eCounterType);
	}

	
	int serverCount = g_ConnectManager.GetServerCount(sysNo, gConfigManager::instance().ConvertIntToBusiType(nBusiType), "0000");
	if (serverCount == 0)
	{
	}



	// ������Ҫ�������ؾ���͹����л�
	// ҵ�����Ի���Ҫ������
	/*
	* ���û�����ӣ��������ӣ���������
	* �����������ʧ�ܣ���ѯÿһ����������������з���������ʧ�ܣ����ش���
	* ��ǰ����ʧЧ��send����ʧ�ܣ�����
	*/


	
	
	// ҵ������
	ptBeginTime = boost::posix_time::microsec_clock::local_time();
	beginTime = boost::gregorian::to_iso_extended_string(ptBeginTime.date()) + " " + boost::posix_time::to_simple_string(ptBeginTime.time_of_day());;

	for (int retry=0; retry<1; retry++)
	{
		bool bConnect = false;
	
		// ��ѯÿһ��������
		for (int i=0; i<serverCount; i++)
		{
			if (req->GetSession()->counterConnect->IsConnected())
			{
				// �ѽ������ӣ�����ѭ��
				bConnect = true;
				break;
			}
			else
			{
				ptBeginTime = boost::posix_time::microsec_clock::local_time();
				beginTime = boost::gregorian::to_iso_extended_string(ptBeginTime.date()) + " " + boost::posix_time::to_simple_string(ptBeginTime.time_of_day());;

				if (req->GetSession()->counterConnect->CreateConnect())
				{
					// �������ӳɹ�������ѭ��
					bConnect = true;
					break;
				}
				else
				{
					// ��������ʧ��
					// ��ѯ�㷨��������һ��������
					bConnect = false;

					Counter * counter = NULL;
					counter = g_ConnectManager.GetServer(sysNo, gConfigManager::instance().ConvertIntToBusiType(nBusiType), "0000");
					req->GetSession()->counterConnect->SetCounterServer(counter);

					counterIp = counter->m_sIP;
					counterPort = boost::lexical_cast<std::string>(counter->m_nPort);
					counterType = GetCounterType(counter->m_eCounterType);
					
					logLevel = Trade::TradeLog::ERROR_LEVEL;

					errCode = boost::lexical_cast<std::string>(CONNECT_COUNTER_ERROR);
					errMsg = gError::instance().GetErrMsg(CONNECT_COUNTER_ERROR);

					boost::posix_time::ptime ptEndTime = boost::posix_time::microsec_clock::local_time();
					runtime = (ptEndTime - ptBeginTime).total_microseconds();// ΢����

					req->Log(Trade::TradeLog::ERROR_LEVEL, sysNo, sysVer, busiType, funcId, account, clientIp, request, response, status, errCode, errMsg, beginTime, runtime, gatewayIp, gatewayPort, counterIp, counterPort, counterType);
					gFileLogManager::instance().push(req->log);
				}
			}
		} // end for (int i=0; i<serverCount; i++)

		// ���з��������Ӳ���
		if (!bConnect)
		{
			logLevel = Trade::TradeLog::ERROR_LEVEL;

			errCode = boost::lexical_cast<std::string>(CONNECT_ALL_COUNTER_ERROR);
			errMsg = gError::instance().GetErrMsg(CONNECT_ALL_COUNTER_ERROR);

			response = "1" + SOH + "2" + SOH;
			response += "cssweb_code";
			response += SOH;
			response += "cssweb_msg";
			response += SOH;
			response += errCode;
			response += SOH;
			response += errMsg;
			response += SOH;

			
			goto finish;
		}
		
		// ����ҵ��ҵ��ʧ�ܻ�ɹ�����ɹ��ģ�ֻ��ͨ��ʧ�ܲ���Ҫ����
		if (req->GetSession()->counterConnect->Send(request, response, status, errCode, errMsg))
		{
			logLevel = Trade::TradeLog::INFO_LEVEL;

			boost::posix_time::ptime ptEndTime = boost::posix_time::microsec_clock::local_time();
			runtime = (ptEndTime - ptBeginTime).total_microseconds();// ΢����

			//req->Log(Trade::TradeLog::, sysNo, sysVer, busiType, funcId, account, clientIp, request, response, status, errCode, errMsg, beginTime, runtime, gatewayIp, gatewayPort, counterIp, counterPort, counterType);

			break;
		}
		else
		{
			// ͨ��ʧ��,��ʼ����
			// ����һ�Σ�дһ����־
			// fileLog.push(req->log)
			//req->GetSession()->counterConnect->counter->m_eCounterType
			
			
		}
	} // end for retry
	



finish:

	IMessage * resp = NULL;
	
	std::vector<char> msgHeader;

	switch(req->msgType)
	{
	case MSG_TYPE_HTTP:
		{
		resp = new http_message();
		}
		break;
	case MSG_TYPE_TCP_OLD:
		{
		resp = new tcp_message_old();
		}
		break;
	case MSG_TYPE_SSL_PB:
		{
		resp = new ssl_message();
		}
		break;
	case MSG_TYPE_TCP_NEW:
		{
		resp = new CustomMessage();

		MSG_HEADER binRespMsgHeader;
		binRespMsgHeader.CRC = 0;
		binRespMsgHeader.FunctionNo = nFuncId;
		binRespMsgHeader.MsgContentSize = response.size();
		binRespMsgHeader.MsgType = MSG_TYPE_RESPONSE_END;
		binRespMsgHeader.zip = 0;

		//msgHeader.resize(sizeof(MSG_HEADER));

		memcpy(&(resp->m_MsgHeader.front()), &binRespMsgHeader, sizeof(MSG_HEADER));
		}
		break;
	}

	// ������־��Ϣ
	req->Log(logLevel, sysNo, sysVer, busiType, funcId, account, clientIp, request, response, status, errCode, errMsg, beginTime, runtime, gatewayIp, gatewayPort, counterIp, counterPort, counterType);
	resp->log = req->log; 


	// ���ûỰ
	resp->SetSession(req->GetSession());

	
	
	// ������Ϣ����
	resp->SetMsgContent(response);


	

	// ������Ϣͷ
	resp->SetMsgHeader(msgHeader);

	
	

	// �ͷ�����
	req->destroy();

	sendq_.push(resp);

	
	return true;
}

bool TradeServer::GetSysNoAndBusiType(std::string& request, std::string& sysNo, std::string& busiType, std::string& sysVer, std::string& account, std::string& funcId, std::string& clientIp)
{
	
	std::string SOH = "\x01";

	std::vector<std::string> keyvalues;
	boost::split(keyvalues, request, boost::is_any_of(SOH));


	for (std::vector<std::string>::iterator it = keyvalues.begin(); it != keyvalues.end(); it++)
	{
		std::string keyvalue = *it;


		if (keyvalue.empty())
			break;

		std::vector<std::string> kv;
		boost::split(kv, keyvalue, boost::is_any_of("="));

		std::string key = "";
		if (!kv[0].empty())
			key = kv[0];

		std::string value = "";
		if (!kv[1].empty())
			value = kv[1];


		if (key == "cssweb_sysNo")
			sysNo = value;

		if (key == "cssweb_busiType")
			busiType = value;

		if (key == "cssweb_sysVer")
		{
			sysVer = value;
		}

		if (key == "cssweb_funcid")
		{
			funcId = value;
		}
		
		if (key == "cssweb_account")
		{
			account = value;
		}

	}

	if (sysNo.empty() || busiType.empty())
		return false;

	return true;
}

std::string TradeServer::GetCounterType(COUNTER_TYPE counterType)
{
	switch (counterType)
	{
	case CT_HS_T2:
		return "1";
	case CT_HS_COM:
		return "2";
	case CT_JZ_WIN:
		return "3";
	case CT_JZ_LINUX:
		return "4";
	case CT_DINGDIAN:
		return "5";
	case CT_JSD:
		return "6";
	case CT_XINYI:
		return "7";
	default:
		return "0";
	}
}