OBJS = TraceGen.o BucketTree.o MicRuleTree.o OFswitch.o RuleList.o SimuFinal.o TraceAnalyze.o
CC = g++
DEBUG = -g
#BOOST_DYN = -lboost_log_setup -lboost_log
BOOST_I = -I -DBOOST_LOG_DYN_LINK
CFLAGS= -std=c++11 -Wall -c $(DEBUG)
LFLAGS= -std=c++11 -Wall $(DEBUG)
#BOOST_L = -L $(BOOST_ROOT)/stage/lib -DBOOST_LOG_DYN_LINK 
BOOST_DYN = -lboost_log_setup -lboost_log -lboost_thread -lboost_system -lboost_filesystem -lboost_iostreams -lz
STD_DYN = -pthread
#BOOST_I = -I $(BOOST_ROOT) -DBOOST_LOG_DYN_LINK

demo.out: $(OBJS)
	$(CC) $(BOOST_L) $(LFLAGS) $(OBJS) $(BOOST_DYN) $(STD_DYN) -o demo.out 

SimuFinal.o: SimuFinal.cpp Rule.hpp RuleList.h BucketTree.h MicRuleTree.h OFswitch.h TraceGen.h TraceAnalyze.h stdafx.h 
	$(CC) $(BOOST_I) $(CFLAGS) SimuFinal.cpp

BucketTree.o: BucketTree.cpp BucketTree.h RuleList.h Rule.hpp Address.hpp stdafx.h
	$(CC) $(CFLAGS) BucketTree.cpp

MicRuleTree.o: MicRuleTree.cpp MicRuleTree.h stdafx.h RuleList.h Address.hpp Rule.hpp
	$(CC) $(CFLAGS) MicRuleTree.cpp

OFswitch.o: OFswitch.cpp OFswitch.h stdafx.h RuleList.h BucketTree.h MicRuleTree.h lru_cache.hpp Rule.hpp 
	$(CC) $(CFLAGS) OFswitch.cpp

TraceGen.o: TraceGen.cpp TraceGen.h Address.hpp RuleList.h stdafx.h Rule.hpp
	$(CC) $(CFLAGS) TraceGen.cpp

TraceAnalyze.o: TraceAnalyze.cpp TraceAnalyze.h Address.hpp
	$(CC) $(CFLAGS) TraceAnalyze.cpp

RuleList.o: RuleList.cpp RuleList.h Rule.hpp Address.hpp stdafx.h 
	$(CC) $(CFLAGS) RuleList.cpp

clean:
	\rm *.o *~ demo.out
