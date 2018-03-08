/* 用于测试 */
#ifndef HANDLER_H_INCLUDED
#define HANDLER_H_INCLUDED

#include "server_base.h"
#include<fstream>
using namespace std;
using namespace Wenmingxing;

template<typename SERVER_TYPE>
void start_server(SERVER_TYPE &server) {
    //向服务器增加请求资源的处理方法

    //处理访问/string的POST请求，返回POST的字符串
    server.resource["^/string/?$"]["POST"] = [](ostream& response, Request& request) {
        //从istream中获取字符串(*request.content)
        stringstream ss;
        *request.content >> ss.rdbuf(); //将请求内容读到stringstream中
        string content = ss.str();

        //直接返回请求结果
        response << "HTTP/1.1 200 OK \r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
    };

    //处理访问/info的GET请求，返回请求的信息
    server.resource["/info/?$"]["GET"] = [](ostream& response, Request& request) {
        stringstream content_stream;
        content_stream << "<h1>Request:</h1>";
        content_stream << request.method << " " << request.path << " HTTP/" << request.http_version << "<br>";
        for (auto& header:request.header) {
            content_stream << header.first << " : " << header.second << "<br>";
        }

        //获得content_stream的长度(使用content.tellp())
        content_stream.seekp(0, ios::end);

        response << "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
    };

    //处理访问/match/[字母+数字组成的字符串]的GET请求，例如执行GET /match/abc123,将返回abc123
    server.resource["^/match/([0-9a-zA-Z]+)/?$"]["GET"] = [](ostream& response, Request& request) {
        string number = request.path_match[1];
        response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n" << number;
    };

    //处理默认GET请求，如果没有其他匹配成功，则这个匿名函数会被调用
    //将应答web/目录及其子目录中的文件
    //默认文件:index.html
    server.default_resource["^/?(.*)&"]["GET"] = [](ostream& response, Request& request) {
        string filename = "www/";
        string path = request.path_match[1];

        //防止使用'..'来访问web/目录外的内容
        size_t last_pos = path.rfind(".");
        size_t current_pos = 0;
        size_t pos;
        while ((pos = path.find('.', current_pos)) != string::npos && pos != last_pos) {
            current_pos = pos;
            path.erase(pos, 1);
            last_pos--;
        }

        filename += path;
        ifstream ifs;
        //简单的平台无关的文件或目录检查
        if (filename.find('.') == string::npos) {
            if (filename[filename.length()-1] != '/')
                filename += '/';
            filename += "index.html";
        }
        ifs.open(filename, ifstream::in);
        if (ifs) {
            ifs.seekg(0, ios::end);
            size_t length = ifs.tellg();

            ifs.seekg(0, ios::beg);

            //文件内容拷贝到esponse-stream中，不应该用于大型文件
            response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n" << ifs.rdbuf();

            ifs.close();
        }
        else {
            //文件不存在，返回无法打开文件
            string content = "Could not open file" + filename;
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
        }
    };

    //运行HTTP服务器
    server.start();
}

#endif // HANDLER_H_INCLUDED
