// @Author Lin Ya
// @Email xxbbb@vip.qq.com
// 用于解析命令行参数
#include <getopt.h>
#include <string>
#include "EventLoop.h"
#include "Server.h"
#include "base/Logging.h"


int main(int argc, char *argv[]) {
  int threadNum = 4;
  int port = 80;
  std::string logPath = "./WebServer.log";

  // parse args  解析参数
  int opt;
  const char *str = "t:l:p:";
  while ((opt = getopt(argc, argv, str)) != -1) {
    switch (opt) {
      case 't': {
        threadNum = atoi(optarg);
        break;
      }
      case 'l': {
        logPath = optarg;
        if (logPath.size() < 2 || optarg[0] != '/') {
          printf("logPath should start with \"/\"\n");
          abort();
        }
        break;
      }
      case 'p': {
        port = atoi(optarg);
        break;
      }
      default:
        break;
    }
  }
  // 第三个参数-日志目录-用于输出日志
  Logger::setLogFileName(logPath);
// STL库在多线程上应用
  // 条件编译：如果_PTHREADS没有被定义，则输出相应提示
#ifndef _PTHREADS
  LOG << "_PTHREADS is not defined !";
#endif
  
  // 前两个参数用于启动服务器
  EventLoop mainLoop;
  Server myHTTPServer(&mainLoop, threadNum, port);
  myHTTPServer.start();
  mainLoop.loop();
  return 0;
}
