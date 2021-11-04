#include "parser.h"
#include <unistd.h>
#include <algorithm>
#include <unordered_map>

using namespace std;

struct Request_line *parse_Request_line(char *request_text) {
    auto *rl = new Request_line;

    // Method
    size_t meth_len = strcspn(request_text, " ");
    if (memcmp(request_text, "GET", strlen("GET")) == 0) {
        rl->method = GET;
    } else if (memcmp(request_text, "HEAD", strlen("HEAD")) == 0) {
        rl->method = HEAD;
    } else {
        rl->method = UNSUPPORTED;
    }
    request_text += meth_len + 1; // move past <SP>

    //Request_target
    size_t target_len = strcspn(request_text, " ");
    rl->request_target.assign(request_text, target_len);
    rl->request_target[target_len] = '\0';
    request_text += target_len + 1; //move past <SP>

    //HTTP_version
    size_t ver_len = strcspn(request_text, CRLF);
    rl->HTTP_version.assign(request_text, ver_len);
    rl->HTTP_version[ver_len] = '\0';
    request_text += ver_len + 2; // move past <CR><LF>
    return rl;
}


void parse_Header_field(char *request_text, int msg_sock, string &code, unordered_map<string, string> &headers) {
    set<string> names = {"Content-length", "Connection", "Server", "Content-type"};
    string header_field = (string) request_text;
    size_t name_len = strcspn(request_text, ":");
    size_t start = 0;
    if (name_len == string::npos) {
        code = "400";
        return;
    }

    string field_name = header_field.substr(start, name_len - start);
    start = name_len + 1; //move past :
    while (header_field[start] == ' ') {
        ++start;
    }

    size_t value_len = strcspn(request_text, "\r\n");
    if (!value_len) {
        code = "400";
        return;
    }
    string field_value = header_field.substr(start, value_len - start);
    while (header_field[value_len] == ' ') {
        ++value_len;
    }

    if (header_field.substr(value_len) != CRLF) {
        code = "400";
        return;
    }

    if (field_name == "Connection" && field_value == "close") {
        code = "400";
        return;
    }

    if (field_name == "Content-length" && field_value != "0") {
        code = "400";
        return;
    }
    if(!(names.find(field_name) == names.end())){
        headers[field_name] = field_value;
    }
}
