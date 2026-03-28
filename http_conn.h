#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <netinet/in.h>
#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::size_t content_length = 0;
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

    enum LINE_STATUS {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

    HttpConn();
    void init(int sockfd, const sockaddr_in &addr);
    void close_conn();

    bool read_once();
    HTTP_CODE process();
    bool write();

    int fd() const;
    bool has_response() const;

public:
    int m_state; // 0: read, 1: write (for future threadpool integration)

private:
    HTTP_CODE parse_request(const std::string &raw_request, HttpRequest &request);
    HTTP_CODE parse_request_line(const std::string &line, HttpRequest &request);
    HTTP_CODE parse_header_line(const std::string &line, HttpRequest &request);
    HTTP_CODE parse_content(const std::string &raw_request, std::size_t body_start, HttpRequest &request);
    LINE_STATUS parse_line(const std::string &raw_request, std::size_t &checked_idx, std::size_t read_idx, std::size_t &line_end) const;

    std::string route_and_build(const HttpRequest &request);
    std::string build_response(int status_code, const std::string &body) const;

private:
    int m_sockfd;
    sockaddr_in m_address;
    std::string m_read_buffer;
    std::string m_write_buffer;
};

#endif
