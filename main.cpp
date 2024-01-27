#include "config.h"
#include "memoryPool/memoryPool.h"
#include <iostream>
#include "./log/async_logging.h"
#include "./log/logging.h"


off_t kRollSize = 1 * 1000 * 1000; // 只设置1M
static AsyncLogging *g_asyncLog = NULL;

static void asyncOutput(const char *msg, int len) {
    g_asyncLog->append(msg, len);
}

int initLog() {
    printf("pid = %d\n", getpid());
    g_logLevel = Logger::INFO;   // 设置级别
    char name[256] = "serverLog"; //日志名字  tuchuang.log
    // 回滚大小kRollSize（1M）, 最大1秒刷一次盘（flush）
    AsyncLogging *log =
        new AsyncLogging(::basename(name), kRollSize,1); // 注意，每个文件的大小
                             // 还取决于时间的问题，不是到了大小就一定换文件。
    Logger::setOutput(asyncOutput);   // 不是说只有一个实例

    g_asyncLog = log;
    log->start(); // 启动日志写入线程

    return 0;
}

void deInitLog() {
    if (g_asyncLog) {
        delete g_asyncLog;
        g_asyncLog = NULL;
    }
}


int main(int argc,char *argv[])
{
    if(argc < 2)
    {
        std::cout<<"usage "<<argv[0]<<" configFile.json"<<std::endl;
        return 1;
    }
    /*保证程序关闭时 关闭日志*/
    // log4cplus::Initializer initializer;
    // printf("sasasa\n");
    // getchar();
    Config config;
    // config.parse_config_file("configFile.json");
    config.parse_config_file(argv[1]);
    // print(config);
    initLog();
    WebServer server;
    
    
    server.init(config.PORT,config.user,config.password,config.databaseName,config.LOGWrite,
                    config.OPT_LINGER,config.TRIGMode,config.sql_num,
                    config.thread_num,config.close_log,config.actor_model);
    
    // server.log_write();

    server.sql_pool();
        
    server.trig_mode();

    server.eventListen();

    server.eventLoop();

    deInitLog();

    return 0;
}
