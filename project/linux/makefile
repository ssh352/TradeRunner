CC       = g++
CXXFLAGS = -I../../inc -I../../../inc -I../../../inc/rapidjson -I../../../inc/xtp
LIBS     = -L../../../../lib -lpthread -lboost_system -lboost_chrono -lboost_date_time -lboost_locale -lboost_regex -lboost_thread -lboost_filesystem -lboost_log -lboost_log_setup -lsodium -lxtptraderapi

VPATH    = ../../src

SRCS     = main.cpp \
	       AsynSession.cpp \
	       MyApp.cpp \
	       MyServer.cpp \
	       MySession.cpp \
	       OrderData.cpp \
	       semaphore.cpp \
	       XtpTradeReq.cpp \
	       XtpTradeRes.cpp \
           Config.cpp \
	       Log.cpp
		   
OBJS     = $(SRCS:.cpp=.o)
TARGET   = $(HOME)/bin/TradeRunner
$(TARGET):$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)
.cpp.o:
	$(CC) -c -Wall -std=c++11 -DBOOST_ALL_DYN_LINK $(CXXFLAGS) $<


.PHONY:clean
clean:
	-rm -f $(OBJS) $(TARGET)
