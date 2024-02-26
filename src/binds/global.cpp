#include "bindings.h"

void reg_global(lua_State *L) {
    luabridge::getGlobalNamespace(L).addFunction(
        "clear", *[]() { system("clear"); });

    luabridge::getGlobalNamespace(L).addFunction(
        "exit", *[]() { exit(0); });

    luabridge::getGlobalNamespace(L).addFunction(
        "ls", *[]() { system("ls"); });
}