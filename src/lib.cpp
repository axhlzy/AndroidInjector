#include "test.h"
#include <arpa/inet.h>
#include <iostream>
#include <jni.h>
#include <main.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "magic_enum_all.hpp"

static JavaVM *g_jvm;
static JNIEnv *env;
static std::thread *g_thread = NULL;

// #define LUA_OK		0
// #define LUA_YIELD 1
// #define LUA_ERRRUN 2
// #define LUA_ERRSYNTAX 3
// #define LUA_ERRMEM 4
// #define LUA_ERRERR 5
enum LUA_STATUS {
    LUA_OK_ = 0,
    LUA_YIELD_ = 1,
    LUA_ERRRUN_ = 2,
    LUA_ERRSYNTAX_ = 3,
    LUA_ERRMEM_ = 4,
    LUA_ERRERR_ = 5
};

static int serverSocket, clientSocket;

int l_print(lua_State *L) {
    int n = lua_gettop(L);
    lua_getglobal(L, "tostring");
    for (int i = 1; i <= n; i++) {
        const char *s;
        size_t len;
        lua_pushvalue(L, -1);
        lua_pushvalue(L, i);
        lua_call(L, 1, 1);
        s = lua_tolstring(L, -1, &len);
        if (s == nullptr) {
            return luaL_error(L, "'tostring' must return a string to 'print'");
        }
        if (i > 1) {
            write(clientSocket, "\t", 1);
        }
        write(clientSocket, s, len);
        lua_pop(L, 1);
    }
    write(clientSocket, "\n", 1);
    return 0;
}

void repl_socket(lua_State *L) {

    int opt = 1;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(8024)};

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("create socket fail");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(serverSocket, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0) {
        perror("serverSocket bind fail");
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 3) < 0) {
        perror("serverSocket listen fail");
        exit(EXIT_FAILURE);
    }

    if ((clientSocket = accept(serverSocket, reinterpret_cast<struct sockaddr *>(&address), &addrlen)) < 0) {
        perror("clientSocket accept fail");
        exit(EXIT_FAILURE);
    } else {
        std::cout << "Client connected." << std::endl;

        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        if (getpeername(clientSocket, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrLen) < 0) {
            perror("getpeername");
        } else {
            std::cout << "Client IP: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
            std::cout << "Client sin_family: " << clientAddr.sin_family << std::endl;
            std::cout << "Client port: " << ntohs(clientAddr.sin_port) << std::endl;
        }
    }

    lua_pushcfunction(L, l_print);
    lua_setglobal(L, "print");

    char buffer[1024] = {0};
    int valread;
    std::string input;
    dup2(clientSocket, STDOUT_FILENO);
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        fflush(stdout);
        valread = read(clientSocket, buffer, sizeof(buffer));
        if (valread == 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        } else if (valread == -1) {
            perror("read");
            break;
        } else {
            input = buffer;
            if (input == "exit" || input == "q") {
                write(clientSocket, "Client requested exit. \n", 24);
                close(clientSocket);
                close(serverSocket);
                break;
            }
            int status = luaL_dostring(L, input.c_str());
            if (status != LUA_OK) {
                auto msg = lua_tostring(L, -1);
                lua_writestringerror("%s\n", msg);
                write(clientSocket, msg, strlen(msg));
                lua_pop(L, 1);
            } else {
                auto status_enum = reinterpret_cast<LUA_STATUS &>(status);
                auto status_name = magic_enum::enum_name<LUA_STATUS>(status_enum);
                std::string status = std::string(status_name.substr(0, status_name.size() - 1)) + "\n";
                write(clientSocket, status.c_str(), status_name.size());
            }
        }
    }
}

static void repl(lua_State *L) {
    std::string input;
    while (true) {
        printf("exec > ");
        std::getline(std::cin, input);
        if (input == "exit" || input == "q")
            break;
        int status = luaL_dostring(L, input.c_str());
        auto status_enum = reinterpret_cast<LUA_STATUS &>(status);
        auto status_name = magic_enum::enum_name<LUA_STATUS>(status_enum);
        if (status != LUA_OK) {
            const char *msg = lua_tostring(L, -1);
            lua_writestringerror("%s\n", msg);
            lua_pop(L, 1);
        }
    }
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
    if (vm == nullptr)
        return JNI_VERSION_1_6;
    logd("------------------- JNI_OnLoad -------------------");
    if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_OK) {
        logd("[*] GetEnv OK | env:%p | vm:%p", env, vm);
    }
    if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
        logd("[*] AttachCurrentThread OK");
    }
    g_jvm = vm;

    g_thread = new std::thread([]() {
        startLuaVM();
    });
    g_thread->detach();

    return JNI_VERSION_1_6;
}

void startLuaVM() {

    logd("[*] Lua VM started"); // android_logcat

    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    bind_libs(L);

    // test(L);

    // 将标准输出重定向到当前线程的socket

    // repl(L);

    repl_socket(L);

    // lua_close(L);
}