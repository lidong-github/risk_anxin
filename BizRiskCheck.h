#if !defined(__MA_RISK_CHECK_H__)
#define __MA_RISK_CHECK_H__

#include "iBizRiskCheck.h"
#include "iDaoRiskCuacct.h"
#include "iDaoRiskRuleCfg.h"
#include "iDaoRiskClassInfo.h"
#include "iDaoRiskInfoLog.h"
#include "iDaoRiskDataQuery.h"

#include "maMktData.h"
#include "maMsgData.h"
#include "maCosOrderComm.h"

#if defined(OS_IS_WINDOWS)
#include <unordered_map>
#else
#include <tr1/unordered_map>
#include <tr1/memory>
#endif

BGN_NAMESPACE_MA

#define EXCHANGE_IS_ALL        0
#define EXCHANGE_MATCH         1
#define EXCHANGE_NOT_MATCH     2

#define STK_BIZ_JZJY_BUY       "0B"
#define STK_BIZ_JZJY_SALL      "0S"

#define SQL_LENGTH             512          //��䳤��
#define HK_DATA_XA            "XaMemHQ"     //�ڴ�����xa
#define COS_DATA_XA           "XaDskDB"     //COS���ݿ�xa
//#define STK_DATA_XA           "XaStkDB"     //�������ݿ�xa
//#define OPT_DATA_XA           "XaOptDB"     //��Ȩ���ݿ�xa

#define RISK_SHM_NAME         "RULE_UPDATE" //�����ڴ�����

//--------------------------------------------------
// ��CBizRiskCheckInit����ؼ���ʼ������
//--------------------------------------------------
class CBizRiskCheckInit : public IBizRiskCheck
{
  DECLARE_DYNCREATE(CBizRiskCheckInit)

public:
  CBizRiskCheckInit(void) {};
  ~CBizRiskCheckInit(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int SetServiceBizEnv(CObjectPtr<IServiceBizEnv> &ptrServiceBizEnv);
  virtual int RiskRulesUpdateNote();
  virtual int RiskCheckDataInit();
  virtual int RiskCheckImplement(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo);
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

public:
  //���ݿ��������
  int DBEngineInit(char *p_pszXaName);
  int DoRiskDataSQL(char * p_pszRiskValue, const char * p_pszSqlBuff);

  //�Ƿ���ݽ����г����
  int IsExchangeCheck(char *p_pszClassParm, char p_chSrcEx);

  //��ֵ�������� siMultiple:��ֵ�����������ڴ��ٷֱȻ�����С����ıȽ�
  int RiskpThresholdResolve(ST_RULE_INFO &p_refstCuacctRuleInfo, long long p_llInputValue, long p_lMultiple = 1);

  //�ڴ����鵱ǰ�۸�
  int GetCurrPriceFromMemMkt(long long &p_refllCurPrice, char p_chExChange, char * p_pszTrdCode);
  //���ݿ�������Ϣ���ȡ��ǰ��

  int GetCurrPriceFromDataBase(long long &p_refllCurPrice, ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, char* p_pszBoad, char* p_pszTrdCode);

  //���н���֤ȯ��Ϊת��
  int StkBiztoBsFlag(const int p_iStkBiz,char *p_szBsFlag,int p_iBsFlagSize);

  //���н��׽����г�ת��
  int StkBdToMkt(char *p_szStkBd, char &p_chMarket);

private:
  int CheckLoadRules();
  int InsertRiskInfoLog(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, int p_iRuleId, int p_iRiskLevel, const char *p_pszRiskRemark);
  int PubRiskWarnNote(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, int p_iRiskLevel, const char *p_pszRiskRemark);
  void PrintMap(int p_iRetCode);

public:
  CObjectPtr<IDBEngine>          m_ptrDBEngine;
  CObjectPtr<IServiceBizEnv>     m_ptrServiceBizEnv;
  CObjectPtr<IDaoRiskDataQuery>  m_ptrDaoRiskData;
  CObjectPtr<IDaoRiskDataQuery>  m_ptrDaoRiskDataBak;

  std::string                    m_strErrorMsg;   //����������ȡ���ü̳еĹ��к����Ĵ�����Ϣ
  short                          m_siLoadRules;   //�����жϷ�ع�����Ƿ����޸�

private:
  CObjectPtr<IDataRiskInfoLog>   m_ptrDataRiskInfoLog;
  CObjectPtr<IDaoRiskInfoLog>    m_ptrDaoRiskInfoLog;

  CObjectPtr<IPacketMap>         m_ptrPacketMapMake;
  CObjectPtr<IMsgQueue>          m_ptrMsgQueuePub;

  long long                      m_llLastLogSnTime; 
  std::map<int, ST_RULE_INFO>    m_mapRulesInfo;                 //���չ���-������Ϣ
  std::map<std::string, std::vector<int> > m_mapCuacctRules;     //�ʲ��˻�-���չ���
};


//--------------------------------------------------
// ��CBizRiskTradeFrequency��ÿ�뽻��Ƶ��
//--------------------------------------------------
class CBizRiskTradeFrequency : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskTradeFrequency)

public:
  CBizRiskTradeFrequency(void) {};
  ~CBizRiskTradeFrequency(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  //int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// ��CBizRiskTradeAmt�����׽����
//--------------------------------------------------
class CBizRiskTradeAmt : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskTradeAmt)

public:
  CBizRiskTradeAmt(void) {};
  ~CBizRiskTradeAmt(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// ��CBizRiskTradeNum�������������
//--------------------------------------------------
class CBizRiskTradeNum : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskTradeNum)

public:
  CBizRiskTradeNum(void) {};
  ~CBizRiskTradeNum(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// ��CBizRiskTradeFactorNum�����������״������
//--------------------------------------------------
class CBizRiskFactorTradeTimes : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskFactorTradeTimes)

public:
  CBizRiskFactorTradeTimes(void) {};
  ~CBizRiskFactorTradeTimes(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  //int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};


//--------------------------------------------------
// ��CBizRiskStockTradeFactorNum�����������������������
//--------------------------------------------------
class CBizRiskStockFactorTradeTimes : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskStockFactorTradeTimes)

public:
  CBizRiskStockFactorTradeTimes(void) {};
  ~CBizRiskStockFactorTradeTimes(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  //int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};


//--------------------------------------------------
// ��CBizRiskMatchAmt�����򾻽��(�ɽ����)���
//--------------------------------------------------
class CBizRiskMatchAmt : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskMatchAmt)

public:
  CBizRiskMatchAmt(void) {};
  ~CBizRiskMatchAmt(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// ��CBizRiskTradeTimes�������������
//--------------------------------------------------
class CBizRiskTradeTimes : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskTradeTimes)

public:
  CBizRiskTradeTimes(void) {};
  ~CBizRiskTradeTimes(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// ��CBizRiskStockTradeTimes�����ɽ��״���
//--------------------------------------------------
class CBizRiskStockTradeTimes : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskStockTradeTimes)

public:
  CBizRiskStockTradeTimes(void) {};
  ~CBizRiskStockTradeTimes(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// ��CBizRiskStockTradeTimes�����ɵ�����
//--------------------------------------------------
class CBizRiskStockSingleAmt : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskStockSingleAmt)

public:
  CBizRiskStockSingleAmt(void) {};
  ~CBizRiskStockSingleAmt(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// ��CBizRiskDeclareRatio���˻�������
//--------------------------------------------------
class CBizRiskDeclareRatio : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskDeclareRatio)

public:
  CBizRiskDeclareRatio(void) {};
  ~CBizRiskDeclareRatio(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// ��CBizRiskTradePrice�����׼۸�ٷֱ�
//--------------------------------------------------
class CBizRiskTradePrice : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskTradePrice)

public:
  CBizRiskTradePrice(void) {};
  ~CBizRiskTradePrice(void) {};

  // �̳�IObject�Ĳ�������
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

END_NAMESPACE_MA

#endif  // __MA_RISK_CHECK_H__