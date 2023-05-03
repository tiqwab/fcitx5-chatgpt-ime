#include "imeclient.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <fcitx-utils/log.h>

struct HttpResponse {
    std::string http_version;
    int status_code;
    std::string reason_phrase;
    std::map<std::string, std::string> headers;
    std::string body;
};

std::vector<std::string> convert_lines(char *bs) {
    std::vector<std::string> res;
    char *p = bs;
    while (true) {
        char *p_nl = strstr(p, "\r\n");

        std::string s;
        while (p != p_nl && *p != '\0') {
            s.push_back(*p);
            p++;
        }
        res.push_back(s);

        if (*p == '\0') {
            break;
        }
        p += 2;
    }
    return res;
}

std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    size_t end = str.find_last_not_of(" \t\n\r\f\v");

    if (start == std::string::npos) {
        return "";
    }

    return str.substr(start, end - start + 1);
}

void parse_status_line(const std::string& status_line, HttpResponse &http_response) {
    size_t pos;

    // HTTP version
    size_t cur_pos = 0;
    pos = status_line.find(' ', cur_pos);
    http_response.http_version = status_line.substr(cur_pos, pos - cur_pos);
    cur_pos = pos + 1;

    // status code
    pos = status_line.find(' ', cur_pos);
    http_response.status_code = std::stoi(status_line.substr(cur_pos, pos - cur_pos));
    cur_pos = pos + 1;

    // reason phrase
    http_response.reason_phrase = status_line.substr(cur_pos);
}

void parse_header(const std::string& header_line, HttpResponse &http_response) {
    size_t separator_pos = header_line.find(':');
    std::string key = header_line.substr(0, separator_pos);
    std::string value = trim(header_line.substr(separator_pos + 1));
    http_response.headers[key] = value;
}

HttpResponse parse_http_response(char *raw_response) {
    HttpResponse http_response;

    std::vector<std::string> lines = convert_lines(raw_response);

    parse_status_line(lines[0], http_response);

    // FCITX_INFO() << http_response.http_version;
    // FCITX_INFO() << http_response.status_code;
    // FCITX_INFO() << http_response.reason_phrase;

    size_t line_i;
    for (line_i = 1; line_i < lines.size(); line_i++) {
        auto line = lines[line_i];
        if (line.empty()) {
            break;
        }
        parse_header(line, http_response);
    }
    line_i++;

    // for (const auto& header : http_response.headers) {
    //     FCITX_INFO() << header.first + ": " + header.second;
    // }

    for (; line_i < lines.size(); line_i++) {
        http_response.body.append(lines[line_i]);
    }

    return http_response;
}

SendMessageResponse IMEClient::send_message(const std::string& message) {
    int sock;
    struct sockaddr_un server_addr;
    char http_request_template[] = "POST /chat HTTP/1.1\r\nHost: chatgpt-ime\r\nContent-Length: %d\r\nContent-Type: application/json\r\n\r\n{\"message\": \"%s\"}";
    char buffer[4096];

    SendMessageResponse response;

    auto handle_error = [&](const std::string& message) {
        char *error_message = strerror(errno);
        response.is_success = false;
        response.error_message = message + ": " + std::string(error_message);
        close(sock);
    };

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        handle_error("socket");
        return response;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_file_path_.c_str(), sizeof(server_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        handle_error("connect");
        return response;
    }

    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, http_request_template, 15 + message.size(), message.c_str());

    if (send(sock, buffer, strlen(buffer), 0) == -1) {
        handle_error("send");
        return response;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(sock, buffer, sizeof(buffer) - 1, 0) == -1) {
        handle_error("recv");
        return response;
    }


    auto http_response = parse_http_response(buffer);
    if (http_response.status_code != 200) {
        auto json = nlohmann::json::parse(http_response.body);
        response.is_success = false;
        response.error_message = json["error_message"];
        close(sock);
        return response;
    }

    auto json = nlohmann::json::parse(http_response.body);
    response.candidates = json["candidates"];

    response.is_success = true;
    close(sock);
    return response;
}