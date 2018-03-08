#ifndef SERVER_HTTP_H_INCLUDED
#define SERVER_HTTP_H_INCLUDED

#include "server_base.h"

namespace ShiyanlouWeb {
    typedef boost::asio::ip::tcp::socket HTTP;
    template<>
    class Server<HTTP> : public ServerBase<HTTP> {
    public:
        //构造函数：通过端口号、线程数来构造Web服务器，HTTP服务器较简单，不需要做相关配置工作
        Server(unsigned short port, size_t num_threads = 1) : ServerBase<HTTP>::ServerBase(port, num_threads) {};
    private:
        //实现accept方法
        void accept() {
            //为当前连接创建一个新的socket
            //shared_ptr用于传递临时对象给匿名函数
            //socket会被推导为shared_ptr<HTTP>类型
            /* 要保证async_accept的整个异步操作期间socket保持有效（否则在异步操作期间socket没了，并且客户端连接之后这个socket对象还有用），
             * 这里的方法是使用一个智能指针shared_prt<socket>，并将这个指针作为参数绑定到回调函数上。 这也是前面使用智能指针的原因。
            */
            auto socket = std::make_shared<HTTP>(m_io_service)；

            /* 在Asio中，异步方式的函数名称前面都有async_前缀，函数参数里会要求放一个回调函数。
             * 异步操作执行后，不管有没有完成都会立即返回，这是可以做一些其他事情，知道回调函数被调用，说明异步操作完成。
             * 回调函数一般会接受一个error_code参数，一般使用bind来绑定其他参数，或使用lambda代替bind
            */
            acceptor.async_accept(*socket, [this,socket](const boost::system::error_code& ec)
                                  {
                                      //立即启动并接收一个连接
                                      accept();
                                      //如果出现错误，则进行相应的request与respond
                                      if (!ec) process_request_and_respond(socket);
                                  });
        }
    };
}

#endif // SERVER_HTTP_H_INCLUDED
