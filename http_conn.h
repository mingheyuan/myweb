#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
};

class HttpConn {
public:
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    enum HTTP_CODE {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        FILE_REQUEST,
        NOT_FOUND
    };

    std::string process(const std::string &raw_request);

private:
    HTTP_CODE parse_request(const std::string &raw_request, HttpRequest &request);
    HTTP_CODE parse_request_line(const std::string &line, HttpRequest &request);
    HTTP_CODE parse_header_line(const std::string &line, HttpRequest &request);

    std::string route_and_build(const HttpRequest &request);
    std::string build_response(int status_code, const std::string &body) const;
};

#endif
