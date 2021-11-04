#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <fstream>

#include "err.h"
#include "parser.h"

using namespace std;

#define QUEUE_LENGTH     5
#define HTTP_VERSION "HTTP/1.1"
#define SP " "

int getdir(const string& dir, vector<string> &files) {
    DIR *dp;
    struct dirent *dirp;

    if ((dp = opendir(dir.c_str())) == nullptr) {
        cout << EXIT_FAILURE << endl;
        return errno;
    }
    while ((dirp = readdir(dp)) != nullptr)
        files.emplace_back(dirp->d_name);
    closedir(dp);
    return 0;
}


vector<string> split_string(const string &str, char delim) {
    auto result = vector<string>{};
    auto ss = stringstream{str};

    for (string line; getline(ss, line, delim);)
        result.push_back(line);

    return result;
}


string getfile(string File, string dir, const string& correlated, string &code) {
    vector<string> files = vector<string>();
    bool found = false;
    File.erase(0, 1);
    getdir(dir, files);
    string line;
    for (auto &file : files) {
        if (file == File) {
            found = true;
            file = dir.append("/").append(File);
            ifstream f(file);
            f.open(file, ifstream::in);
            stringstream s;

            if (f.is_open()) {
                s << f.rdbuf();
                line = s.str();
            } else {
                //code = "501";
                found = false;
            }
            f.close();
        }
    }
    if (!found) {
        const string &file = correlated;
        ifstream f(file);
        f.open(file, ifstream::in);
        stringstream s;
        if (f.is_open()) {
            s << f.rdbuf();
            line = s.str();
            vector<string> split = split_string(line, '\n');
            for (const auto &sp : split) {
                istringstream iss(sp);
                std::vector<std::string> names{
                        std::istream_iterator<std::string>(iss), {}
                };
                for (const auto &name : names) {
                    string r;
                    r.append("/").append(File);
                    if (name == r) {
                        code = "302";
                        line = sp;
                        line.append(CRLF);
                        return line;
                    }
                    code = "404";
                }
            }
        }
        f.close();
    }
    return line;
}

string Reason_phrase(const string &code) {
    if (code == "200")
        return "OK";
    else if (code == "302")
        return "Found";
    else if (code == "400")
        return "Bad Request";
    else if (code == "404")
        return "Not Found";
    else if (code == "500")
        return "Internal Server Error";
    else if (code == "501")
        return "Not Implemented";
    else
        return "";
}


void response(const string &status_code, enum Method method, const string &content,
              const unordered_map<string, string> &header_fields, string &result) {
    bool content_length = false;
    bool connection = false;
    string reason_phrase = Reason_phrase(status_code);
    result.append(SP);
    result.append(status_code);
    result.append(SP);
    result.append(reason_phrase);
    result.append(CRLF);
    for (const auto &h : header_fields) {
        result.append(h.first);
        if (h.first == "Content-length") content_length = true;
        if (h.first == "Connection") connection = true;
        result.append(": ");
        result.append(h.second);
        result.append(CRLF);
    }
    if (!content_length) {
        result.append("Content-length: ");
        result.append(to_string(content.length()));
        result.append(CRLF);
    }
    if (!connection) {
        if (status_code == "200")
            result.append("Connection: alive");
        else
            result.append("Connection: lost");
        result.append(CRLF);
    }
    result.append("Content-type: application/octet-stream").append(CRLF);
    if (method == GET) {
        result.append(content);
    }
    result.append(CRLF);
}

int main(int argc, char *argv[]) {
    int sock, msg_sock = 0;
    struct sockaddr_in server_address{};
    struct sockaddr_in client_address{};
    socklen_t client_address_len;

    size_t port_num = 8080;
    char buffer[BUFFER_SIZE];
    ssize_t len, snd_len;

    if(argc < 3 || argc > 4) {
        write(msg_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
        syserr("close");
    }
    if(argc == 4){;
        char *pEnd;
        port_num = strtol(argv[3], &pEnd, 10);
    }
    if(!argv[1][1]) {
        write(msg_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
        syserr("close");
    }

    sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
    if (sock < 0)
        syserr("socket");
    // after socket() call; we should close(sock) on any execution path;
    // since all execution paths exit immediately, sock would be closed when program terminates

    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port_num); // listening on port PORT_NUM

    // bind the socket to a concrete address
    if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
        syserr("bind");

    // switch to listening (passive open)
    if (listen(sock, QUEUE_LENGTH) < 0)
        syserr("listen");

    printf("accepting client connections on port %hu\n", ntohs(server_address.sin_port));

    string code = "200";
    while (code == "200") {
        client_address_len = sizeof(client_address);
        // get client connection from the socket
        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
        if (msg_sock < 0)
            syserr("accept");
        do {
            string response_text = HTTP_VERSION;
            len = read(msg_sock, &buffer, sizeof(buffer));
            if (len < 0) {
                syserr("reading from client socket");
            } else {
                auto *req = new Request;
                memset(req, 0, sizeof(struct Request));
                size_t length = strlen(buffer + 2);
                struct Request_line req_line = *parse_Request_line(buffer);
                req->request_line = &req_line;
                unordered_map<string, string> header_fields;
                if (req_line.method == UNSUPPORTED) {
                    write(msg_sock, "HTTP/1.1 501 Not Implemented\r\n\r\n", 32);
                    syserr("close");
                }
                if (req_line.request_target[0] != '/' || req_line.HTTP_version != HTTP_VERSION) {
                    write(msg_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
                    syserr("close");
                }

                string buff_file = getfile(req_line.request_target, argv[1], argv[2], code);

                if (code == "404") {
                    write(msg_sock, "HTTP/1.1 404 Not Found\r\n\r\n", 26);
                    syserr("close");
                } else if (code == "302") {
                    if (buff_file.length() >= BUFFER_SIZE) {
                        code = "500";
                    }
                    for (int i = 0; i < buff_file.length(); ++i) {
                        buffer[i] = buff_file[i];
                    }
                    buffer[buff_file.length()] = '\r';
                    buffer[buff_file.length()+1] = '\n';
                    write(msg_sock, "HTTP/1.1 302 Found\r\nLocation: ", 29);
                    snd_len = write(msg_sock, buffer, buff_file.length()+2);
                    syserr("close");
                }

                memset(buffer, 0, sizeof(buffer));
                len = read(msg_sock, &buffer, sizeof(buffer));
                while ((buffer[0] != '\r' && buffer[1] != '\n')) {
                    parse_Header_field(buffer, msg_sock, code, header_fields);
                    memset(buffer, 0, sizeof(buffer));
                    len = read(msg_sock, &buffer, sizeof(buffer));
                }
            
                memset(buffer, 0, sizeof(buffer));
                len = read(msg_sock, &buffer, sizeof(buffer));
                req->message_body.assign(buffer, len);
                req->message_body[len] = '\0';
                if (req->message_body != "/0" && req->message_body != CRLF) {
                    write(msg_sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
                    syserr("close");
                }

                response(code, req->request_line->method, buff_file, header_fields, response_text);
                memset(buffer, 0, sizeof(buffer));
                if (response_text.length() >= BUFFER_SIZE) {
                    code = "500";
                }
                for (int i = 0; i < response_text.length(); ++i) {
                    buffer[i] = response_text[i];
                }
                snd_len = write(msg_sock, buffer, response_text.length());
                if (code != "200")
                    syserr("close");
            }
        } while (len > 0);
        if (close(msg_sock) < 0)
            syserr("close");
    }
    return 0;
}
