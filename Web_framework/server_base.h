#ifndef SERVER_BASE_H_INCLUDED
#define SERVER_BASE_H_INCLUDED

#include<unordered_map>
#include<thread>

namespace ShiyanlouWeb {
    /*Request结构体用于解析请求，如请求方法，请求路径，HTTP版本等信息*/
    struct Request {
        //请求方法，GET，POST；请求路径；HTTP版本
        std::string method, path, http_version;
        //对content使用智能指针进行引用计数，用于保存请求体中包含的内容
        std::shared_ptr<std::istream> content;
        //因为不关心header中信息的顺序，所以采用unordered_map来保存header
        std::unordered_map<std::string, std::string> headers;
        //用正则表达式处理路径是否匹配问题
        std::smatch path_match;
    };

    //使用typedef简化资源类型的表示方法
    /* resource_type是一个map，其键为一个字符串，值则是一个无序容器unordered_map
     * 这个unordered_map的键依然是一个字符串，其值接受一个返回类型为空、参数为ostream与Request的函数。
     * 因此，我们在使用这套框架的时候，当我们有了一个server对象，定义资源可以如下形式：
     * server.resource["^/info/?$"]["GET"] = [](ostream& response, Request& request) {//处理请求及资源}；
     * 其中map用于存储请求路径的表达式，而unordered_map用于存储请求方法，lambda表达式保存处理方法。
    */
    typedef std::map<std::string, std::unordered_map<std::string,
    std::function<void(std::ostream&, Request&)>>> resource_type;

    //socket_type为HTTP或HTTPS
    template <typename socket_type> class ServerBase {
    public:
        //用于服务器访问资源处理方式
        resource_type resource;
        //用于保存默认资源的处理方式
        resource_type default_resource;

        //构造函数
        ServerBase(unsigned short port, size_t num_threads = 1):
            endpoint(boost::asio::ip::tcp::v4(), port),
            acceptor(m_io_service, endpoint),
            num_threads(num_threads) {}

        //启动服务器
        void start() {
            //默认资源放在vector的末尾，用作默认应答
            //默认的请求会在找不到匹配请求路径时，进行访问，故在最后添加
            for (auto it = resource.begin(); it != resource.end(); ++it) {
                all.resources.push_back(it);
            }
            for (auto it = default_resource.begin(); it != default_resource.end(); ++it) {
                all_resources.push_back(it);
            }

            //调用socket的连接方式，还需要子类来实现accept()逻辑
            accept();   //这是protected中的虚函数

            //如果num_threads>1，那么m_io_service.run()
            //将运行（num_threads-1）线程成为线程池
            for (size_t c = 1; c < num_threads; ++c) {
                threads.emplace_back([this](){m_io_service.run();});
            }

            //主线程
            m_io_service.run();

            //等待其他线程，如果有的话，就等待这些线程的结束
            for (auto& t:threads)
                t.join();
        }


    protected:
        /* boost.asio的使用 */
        //asio要求每个应用都具有一个io_service对象的调度器,所有异步IO时间都要通过它来分发处理
        //换句话说，需要IO的对象的构造函数，都需要传入一个io_service对象
        boost::asio::io_serviece m_io_service;
        //实现TCP socket连接，需要一个acceptor对象，而初始化一个acceptor对象就需要一个endpoint对象
        boost::asio::ip::tcp::endpoint endpoint;    //endpoint即为一个socket位于服务端的端点（IP，端口号，协议版本）
        boost::asio::ip::tcp::acceptor acceptor;    //acceptor对象被用于建立连接，通过io_service与endpoint构造，并在指定端口上等待连接

        /* 用于实现线程池 */
        size_t num_threads;
        std::vector<std::thread> threads;

        //用于内部实现对所有资源的处理，所有资源都会在vector尾部添加，并在start()中创建
        std::vector<resource_type::iterator> all_resource;

        //需要不同类型的服务器实现这个方法，以分别处理HTTP与HTTPS请求，所以定义为虚函数
        //HTTP与HTTPS两种服务器之间的处理请求、返回请求唯一的区别就在于是如何处理与客户端建立连接的方式上，也就是accept方法
        virtual void accept() {}

        //处理请求和应答
        void process_request_and respond(std::shared_ptr<socket_type> socket) const {
            //为async_read_untile()创建新的读缓存
            //shared_ptr用于传递临时对象给匿名参数
            //会被推导为std::shared_ptr<boost::asio::streambuf>
            auto read_buffer = std::make_shared<boost::asio::streambuf>();

            boost::asio::async_read_until(*socket, *read_buffer, "\r\n\r\n",
                                [this,socket, read_buffer](const boost::system::error_code& ec,size_t bytes_transferred)
                                {
                                    if(!ec) {
                                            //read_buffer->size()的大小不一定和bytes_transferred相等
                                            //在async_read_until操作成功后，streambuf在界定符之外可能包含一些额外的数据
                                            //所以较好的做法是直接从流中提取并解析当前read_buffer左边的报头，再拼接async_read后面的内容
                                            size_t total = read_buffer->size();

                                            //转换成istream
                                            std::istream stream(read_buffer.get());

                                            //被推导为std::shared_ptr<Request>类型
                                            auto request = std::make_shared<Request>();

                                            //接下来将stream中的请求信息进行解析，然后保存到request对象中
                                            *request = parse_request(stream);

                                            size_t num_additional_bytes = total - bytes_transferred;

                                            //如果满足，同样读取
                                            if (request->header.count("Content-Length") > 0) {
                                                boost::asio::async_read(*socket, *read_buffer,
                                                boost::asio::transfer_exactly(stoull(request->header["Content-Length"]) - num_additional_bytes),
                                                [this, socket, read_buffer, request](const boost::system::error_code& ec, size_t bytes_transferred) {
                                                    if (!ec) {
                                                        //将指针作为istream对象存储到read_buffer中
                                                        request->content = std::shared_ptr<std::istream>(new std::istream(read_buffer.get()));
                                                        respond(socket, request);
                                                    }
                                                });
                                            } else {
                                            respond(socket, request);
                                            }
                                          }
                                });
        }

        //用于解析请求
        Request parse_request(std::istream& stream) const {
            Request request;

            std::regex e("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");

            std::smatch sub_match;

            //从第一行中解析出，方法；路径；HTTP版本
            std::string line;
            getline(stream, line);
            line_pop_back();
            if (std::regex_match(line, sub_match, e)) {
                request.method = sub_match[1];
                request.path = sub_match[2];
                request.http_version = sub_match[3];

                bool matched;
                e="^([^:]*): ?(.*)$";
                //解析头部其他信息
                do {
                    getline(stream, line);
                    line.pop_back();
                    matched = std::regex_match(line, sub_match, e);
                    if (matched) {
                        request.header[sub_match[1]] = sub_match[2];
                    }
                } while (matched = true);
            }
            return request;
        }

        //应答
        void respond(std::shared_ptr<socket_type> socket, std::shared_ptr<Request> request) const {
            //对请求路径和方法进行匹配查找，并生成响应
            for (auto res_it:all_resources) {
                std::regex e(res_it->first);
                std::smatch sm_res;
                if (std::regex_match(request->path, sm_res, e)) {
                    if (res_it->second.count(request->method) > 0) {
                        request->path_match = move(sm_res);

                        //会被推导为std::shared_ptr<boost::asio::streambuf>
                        auto write_buffer = std::make_shared<boost::asio::streambuf>();
                        std::ostream response(write_buffer.get());
                        res_it->second[request->method](response, *request);

                        //在lambda中捕获write_buffer使其不会再async_write完成前被销毁
                        boost::asio::async_write(*socket, *write_buffer,
                            [this, socket, request, write_buffer](const boost::system::error_code& ec, size_t bytes_transferred){
                                                //HTTP持久连接（HTTP1.1）
                                                if(!ec && stof(request->http_version) > 1.05)
                                                    process_request_and_respond(socket);
                                                });
                                                return;
                    }
                }
            }
        }

    };

    template<typename socket_type> class Server : public ServerBase<socket_type> {};
}

#endif // SERVER_BASE_H_INCLUDED
