#ifndef ERROR_CODE_MSG_H
#define ERROR_CODE_MSG_H

#include <map>
#include <string>
#include <boost/thread/detail/singleton.hpp>

#define CONNECT_COUNTER_ERROR 1000
#define CONNECT_ALL_COUNTER_ERROR 1001
#define PARAM_ERROR 1002
#define MSG_HEADER_ERROR 1003
#define CONFIG_ERROR 1004
#define BUSI_CRYPT_ERROR 1005

class Error
{
private:
	std::map<int, std::string> err;

public:
	void init()
	{
		err[CONNECT_COUNTER_ERROR] = "���ӹ�̨ʧ��";
		err[CONNECT_ALL_COUNTER_ERROR] = "�������й�̨ʧ��";
		err[PARAM_ERROR] = "�����������δ��¼";
		err[MSG_HEADER_ERROR] = "��Ϣͷ����";
		err[CONFIG_ERROR] = "�����ļ�����";
		err[BUSI_CRYPT_ERROR] = "ҵ���ӽ��ܴ���";
	}

	std::string GetErrMsg(int errCode)
	{
		return err[errCode];
	}
};
typedef boost::detail::thread::singleton<Error> gError;

#endif