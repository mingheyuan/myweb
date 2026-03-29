#include "http_conn.h"
#include "logger.h"
#include "sql_connection_pool.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

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

std::unordered_map<std::string, std::string> parse_form_urlencoded(const std::string &body)
{
    std::unordered_map<std::string, std::string> kv;
    std::size_t start = 0;
    while (start < body.size()) {
        std::size_t amp = body.find('&', start);
        if (amp == std::string::npos) {
            amp = body.size();
        }

        std::string pair = body.substr(start, amp - start);
        std::size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = pair.substr(0, eq);
            std::string value = pair.substr(eq + 1);
            kv[key] = value;
        }

        start = amp + 1;
    }
    return kv;
}

std::string escape_mysql_string(MYSQL *conn, const std::string &input)
{
    if (!conn) {
        return std::string();
    }
    std::string out;
    out.resize(input.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn, &out[0], input.c_str(), input.size());
    out.resize(len);
    return out;
}
} // namespace

HttpConn::HttpConn() : m_state(0), m_sockfd(-1)
{
}

void HttpConn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    m_state = 0;
    m_read_buffer.clear();
    m_write_buffer.clear();
}

void HttpConn::close_conn()
{
    if (m_sockfd != -1) {
        close(m_sockfd);
        m_sockfd = -1;
    }
    m_read_buffer.clear();
    m_write_buffer.clear();
}

bool HttpConn::read_once()
{
    if (m_sockfd == -1) {
        return false;
    }

    char buf[4096];
    while (true) {
        ssize_t nread = recv(m_sockfd, buf, sizeof(buf), 0);
        if (nread > 0) {
            m_read_buffer.append(buf, static_cast<std::size_t>(nread));
            continue;
        }

        if (nread == 0) {
            return false;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

HttpConn::HTTP_CODE HttpConn::process()
{
    HttpRequest request;
    HTTP_CODE code = parse_request(m_read_buffer, request);

    if (code == NO_REQUEST) {
        return NO_REQUEST;
    }

    if (code == BAD_REQUEST) {
        Logger::instance().warn("http parse bad request");
        m_write_buffer = build_response(400, "Bad Request\n");
        m_state = 1;
        return BAD_REQUEST;
    }

    if (code == GET_REQUEST || code == FILE_REQUEST) {
        m_write_buffer = route_and_build(request);
        m_state = 1;
        return code;
    }

    m_write_buffer = build_response(400, "Bad Request\n");
    m_state = 1;
    return BAD_REQUEST;
}

bool HttpConn::write()
{
    if (m_sockfd == -1 || m_write_buffer.empty()) {
        return false;
    }

    const char *data = m_write_buffer.c_str();
    std::size_t left = m_write_buffer.size();
    while (left > 0) {
        ssize_t nsend = send(m_sockfd, data, left, MSG_NOSIGNAL);
        if (nsend > 0) {
            data += nsend;
            left -= static_cast<std::size_t>(nsend);
            continue;
        }
        if (nsend == -1 && errno == EINTR) {
            continue;
        }
        Logger::instance().warn(std::string("send failed: ") + std::strerror(errno));
        return false;
    }

    m_write_buffer.clear();
    m_read_buffer.clear();
    m_state = 0;
    return true;
}

int HttpConn::fd() const
{
    return m_sockfd;
}

bool HttpConn::has_response() const
{
    return !m_write_buffer.empty();
}

HttpConn::HTTP_CODE HttpConn::parse_request(const std::string &raw_request, HttpRequest &request)
{
    CHECK_STATE state = CHECK_STATE_REQUESTLINE;
    std::size_t checked_idx = 0;
    const std::size_t read_idx = raw_request.size();

    while (true) {
        std::size_t line_end = 0;
        LINE_STATUS line_status = parse_line(raw_request, checked_idx, read_idx, line_end);

        if (line_status == LINE_OPEN) {
            return NO_REQUEST;
        }
        if (line_status == LINE_BAD) {
            return BAD_REQUEST;
        }

        std::string line = raw_request.substr(checked_idx, line_end - checked_idx);
        checked_idx = line_end + 2;

        if (state == CHECK_STATE_REQUESTLINE)
        {
            HTTP_CODE rc = parse_request_line(line, request);
            if (rc != NO_REQUEST) {
                return rc;
            }
            state = CHECK_STATE_HEADER;
            continue;
        }

        if (state == CHECK_STATE_HEADER)
        {
            if (line.empty())
            {
                if (request.content_length == 0) {
                    return GET_REQUEST;
                }
                state = CHECK_STATE_CONTENT;
                return parse_content(raw_request, checked_idx, request);
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

    if (request.method != "GET" && request.method != "POST") {
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

    if (key == "content-length") {
        std::istringstream iss(value);
        iss >> request.content_length;
        if (iss.fail()) {
            return BAD_REQUEST;
        }
    }

    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::parse_content(const std::string &raw_request, std::size_t body_start, HttpRequest &request)
{
    if (request.content_length == 0) {
        request.body.clear();
        return GET_REQUEST;
    }

    if (raw_request.size() < body_start + request.content_length) {
        return NO_REQUEST;
    }

    request.body = raw_request.substr(body_start, request.content_length);
    return GET_REQUEST;
}

HttpConn::LINE_STATUS HttpConn::parse_line(const std::string &raw_request, std::size_t &checked_idx, std::size_t read_idx, std::size_t &line_end) const
{
    for (std::size_t i = checked_idx; i < read_idx; ++i) {
        if (raw_request[i] == '\r') {
            if (i + 1 >= read_idx) {
                return LINE_OPEN;
            }
            if (raw_request[i + 1] == '\n') {
                line_end = i;
                return LINE_OK;
            }
            return LINE_BAD;
        }
        if (raw_request[i] == '\n') {
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

std::string HttpConn::route_and_build(const HttpRequest &request)
{
    if (request.method == "POST" && (request.path == "/register" || request.path == "/login")) {
        if (!SqlConnectionPool::instance().is_ready()) {
            return build_response(500, "Database not ready\n");
        }

        std::unordered_map<std::string, std::string> form = parse_form_urlencoded(request.body);
        std::string username = form["username"];
        std::string password = form["password"];
        if (username.empty() || password.empty()) {
            return build_response(400, "username/password required\n");
        }

        MYSQL *mysql = nullptr;
        SqlConnRAII mysql_raii(&mysql);
        if (!mysql) {
            return build_response(500, "Database connection failed\n");
        }

        std::string esc_user = escape_mysql_string(mysql, username);
        std::string esc_pwd = escape_mysql_string(mysql, password);

        if (request.path == "/register") {
            std::string check_sql = "SELECT username FROM user WHERE username='" + esc_user + "' LIMIT 1";
            if (mysql_query(mysql, check_sql.c_str()) != 0) {
                Logger::instance().error(std::string("mysql query failed(register-check): ") + mysql_error(mysql));
                return build_response(500, "Database query error\n");
            }

            MYSQL_RES *res = mysql_store_result(mysql);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                mysql_free_result(res);
                if (row) {
                    return build_response(200, "register failed: user exists\n");
                }
            }

            std::string insert_sql = "INSERT INTO user(username, passwd) VALUES('" + esc_user + "','" + esc_pwd + "')";
            if (mysql_query(mysql, insert_sql.c_str()) != 0) {
                Logger::instance().error(std::string("mysql query failed(register-insert): ") + mysql_error(mysql));
                return build_response(500, "register failed\n");
            }
            return build_response(200, "register success\n");
        }

        std::string login_sql = "SELECT passwd FROM user WHERE username='" + esc_user + "' LIMIT 1";
        if (mysql_query(mysql, login_sql.c_str()) != 0) {
            Logger::instance().error(std::string("mysql query failed(login): ") + mysql_error(mysql));
            return build_response(500, "Database query error\n");
        }

        MYSQL_RES *res = mysql_store_result(mysql);
        if (!res) {
            return build_response(500, "Database result error\n");
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        if (!row) {
            mysql_free_result(res);
            return build_response(200, "login failed: user not found\n");
        }

        std::string stored = row[0] ? row[0] : "";
        mysql_free_result(res);
        if (stored == password) {
            return build_response(200, "login success\n");
        }
        return build_response(200, "login failed: wrong password\n");
    }

    if (request.method == "POST" && request.path == "/echo") {
        return build_response(200, std::string("echo: ") + request.body + "\n");
    }

    if (request.path == "/") {
        return build_response(200, "TinyWebServer stage4: state machine parser\n");
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
    } else if (status_code == 500) {
        oss << "HTTP/1.1 500 Internal Server Error\r\n";
    } else {
        oss << "HTTP/1.1 404 Not Found\r\n";
    }

    oss << "Content-Type: text/plain; charset=utf-8\r\n";
    oss << "Connection: close\r\n";
    oss << "Content-Length: " << body.size() << "\r\n\r\n";
    oss << body;
    return oss.str();
}
