#include "server_https.h"
#include "handler.h"

using namespace Wenmingxing;

int main()
{
    //HTTPS服务运行在12345端口，并启用四个线程,并以数字证书和私钥初始化
    Server<HTTPS> server(12345, 4, "server.crt", "server.key");
    start_server<Server<HTTPS>>(server);

    return 0;
}
