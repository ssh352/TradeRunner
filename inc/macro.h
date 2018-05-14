#ifndef MACRO_H_
#define MACRO_H_

#define TICKER_LEN            6
#define ORDER_EXCH_ID_LEN     17
#define ORDER_LOCAL_ID        11
#define EXEC_ID               18

#define MAX_USERNAME	64
#define MAX_PWD			64
#define MAX_IP			16
#define MAX_KEY			65
#define MAX_ERROR_MSG   256

#define UINT64_T              1
#define INT64_T               2
#define INT_T                 3
#define CHAR_T                4
#define DOUBLE                5

#define RECONNEC_INTERVAL_TIME					3		 // 重新登录的间隔时间

// 交易功能号
#define TS_FUNC_INSERT_ORDER					"200000" // 委托下单
#define TS_FUNC_CANCEL_ORDER					"300000" // 撤单
#define TS_FUNC_MONEY_ACCOUNT_INFO				"400010" // 资金账户信息查询
#define TS_FUNC_ASSET							"400020" // 资产明细查询
#define TS_FUNC_ORDER_QUERY_FOR_ID				"400031" // 根据订单ID查询订单
#define TS_FUNC_ORDER_QUERY						"400032" // 根据合约号和日期查询订单
#define TS_FUNC_POSITION_QUERY					"400050" // 查询持仓
#define TS_FUNC_TRADE_QUERY						"400061" // 根据多条件查询成交
#define TS_FUNC_TRADE_QUERY_FOR_ID				"400062" // 根据成交ID查询成交
// 异步数据应答功能号
#define TS_FUNC_ASYN_PUSH						"900000" // 异步推送
#define TS_FUNC_TRADE_RES						"200100" // 成交应答
#define TS_FUNC_ALL_TRADE_RES					"200200" // 全部成交应答
#define TS_FUNC_LEFT_CANCEL_ORDER_RES			"200300" // 剩撤应答
#define TS_FUNC_CANCEL_ORDER_RES				"300100" // 撤单应答
#define TS_FUNC_INSERT_ORDER_FAILED_RES			"200101" // 报单异常回报
#define TS_FUNC_CANCEL_FAILED_RES				"300101" // 撤单异常回报
#define TS_FUNC_NO_TRADE_RES					"200101" // 未成交

//响应码
#define TS_RES_CODE_SUCCESS                     "000000" // 操作成功
#define TS_RES_CODE_TRADE_LOGIN_FAILED			"000001" // 交易登录失败
#define TS_RES_CODE_QUOTE_LOGIN_FAILED			"000002" // 行情登录失败
#define TS_RES_CODE_ENTRUST_FAILED				"000003" // 下单失败
#define TS_RES_CODE_CANCEL_ORDER_FAILED			"000004" // 撤单失败
#define TS_RES_CODE_QUERY_ORDER_FAILED			"000005" // 订单查询失败
#define TS_RES_CODE_QUERY_TRADE_FAILED			"000006" // 成交查询失败
#define TS_RES_CODE_QUERY_HOLD_FAILED			"000007" // 持仓查询失败
#define TS_RES_CODE_QUERY_ASSERT_FAILED			"000008" // 资产查询失败


#endif