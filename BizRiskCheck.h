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

#define SQL_LENGTH             512          //语句长度
#define HK_DATA_XA            "XaMemHQ"     //内存行情xa
#define COS_DATA_XA           "XaDskDB"     //COS数据库xa
//#define STK_DATA_XA           "XaStkDB"     //订单数据库xa
//#define OPT_DATA_XA           "XaOptDB"     //期权数据库xa

#define RISK_SHM_NAME         "RULE_UPDATE" //共享内存名字

//--------------------------------------------------
// 类CBizRiskCheckInit：风控检查初始化基类
//--------------------------------------------------
class CBizRiskCheckInit : public IBizRiskCheck
{
  DECLARE_DYNCREATE(CBizRiskCheckInit)

public:
  CBizRiskCheckInit(void) {};
  ~CBizRiskCheckInit(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int SetServiceBizEnv(CObjectPtr<IServiceBizEnv> &ptrServiceBizEnv);
  virtual int RiskRulesUpdateNote();
  virtual int RiskCheckDataInit();
  virtual int RiskCheckImplement(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo);
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

public:
  //数据库操作函数
  int DBEngineInit(char *p_pszXaName);
  int DoRiskDataSQL(char * p_pszRiskValue, const char * p_pszSqlBuff);

  //是否根据交易市场检查
  int IsExchangeCheck(char *p_pszClassParm, char p_chSrcEx);

  //阈值解析函数 siMultiple:阈值扩大倍数，便于带百分比或者有小数点的比较
  int RiskpThresholdResolve(ST_RULE_INFO &p_refstCuacctRuleInfo, long long p_llInputValue, long p_lMultiple = 1);

  //内存行情当前价格
  int GetCurrPriceFromMemMkt(long long &p_refllCurPrice, char p_chExChange, char * p_pszTrdCode);
  //数据库行情信息表读取当前价

  int GetCurrPriceFromDataBase(long long &p_refllCurPrice, ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, char* p_pszBoad, char* p_pszTrdCode);

  //集中交易证券行为转换
  int StkBiztoBsFlag(const int p_iStkBiz,char *p_szBsFlag,int p_iBsFlagSize);

  //集中交易交易市场转换
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

  std::string                    m_strErrorMsg;   //子类用来获取调用继承的公有函数的错误信息
  short                          m_siLoadRules;   //用了判断风控规则表是否已修改

private:
  CObjectPtr<IDataRiskInfoLog>   m_ptrDataRiskInfoLog;
  CObjectPtr<IDaoRiskInfoLog>    m_ptrDaoRiskInfoLog;

  CObjectPtr<IPacketMap>         m_ptrPacketMapMake;
  CObjectPtr<IMsgQueue>          m_ptrMsgQueuePub;

  long long                      m_llLastLogSnTime; 
  std::map<int, ST_RULE_INFO>    m_mapRulesInfo;                 //风险规则-规则信息
  std::map<std::string, std::vector<int> > m_mapCuacctRules;     //资产账户-风险规则
};


//--------------------------------------------------
// 类CBizRiskTradeFrequency：每秒交易频率
//--------------------------------------------------
class CBizRiskTradeFrequency : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskTradeFrequency)

public:
  CBizRiskTradeFrequency(void) {};
  ~CBizRiskTradeFrequency(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  //int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// 类CBizRiskTradeAmt：交易金额检查
//--------------------------------------------------
class CBizRiskTradeAmt : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskTradeAmt)

public:
  CBizRiskTradeAmt(void) {};
  ~CBizRiskTradeAmt(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// 类CBizRiskTradeNum：交易数量检查
//--------------------------------------------------
class CBizRiskTradeNum : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskTradeNum)

public:
  CBizRiskTradeNum(void) {};
  ~CBizRiskTradeNum(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// 类CBizRiskTradeFactorNum：条件单交易次数检查
//--------------------------------------------------
class CBizRiskFactorTradeTimes : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskFactorTradeTimes)

public:
  CBizRiskFactorTradeTimes(void) {};
  ~CBizRiskFactorTradeTimes(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  //int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};


//--------------------------------------------------
// 类CBizRiskStockTradeFactorNum：个股条件单交易数量检查
//--------------------------------------------------
class CBizRiskStockFactorTradeTimes : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskStockFactorTradeTimes)

public:
  CBizRiskStockFactorTradeTimes(void) {};
  ~CBizRiskStockFactorTradeTimes(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  //int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};


//--------------------------------------------------
// 类CBizRiskMatchAmt：单向净金额(成交金额)检查
//--------------------------------------------------
class CBizRiskMatchAmt : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskMatchAmt)

public:
  CBizRiskMatchAmt(void) {};
  ~CBizRiskMatchAmt(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// 类CBizRiskTradeTimes：交易数量检查
//--------------------------------------------------
class CBizRiskTradeTimes : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskTradeTimes)

public:
  CBizRiskTradeTimes(void) {};
  ~CBizRiskTradeTimes(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// 类CBizRiskStockTradeTimes：个股交易次数
//--------------------------------------------------
class CBizRiskStockTradeTimes : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskStockTradeTimes)

public:
  CBizRiskStockTradeTimes(void) {};
  ~CBizRiskStockTradeTimes(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// 类CBizRiskStockTradeTimes：个股单向金额
//--------------------------------------------------
class CBizRiskStockSingleAmt : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskStockSingleAmt)

public:
  CBizRiskStockSingleAmt(void) {};
  ~CBizRiskStockSingleAmt(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// 类CBizRiskDeclareRatio：账户报撤比
//--------------------------------------------------
class CBizRiskDeclareRatio : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskDeclareRatio)

public:
  CBizRiskDeclareRatio(void) {};
  ~CBizRiskDeclareRatio(void) {};

  // 继承IObject的操作方法
  virtual int Initialize(void);
  virtual int Uninitialize(void);

public:
  virtual int RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);

private:
  int RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
  int RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo);
};

//--------------------------------------------------
// 类CBizRiskTradePrice：交易价格百分比
//--------------------------------------------------
class CBizRiskTradePrice : public CBizRiskCheckInit
{
  DECLARE_DYNCREATE(CBizRiskTradePrice)

public:
  CBizRiskTradePrice(void) {};
  ~CBizRiskTradePrice(void) {};

  // 继承IObject的操作方法
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