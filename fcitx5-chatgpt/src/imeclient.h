#ifndef _FCITX5_CHATGPT_IMECLIENT_H
#define _FCITX5_CHATGPT_IMECLIENT_H

#include <string>
#include <utility>
#include <vector>

struct SendMessageResponse {
    bool is_success;
    std::vector<std::string> candidates;
    std::string error_message;
};

class IMEClient {
public:
    explicit IMEClient(std::string socket_file_path): socket_file_path_(std::move(socket_file_path)) {}

    SendMessageResponse send_message(const std::string& message);

private:
    std::string socket_file_path_;
};

#endif // _FCITX5_CHATGPT_IMECLIENT_H
