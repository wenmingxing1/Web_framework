#include "server_http.h"
#include "handler.h"

int main()
{
    //HTTP服务运行在12345端口，并启用四个线程
    Server<HTTP> server(12345,4);
    start_server<Server<HTTP>>(server);
    return 0;
}
