//--------------------------------------------------------------------------------------------------
// 版权声明：本程序模块属于金证微内核架构平台(KMAP)的一部分
//           金证科技股份有限公司  版权所有
//
// 文件名称：BizRiskCheck.cpp
// 模块名称：风控检查
// 模块描述：
// 开发作者：laijt
// 创建日期：2018-04-25
// 模块版本：1.0.000.000
// 2018-06-06    002.000.000  laijt     将读取的数据由COS数据库修改为快订数据库，暂时只检查现货、信用
// 2018-08.20    002.000.001  汪振鸣     修改风控加载逻辑
// 2019-06.03    002.000.002  李东       增加对集中交易的事前风控，对获取xa变为动态
// 2019-10-25    002.000.003  李东       增加单笔交易数量、条件单交易次数、个股条件单交易次数。修复市价的判断方式。
//--------------------------------------------------------------------------------------------------
#include "BizRiskCheck.h"
#include "maGlobal.h"
#include "maException.h"
#include "xsdk_datetime.h"
#include "maErrorBiz.h"
#include "maDefineBiz.h"
#include "BizFuncApi.h"
#include "xsdk_string.h"

#if defined(OS_IS_WINDOWS)
  #include <unordered_map>
#else
  #include <tr1/unordered_map>
#endif


USE_NAMESPACE_MA
IMPLEMENT_DYNCREATE(CBizRiskCheckInit, IBizRiskCheck, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskTradeFrequency, CBizRiskCheckInit, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskTradeAmt, CBizRiskCheckInit, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskTradeNum, CBizRiskCheckInit, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskFactorTradeTimes, CBizRiskCheckInit, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskStockFactorTradeTimes, CBizRiskCheckInit, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskMatchAmt, CBizRiskCheckInit, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskTradeTimes, CBizRiskCheckInit, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskStockTradeTimes, CBizRiskCheckInit, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskStockSingleAmt, CBizRiskCheckInit, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskDeclareRatio, CBizRiskCheckInit, _V("001.000.001"))
IMPLEMENT_DYNCREATE(CBizRiskTradePrice, CBizRiskCheckInit, _V("001.000.001"))

BOOL IsLimitPrice(short p_siStkBizAcction)
{
  if (STK_BIZ_ACTION_LIMIT == p_siStkBizAcction || STK_BIZ_ACTION_LIMIT_GFD_OPT == p_siStkBizAcction
    || STK_BIZ_ACTION_LIMIT_FOK_OPT == p_siStkBizAcction || STK_BIZ_ACTION_LIMIT_GFD == p_siStkBizAcction
    || STK_BIZ_ACTION_LIMIT_FOK == p_siStkBizAcction || STK_BIZ_ACTION_LIMIT_IOC == p_siStkBizAcction)
  {
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

//--------------------------------------
//类CBizRiskCheckInit的实现-风控初始化基类
//--------------------------------------
int CBizRiskCheckInit::Initialize(void)
{
  int iRetCode = MA_OK;

  m_mapRulesInfo.clear();
  m_mapCuacctRules.clear();

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();
  m_strErrorMsg.clear();
  m_llLastLogSnTime = 0;
  m_siLoadRules = 0;

  return iRetCode;
}

int CBizRiskCheckInit::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskCheckInit::SetServiceBizEnv(CObjectPtr<IServiceBizEnv> &ptrServiceBizEnv)
{
  int iRetCode = MA_OK;

  _ma_try
  { 
    if(ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM,
        "CBizRiskCheckInit::{@1} invalid parameter [ptrServiceBizEnv] is NULL", &_P(__FUNCTION__));
    }

    if (m_ptrPacketMapMake.IsNull())
    {
      if (m_ptrPacketMapMake.Create("CPacketMap").IsNull())
      {
        _ma_throw ma::CFuncException(MA_ERROR_INIT_OBJECT, 
          "CBizRiskCheckInit::{@1} Create IMsgQueue fail",  &_P(__FUNCTION__));
      }
    }

    if (m_ptrDaoRiskData.IsNull())
    {
      if ( m_ptrDaoRiskData.Create("CDaoRiskDataQuery").IsNull())
      {
        _ma_throw ma::CBizException(MA_ERROR_UNDEFINED_OBJECT,
          "CBizRiskCheckInit[{@1}] create object m_ptrDaoRiskData failed", &_P(__LINE__));
      }
    }

    if (m_ptrDaoRiskDataBak.IsNull())
    {
      if ( m_ptrDaoRiskDataBak.Create("CDaoRiskDataQuery").IsNull())
      {
        _ma_throw ma::CBizException(MA_ERROR_UNDEFINED_OBJECT,
          "CBizRiskCheckInit[{@1}] create object m_ptrDaoRiskDataBak failed", &_P(__LINE__));
      }
    }

    m_ptrServiceBizEnv = ptrServiceBizEnv;
  }
  _ma_catch_finally
  {
  }

  return iRetCode;
}

int CBizRiskCheckInit::StkBiztoBsFlag(const int p_iStkBiz,char *p_szBsFlag,int p_iBsFlagSize)
{
  if (100 == p_iStkBiz) strncpy(p_szBsFlag, "0B" ,p_iBsFlagSize);
  else if (101 == p_iStkBiz) strncpy(p_szBsFlag, "0S"  ,p_iBsFlagSize);
  else if (103 == p_iStkBiz) strncpy(p_szBsFlag, "0D"  ,p_iBsFlagSize);
  else if (104 == p_iStkBiz) strncpy(p_szBsFlag, "0D"  ,p_iBsFlagSize);
  else if (106 == p_iStkBiz) strncpy(p_szBsFlag, "0J"  ,p_iBsFlagSize);
  else if (107 == p_iStkBiz) strncpy(p_szBsFlag, "0Y"  ,p_iBsFlagSize);
  else if (108 == p_iStkBiz) strncpy(p_szBsFlag, "0E"  ,p_iBsFlagSize);
  else if (110 == p_iStkBiz) strncpy(p_szBsFlag, "0n"  ,p_iBsFlagSize);
  else if (111 == p_iStkBiz) strncpy(p_szBsFlag, "0p"  ,p_iBsFlagSize);
  else if (113 == p_iStkBiz) strncpy(p_szBsFlag, "0P" ,p_iBsFlagSize);
  else if (130 == p_iStkBiz) strncpy(p_szBsFlag, "2B" ,p_iBsFlagSize);
  else if (131 == p_iStkBiz) strncpy(p_szBsFlag, "2S" ,p_iBsFlagSize);
  else if (150 == p_iStkBiz) strncpy(p_szBsFlag, "0R"  ,p_iBsFlagSize);
  else if (151 == p_iStkBiz) strncpy(p_szBsFlag, "0Q"  ,p_iBsFlagSize);
  else if (152 == p_iStkBiz) strncpy(p_szBsFlag, "0D"  ,p_iBsFlagSize);
  else if (153 == p_iStkBiz) strncpy(p_szBsFlag, "0J"  ,p_iBsFlagSize);
  else if (160 == p_iStkBiz) strncpy(p_szBsFlag, "0G"  ,p_iBsFlagSize);
  else if (161 == p_iStkBiz) strncpy(p_szBsFlag, "0H"  ,p_iBsFlagSize);
  else if (162 == p_iStkBiz) strncpy(p_szBsFlag, "0M"  ,p_iBsFlagSize);
  else if (163 == p_iStkBiz) strncpy(p_szBsFlag, "0N"  ,p_iBsFlagSize);
  else if (164 == p_iStkBiz) strncpy(p_szBsFlag, "0R"  ,p_iBsFlagSize);
  else if (165 == p_iStkBiz) strncpy(p_szBsFlag, "0Q"  ,p_iBsFlagSize);
  else if (170 == p_iStkBiz) strncpy(p_szBsFlag, "03" ,p_iBsFlagSize);
  else if (171 == p_iStkBiz) strncpy(p_szBsFlag, "04" ,p_iBsFlagSize);
  else if (180 == p_iStkBiz) strncpy(p_szBsFlag, "0K" ,p_iBsFlagSize);
  else if (181 == p_iStkBiz) strncpy(p_szBsFlag, "01" ,p_iBsFlagSize);
  else if (182 == p_iStkBiz) strncpy(p_szBsFlag, "02" ,p_iBsFlagSize);
  else if (183 == p_iStkBiz) strncpy(p_szBsFlag, "0I"  ,p_iBsFlagSize);
  else if (184 == p_iStkBiz) strncpy(p_szBsFlag, "0C" ,p_iBsFlagSize);
  else if (187 == p_iStkBiz) strncpy(p_szBsFlag, "90" ,p_iBsFlagSize);
  else if (188 == p_iStkBiz) strncpy(p_szBsFlag, "91" ,p_iBsFlagSize);
  else if (190 == p_iStkBiz) strncpy(p_szBsFlag, "05"  ,p_iBsFlagSize);
  else if (191 == p_iStkBiz) strncpy(p_szBsFlag, "0Z"  ,p_iBsFlagSize);
  else if (192 == p_iStkBiz) strncpy(p_szBsFlag, "06"  ,p_iBsFlagSize);
  else if (193 == p_iStkBiz) strncpy(p_szBsFlag, "0k"  ,p_iBsFlagSize);
  else if (194 == p_iStkBiz) strncpy(p_szBsFlag, "03" ,p_iBsFlagSize);
  else if (195 == p_iStkBiz) strncpy(p_szBsFlag, "04" ,p_iBsFlagSize);
  else if (198 == p_iStkBiz) strncpy(p_szBsFlag, "0D"  ,p_iBsFlagSize);
  else if (200 == p_iStkBiz) strncpy(p_szBsFlag, "01"  ,p_iBsFlagSize);
  else if (201 == p_iStkBiz) strncpy(p_szBsFlag, "0m"  ,p_iBsFlagSize);
  else if (202 == p_iStkBiz) strncpy(p_szBsFlag, "07"  ,p_iBsFlagSize);
  else if (203 == p_iStkBiz) strncpy(p_szBsFlag, "08"  ,p_iBsFlagSize);
  else if (204 == p_iStkBiz) strncpy(p_szBsFlag, "09"  ,p_iBsFlagSize);
  else if (205 == p_iStkBiz) strncpy(p_szBsFlag, "09"  ,p_iBsFlagSize);
  else if (230 == p_iStkBiz) strncpy(p_szBsFlag, "WB" ,p_iBsFlagSize);
  else if (231 == p_iStkBiz) strncpy(p_szBsFlag, "WS" ,p_iBsFlagSize);
  else if (232 == p_iStkBiz) strncpy(p_szBsFlag, "W1" ,p_iBsFlagSize);
  else if (233 == p_iStkBiz) strncpy(p_szBsFlag, "W2" ,p_iBsFlagSize);
  else if (300 == p_iStkBiz) strncpy(p_szBsFlag, "0U"  ,p_iBsFlagSize);
  else if (301 == p_iStkBiz) strncpy(p_szBsFlag, "0V"  ,p_iBsFlagSize);
  else if (302 == p_iStkBiz) strncpy(p_szBsFlag, "0W"  ,p_iBsFlagSize);
  else if (303 == p_iStkBiz) strncpy(p_szBsFlag, "0X"  ,p_iBsFlagSize);
  else if (330 == p_iStkBiz) strncpy(p_szBsFlag, "0Z"  ,p_iBsFlagSize);
  else if (345 == p_iStkBiz) strncpy(p_szBsFlag, "00" ,p_iBsFlagSize);
  else if (800 == p_iStkBiz) strncpy(p_szBsFlag, "0D" ,p_iBsFlagSize);
  else if (801 == p_iStkBiz) strncpy(p_szBsFlag, "0S" ,p_iBsFlagSize);
  else if (803 == p_iStkBiz) strncpy(p_szBsFlag, "2X" ,p_iBsFlagSize);
  else if (804 == p_iStkBiz) strncpy(p_szBsFlag, "2Y" ,p_iBsFlagSize);
  else if (820 == p_iStkBiz) strncpy(p_szBsFlag, "86" ,p_iBsFlagSize);
  else if (821 == p_iStkBiz) strncpy(p_szBsFlag, "85" ,p_iBsFlagSize);
  else if (825 == p_iStkBiz) strncpy(p_szBsFlag, "90" ,p_iBsFlagSize);
  else if (826 == p_iStkBiz) strncpy(p_szBsFlag, "91" ,p_iBsFlagSize);
  else if (830 == p_iStkBiz) strncpy(p_szBsFlag, "2e" ,p_iBsFlagSize);
  else if (831 == p_iStkBiz) strncpy(p_szBsFlag, "2f" ,p_iBsFlagSize);
  else if (832 == p_iStkBiz) strncpy(p_szBsFlag, "2g" ,p_iBsFlagSize);
  else if (833 == p_iStkBiz) strncpy(p_szBsFlag, "2h" ,p_iBsFlagSize);
  else if (834 == p_iStkBiz) strncpy(p_szBsFlag, "1w" ,p_iBsFlagSize);
  else if (835 == p_iStkBiz) strncpy(p_szBsFlag, "1l" ,p_iBsFlagSize);
  else if (855 == p_iStkBiz) strncpy(p_szBsFlag, "1K" ,p_iBsFlagSize);
  else if (856 == p_iStkBiz) strncpy(p_szBsFlag, "1M" ,p_iBsFlagSize);
  else if (865 == p_iStkBiz) strncpy(p_szBsFlag, "1N" ,p_iBsFlagSize);
  else if (866 == p_iStkBiz) strncpy(p_szBsFlag, "1O" ,p_iBsFlagSize);
  else if (880 == p_iStkBiz) strncpy(p_szBsFlag, "1p" ,p_iBsFlagSize);
  else if (881 == p_iStkBiz) strncpy(p_szBsFlag, "1q" ,p_iBsFlagSize);
  else if (882 == p_iStkBiz) strncpy(p_szBsFlag, "1r" ,p_iBsFlagSize);
  else if (883 == p_iStkBiz) strncpy(p_szBsFlag, "1s" ,p_iBsFlagSize);
  else if (884 == p_iStkBiz) strncpy(p_szBsFlag, "1t" ,p_iBsFlagSize);
  else if (885 == p_iStkBiz) strncpy(p_szBsFlag, "1u" ,p_iBsFlagSize);
  else if (700 == p_iStkBiz) strncpy(p_szBsFlag, "0B"  ,p_iBsFlagSize);
  else if (701 == p_iStkBiz) strncpy(p_szBsFlag, "0S"  ,p_iBsFlagSize);
  else if (702 == p_iStkBiz) strncpy(p_szBsFlag, "0B"  ,p_iBsFlagSize);
  else if (703 == p_iStkBiz) strncpy(p_szBsFlag, "0S"  ,p_iBsFlagSize);
  else if (704 == p_iStkBiz) strncpy(p_szBsFlag, "0B"  ,p_iBsFlagSize);
  else if (705 == p_iStkBiz) strncpy(p_szBsFlag, "0S"  ,p_iBsFlagSize);
  else if (706 == p_iStkBiz) strncpy(p_szBsFlag, "0S"  ,p_iBsFlagSize);
  else if (707 == p_iStkBiz) strncpy(p_szBsFlag, "0B"  ,p_iBsFlagSize);
  else if (708 == p_iStkBiz) strncpy(p_szBsFlag, "0s" ,p_iBsFlagSize);
  else if (709 == p_iStkBiz) strncpy(p_szBsFlag, "0t" ,p_iBsFlagSize);
  else if (710 == p_iStkBiz) strncpy(p_szBsFlag, "0w" ,p_iBsFlagSize);
  else strncpy(p_szBsFlag, "   ", p_iBsFlagSize); 

  return MA_OK;
}

int CBizRiskCheckInit::StkBdToMkt(char *p_szStkBd, char  &p_chMarket)
{
  if (strcmp(p_szStkBd, "00") == 0) p_chMarket = '0';
  else if(strcmp(p_szStkBd, "10") == 0)  p_chMarket = '1';
  else if(strcmp(p_szStkBd, "01") == 0)  p_chMarket = '2';
  else if(strcmp(p_szStkBd, "11") == 0)  p_chMarket = '3';
  else if(strcmp(p_szStkBd, "13") == 0)  p_chMarket = '5';
  else if(strcmp(p_szStkBd, "20") == 0)  p_chMarket = '6';
  else if(strcmp(p_szStkBd, "21") == 0)  p_chMarket = '7';
  else if(strcmp(p_szStkBd, "03") == 0)  p_chMarket = 'S';
  else  p_chMarket = ' ';
  return MA_OK;
}

int CBizRiskCheckInit::DBEngineInit(char *p_pszXaName)
{
  int iRetCode = MA_OK;
  CObjectPtr<IXa> ptrXa;

  _ma_try
  {
    if (NULL == p_pszXaName)
    {
      iRetCode = MA_KO;
      m_strLastErrorText = "XaName为空";
      _ma_leave;
    }

    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_BIZ_OBJECT_INIT, 
        "CBizRiskCheckInit[{@1}] m_ptrServiceBizEnv is not init", &_P(__LINE__));
    }

    if (m_ptrServiceBizEnv->SelectXa(ptrXa, p_pszXaName) != MA_OK
      ||m_ptrServiceBizEnv->ConvertXa2DBEngine(m_ptrDBEngine, ptrXa)!= MA_OK)
    {
      _ma_throw ma::CBizException(MA_ERROR_BIZ_OBJECT_INIT, "数据引擎设置失败：Xa[{@1}]", &_P(p_pszXaName));
    }

    if (m_ptrDaoRiskData->SetDBEngine(m_ptrDBEngine) != MA_OK)
    {
      _ma_throw ma::CBizException(MA_ERROR_UNDEFINED_OBJECT,
        "CBizRiskCheckInit[{@1}] m_ptrDaoRiskData SetDBEngine failed", &_P(__LINE__));
    }
  }

  _ma_catch_finally
  {
  }

  m_strErrorMsg = m_strLastErrorText;  //子类调用该函数时，获取错误信息

  return  iRetCode;
}

int CBizRiskCheckInit::DoRiskDataSQL(char * p_pszRiskValue, const char * p_pszSqlBuff)
{
  int iRetCode = MA_OK;

  _ma_try
  {
    //if (m_ptrDaoRiskData.IsNull())
    //{
    //  if ( m_ptrDaoRiskData.Create("CDaoRiskDataQuery").IsNull()
    //    //|| m_ptrServiceBizEnv->BindDao2CurrentXa(m_ptrDaoRiskData.Ptr()) != MA_OK
    //    )
    //  {
    //    _ma_throw ma::CBizException(MA_ERROR_UNDEFINED_OBJECT,
    //      "CBizRiskCheckInit[{@1}] create object m_ptrDaoRiskData failed", &_P(__LINE__));
    //  }

    //  if (m_ptrDaoRiskData->SetDBEngine(m_ptrDBEngine) != MA_OK)
    //  {
    //    _ma_throw ma::CBizException(MA_ERROR_UNDEFINED_OBJECT,
    //      "CBizRiskCheckInit[{@1}] m_ptrDaoRiskData SetDBEngine failed", &_P(__LINE__));
    //  }
    //}
  }

  _ma_catch_finally
  {
  }

  return iRetCode;
}

int CBizRiskCheckInit::CheckLoadRules()
{
  int iRetCode = MA_OK;

  short int siLoadTimes = 0;
  IKernelEnv *pclKernelEnv = NULL;

  _ma_try
  {   
    //从共享区
    if ((pclKernelEnv = ma::g_hKernelEnv->UN_HANDLE.pclKernelEnv) == NULL
      || !pclKernelEnv->IsKindOf(RUNTIME_OBJECT(IKernelEnv)))
    {
      iRetCode = MA_KO;
      ma::ThrowError(NULL, "CBizRiskCheckInit[{@1}] CheckLoadRules() pclKernelEnv->IsKindOf() Fail", &_P(__LINE__));
      _ma_leave;
    }

    iRetCode = pclKernelEnv->ReadShareArea(RISK_SHM_NAME, &siLoadTimes, sizeof(siLoadTimes), true);
    if (MA_OK == iRetCode)
    {
      //判断值是否有变化
      //ThrowTrace(NULL, "--siLoadTimes[{@1}]  m_siLoadRules[{@2}]---", &(_P(siLoadTimes) + _P(m_siLoadRules)));
      if (siLoadTimes > m_siLoadRules)
      {
        m_siLoadRules = siLoadTimes;
        ThrowTrace(NULL, "---------{@1} 检测到风控规则有修改,重新加载---------", &_P(__FUNCTION__));        
      }
      else
      {
        iRetCode = MA_NO_DATA;
      }
    }   
    else
    {
      siLoadTimes = 1;
      m_siLoadRules = siLoadTimes;
      pclKernelEnv->CreateShareArea(RISK_SHM_NAME, sizeof(siLoadTimes), true);
      pclKernelEnv->WriteShareArea(RISK_SHM_NAME, &siLoadTimes, sizeof(siLoadTimes), true);
      iRetCode = MA_OK;
    } 
  }

  _ma_catch_finally
  {

  }
  return iRetCode;
}

int CBizRiskCheckInit::PubRiskWarnNote(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, int p_iRiskLevel, const char *p_pszRiskRemark)
{
  int iRetCode = MA_OK;

  int iValueLen = 0;
  SYSTEMTIME stCurrentTime = {0};
  long long llCurrentTime = 0;

  char szMsgId[32 + 1] = {"RISK_WARN_1234567890"};
  char szTopic[12] = {"TRADERISK0"};
  char szFuncId[8 + 1] = {"00102028"};
  char szPubKey2[16 + 1] = {"FLUX_RIGHT"};
  CMsgData clPubMsg;

  _ma_try
  {
    if (NULL == p_pszRiskRemark)
    {
      _ma_leave;
    }

    m_ptrPacketMapMake->BeginWrite();
    m_ptrPacketMapMake->SetHdrColValue("01", strlen("01"), MAP_FID_PKT_VER);
    m_ptrPacketMapMake->SetHdrColValue('2', MAP_FID_RESEND_FLAG);
    m_ptrPacketMapMake->SetHdrColValue(MAP_PKT_TYPE_BIZ, MAP_FID_PKT_TYPE);
    m_ptrPacketMapMake->SetHdrColValue(MAP_MSG_TYPE_ANS, MAP_FID_MSG_TYPE);
    m_ptrPacketMapMake->SetHdrColValue(szFuncId, strlen(szFuncId), MAP_FID_FUNC_ID);
    m_ptrPacketMapMake->SetHdrColValue(szMsgId, strlen(szMsgId), MAP_FID_MSG_ID);
    m_ptrPacketMapMake->SetHdrColValue(p_refstRiskCheckInfo.szUserSession, strlen(p_refstRiskCheckInfo.szUserSession), MAP_FID_USER_SESSION);

    //设置发布主题、发布关键字1
    m_ptrPacketMapMake->SetHdrColValue(szTopic, strlen(szTopic), MAP_FID_PUB_TOPIC);
    m_ptrPacketMapMake->SetHdrColValue(p_refstRiskCheckInfo.szCuacctCode, strlen(p_refstRiskCheckInfo.szCuacctCode), MAP_FID_PUB_KEY1);
    m_ptrPacketMapMake->SetHdrColValue(szPubKey2, strlen(szPubKey2), MAP_FID_PUB_KEY2);

    xsdk::GetCurrentTimestamp(stCurrentTime);
    xsdk::DatetimeToInt64(llCurrentTime, stCurrentTime);
    m_ptrPacketMapMake->SetHdrColValue(llCurrentTime, MAP_FID_TIMESTAMP);

    //构建第一结果集
    //m_ptrPacketMapMake->CreateTable();
    //m_ptrPacketMapMake->AddRow();
    //m_ptrPacketMapMake->SetValue(p_iRiskLevel, "8817");                          // MSG_CODE
    //m_ptrPacketMapMake->SetValue(1, "8818");                                   // MSG_LEVEL
    //m_ptrPacketMapMake->SetValue(p_pszRiskRemark, strlen(p_pszRiskRemark), "8819");  // MSG_TEXT
    //m_ptrPacketMapMake->SetValue("", 0, "8820");                               // MSG_DEBUG
    //m_ptrPacketMapMake->SaveRow();
    //m_ptrPacketMapMake->EndWrite();

    m_ptrPacketMapMake->SetValue(p_refstRiskCheckInfo.szCuacctCode, strlen(p_refstRiskCheckInfo.szCuacctCode), "8810");
    m_ptrPacketMapMake->SetValue("1", strlen("1"), "8811");
    m_ptrPacketMapMake->SetValue("127.0.0.1", strlen("127.0.0.1"), "8812");
    m_ptrPacketMapMake->SetValue("F", strlen("F"), "8813");
    m_ptrPacketMapMake->SetValue("123456", strlen("123456"), "8814");
    m_ptrPacketMapMake->SetValue(szFuncId, strlen(szFuncId), "8815");
    m_ptrPacketMapMake->SetValue(p_refstRiskCheckInfo.szCuacctCode, strlen(p_refstRiskCheckInfo.szCuacctCode), "CUACCT_CODE");
    m_ptrPacketMapMake->SetValue(p_refstRiskCheckInfo.szCuacctType, strlen(p_refstRiskCheckInfo.szCuacctType), "CUACCT_TYPE");
    m_ptrPacketMapMake->SetValue(p_iRiskLevel, "ERROR_ID");                          // MSG_CODE
    m_ptrPacketMapMake->SetValue(p_pszRiskRemark, strlen(p_pszRiskRemark), "ERROR_MSG");  // MSG_TEXT
    m_ptrPacketMapMake->EndWrite();
    m_ptrPacketMapMake->Make(clPubMsg);
    iRetCode = m_ptrServiceBizEnv->GetPubMsgQueue(m_ptrMsgQueuePub);
    if (iRetCode != MA_OK)
    {
      ThrowError(NULL, "CBizRiskCheckInit m_ptrServiceBizEnv->GetPubMsgQueue() fail, error:{@1}, return[{@2}]",
        &(_P(m_ptrServiceBizEnv->GetLastErrorText()) + _P(iRetCode)));
      _ma_leave;
    }

    iRetCode = m_ptrMsgQueuePub->Put(clPubMsg);
    if (iRetCode != MA_OK)
    {
      ThrowError(NULL, "CBizRiskCheckInit m_ptrMsgQueuePub->Put() fail, error:{@1}, return[{@2}]",
        &(_P(m_ptrServiceBizEnv->GetLastErrorText()) + _P(iRetCode)));
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
    return iRetCode;
  }
}

int CBizRiskCheckInit::InsertRiskInfoLog(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, int p_iRuleId, int p_iRiskLevel, const char *p_pszRiskRemark)
{
  int iRetCode = MA_OK;

  long long llCurrTime = 0;
  char szRiskLogSn[32 + 1] = {0};
  _ma_try
  {
    if (m_ptrDaoRiskInfoLog.IsNull())
    {
      if (m_ptrDaoRiskInfoLog.Create("CDaoRiskInfoLog").IsNull()
        || m_ptrDataRiskInfoLog.Create("CDataRiskInfoLog").IsNull()
        || m_ptrServiceBizEnv->BindDao2CurrentXa(m_ptrDaoRiskInfoLog.Ptr()) != MA_OK)
      {
        _ma_throw ma::CFuncException(MA_ERROR_UNDEFINED_OBJECT, "CBizRiskCheckInit create object {@1} failed",
          &(_P("m_ptrDaoRiskInfoLog/m_ptrDataRiskInfoLog")));
      }
    }

    m_ptrDataRiskInfoLog->Initialize();

    //保证 szRiskLogSn 的唯一性
    llCurrTime = atoll(p_refstRiskCheckInfo.szCurrTime);
    if (m_llLastLogSnTime >= llCurrTime)
    {
      m_llLastLogSnTime = m_llLastLogSnTime + 1;
    }
    else
    {
      m_llLastLogSnTime = llCurrTime;      
    }

    snprintf(szRiskLogSn, sizeof(szRiskLogSn), "%lld%d%d", m_llLastLogSnTime, p_refstRiskCheckInfo.uiCurrNodeId, GetCurrentTid());
    m_ptrDataRiskInfoLog->SetRiskLogSn(szRiskLogSn, strlen(szRiskLogSn));
    m_ptrDataRiskInfoLog->SetRuleId(p_iRuleId);
    m_ptrDataRiskInfoLog->SetCuacctCode(p_refstRiskCheckInfo.szCuacctCode, strlen(p_refstRiskCheckInfo.szCuacctCode));
    m_ptrDataRiskInfoLog->SetCuacctType(p_refstRiskCheckInfo.szCuacctType[0]);
    m_ptrDataRiskInfoLog->SetExchange(p_refstRiskCheckInfo.szExchange[0]);
    m_ptrDataRiskInfoLog->SetBoard(p_refstRiskCheckInfo.szStkbd, strlen(p_refstRiskCheckInfo.szStkbd));
    m_ptrDataRiskInfoLog->SetTrdCode(p_refstRiskCheckInfo.szTrdCode, strlen(p_refstRiskCheckInfo.szTrdCode));
    m_ptrDataRiskInfoLog->SetTrdBiz(p_refstRiskCheckInfo.siStkBiz);
    m_ptrDataRiskInfoLog->SetTrdBizAcction(p_refstRiskCheckInfo.siStkBizAction);
    m_ptrDataRiskInfoLog->SetOrderDate(p_refstRiskCheckInfo.uiOrderDate);
    m_ptrDataRiskInfoLog->SetOrderQty(p_refstRiskCheckInfo.llOrderQty);
    m_ptrDataRiskInfoLog->SetOrderPrice(p_refstRiskCheckInfo.llOrderPrice);
    m_ptrDataRiskInfoLog->SetRiskLevel(p_iRiskLevel);
    m_ptrDataRiskInfoLog->SetRiskRemark(p_pszRiskRemark, strlen(p_pszRiskRemark));
    iRetCode = m_ptrDaoRiskInfoLog->Insert(m_ptrDataRiskInfoLog);
    if (iRetCode != MA_OK)
    {
      _ma_throw ma::CFuncException(iRetCode, m_ptrDaoRiskInfoLog->GetLastErrorText());
    }
  }

  _ma_catch_finally
  {
  }
  return iRetCode;
}

//获取内存行情当前价格
int CBizRiskCheckInit::GetCurrPriceFromMemMkt(long long & p_refllCurPrice, char p_chExChange, char * p_pszTrdCode)
{
  int iRetCode = MA_OK;

  CObjectPtr<IXa> ptrXaMemHQ;
  CObjectPtr<IMsgQueue> ptrXaMsgQueueKV;
  char szMemHQKey[32] = {0};
  CMsgData clMsgData;
  ST_MKT_DATA *pstMktData = NULL;
  static char chMemHQ = 0x00; // 启用内存行情标志(0-未启用 1-启用)

  _ma_try
  {
    if (NULL == p_pszTrdCode)
    {
      return iRetCode;
    }

    if (chMemHQ == '0'
      || m_ptrServiceBizEnv->SelectXa(ptrXaMemHQ, HK_DATA_XA) != MA_OK
      || m_ptrServiceBizEnv->ConvertXa2MsgQueue(ptrXaMsgQueueKV, ptrXaMemHQ) != MA_OK
      || ptrXaMsgQueueKV.IsNull())
    {
      iRetCode = MA_NO_DATA;
      m_strLastErrorText = "内存行情尚未配置";
      _ma_leave;
    }
    chMemHQ = '1';

    snprintf(szMemHQKey, sizeof(szMemHQKey), "%c.%s", p_chExChange, p_pszTrdCode);
    iRetCode = ptrXaMsgQueueKV->Get(clMsgData, szMemHQKey);
    if (iRetCode != MA_OK)
    {
      if (MA_NO_DATA == iRetCode)
      {
        iRetCode = MA_OK; //重置OK，接下来从订单行情信息表获取
        m_strLastErrorText = "没有该品种的内存行情信息";
        ThrowWarn(NULL, "GetCurrPriceFromMemMkt[{@1}] 没有品种[{@2}]的内存行情信息", &(_P(__LINE__) + _P(szMemHQKey)));
      }
      else
      {
        m_strLastErrorText = ptrXaMsgQueueKV->GetLastErrorText(); 
      }
      _ma_leave;
    }

    if (clMsgData.Size() < sizeof(ST_MKT_DATA))
    {
      iRetCode = MA_NO_DATA;
      m_strLastErrorText = "内存行情格式错误";
      _ma_leave;
    }

    pstMktData = (ST_MKT_DATA *)clMsgData.Data();
    p_refllCurPrice = pstMktData->uiCurPrice;
  }

  _ma_catch_finally
  {
    m_strErrorMsg = m_strLastErrorText;  //子类调用该函数时，获取错误信息
    return iRetCode;
  }
}

//数据库行情信息表读取当前价
int CBizRiskCheckInit::GetCurrPriceFromDataBase(long long &p_refllCurPrice, ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, char* p_pszBoad, char* p_pszTrdCode)
{
  int iRetCode = MA_OK;

  char szSQL[SQL_LENGTH] = {0};
  CObjectPtr<IXa>         ptrXa;
  CObjectPtr<IDBEngine>   ptrDBEngine;

  _ma_try
  {
    if (NULL == p_pszTrdCode || NULL == p_pszBoad)
    {
      return iRetCode;
    }

    // CUACCT_TYPE_STOCK           '0'     //股票
    // CUACCT_TYPE_OPTION          '1'     //期权
    // CUACCT_TYPE_FUTURE          '2'     //期货
    // CUACCT_TYPE_CREDIT          '3'     //信用
    if ('0' == p_refstRiskCheckInfo.szCuacctType[0] || '3' == p_refstRiskCheckInfo.szCuacctType[0] || '1' == p_refstRiskCheckInfo.szCuacctType[0])
    {
      if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        snprintf(szSQL, SQL_LENGTH, "select CLOSING_PRICE, CURRENT_PRICE from STK_MKTINFO WHERE STKBD = '%s' and STK_CODE = '%s'",
          p_pszBoad, p_pszTrdCode);
      }
      else if('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        char szMarket = {0};
        StkBdToMkt(p_pszBoad,szMarket);
        snprintf(szSQL, SQL_LENGTH, "select closeprice, lastprice from stkprice  WHERE  market = '%c' and  stkcode = '%s'",
          szMarket, p_pszTrdCode);
      }
      else if('1' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        snprintf(szSQL, SQL_LENGTH, "select PRE_SETT_PRICE, OPT_CURR_PRICE from OPT_MKTINFO WHERE STKBD = '%s' and OPT_NUM = '%s'",
          p_pszBoad, p_pszTrdCode);
      }
      else
      {
        iRetCode = MA_ERROR_BIZ_OBJECT_INIT;
        ma::ThrowError(NULL, "{CBizRiskMatchAmt[{@1}] 子系统类型[{@2}] 没有匹配项", 
          &(_P(__LINE__) + _P(p_refstRiskCheckInfo.szSubsysSnType)));
        _ma_leave;
      }
    }
    else
    {
      p_refllCurPrice = 10000;  //默认1元，期货暂时无法从数据库获取当前价
      return iRetCode;
    }

    if (m_ptrServiceBizEnv->SelectXa(ptrXa, p_refstRiskCheckInfo.szSubSysDbConnstr) != MA_OK
      ||m_ptrServiceBizEnv->ConvertXa2DBEngine(ptrDBEngine, ptrXa)!= MA_OK)
    {
      _ma_throw ma::CBizException(MA_ERROR_BIZ_OBJECT_INIT, "数据引擎设置失败：Xa[{@1}]", &_P(p_refstRiskCheckInfo.szSubSysDbConnstr));
    }

    if (m_ptrDaoRiskDataBak->SetDBEngine(ptrDBEngine) != MA_OK)
    {
      _ma_throw ma::CBizException(MA_ERROR_UNDEFINED_OBJECT,
        "CBizRiskCheckInit[{@1}] m_ptrDaoRiskDataBak SetDBEngine failed", &_P(__LINE__));
    }
    if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
    {
      char szTempPrice[64] = {0};

      iRetCode = m_ptrDaoRiskDataBak->Select_JZJY_CurrPrice(szTempPrice, sizeof(szTempPrice), szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskDataBak->GetLastErrorText());
      }
      else if (MA_NO_DATA == iRetCode)
      {
        //iRetCode = MA_OK;
        ma::ThrowWarn(NULL, "GetCurrPriceFromDataBase[{@1}] 行情信息表没有找到[{@2}.{@3}]价格信息", 
          &(_P(__LINE__) + _P(p_pszBoad) + _P(p_pszTrdCode)));
      }
      else
      {
        p_refllCurPrice = xsdk::CPrice4(szTempPrice).CvtToLonglong();
      }
    }
    else 
    {
      iRetCode = m_ptrDaoRiskDataBak->Select_CurrPrice(p_refllCurPrice, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskDataBak->GetLastErrorText());
      }
      else if (MA_NO_DATA == iRetCode)
      {
        //iRetCode = MA_OK;
        ma::ThrowWarn(NULL, "GetCurrPriceFromDataBase[{@1}] 行情信息表没有找到[{@2}.{@3}]价格信息", 
          &(_P(__LINE__) + _P(p_pszBoad) + _P(p_pszTrdCode)));
      }
    }
  }

  _ma_catch_finally
  {
    m_strErrorMsg = m_strLastErrorText;  //子类调用该函数时，获取错误信息
    return iRetCode;
  }
}

int CBizRiskCheckInit::RiskRulesUpdateNote()
{
  int iRetCode = MA_OK;

  short int siLoadTimes = 0;
  IKernelEnv *pclKernelEnv = NULL;

  _ma_try
  {   
    //从共享区
    if ((pclKernelEnv = ma::g_hKernelEnv->UN_HANDLE.pclKernelEnv) == NULL
      || !pclKernelEnv->IsKindOf(RUNTIME_OBJECT(IKernelEnv)))
    {
      iRetCode = MA_KO;
      ma::ThrowError(NULL, "CBizRiskCheckInit[{@1}] RiskRulesUpdate() pclKernelEnv->IsKindOf() Fail", &_P(__LINE__));
      _ma_leave;
    }

    iRetCode = pclKernelEnv->ReadShareArea(RISK_SHM_NAME, &siLoadTimes, sizeof(siLoadTimes), true);
    if (MA_OK == iRetCode)
    {
      siLoadTimes += 1;      
      //ThrowTrace(NULL, "11111111siLoadTimes[{@1}]  ---", &(_P(siLoadTimes)));
      pclKernelEnv->WriteShareArea(RISK_SHM_NAME, &siLoadTimes, sizeof(siLoadTimes), true);
    }    
    else
    {
      siLoadTimes += 1;
      //ThrowTrace(NULL, "222222siLoadTimes[{@1}]  ---", &(_P(siLoadTimes)));
      pclKernelEnv->CreateShareArea(RISK_SHM_NAME, sizeof(siLoadTimes), true);
      pclKernelEnv->WriteShareArea(RISK_SHM_NAME, &siLoadTimes, sizeof(siLoadTimes), true);
      iRetCode = MA_OK;
    }
  }

  _ma_catch_finally
  {
    return iRetCode;
  }
}

void CBizRiskCheckInit::PrintMap(int p_iRetCode)
{
  char szRules[8] = {0};
  char szRiskCuacct[528] = {0};

  std::map<std::string, std::vector<int> >::iterator iter = m_mapCuacctRules.begin();
  for(; iter!= m_mapCuacctRules.end(); iter++)
  {
    strcat(szRiskCuacct, "{");
    strcat(szRiskCuacct, iter->first.c_str());
    strcat(szRiskCuacct, ": ");
    for (int i=0; i<iter->second.size(); i++)
    {
      sprintf(szRules, "%d|", iter->second[i]);
      strcat(szRiskCuacct, szRules);
    }
    strcat(szRiskCuacct, "}");
  }

  ThrowTrace(NULL, "iRetCode[{@1}] 风控账户信息{@2}", &(_P(p_iRetCode) + _P(szRiskCuacct)));
}

int CBizRiskCheckInit::RiskCheckDataInit()
{
  int iRetCode = MA_OK;

  CObjectPtr<IDaoRiskCuacct>            ptrDaoRiskCuacct;              
  CObjectPtr<IDataRiskCuacct>           ptrDataRiskCuacct;             
  CObjectPtr<IDataRiskCuacctEx1>        ptrDataRiskCuacctEx1; 

  CObjectPtr<IDaoRiskRuleCfg>           ptrDaoRiskRuleCfg;              
  CObjectPtr<IDataRiskRuleCfg>          ptrDataRiskRuleCfg;             
  CObjectPtr<IDataRiskRuleCfgUidx1>     ptrDataRiskRuleCfgUidx1; 

  CObjectPtr<IDaoRiskClassInfo>         ptrDaoRiskClassInfo;              
  CObjectPtr<IDataRiskClassInfo>        ptrDataRiskClassInfo;             
  CObjectPtr<IDataRiskClassInfoUidx1>   ptrDataRiskClassInfoUidx1; 

  _ma_try
  {
    if (ptrDaoRiskCuacct.Create("CDaoRiskCuacct").IsNull()
      || ptrDataRiskCuacct.Create("CDataRiskCuacct").IsNull()
      || ptrDataRiskCuacctEx1.Create("CDataRiskCuacctEx1").IsNull()
      || m_ptrServiceBizEnv->BindDao2CurrentXa(ptrDaoRiskCuacct.Ptr()) != MA_OK)
    {
      _ma_throw ma::CFuncException(MA_ERROR_UNDEFINED_OBJECT, "CBizRiskCheckInit create object {@1} failed",
        &(_P("CDaoRiskCuacct/CDataRiskCuacct/CDataRiskCuacctUidx1")));
    }

    if (ptrDaoRiskRuleCfg.Create("CDaoRiskRuleCfg").IsNull()
      || ptrDataRiskRuleCfg.Create("CDataRiskRuleCfg").IsNull()
      || ptrDataRiskRuleCfgUidx1.Create("CDataRiskRuleCfgUidx1").IsNull()
      || m_ptrServiceBizEnv->BindDao2CurrentXa(ptrDaoRiskRuleCfg.Ptr()) != MA_OK)
    {
      _ma_throw ma::CFuncException(MA_ERROR_UNDEFINED_OBJECT, "CBizRiskCheckInit create object {@1} failed",
        &(_P("CDaoRiskRuleCfg/CDataRiskRuleCfg/CDataRiskRuleCfgUidx1")));
    }

    if (ptrDaoRiskClassInfo.Create("CDaoRiskClassInfo").IsNull()
      || ptrDataRiskClassInfo.Create("CDataRiskClassInfo").IsNull()
      || ptrDataRiskClassInfoUidx1.Create("CDataRiskClassInfoUidx1").IsNull()
      || m_ptrServiceBizEnv->BindDao2CurrentXa(ptrDaoRiskClassInfo.Ptr()) != MA_OK)
    {
      _ma_throw ma::CFuncException(MA_ERROR_UNDEFINED_OBJECT, "CBizRiskCheckInit create object {@1} failed",
        &(_P("CDaoRiskClassInfo/CDataRiskClassInfo/CDataRiskClassInfoUidx1")));
    }

    m_mapCuacctRules.clear();
    m_mapRulesInfo.clear();

    //加载需要风控的资产账户
    int iHandleCursor = -1;
    iRetCode = ptrDaoRiskCuacct->OpenCursor(iHandleCursor, ptrDataRiskCuacct.Ptr(), ptrDataRiskCuacctEx1.Ptr());
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CFuncException(iRetCode, "CBizRiskCheckInit[{@1}] RISK_CUACCT OpenCursor failed, iRetCode[{@2}], error:{@3}",
        &(_P(__LINE__) + _P(iRetCode) + _P(ptrDaoRiskCuacct->GetLastErrorText())));
    }
    else if (iRetCode == MA_NO_DATA)
    {
      iRetCode = MA_OK;
      // ma::ThrowInfo(NULL, "CBizRiskCheckInit[{@1}] RISK_CUACCT is null, Please Check DataBase!", &_P(__LINE__));
      _ma_leave;
    }
    else
    {
      std::string ssTrdCuacctCode;
      std::vector<int> vecRules;

      char *pRuleStr = NULL;
      char chCuacctType = 0;
      char szCuacctCode[16 + 1] = {0};
      char szTrdCuacctCode[32] = {0};
      char szRuleIdStr[256 + 1] = {0};  
      while (ptrDaoRiskCuacct->Fetch(iHandleCursor) == MA_OK)
      {  
        vecRules.clear();
        memset(szRuleIdStr, 0x00, sizeof(szRuleIdStr));

        sprintf(szTrdCuacctCode, "%s%c\0", ptrDataRiskCuacct->GetCuacctCode(), ptrDataRiskCuacct->GetCuacctType());
        strncpy(szRuleIdStr, ptrDataRiskCuacct->GetCuacctRules(), sizeof(szRuleIdStr));

        //szRuleIdStr 字符串格式 101|102|103|..
        if (strlen(szRuleIdStr) < 2)
        {
          continue;
        }

        vecRules.push_back(atoi(szRuleIdStr));
        pRuleStr = strchr(szRuleIdStr, '|');
        while(pRuleStr != NULL)
        {
          vecRules.push_back(atoi(pRuleStr + 1));
          pRuleStr = strchr(pRuleStr + 1, '|');
        }

        m_mapCuacctRules[szTrdCuacctCode] = vecRules;
      }
      ptrDaoRiskCuacct->CloseCursor(iHandleCursor);
    }


    //加载规则信息
    ST_RULE_INFO stRuleInfo;
    std::map<std::string, std::vector<int> >::iterator iter = m_mapCuacctRules.begin();
    for(; iter!= m_mapCuacctRules.end(); iter++)
    {
      for (int i = 0; i < iter->second.size(); i ++)
      {
        memset(&stRuleInfo, 0x00, sizeof(stRuleInfo));
        //判断规则id是否已经存在
        std::map<int, ST_RULE_INFO>::iterator itrule = m_mapRulesInfo.find(iter->second[i]);
        if (itrule != m_mapRulesInfo.end())
        {
          continue;
        }

        ptrDataRiskRuleCfg->Initialize();
        ptrDataRiskRuleCfgUidx1->SetRuleId(iter->second[i]);
        iRetCode = ptrDaoRiskRuleCfg->Select(ptrDataRiskRuleCfg, ptrDataRiskRuleCfgUidx1);
        if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
        {
          _ma_throw ma::CFuncException(iRetCode, "CBizRiskCheckInit[{@1}] RISK_RULE_CFG select failed, iRetCode[{@2}], error:{@3}",
            &(_P(__LINE__) + _P(iRetCode) + _P(ptrDaoRiskRuleCfg->GetLastErrorText())));
        }
        else if (iRetCode == MA_NO_DATA)
        {
          continue;
        }

        stRuleInfo.chJudge = ptrDataRiskRuleCfg->GetJudgeMethod();
        stRuleInfo.llThresholdFst = ptrDataRiskRuleCfg->GetThresholdFirst();
        stRuleInfo.llThresholdSed = ptrDataRiskRuleCfg->GetThresholdSecond();
        stRuleInfo.llThresholdThd = ptrDataRiskRuleCfg->GetThresholdThird();
        strncpy(stRuleInfo.szUnit, ptrDataRiskRuleCfg->GetThresholdUnit(), sizeof(stRuleInfo.szUnit));

        //获取类信息
        ptrDataRiskClassInfo->Initialize();
        ptrDataRiskClassInfoUidx1->SetRiskId(ptrDataRiskRuleCfg->GetRiskId());
        iRetCode = ptrDaoRiskClassInfo->Select(ptrDataRiskClassInfo, ptrDataRiskClassInfoUidx1);
        if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
        {
          _ma_throw ma::CFuncException(NULL, "CBizRiskCheckInit[{@1}] RISK_CLASS_INFO select failed, iRetCode[{@2}], error:{@3}",
            &(_P(__LINE__) + _P(iRetCode) + _P(ptrDaoRiskClassInfo->GetLastErrorText())));
        }
        else if (iRetCode == MA_NO_DATA)
        {
          ma::ThrowWarn(NULL, "CBizRiskCheckInit[{@1}] RISK_CLASS_INFO RiskId[{@2}] is not exsit, please check data",
            &(_P(__LINE__) + _P(ptrDataRiskRuleCfg->GetRiskId())));
          continue;
        }

        strncpy(stRuleInfo.szRiskClass, ptrDataRiskClassInfo->GetRiskClass(), sizeof(stRuleInfo.szRiskClass));
        strncpy(stRuleInfo.szRiskRemark, ptrDataRiskClassInfo->GetRiskName(), sizeof(stRuleInfo.szRiskRemark));
        strncpy(stRuleInfo.szRiskClassParm, ptrDataRiskClassInfo->GetCustomParameter(), sizeof(stRuleInfo.szRiskClassParm));

        m_mapRulesInfo[iter->second[i]] = stRuleInfo;
      }
    }
  }

  _ma_catch_finally
  {
    //PrintMap(iRetCode);
  }

  return iRetCode;
}

int CBizRiskCheckInit::RiskCheckImplement(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo)
{
  int iRetCode = MA_OK;
  int iRetCheck = MA_OK;

  char szTrdCuacct[32] = {0};
  std::string strWarnMsg = "";
  //CObjectPtr<IBizRiskCheck> ptrBizRiskCheck;

  _ma_try
  {
    //暂时只检查股票
    if (p_refstRiskCheckInfo.szCuacctType[0] != '0' && p_refstRiskCheckInfo.szCuacctType[0] != '3')
    {
      return iRetCode;
    }

    //非交易业务不检查
    if (STK_BIZ_MORTGAGE_IN == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_MORTGAGE_OUT == p_refstRiskCheckInfo.siStkBiz
      || STK_BIZ_BONDEXISTED_BACK == p_refstRiskCheckInfo.siStkBiz || TRD_BIZ_OPT_LOCK == p_refstRiskCheckInfo.siStkBiz
      || TRD_BIZ_OPT_UNLOCK == p_refstRiskCheckInfo.siStkBiz)
    {
      return iRetCode;
    }    

    //检查规则是否有修改
    iRetCode = CheckLoadRules();
    if (MA_OK == iRetCode)
    {
      //PrintMap(9999);
      iRetCode = RiskCheckDataInit();
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_leave;
      }
    }

    iRetCode = MA_OK; //注意重置

    //风险账户或风险规则信息为空-直接返回    
    if (m_mapCuacctRules.empty() || m_mapRulesInfo.empty())
    {
      return iRetCode;
    }


    sprintf(szTrdCuacct, "%s%s", p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szCuacctType);
    std::map<std::string, std::vector<int> >::iterator iter = m_mapCuacctRules.find(szTrdCuacct);
    if (iter == m_mapCuacctRules.end())
    {
      return iRetCode;
    }

    //开始规则检查
    for (int i = 0; i < iter->second.size(); i ++)
    {
      //判断规则id是否已经存在
      std::map<int, ST_RULE_INFO>::iterator itrule = m_mapRulesInfo.find(iter->second[i]);
      if (itrule == m_mapRulesInfo.end())
      {
        continue;
      }

      //如果阈值3和2都为0，则不检查
      if (0 == itrule->second.llThresholdThd && 0 == itrule->second.llThresholdSed)
      {
        continue;
      }

      if ((itrule->second.ptrBizRiskCheck).IsNull())
      {
        //创建风险类
        if ((itrule->second.ptrBizRiskCheck).Create(itrule->second.szRiskClass).IsNull())
        {
          ThrowError(NULL, "CBizRiskCheckInit::RiskCheckImplement Create[{@1}] fail", &_P(itrule->second.szRiskClass));
          continue;
        }

        //设置环境变量
        (itrule->second.ptrBizRiskCheck)->SetServiceBizEnv(m_ptrServiceBizEnv);
      }

      //ThrowTrace(NULL, "RiskCheckInterface Check, RiskClass[{@1}] rule[{@2}] ",  &(_P(itrule->second.szRiskClass) + _P(itrule->first)));
      //调用接口函数
      (itrule->second.ptrBizRiskCheck)->Initialize();
      iRetCheck = (itrule->second.ptrBizRiskCheck)->RiskCheckInterface(p_refstRiskCheckInfo, itrule->second);
      if (iRetCheck != MA_OK && iRetCheck != RISK_WARN_ORDER && iRetCheck != RISK_REFUSE_ORDER )
      {
        _ma_throw ma::CFuncException(iRetCheck, "CBizRiskCheckInit[{@1}] RiskCheckInterface failed, rule[{@2}] error:{@3}",
          &(_P(__LINE__) + _P(itrule->first) + _P((itrule->second.ptrBizRiskCheck)->GetLastErrorText())));
      }
      else if (iRetCheck == RISK_WARN_ORDER)
      {
        //警告信息-推送到客户端，写入风险日志表
        m_strLastErrorText = (itrule->second.ptrBizRiskCheck)->GetLastErrorText();
        m_strLastErrorText += "\r\n";
        strWarnMsg += m_strLastErrorText;
        //PubRiskWarnNote(p_refstRiskCheckInfo, iRetCode, m_strLastErrorText.c_str());
        iRetCode = InsertRiskInfoLog(p_refstRiskCheckInfo, iter->second[i], iRetCheck, m_strLastErrorText.c_str());
        if (iRetCode != MA_OK)
        {
          _ma_leave;
        }
        //ThrowTrace(NULL, "rule[{@1}]  iRetCheck[{@2}], strWarnMsg[{@3}]", &(_P(iter->second[i]) + _P(iRetCheck) + _P(strWarnMsg.c_str())));
      }
      else if (iRetCheck == RISK_REFUSE_ORDER)
      {
        //危险信息-直接拒单，写入风险日志表
        m_strLastErrorText = (itrule->second.ptrBizRiskCheck)->GetLastErrorText();
        iRetCode = InsertRiskInfoLog(p_refstRiskCheckInfo, iter->second[i], iRetCheck, m_strLastErrorText.c_str());
        if (iRetCode == MA_OK)
        {
          iRetCode = iRetCheck;  //如果插入成功，重置一下返回码
        }
        _ma_leave;
      }
    }
  }

  _ma_catch_finally
  {
    //ptrBizRiskCheck.Release();
    //ThrowTrace(NULL, "iRetCheck[{@1}], strWarnMsg[{@2}]", &(_P(iRetCheck) + _P(strWarnMsg.c_str())));
    if (strWarnMsg.length() > 1 && iRetCheck != RISK_REFUSE_ORDER)
    {
      PubRiskWarnNote(p_refstRiskCheckInfo, iRetCheck, strWarnMsg.c_str());
    }
  }

  return iRetCode;
}

int CBizRiskCheckInit::IsExchangeCheck(char *p_pszClassParm, char p_chSrcEx)
{
  short siLen = 6;

  //判断是否按交易市场检查：深交所：0 上交所：1 全市场：*
  if (NULL == p_pszClassParm)
  {
    return EXCHANGE_IS_ALL;
  }

  if (0 == strncmp(p_pszClassParm, "STKEX:", siLen))
  {
    if (p_pszClassParm[siLen] != '*' && p_pszClassParm[siLen] != p_chSrcEx)
    {
      return EXCHANGE_NOT_MATCH;
    }
    else if(p_pszClassParm[siLen] == p_chSrcEx)
    {
      return EXCHANGE_MATCH;
    }
  }

  return EXCHANGE_IS_ALL;
}

int CBizRiskCheckInit::RiskpThresholdResolve(ST_RULE_INFO &p_refstRuleInfo, long long p_llInputValue, long p_lMultiple)
{
  int iRetCode = MA_OK;

  char szBuffInfo[256] = {0};
  char szUnite[8] = {0};

  _ma_try
  {
    //0-大于、1-小于、2-等于
    //siMultiple:阈值扩大倍数，便于带百分比或者有小数点的比较
    if ('0' == p_refstRuleInfo.chJudge)
    {
      if (p_refstRuleInfo.llThresholdThd > 0 && p_llInputValue > p_refstRuleInfo.llThresholdThd*p_lMultiple)
      {
        iRetCode = RISK_REFUSE_ORDER;
        sprintf(szBuffInfo, "拒单:%s[%0.2lf%s]大于阈值[%ld%s]", p_refstRuleInfo.szRiskRemark, p_llInputValue*1.0/p_lMultiple, 
          p_refstRuleInfo.szUnit, p_refstRuleInfo.llThresholdThd, p_refstRuleInfo.szUnit);
      }
      else if (p_refstRuleInfo.llThresholdSed > 0 && p_llInputValue > p_refstRuleInfo.llThresholdSed*p_lMultiple)
      {
        iRetCode = RISK_WARN_ORDER;
        sprintf(szBuffInfo, "警告:%s[%0.2lf%s]大于阈值[%ld%s]", p_refstRuleInfo.szRiskRemark, p_llInputValue*1.0/p_lMultiple, 
          p_refstRuleInfo.szUnit, p_refstRuleInfo.llThresholdSed, p_refstRuleInfo.szUnit);
      }
    }
    else if ('1' == p_refstRuleInfo.chJudge)
    {
      if (p_refstRuleInfo.llThresholdThd > 0 && p_llInputValue < p_refstRuleInfo.llThresholdThd*p_lMultiple)
      {
        iRetCode = RISK_REFUSE_ORDER;
        sprintf(szBuffInfo, "拒单:%s[%0.2lf%s]小于阈值[%ld%s]", p_refstRuleInfo.szRiskRemark, p_llInputValue*1.0/p_lMultiple, 
          p_refstRuleInfo.szUnit, p_refstRuleInfo.llThresholdThd, p_refstRuleInfo.szUnit);
      }
      else if (p_refstRuleInfo.llThresholdSed > 0 && p_llInputValue < p_refstRuleInfo.llThresholdSed*p_lMultiple)
      {
        iRetCode = RISK_WARN_ORDER;
        sprintf(szBuffInfo, "警告:%s[%0.2lf%s]小于阈值[%ld%s]", p_refstRuleInfo.szRiskRemark, p_llInputValue*1.0/p_lMultiple, 
          p_refstRuleInfo.szUnit, p_refstRuleInfo.llThresholdSed, p_refstRuleInfo.szUnit);
      }
    }
    else
    {
      if (p_refstRuleInfo.llThresholdThd > 0 && p_llInputValue == p_refstRuleInfo.llThresholdThd*p_lMultiple)
      {
        iRetCode = RISK_REFUSE_ORDER;
        sprintf(szBuffInfo, "%s[%0.2lf%s]等于阈值[%ld%s]", p_refstRuleInfo.szRiskRemark, p_llInputValue*1.0/p_lMultiple, 
          p_refstRuleInfo.szUnit, p_refstRuleInfo.llThresholdThd, p_refstRuleInfo.szUnit);
      }
      else if (p_refstRuleInfo.llThresholdSed > 0 && p_llInputValue == p_refstRuleInfo.llThresholdSed*p_lMultiple)
      {
        iRetCode = RISK_WARN_ORDER;
        sprintf(szBuffInfo, "%s[%0.2lf%s]等于阈值[%ld%s]", p_refstRuleInfo.szRiskRemark, p_llInputValue*1.0/p_lMultiple, 
          p_refstRuleInfo.szUnit, p_refstRuleInfo.llThresholdSed, p_refstRuleInfo.szUnit);
      }
    }
  }

  _ma_catch_finally
  {

  }
  m_strErrorMsg = szBuffInfo;  //子类调用该函数时，获取错误信息

  return  iRetCode;
}

int CBizRiskCheckInit::RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  return  MA_OK;
}

//--------------------------------------
//类CBizRiskTradeFrequency的实现--每秒交易频率 
//--------------------------------------
int CBizRiskTradeFrequency::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskTradeFrequency::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskTradeFrequency::RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  return RiskCheckCosDatabase(p_refstRiskCheckInfo,p_refstRuleInfo);
}

int CBizRiskTradeFrequency::RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  char szSQL[SQL_LENGTH] = {0};
  long long llResultValue =  0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskTradeFrequency invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    iRetCode = DBEngineInit(COS_DATA_XA);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //组织SQL语句
    if (EXCHANGE_MATCH == siRetExChange)
    {

      snprintf(szSQL, SQL_LENGTH, "select count(*) from COS_ORDER WHERE CUACCT_CODE = '%s' and CUACCT_TYPE = '%s' \
      and STKEX = '%s'and ORDER_TIME like '%s%%'", p_refstRiskCheckInfo.szCuacctCode, 
      p_refstRiskCheckInfo.szCuacctType, p_refstRiskCheckInfo.szExchange, p_refstRiskCheckInfo.szOrderTime);
    }
    else
    {
      snprintf(szSQL, SQL_LENGTH, "select count(*) from COS_ORDER WHERE CUACCT_CODE = '%s' and CUACCT_TYPE = '%s' \
      and ORDER_TIME like '%s%%'", p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szCuacctType,
      p_refstRiskCheckInfo.szOrderTime);
    }

    iRetCode = m_ptrDaoRiskData->Select_Value(llResultValue, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    llResultValue += 1;
    //m_strLastErrorText="每秒交易频率：";  
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}


//------------------------------------------- 
//类CBizRiskTradeAmt的实现--单笔交易金额检查
//-------------------------------------------
int CBizRiskTradeAmt::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskTradeAmt::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskTradeAmt::RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  return p_refstRiskCheckInfo.bIsCosDatabase ? 
    RiskCheckCosDatabase(p_refstRiskCheckInfo,p_refstRuleInfo)
    : RiskCheckSubsysDatabase(p_refstRiskCheckInfo,p_refstRuleInfo);
}

int CBizRiskTradeAmt::RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  long long llResultValue =  0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      iRetCode = MA_ERROR_INVALID_PARAM;
      _ma_throw ma::CBizException(iRetCode, "CBizRiskTradeAmt invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    if (0 == p_refstRiskCheckInfo.llOrderPrice)
    {
      //从内存行情获取当前价
      iRetCode = GetCurrPriceFromMemMkt(p_refstRiskCheckInfo.llOrderPrice, p_refstRiskCheckInfo.szExchange[0], p_refstRiskCheckInfo.szTrdCode);
      if (iRetCode != MA_OK)
      {
        m_strLastErrorText = m_strErrorMsg;
        _ma_leave;
      }

      //如果内存行情没有取到当前价,报错
      if (0 == p_refstRiskCheckInfo.llOrderPrice)
      {
        _ma_throw ma::CBizException(RISK_NO_CURR_PRICE, "委托价格为0且从内存行情中获取不到当前价");
      }
    }

    //阈值单位:万/笔, 统一计算单位：元
    llResultValue = (p_refstRiskCheckInfo.llOrderPrice * p_refstRiskCheckInfo.llOrderQty)/p_refstRiskCheckInfo.siOrderCounts;
    //m_strLastErrorText="单笔交易金额检查："; 
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue, 10000*10000);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;


}
int CBizRiskTradeAmt::RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{

  int iRetCode = MA_OK;

  short siRetExChange = 0;
  long long llResultValue =  0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      iRetCode = MA_ERROR_INVALID_PARAM;
      _ma_throw ma::CBizException(iRetCode, "CBizRiskTradeAmt invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    if (0 == p_refstRiskCheckInfo.llOrderPrice)
    {
      //从内存行情获取当前价
      iRetCode = GetCurrPriceFromMemMkt(p_refstRiskCheckInfo.llOrderPrice, p_refstRiskCheckInfo.szExchange[0], p_refstRiskCheckInfo.szTrdCode);
      if (iRetCode != MA_OK)
      {
        m_strLastErrorText = m_strErrorMsg;
        _ma_leave;
      }

      //如果内存行情没有取到当前价-从订单或期权数据库证券行情信息表获取
      if (0 == p_refstRiskCheckInfo.llOrderPrice)
      {
        iRetCode = GetCurrPriceFromDataBase(p_refstRiskCheckInfo.llOrderPrice, p_refstRiskCheckInfo,
          p_refstRiskCheckInfo.szStkbd, p_refstRiskCheckInfo.szTrdCode);
        if (iRetCode != MA_OK)
        {
          m_strLastErrorText = m_strErrorMsg;
          _ma_leave;
        }
      }
    }

    //阈值单位:万/笔, 统一计算单位：元
    llResultValue = (p_refstRiskCheckInfo.llOrderPrice * p_refstRiskCheckInfo.llOrderQty)/p_refstRiskCheckInfo.siOrderCounts;
    //m_strLastErrorText="单笔交易金额检查："; 
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue, 10000*10000);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}


//------------------------------------------- 
//类CBizRiskTradeNum的实现--单笔交易数量检查
//-------------------------------------------
int CBizRiskTradeNum::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskTradeNum::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskTradeNum::RiskCheckInterface(ST_RISK_CHECK_INFO & p_refstRiskCheckInfo, ST_RULE_INFO & p_refstRuleInfo)
{
  return p_refstRiskCheckInfo.bIsCosDatabase ?
    RiskCheckCosDatabase(p_refstRiskCheckInfo, p_refstRuleInfo)
    : RiskCheckSubsysDatabase(p_refstRiskCheckInfo, p_refstRuleInfo);
}

int CBizRiskTradeNum::RiskCheckCosDatabase(ST_RISK_CHECK_INFO & p_refstRiskCheckInfo, ST_RULE_INFO & p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  long long llResultValue = 0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      iRetCode = MA_ERROR_INVALID_PARAM;
      _ma_throw ma::CBizException(iRetCode, "CBizRiskTradeNum invalid parameter m_ptrServiceBizEnv is NULL");
    }

  //判断是否匹配交易所
  siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
  if (EXCHANGE_NOT_MATCH == siRetExChange)
  {
    return iRetCode;
  }

  if (0 == p_refstRiskCheckInfo.llOrderQty)
  {
    _ma_leave;
  }

  //m_strLastErrorText="单笔交易数量检查："; 
  iRetCode = RiskpThresholdResolve(p_refstRuleInfo, p_refstRiskCheckInfo.llOrderQty);
  if (iRetCode != MA_OK)
  {
    m_strLastErrorText += m_strErrorMsg;
    _ma_leave;
  }
  }

    _ma_catch_finally
  {
  }

  return  iRetCode;


}

int CBizRiskTradeNum::RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO & p_refstRiskCheckInfo, ST_RULE_INFO & p_refstRuleInfo)
{

  int iRetCode = MA_OK;

  short siRetExChange = 0;
  long long llResultValue = 0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      iRetCode = MA_ERROR_INVALID_PARAM;
      _ma_throw ma::CBizException(iRetCode, "CBizRiskTradeAmt invalid parameter m_ptrServiceBizEnv is NULL");
    }

  //判断是否匹配交易所
  siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
  if (EXCHANGE_NOT_MATCH == siRetExChange)
  {
    return iRetCode;
  }

  if (0 == p_refstRiskCheckInfo.llOrderQty)
  {
        _ma_leave;
  }

  //m_strLastErrorText="单笔交易数量检查："; 
  iRetCode = RiskpThresholdResolve(p_refstRuleInfo, p_refstRiskCheckInfo.llOrderQty);
  if (iRetCode != MA_OK)
  {
    m_strLastErrorText += m_strErrorMsg;
    _ma_leave;
  }
  }

    _ma_catch_finally
  {
  }

  return  iRetCode;
}


//------------------------------------------- 
//类CBizRiskFactorTradeTimes的实现--条件单交易次数检查
//-------------------------------------------
int CBizRiskFactorTradeTimes::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskFactorTradeTimes::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskFactorTradeTimes::RiskCheckInterface(ST_RISK_CHECK_INFO & p_refstRiskCheckInfo, ST_RULE_INFO & p_refstRuleInfo)
{
  return  RiskCheckCosDatabase(p_refstRiskCheckInfo, p_refstRuleInfo);
}

int CBizRiskFactorTradeTimes::RiskCheckCosDatabase(ST_RISK_CHECK_INFO & p_refstRiskCheckInfo, ST_RULE_INFO & p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  long long llResultValue = 0;
  char szSQL[SQL_LENGTH] = { 0 };


  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      iRetCode = MA_ERROR_INVALID_PARAM;
      _ma_throw ma::CBizException(iRetCode, "CBizRiskTradeNum invalid parameter m_ptrServiceBizEnv is NULL");
    }
  //判断是否匹配交易所
  siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
  if (EXCHANGE_NOT_MATCH == siRetExChange)
  {
    return iRetCode;
  }

  // 不检查普通单
  if (0 == p_refstRiskCheckInfo.siAttrCode)
  {
    _ma_leave;
  }
  iRetCode = DBEngineInit(COS_DATA_XA);
  if (iRetCode != MA_OK)
  {
    m_strLastErrorText = m_strErrorMsg;
    _ma_leave;
  }

  if (EXCHANGE_MATCH == siRetExChange)
  {
    //组织SQL语句-计算正在执行与暂停状态的条件单
    snprintf(szSQL, SQL_LENGTH, "select COUNT(*) from COS_ORDER WHERE CUACCT_CODE = '%s'  and CUACCT_TYPE = '%s'\
      and ATTR_CODE != 0 and  STKEX = '%s' and EXE_STATUS not in('%s','%s') ",
        p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szCuacctType, p_refstRiskCheckInfo.szExchange, ORDER_EXE_STATUS_PART_CANCELING, ORDER_EXE_STATUS_INVALIDE);

      iRetCode = m_ptrDaoRiskData->Select_Value(llResultValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }
    }
    else
    {
      snprintf(szSQL, SQL_LENGTH, "select COUNT(*) from COS_ORDER WHERE CUACCT_CODE = '%s' and CUACCT_TYPE = '%s'\
       and ATTR_CODE != 0 and EXE_STATUS not in('%c','%c')",
        p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szCuacctType, ORDER_EXE_STATUS_PART_CANCELING, ORDER_EXE_STATUS_INVALIDE);

      iRetCode = m_ptrDaoRiskData->Select_Value(llResultValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }
    }

    llResultValue = llResultValue + 1;

    //m_strLastErrorText="条件单交易次数检查："; 
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

    _ma_catch_finally
  {
  }

  return  iRetCode;
}


//------------------------------------------- 
//类CBizRiskStockFactorTradeTimes的实现--个股条件单交易次数
//-------------------------------------------
int CBizRiskStockFactorTradeTimes::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskStockFactorTradeTimes::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskStockFactorTradeTimes::RiskCheckInterface(ST_RISK_CHECK_INFO & p_refstRiskCheckInfo, ST_RULE_INFO & p_refstRuleInfo)
{
  return RiskCheckCosDatabase(p_refstRiskCheckInfo, p_refstRuleInfo);
}

int CBizRiskStockFactorTradeTimes::RiskCheckCosDatabase(ST_RISK_CHECK_INFO & p_refstRiskCheckInfo, ST_RULE_INFO & p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  long long llResultValue = 0;
  char szSQL[SQL_LENGTH] = { 0 };


  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      iRetCode = MA_ERROR_INVALID_PARAM;
      _ma_throw ma::CBizException(iRetCode, "CBizRiskTradeNum invalid parameter m_ptrServiceBizEnv is NULL");
    }
  //判断是否匹配交易所
  siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
  if (EXCHANGE_NOT_MATCH == siRetExChange)
  {
    return iRetCode;
  }

  // 不检查普通单
  if (0 == p_refstRiskCheckInfo.siAttrCode)
  {
    _ma_leave;
  }
  iRetCode = DBEngineInit(COS_DATA_XA);
  if (iRetCode != MA_OK)
  {
    m_strLastErrorText = m_strErrorMsg;
    _ma_leave;
  }

  if (EXCHANGE_MATCH == siRetExChange)
  {
    //组织SQL语句-计算正在执行与暂停状态的条件单
    snprintf(szSQL, SQL_LENGTH, "select COUNT(*) from COS_SUB_ORDER WHERE CUACCT_CODE = '%s' and CUACCT_TYPE = '%s'\
         and TRD_CODE = '%s'  and  STKEX = '%s'  and ATTR_CODE != 0 and EXE_STATUS not in('%c','%c')",
        p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szCuacctType, p_refstRiskCheckInfo.szTrdCode, p_refstRiskCheckInfo.szExchange, ORDER_EXE_STATUS_PART_CANCELING, ORDER_EXE_STATUS_INVALIDE);

      iRetCode = m_ptrDaoRiskData->Select_Value(llResultValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }
    }
    else
    {
      snprintf(szSQL, SQL_LENGTH, "select COUNT(*) from COS_ORDER WHERE CUACCT_CODE = '%s' and CUACCT_TYPE = '%s'\
         and TRD_CODE = '%s' and ATTR_CODE != 0 and EXE_STATUS not in('%c','%c')",
        p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szCuacctType, p_refstRiskCheckInfo.szTrdCode, ORDER_EXE_STATUS_PART_CANCELING, ORDER_EXE_STATUS_INVALIDE);

      iRetCode = m_ptrDaoRiskData->Select_Value(llResultValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }
    }

    llResultValue = llResultValue + 1;

    //m_strLastErrorText="条件单交易次数检查："; 
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

    _ma_catch_finally
  {
  }

  return  iRetCode;
}


//------------------------------------------- 
//类CBizRiskMatchAmt的实现--单向净金额(成交金额)检查
//-------------------------------------------
int CBizRiskMatchAmt::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskMatchAmt::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskMatchAmt::RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  bool bIsJzjy = '0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1];
  return !bIsJzjy && p_refstRiskCheckInfo.bIsCosDatabase ? 
    RiskCheckCosDatabase(p_refstRiskCheckInfo,p_refstRuleInfo)
    : RiskCheckSubsysDatabase(p_refstRiskCheckInfo,p_refstRuleInfo);
}

int CBizRiskMatchAmt::RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  long long llBuyValue = 0;
  long long llSallValue = 0;
  long long llResultValue = 0;

  //char szSubSysDbConnstr[32] = {0};
  char szBuff[32] = {0};
  char szSQL[SQL_LENGTH] = {0};
  bool bFlag = true;
  //char szSubsysType[2+1];

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskMatchAmt invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    iRetCode = DBEngineInit(COS_DATA_XA);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //判断买卖方向-暂时根据业务类型
    //if (STK_BIZ_BUY == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_SALL == p_refstRiskCheckInfo.siStkBiz)
    //{
    //  sprintf(szBuff, "and TRD_BIZ = %d", p_refstRiskCheckInfo.siStkBiz);
    //}
    //else if (STK_BIZ_BUY_OPEN == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_BUY_CLOSE == p_refstRiskCheckInfo.siStkBiz)
    //{
    //  sprintf(szBuff, "and TRD_BIZ in(%d, %d)", STK_BIZ_BUY_OPEN, STK_BIZ_BUY_CLOSE);
    //}
    //else if (STK_BIZ_SALL_CLOSE == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_SALL_OPEN == p_refstRiskCheckInfo.siStkBiz)
    //{
    //  sprintf(szBuff, "and TRD_BIZ in(%d, %d)", STK_BIZ_SALL_CLOSE, STK_BIZ_SALL_OPEN);
    //}
    if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
    {
      if (EXCHANGE_MATCH == siRetExChange)
      {
        sprintf(szBuff, "and STKEX = '%s'", p_refstRiskCheckInfo.szExchange);
      }

      //组织SQL语句-买入金额
      snprintf(szSQL, SQL_LENGTH, "select sum(MATCHED_AMT) from STK_MATCHING WHERE CUACCT_CODE = '%s' %s and STK_BIZ = %d", 
        p_refstRiskCheckInfo.szCuacctCode, szBuff, STK_BIZ_BUY);

      iRetCode = m_ptrDaoRiskData->Select_Value(llBuyValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }

      //组织SQL语句-卖出金额
      snprintf(szSQL, SQL_LENGTH, "select sum(MATCHED_AMT) from STK_MATCHING WHERE CUACCT_CODE = '%s' %s and STK_BIZ = %d", 
        p_refstRiskCheckInfo.szCuacctCode, szBuff, STK_BIZ_SALL);

      iRetCode = m_ptrDaoRiskData->Select_Value(llSallValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }

      //阈值单位:万元，统一计算单位：元
      //llResultValue = abs(llBuyValue - llSallValue);   //abs 函数在linux下计算超过100万*10000*10000 时会出现异常
      llResultValue = llBuyValue*10 - llSallValue*10;    //快速订单系统 MATCHED_AMT 字段扩大了1000倍，而价格字段是扩大了10000倍
      if (llResultValue < 0)
      {
        bFlag = false;
        llResultValue = (-1)*llResultValue;
      }
    }
    else if('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
    {
      if (EXCHANGE_MATCH == siRetExChange)
      {
        sprintf(szBuff, "and market = '%s'", p_refstRiskCheckInfo.szExchange);
      }

      //组织SQL语句-买入金额
      snprintf(szSQL, SQL_LENGTH, "select sum(matchamt) from match WHERE fundid = '%s' %s and bsflag = '%s'", 
        p_refstRiskCheckInfo.szCuacctCode, szBuff, STK_BIZ_JZJY_BUY);

      iRetCode = m_ptrDaoRiskData->Select_Value(llBuyValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }

      //组织SQL语句-卖出金额
      snprintf(szSQL, SQL_LENGTH, "select sum(matchamt) from match WHERE fundid = '%s' %s and bsflag = '%s'", 
        p_refstRiskCheckInfo.szCuacctCode, szBuff, STK_BIZ_JZJY_SALL);

      iRetCode = m_ptrDaoRiskData->Select_Value(llSallValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }

      llResultValue = llBuyValue*10000 - llSallValue*10000; 
      if (llResultValue < 0)
      {
        bFlag = false;
        llResultValue = (-1)*llResultValue;
      }
    }

    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue, 10000*10000);
    if (iRetCode != MA_OK)
    {
      if (bFlag && STK_BIZ_SALL == p_refstRiskCheckInfo.siStkBiz)
      {
        iRetCode = MA_OK;
        m_strLastErrorText.clear();
      }
      else if (!bFlag && STK_BIZ_BUY == p_refstRiskCheckInfo.siStkBiz)
      {
        iRetCode = MA_OK;
        m_strLastErrorText.clear();
      }
      else
      {
        m_strLastErrorText += m_strErrorMsg;
      }
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}


int CBizRiskMatchAmt::RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  long long llBuyValue = 0;
  long long llSallValue = 0;
  long long llResultValue = 0;

  //char szSubSysDbConnstr[32] = {0};
  char szBuff[32] = {0};
  char szSQL[SQL_LENGTH] = {0};
  bool bFlag = true;
  //char szSubsysType[2+1];

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskMatchAmt invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    iRetCode = DBEngineInit(p_refstRiskCheckInfo.szSubSysDbConnstr);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //判断买卖方向-暂时根据业务类型
    //if (STK_BIZ_BUY == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_SALL == p_refstRiskCheckInfo.siStkBiz)
    //{
    //  sprintf(szBuff, "and TRD_BIZ = %d", p_refstRiskCheckInfo.siStkBiz);
    //}
    //else if (STK_BIZ_BUY_OPEN == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_BUY_CLOSE == p_refstRiskCheckInfo.siStkBiz)
    //{
    //  sprintf(szBuff, "and TRD_BIZ in(%d, %d)", STK_BIZ_BUY_OPEN, STK_BIZ_BUY_CLOSE);
    //}
    //else if (STK_BIZ_SALL_CLOSE == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_SALL_OPEN == p_refstRiskCheckInfo.siStkBiz)
    //{
    //  sprintf(szBuff, "and TRD_BIZ in(%d, %d)", STK_BIZ_SALL_CLOSE, STK_BIZ_SALL_OPEN);
    //}
    if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
    {
      if (EXCHANGE_MATCH == siRetExChange)
      {
        sprintf(szBuff, "and STKEX = '%s'", p_refstRiskCheckInfo.szExchange);
      }

      //组织SQL语句-买入金额
      snprintf(szSQL, SQL_LENGTH, "select sum(MATCHED_AMT) from STK_MATCHING WHERE CUACCT_CODE = '%s' %s and STK_BIZ = %d", 
        p_refstRiskCheckInfo.szCuacctCode, szBuff, STK_BIZ_BUY);

      iRetCode = m_ptrDaoRiskData->Select_Value(llBuyValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }

      //组织SQL语句-卖出金额
      snprintf(szSQL, SQL_LENGTH, "select sum(MATCHED_AMT) from STK_MATCHING WHERE CUACCT_CODE = '%s' %s and STK_BIZ = %d", 
        p_refstRiskCheckInfo.szCuacctCode, szBuff, STK_BIZ_SALL);

      iRetCode = m_ptrDaoRiskData->Select_Value(llSallValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }

      //阈值单位:万元，统一计算单位：元
      //llResultValue = abs(llBuyValue - llSallValue);   //abs 函数在linux下计算超过100万*10000*10000 时会出现异常
      llResultValue = llBuyValue*10 - llSallValue*10;    //快速订单系统 MATCHED_AMT 字段扩大了1000倍，而价格字段是扩大了10000倍
      if (llResultValue < 0)
      {
        bFlag = false;
        llResultValue = (-1)*llResultValue;
      }
    }
    else if('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
    {
      if (EXCHANGE_MATCH == siRetExChange)
      {
        sprintf(szBuff, "and market = '%s'", p_refstRiskCheckInfo.szExchange);
      }

      //组织SQL语句-买入金额
      snprintf(szSQL, SQL_LENGTH, "select sum(matchamt) from match WHERE fundid = '%s' %s and bsflag = '%s'", 
        p_refstRiskCheckInfo.szCuacctCode, szBuff, STK_BIZ_JZJY_BUY);

      iRetCode = m_ptrDaoRiskData->Select_Value(llBuyValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }

      //组织SQL语句-卖出金额
      snprintf(szSQL, SQL_LENGTH, "select sum(matchamt) from match WHERE fundid = '%s' %s and bsflag = '%s'", 
        p_refstRiskCheckInfo.szCuacctCode, szBuff, STK_BIZ_JZJY_SALL);

      iRetCode = m_ptrDaoRiskData->Select_Value(llSallValue, szSQL);
      if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
      {
        _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
      }

      llResultValue = llBuyValue*10000 - llSallValue*10000; 
      if (llResultValue < 0)
      {
        bFlag = false;
        llResultValue = (-1)*llResultValue;
      }
    }

    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue, 10000*10000);
    if (iRetCode != MA_OK)
    {
      if (bFlag && STK_BIZ_SALL == p_refstRiskCheckInfo.siStkBiz)
      {
        iRetCode = MA_OK;
        m_strLastErrorText.clear();
      }
      else if (!bFlag && STK_BIZ_BUY == p_refstRiskCheckInfo.siStkBiz)
      {
        iRetCode = MA_OK;
        m_strLastErrorText.clear();
      }
      else
      {
        m_strLastErrorText += m_strErrorMsg;
      }
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}


//------------------------------------------- 
//类CBizRiskTradeTimes的实现--交易数量检查
//-------------------------------------------
int CBizRiskTradeTimes::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskTradeTimes::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskTradeTimes::RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  bool bIsJzjy = '0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1];
  return !bIsJzjy && p_refstRiskCheckInfo.bIsCosDatabase ? 
    RiskCheckCosDatabase(p_refstRiskCheckInfo,p_refstRuleInfo)
    : RiskCheckSubsysDatabase(p_refstRiskCheckInfo,p_refstRuleInfo);
}


int CBizRiskTradeTimes::RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  char szSQL[SQL_LENGTH] = {0};
  long long llResultValue =  0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskTradeTimes invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    iRetCode = DBEngineInit(COS_DATA_XA);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //组织SQL语句
    if (EXCHANGE_MATCH == siRetExChange)
    {
      //查询订单数据库-去掉撤单流水委托记录
      snprintf(szSQL, SQL_LENGTH, "select count(*) from COS_SUB_ORDER WHERE CUACCT_CODE = '%s' and IS_WITHDRAW != 'T' and STKEX = '%s'", 
        p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szExchange);
    }
    else
    {
      snprintf(szSQL, SQL_LENGTH, "select count(*) from COS_SUB_ORDER WHERE CUACCT_CODE = '%s' and IS_WITHDRAW != 'T'", 
        p_refstRiskCheckInfo.szCuacctCode);
    }

    iRetCode = m_ptrDaoRiskData->Select_Value(llResultValue, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    llResultValue += p_refstRiskCheckInfo.siOrderCounts;
    //m_strLastErrorText="交易数量检查：";
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}

int CBizRiskTradeTimes::RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  char szSQL[SQL_LENGTH] = {0};
  long long llResultValue =  0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskTradeTimes invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    //初始化引擎
    iRetCode = DBEngineInit(p_refstRiskCheckInfo.szSubSysDbConnstr);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //组织SQL语句
    if (EXCHANGE_MATCH == siRetExChange)
    {
      if('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        //查询订单数据库-去掉撤单流水委托记录
        snprintf(szSQL, SQL_LENGTH, "select count(*) from STK_ORDER WHERE CUACCT_CODE = '%s' and IS_WITHDRAW != 'T' and STKEX = '%s'", 
          p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szExchange);
      }
      else if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        //集中交易
        snprintf(szSQL, SQL_LENGTH, "select count(*) from orderrec WHERE fundid = '%s' and cancelflag != 'T' and market = '%s'", 
          p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szExchange);
      }
    }
    else
    {
      if('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        //查询订单数据库-去掉撤单流水委托记录
        snprintf(szSQL, SQL_LENGTH, "select count(*) from STK_ORDER WHERE CUACCT_CODE = '%s' and IS_WITHDRAW != 'T'", 
          p_refstRiskCheckInfo.szCuacctCode);

      }
      else if('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        //集中交易
        snprintf(szSQL, SQL_LENGTH, "select count(*) from orderrec WHERE fundid = '%s' and cancelflag != 'T'", 
          p_refstRiskCheckInfo.szCuacctCode);
      }
    }

    iRetCode = m_ptrDaoRiskData->Select_Value(llResultValue, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    llResultValue += p_refstRiskCheckInfo.siOrderCounts;
    //m_strLastErrorText="交易数量检查：";
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}


//------------------------------------------- 
//类CBizRiskStockTradeTimes的实现--个股交易次数检查
//-------------------------------------------
int CBizRiskStockTradeTimes::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskStockTradeTimes::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskStockTradeTimes::RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  bool bIsJzjy = '0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1];
  return !bIsJzjy && p_refstRiskCheckInfo.bIsCosDatabase ? 
    RiskCheckCosDatabase(p_refstRiskCheckInfo,p_refstRuleInfo)
    : RiskCheckSubsysDatabase(p_refstRiskCheckInfo,p_refstRuleInfo);
}


int CBizRiskStockTradeTimes::RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  char szSQL[SQL_LENGTH] = {0};
  long long llResultValue =  0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskStockTradeTimes invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    iRetCode = DBEngineInit(COS_DATA_XA);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //组织SQL语句-深圳市场和上海市场股票代码不一样，所以无需加入交易所条件搜索  
    snprintf(szSQL, SQL_LENGTH, "select count(*) from COS_SUB_ORDER WHERE CUACCT_CODE = '%s' \
                                and IS_WITHDRAW != 'T' and TRD_CODE = '%s' and ORDER_STATUS != '%c'", 
                                p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szTrdCode, ORDER_EXE_STATUS_CANCEL);

    iRetCode = m_ptrDaoRiskData->Select_Value(llResultValue, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    llResultValue += p_refstRiskCheckInfo.siOrderCounts;
    //m_strLastErrorText="个股交易次数检查：";
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}

int CBizRiskStockTradeTimes::RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  char szSQL[SQL_LENGTH] = {0};
  long long llResultValue =  0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskStockTradeTimes invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    //初始化引擎
    iRetCode = DBEngineInit(p_refstRiskCheckInfo.szSubSysDbConnstr);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
    {
      //组织SQL语句-深圳市场和上海市场股票代码不一样，所以无需加入交易所条件搜索  
      snprintf(szSQL, SQL_LENGTH, "select count(*) from STK_ORDER WHERE CUACCT_CODE = '%s' \
      and IS_WITHDRAW != 'T' and STK_CODE = '%s' and ORDER_STATUS != '%c'", 
      p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szTrdCode, ORDER_EXE_STATUS_CANCEL);
    }
    else if('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
    {
      snprintf(szSQL, SQL_LENGTH, "select count(*) from orderrec WHERE fundid = '%s' \
      and cancelflag != 'T' and stkcode = '%s' and orderstatus != '%c'", 
      p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szTrdCode, ORDER_EXE_STATUS_CANCEL);
    }

    iRetCode = m_ptrDaoRiskData->Select_Value(llResultValue, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    llResultValue += p_refstRiskCheckInfo.siOrderCounts;
    //m_strLastErrorText="个股交易次数检查：";
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}


//------------------------------------------- 
//类CBizRiskStockSingleAmt的实现--个股单向交易金额检查
//-------------------------------------------
int CBizRiskStockSingleAmt::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskStockSingleAmt::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskStockSingleAmt::RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  bool bIsJzjy = '0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1];
  return !bIsJzjy && p_refstRiskCheckInfo.bIsCosDatabase ? 
    RiskCheckCosDatabase(p_refstRiskCheckInfo,p_refstRuleInfo)
    : RiskCheckSubsysDatabase(p_refstRiskCheckInfo,p_refstRuleInfo);
}

int CBizRiskStockSingleAmt::RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siSysType = 0;
  short siRetExChange = 0;
  long long llResultValue =  0;
  long long llCurrPrice = 0;

  char szBsflag[2+1] = {0};
  char szTrdBizBuff[32] = {0};
  char szSQL[SQL_LENGTH] = {0};

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskStockSingleAmt invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    iRetCode = DBEngineInit(COS_DATA_XA);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //从内存行情获取当前价
    iRetCode = GetCurrPriceFromMemMkt(llCurrPrice, p_refstRiskCheckInfo.szExchange[0], p_refstRiskCheckInfo.szTrdCode);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //如果内存行情没有取到当前价-从订单或期权数据库证券行情信息表获取
    if (0 == llCurrPrice)
    {
      _ma_throw ma::CBizException(RISK_NO_CURR_PRICE, "从内存行情中获取不到当前价");
    }

    //判断买卖方向-暂时根据业务类型
    if (STK_BIZ_BUY == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_SALL == p_refstRiskCheckInfo.siStkBiz)
    {
      sprintf(szTrdBizBuff, "and TRD_BIZ = %d", p_refstRiskCheckInfo.siStkBiz);
    }
    else if (STK_BIZ_BUY_OPEN == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_BUY_CLOSE == p_refstRiskCheckInfo.siStkBiz)
    {
      sprintf(szTrdBizBuff, "and TRD_BIZ in(%d, %d)", STK_BIZ_BUY_OPEN, STK_BIZ_BUY_CLOSE);
    }
    else if (STK_BIZ_SALL_CLOSE == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_SALL_OPEN == p_refstRiskCheckInfo.siStkBiz)
    {
      sprintf(szTrdBizBuff, "and TRD_BIZ in(%d, %d)", STK_BIZ_SALL_CLOSE, STK_BIZ_SALL_OPEN);
    }

    //组织SQL语句- 市价成交、最优价等取当前价格
    snprintf(szSQL, SQL_LENGTH, "select ORDER_QTY, ORDER_PRICE, WITHDRAWN_QTY, MATCHED_AMT from COS_SUB_ORDER WHERE CUACCT_CODE = '%s' \
                                and TRD_CODE = '%s' and IS_WITHDRAW = 'F' and ORDER_STATUS not in('%c', '%c') %s", p_refstRiskCheckInfo.szCuacctCode, 
                                p_refstRiskCheckInfo.szTrdCode, ORDER_EXE_STATUS_CANCEL, ORDER_EXE_STATUS_INVALIDE, szTrdBizBuff);

    iRetCode = m_ptrDaoRiskData->Select_TrdCodeSingleAmt(llResultValue, llCurrPrice, siSysType, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    if (0 == p_refstRiskCheckInfo.llOrderPrice)
    {
      p_refstRiskCheckInfo.llOrderPrice = llCurrPrice;
    }

    //阈值单位:万元,统一计算单位：元
    llResultValue = llResultValue + p_refstRiskCheckInfo.llOrderQty * p_refstRiskCheckInfo.llOrderPrice;
    //m_strLastErrorText="个股单向交易金额检查："; 
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue, 10000*10000);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}


int CBizRiskStockSingleAmt::RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siSysType = 0;
  short siRetExChange = 0;
  long long llResultValue =  0;
  long long llCurrPrice = 0;

  char szBsflag[2+1] = {0};
  char szTrdBizBuff[32] = {0};
  char szSQL[SQL_LENGTH] = {0};

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskStockSingleAmt invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    //初始化引擎
    iRetCode = DBEngineInit(p_refstRiskCheckInfo.szSubSysDbConnstr);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //从内存行情获取当前价
    iRetCode = GetCurrPriceFromMemMkt(llCurrPrice, p_refstRiskCheckInfo.szExchange[0], p_refstRiskCheckInfo.szTrdCode);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //如果内存行情没有取到当前价-从订单或期权数据库证券行情信息表获取
    if (0 == llCurrPrice)
    {
      iRetCode = GetCurrPriceFromDataBase(llCurrPrice, p_refstRiskCheckInfo,
        p_refstRiskCheckInfo.szStkbd, p_refstRiskCheckInfo.szTrdCode);
      if (iRetCode != MA_OK)
      {
        m_strLastErrorText = m_strErrorMsg;
        _ma_leave;
      }
    }

    if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
    {
      siSysType = 0;

      //判断买卖方向-暂时根据业务类型
      if (STK_BIZ_BUY == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_SALL == p_refstRiskCheckInfo.siStkBiz)
      {
        sprintf(szTrdBizBuff, "and STK_BIZ = %d", p_refstRiskCheckInfo.siStkBiz);
      }
      else if (STK_BIZ_BUY_OPEN == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_BUY_CLOSE == p_refstRiskCheckInfo.siStkBiz)
      {
        sprintf(szTrdBizBuff, "and STK_BIZ in(%d, %d)", STK_BIZ_BUY_OPEN, STK_BIZ_BUY_CLOSE);
      }
      else if (STK_BIZ_SALL_CLOSE == p_refstRiskCheckInfo.siStkBiz || STK_BIZ_SALL_OPEN == p_refstRiskCheckInfo.siStkBiz)
      {
        sprintf(szTrdBizBuff, "and STK_BIZ in(%d, %d)", STK_BIZ_SALL_CLOSE, STK_BIZ_SALL_OPEN);
      }
      //组织SQL语句- 市价成交、最优价等取当前价格
      snprintf(szSQL, SQL_LENGTH, "select ORDER_QTY, ORDER_PRICE, WITHDRAWN_QTY, MATCHED_AMT from STK_ORDER WHERE CUACCT_CODE = '%s' \
      and STK_CODE = '%s' and IS_WITHDRAW = 'F' and ORDER_STATUS not in('%c', '%c') %s", p_refstRiskCheckInfo.szCuacctCode, 
      p_refstRiskCheckInfo.szTrdCode, ORDER_EXE_STATUS_CANCEL, ORDER_EXE_STATUS_INVALIDE, szTrdBizBuff);
    }
    else if('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
    {
      siSysType = 1;
      //字典转换
      StkBiztoBsFlag(p_refstRiskCheckInfo.siStkBiz,szBsflag,sizeof(szBsflag));
      snprintf(szSQL, SQL_LENGTH, "select orderqty, orderprice, cancelqty, matchamt from orderrec WHERE fundid = '%s' \
      and stkcode = '%s' and cancelflag = 'F' and orderstatus not in('%c', '%c') and bsflag = '%s'", p_refstRiskCheckInfo.szCuacctCode, 
      p_refstRiskCheckInfo.szTrdCode, ORDER_EXE_STATUS_CANCEL, ORDER_EXE_STATUS_INVALIDE, szBsflag);
    }
    
    iRetCode = m_ptrDaoRiskData->Select_TrdCodeSingleAmt(llResultValue, llCurrPrice, siSysType, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    if (0 == p_refstRiskCheckInfo.llOrderPrice)
    {
      p_refstRiskCheckInfo.llOrderPrice = llCurrPrice;
    }

    //阈值单位:万元,统一计算单位：元
    llResultValue = llResultValue + p_refstRiskCheckInfo.llOrderQty * p_refstRiskCheckInfo.llOrderPrice;
    //m_strLastErrorText="个股单向交易金额检查："; 
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue, 10000*10000);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}


//------------------------------------------- 
//类CBizRiskDeclareRatio的实现--报撤单百分比检查
//-------------------------------------------
int CBizRiskDeclareRatio::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskDeclareRatio::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskDeclareRatio::RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  bool bIsJzjy = '0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1];
  return !bIsJzjy && p_refstRiskCheckInfo.bIsCosDatabase ? 
    RiskCheckCosDatabase(p_refstRiskCheckInfo,p_refstRuleInfo)
    : RiskCheckSubsysDatabase(p_refstRiskCheckInfo,p_refstRuleInfo);
}

int CBizRiskDeclareRatio::RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  char szSQL[SQL_LENGTH] = {0};
  long long llDeclareNum = 0;
  long long llAllOrderNum = 0;
  float flResultValue = 0;
  long long llResultValue =  0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskDeclareRatio invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }


    iRetCode = DBEngineInit(COS_DATA_XA);

    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //组织SQL语句-报单笔数
    if (EXCHANGE_MATCH == siRetExChange)
    {
      snprintf(szSQL, SQL_LENGTH, "select count(*) from COS_SUB_ORDER WHERE CUACCT_CODE = '%s' \
                                  and STKEX = '%s' and ORDER_STATUS in('%c', '%c')", p_refstRiskCheckInfo.szCuacctCode, 
                                  p_refstRiskCheckInfo.szExchange, ORDER_EXE_STATUS_MATCH_CANCEL, ORDER_EXE_STATUS_CANCEL);

    }
    else
    {
      snprintf(szSQL, SQL_LENGTH, "select count(*) from COS_SUB_ORDER WHERE CUACCT_CODE = '%s' \
                                  and ORDER_STATUS in('%c', '%c')", p_refstRiskCheckInfo.szCuacctCode, 
                                  ORDER_EXE_STATUS_MATCH_CANCEL, ORDER_EXE_STATUS_CANCEL);
    }

    iRetCode = m_ptrDaoRiskData->Select_Value(llDeclareNum, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    //组织SQL语句-总笔数
    if (EXCHANGE_MATCH == siRetExChange)
    {
      snprintf(szSQL, SQL_LENGTH, "select count(*) from COS_SUB_ORDER WHERE CUACCT_CODE = '%s' and IS_WITHDRAW != 'T' and STKEX = '%s'", 
        p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szExchange);
    }
    else
    {
      snprintf(szSQL, SQL_LENGTH, "select count(*) from COS_SUB_ORDER WHERE CUACCT_CODE = '%s' and IS_WITHDRAW != 'T'", 
        p_refstRiskCheckInfo.szCuacctCode);
    }

    iRetCode = m_ptrDaoRiskData->Select_Value(llAllOrderNum, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    if (llAllOrderNum - llDeclareNum > 0)
    {
      flResultValue = float((llDeclareNum*1.0)/llAllOrderNum); //当前笔数不算
      llResultValue = flResultValue*100*100;   //百分比-扩大100倍
    }
    //m_strLastErrorText="报撤单百分比检查：";
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue, 100); //阈值单位%
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}


int CBizRiskDeclareRatio::RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  char szSQL[SQL_LENGTH] = {0};
  long long llDeclareNum = 0;
  long long llAllOrderNum = 0;
  float flResultValue = 0;
  long long llResultValue =  0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskDeclareRatio invalid parameter m_ptrServiceBizEnv is NULL");
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }


    iRetCode = DBEngineInit(p_refstRiskCheckInfo.szSubSysDbConnstr);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //组织SQL语句-报单笔数
    if (EXCHANGE_MATCH == siRetExChange)
    {
      if('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' ==p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        snprintf(szSQL, SQL_LENGTH, "select count(*) from STK_ORDER WHERE CUACCT_CODE = '%s' \
        and STKEX = '%s' and ORDER_STATUS in('%c', '%c')", p_refstRiskCheckInfo.szCuacctCode, 
        p_refstRiskCheckInfo.szExchange, ORDER_EXE_STATUS_MATCH_CANCEL, ORDER_EXE_STATUS_CANCEL);
      }
      else if('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' ==p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        // 集中交易
        snprintf(szSQL, SQL_LENGTH, "select count(*) from orderrec WHERE fundid = '%s' \
        and market = '%s' and orderstatus in('%c', '%c')", p_refstRiskCheckInfo.szCuacctCode, 
        p_refstRiskCheckInfo.szExchange, ORDER_EXE_STATUS_MATCH_CANCEL, ORDER_EXE_STATUS_CANCEL);
      }
    }
    else
    {
      if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        snprintf(szSQL, SQL_LENGTH, "select count(*) from STK_ORDER WHERE CUACCT_CODE = '%s' \
        and ORDER_STATUS in('%c', '%c')", p_refstRiskCheckInfo.szCuacctCode, 
        ORDER_EXE_STATUS_MATCH_CANCEL, ORDER_EXE_STATUS_CANCEL);
      }
      else if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        // 集中交易
        snprintf(szSQL, SQL_LENGTH, "select count(*) from orderrec WHERE fundid = '%s' \
        and orderstatus in('%c', '%c')", p_refstRiskCheckInfo.szCuacctCode, 
        ORDER_EXE_STATUS_MATCH_CANCEL, ORDER_EXE_STATUS_CANCEL);
      }
    }

    iRetCode = m_ptrDaoRiskData->Select_Value(llDeclareNum, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    //组织SQL语句-总笔数
    if (EXCHANGE_MATCH == siRetExChange)
    {
      if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        snprintf(szSQL, SQL_LENGTH, "select count(*) from STK_ORDER WHERE CUACCT_CODE = '%s' and IS_WITHDRAW != 'T' and STKEX = '%s'", 
        p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szExchange);
      }
      else if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        snprintf(szSQL, SQL_LENGTH, "select count(*) from orderrec WHERE fundid = '%s' and cancelflag != 'T' and market = '%s'", 
        p_refstRiskCheckInfo.szCuacctCode, p_refstRiskCheckInfo.szExchange);
      }
     
    }
    else
    {
      if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '0' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        snprintf(szSQL, SQL_LENGTH, "select count(*) from STK_ORDER WHERE CUACCT_CODE = '%s' and IS_WITHDRAW != 'T'", 
          p_refstRiskCheckInfo.szCuacctCode);
      }

      else if ('0' == p_refstRiskCheckInfo.szSubsysSnType[0] && '1' == p_refstRiskCheckInfo.szSubsysSnType[1])
      {
        snprintf(szSQL, SQL_LENGTH, "select count(*) from orderrec WHERE fundid = '%s' and cancelflag != 'T'", 
          p_refstRiskCheckInfo.szCuacctCode);
      }
    }

    iRetCode = m_ptrDaoRiskData->Select_Value(llAllOrderNum, szSQL);
    if (iRetCode != MA_OK && iRetCode != MA_NO_DATA)
    {
      _ma_throw ma::CBizException(iRetCode, m_ptrDaoRiskData->GetLastErrorText());
    }

    if (llAllOrderNum - llDeclareNum > 0)
    {
      flResultValue = float((llDeclareNum*1.0)/llAllOrderNum); //当前笔数不算
      llResultValue = flResultValue*100*100;   //百分比-扩大100倍
    }
    //m_strLastErrorText="报撤单百分比检查：";
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue, 100); //阈值单位%
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}


//------------------------------------------- 
//类CBizRiskTradePrice的实现--交易价格百分比
//-------------------------------------------
int CBizRiskTradePrice::Initialize(void)
{
  int iRetCode = MA_OK;

  m_iLastErrorCode = 0;
  m_strLastErrorText.clear();

  return iRetCode;
}

int CBizRiskTradePrice::Uninitialize(void)
{
  int iRetCode = MA_OK;
  return iRetCode;
}

int CBizRiskTradePrice::RiskCheckInterface(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  return p_refstRiskCheckInfo.bIsCosDatabase ? 
    RiskCheckCosDatabase(p_refstRiskCheckInfo,p_refstRuleInfo)
    : RiskCheckSubsysDatabase(p_refstRiskCheckInfo,p_refstRuleInfo);
}

int CBizRiskTradePrice::RiskCheckCosDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  long long llCurrPrice = 0;
  long long llDiffPrice = 0;
  float fResultValue = 0;
  long long llResultValue = 0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskTradePrice invalid parameter m_ptrServiceBizEnv is NULL");
    }

    if (!IsLimitPrice(p_refstRiskCheckInfo.siStkBizAction))
    {
      _ma_leave;  //市价委托不检查
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    //从内存行情获取当前价
    iRetCode = GetCurrPriceFromMemMkt(llCurrPrice, p_refstRiskCheckInfo.szExchange[0], p_refstRiskCheckInfo.szTrdCode);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    if (0 == llCurrPrice)
    {
      _ma_throw ma::CBizException(RISK_NO_CURR_PRICE, "从内存行情中获取不到当前价");
    }

    //llCurrPrice = 120000; //测试
    llDiffPrice = abs(p_refstRiskCheckInfo.llOrderPrice - llCurrPrice);
    fResultValue = float((llDiffPrice*1.0)/llCurrPrice);
    llResultValue = fResultValue*100*100;   //百分比-扩大100倍
    //m_strLastErrorText="交易价格百分比检查：";
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue, 100);  //阈值单位%
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}

int CBizRiskTradePrice::RiskCheckSubsysDatabase(ST_RISK_CHECK_INFO &p_refstRiskCheckInfo, ST_RULE_INFO &p_refstRuleInfo)
{
  int iRetCode = MA_OK;

  short siRetExChange = 0;
  long long llCurrPrice = 0;
  long long llDiffPrice = 0;
  float fResultValue = 0;
  long long llResultValue = 0;

  _ma_try
  {
    if (m_ptrServiceBizEnv.IsNull())
    {
      _ma_throw ma::CBizException(MA_ERROR_INVALID_PARAM, "CBizRiskTradePrice invalid parameter m_ptrServiceBizEnv is NULL");
    }

    if (!IsLimitPrice(p_refstRiskCheckInfo.siStkBizAction))
    {
      _ma_leave;  //市价委托不检查
    }

    //判断是否匹配交易所
    siRetExChange = IsExchangeCheck(p_refstRuleInfo.szRiskClassParm, p_refstRiskCheckInfo.szExchange[0]);
    if (EXCHANGE_NOT_MATCH == siRetExChange)
    {
      return iRetCode;
    }

    //从内存行情获取当前价
    iRetCode = GetCurrPriceFromMemMkt(llCurrPrice, p_refstRiskCheckInfo.szExchange[0], p_refstRiskCheckInfo.szTrdCode);
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText = m_strErrorMsg;
      _ma_leave;
    }

    //如果内存行情没有取到当前价-从订单或期权数据库证券行情信息表获取
    if (0 == llCurrPrice)
    {
      iRetCode = GetCurrPriceFromDataBase(llCurrPrice, p_refstRiskCheckInfo,
        p_refstRiskCheckInfo.szStkbd, p_refstRiskCheckInfo.szTrdCode);
      if (iRetCode != MA_OK)
      {
        m_strLastErrorText = m_strErrorMsg;
        _ma_leave;
      }
    }

    //llCurrPrice = 120000; //测试
    llDiffPrice = abs(p_refstRiskCheckInfo.llOrderPrice - llCurrPrice);
    fResultValue = float((llDiffPrice*1.0)/llCurrPrice);
    llResultValue = fResultValue*100*100;   //百分比-扩大100倍
    //m_strLastErrorText="交易价格百分比检查：";
    iRetCode = RiskpThresholdResolve(p_refstRuleInfo, llResultValue, 100);  //阈值单位%
    if (iRetCode != MA_OK)
    {
      m_strLastErrorText += m_strErrorMsg;
      _ma_leave;
    }
  }

  _ma_catch_finally
  {
  }

  return  iRetCode;
}

