#ifndef DATA_TYPE_H_
#define DATA_TYPE_H_
#include <memory.h>
#include <stdint.h>
#include "macro.h"
#include "xtp_api_data_type.h"

enum EN_ERROR_CODE
{
	TG_SUCCESS,						// 操作成功
	TG_XTP_LOG_CATALOG_INVALID,		// xtp的日志目录
	TG_XTP_KEY_INVALID,				// xtp的key无效
	TG_XTP_ADDR_INVALID,			// xtp地址无效
	TG_XTP_ACCOUNT_INVALID,			// xtp账号无效
	TG_XTP_REQ_OBJ_INVALID,			// xtp请求对象无效
	TG_LAST_END
};

enum EN_PACKET_TYPE
{
	PT_UNKNOW,				// 未知
	PT_LOGIN,				// 登录
	PT_ORDER,				// 订单
	PT_TRADE,				// 成交
	PT_CANCEL_ORDER,		// 撤单
	PT_ORDER_QUERY,			// 订单查询
	PT_TRADE_QUERY,			// 成交查询
	PT_POSITION_QUERY,		// 持仓查询
	PT_ASSET_QUERY,			// 资金查询
};


typedef struct _Addr
{
	char szIP[MAX_IP];
	int nPort;
	_Addr()
	{
		memset(this, 0, sizeof(_Addr));
	}
}T_Addr;

typedef struct _XtpTradeInfo
{
	T_Addr tAddr;
	char szUserName[MAX_USERNAME];
	char szPwd[MAX_PWD];
	char szKey[MAX_KEY];
	int  clientID;
	_XtpTradeInfo()
	{
		memset(this, 0, sizeof(_XtpTradeInfo));
	}
}T_XtpTradeInfo;

typedef struct _OrderData
{
	uint64_t					  order_xtp_id;						// 委托ID
	uint64_t					  order_client_id;					// 客户ID
	uint64_t					  order_cancel_client_id;			// 撤单客户ID 
	uint64_t					  order_cancel_xtp_id;				// 撤单ID
	char						  ticker[TICKER_LEN];				// 合约号
	XTP_MARKET_TYPE				  market;							// 市场
	double						  req_price;						// 报单价格
	int64_t						  quantity;							// 委托数量
	XTP_PRICE_TYPE				  price_type;						// 价格类型
	XTP_SIDE_TYPE				  side;								// 买卖方向
	XTP_BUSINESS_TYPE			  business_type;					// 业务类型
	int64_t						  qty_traded;						// 成交数量
	int64_t						  insert_time;						// 委托时间
	int64_t						  update_time;						// 最后修改时间
	int64_t						  cancel_num;						// 撤单数量
	double						  cancel_price;						// 撤单价格
	double						  cancel_amount;					// 撤单金额
	int64_t						  cancel_time;						// 撤单时间
	int64_t						  trade_amount;						// 总成交金额
	double						  trdade_sucprice;					// 成交价格
	int64_t						  trdade_quantity;					// 成交数量
	int64_t						  trade_time;						// 成交时间
	char						  exec_id[EXEC_ID];					// 成交编号
	uint64_t					  report_index;						// 成交序号
	char						  order_exch_id[ORDER_EXCH_ID_LEN];	// 报单编号
	char						  trade_type;						// 成交类型
	double						  trd_tfee;							// 总费用
	char						  order_local_id[ORDER_LOCAL_ID];	// 本地报单编号
	char						  order_type;						// 报单类型
	XTP_ORDER_STATUS_TYPE         order_status;						// 委托状态
	XTP_ORDER_SUBMIT_STATUS_TYPE  order_submit_status;				// 报单提交状态
	_OrderData()
	{
		memset(this, 0, sizeof(_OrderData));
	}
}T_OrderData;

// 
typedef struct _Packet
{
	EN_PACKET_TYPE enPacketType;// 应答包的类型
	uint64_t packetID;			// 应答包的requestid
	int nCount;					// 应答包的记录条数
	int nStatus;				// 应答包的状态（1-成功 0-失败）
	
	char *data;
	_Packet()
	{
		enPacketType = PT_UNKNOW;
		nCount = 0;
		data = NULL;
		packetID = 0;
	}
}T_Packet;



#endif