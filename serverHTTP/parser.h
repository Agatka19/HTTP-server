#ifndef PARSER_H
#define PARSER_H
#include <cstring>
#include <bits/stdc++.h>
#define CRLF "\r\n"
#define BUFFER_SIZE   5000

using namespace std;

typedef enum Method {UNSUPPORTED, GET, HEAD} Method;

typedef struct Request_line
{
    enum Method method;
    string request_target; //zasób (plik)
    string HTTP_version;
} Request_line;

typedef struct Request
{
    struct Request_line *request_line;
    string message_body;
} Request;

struct Request_line *parse_Request_line(char *request_text);
void parse_Header_field(char *request_text, int msg_sock, string &code, unordered_map<string, string> &headers);
#endif //PARSER_H
