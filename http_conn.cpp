#include "http_conn.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
std::string trim_copy(const std::string &s)
{
    std::size_t begin = 0;
    while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) {
        ++begin;
    }

    std::size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }

    return s.substr(begin, end - begin);
}

std::string to_lower_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}
} // namespace

std::string HttpConn::process(const std::string &raw_request)
{
    HttpRequest request;
    HTTP_CODE code = parse_request(raw_request, request);
    if (code == BAD_REQUEST) {
        return build_response(400, "Bad Request\n");
    }
    if (code != GET_REQUEST && code != FILE_REQUEST) {
        return build_response(400, "Incomplete Request\n");
    }
    return route_and_build(request);
}

HttpConn::HTTP_CODE HttpConn::parse_request(const std::string &raw_request, HttpRequest &request)
{
    CHECK_STATE state = CHECK_STATE_REQUESTLINE;
    std::size_t cursor = 0;

    while (true) {
        std::size_t line_end = raw_request.find("\r\n", cursor);
        if (line_end == std::string::npos) {
            return BAD_REQUEST;
        }

        std::string line = raw_request.substr(cursor, line_end - cursor);
        cursor = line_end + 2;

        if (state == CHECK_STATE_REQUESTLINE) {
            HTTP_CODE rc = parse_request_line(line, request);
            if (rc != NO_REQUEST) {
                return rc;
            }
            state = CHECK_STATE_HEADER;
            continue;
        }

        if (state == CHECK_STATE_HEADER) {
            if (line.empty()) {
                return GET_REQUEST;
            }

            HTTP_CODE rc = parse_header_line(line, request);
            if (rc != NO_REQUEST) {
                return rc;
            }
        }
    }
}

HttpConn::HTTP_CODE HttpConn::parse_request_line(const std::string &line, HttpRequest &request)
{
    std::istringstream iss(line);
    iss >> request.method >> request.path >> request.version;

    if (request.method.empty() || request.path.empty() || request.version.empty()) {
        return BAD_REQUEST;
    }

    if (request.version != "HTTP/1.1" && request.version != "HTTP/1.0") {
        return BAD_REQUEST;
    }

    if (request.method != "GET") {
        return BAD_REQUEST;
    }

    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::parse_header_line(const std::string &line, HttpRequest &request)
{
    std::size_t pos = line.find(':');
    if (pos == std::string::npos) {
        return BAD_REQUEST;
    }

    std::string key = to_lower_copy(trim_copy(line.substr(0, pos)));
    std::string value = trim_copy(line.substr(pos + 1));
    request.headers[key] = value;
    return NO_REQUEST;
}

std::string HttpConn::route_and_build(const HttpRequest &request)
{
    if (request.path == "/") {
        return build_response(200, "TinyWebServer stage3: http_conn parser\n");
    }
    if (request.path == "/hello") {
        return build_response(200, "hello from tiny webserver\n");
    }
    if (request.path == "/about") {
        return build_response(200, "This is a learning tiny webserver project.\n");
    }
    return build_response(404, "Not Found\n");
}

std::string HttpConn::build_response(int status_code, const std::string &body) const
{
    std::ostringstream oss;
    if (status_code == 200) {
        oss << "HTTP/1.1 200 OK\r\n";
    } else if (status_code == 400) {
        oss << "HTTP/1.1 400 Bad Request\r\n";
    } else {
        oss << "HTTP/1.1 404 Not Found\r\n";
    }

    oss << "Content-Type: text/plain; charset=utf-8\r\n";
    oss << "Connection: close\r\n";
    oss << "Content-Length: " << body.size() << "\r\n\r\n";
    oss << body;
    return oss.str();
}
