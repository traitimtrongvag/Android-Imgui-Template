#pragma once

#include "AnAn/Call_Me.h"
#include "Login.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <chrono>
#include <ctime>

using json = nlohmann::json;

inline std::string g_publicIP;
inline bool g_ipFetched = false;

inline std::string lastVnTime;
inline bool alreadySyncedAt7h = false;
inline std::string err;
inline std::tm vnTime = {};
inline time_t vnBaseTime = 0;
inline std::chrono::system_clock::time_point vnStartTime;

struct VnDate {
    int day;
    int month;
    int year;
};
inline VnDate g_vnDate{0, 0, 0};
inline bool g_dateInit = false;

inline std::string getPublicIP() {
    struct API {
        std::string host;
        std::string path;
        std::string type;
    };
    std::vector<API> apis = {
        {std::string(OBFUSCATE("ip-api.com")), std::string(oxorany("/json")), std::string(oxorany("ip-api"))},
        {std::string(OBFUSCATE("icanhazip.com")), std::string(oxorany("/")), std::string(oxorany("plain"))}
    };
    for (auto& api : apis) {
        httplib::Client cli(api.host.c_str());
        auto res = cli.Get(api.path.c_str());
        if (!res || res->status != 200) continue;
        try {
            if (api.type == std::string(oxorany("ip-api"))) {
                auto j = nlohmann::json::parse(res->body);
                return j.value((const char*)oxorany("query"), std::string(OBFUSCATE("N/A")));
            } else {
                return res->body;
            }
        } catch (...) { continue; }
    }
    return std::string(OBFUSCATE("N/A"));
}

inline void initPublicIP() {
    if (!g_ipFetched) {
        g_publicIP = getPublicIP();
        g_ipFetched = true;
    }
}

inline std::string getVnTime() {
    httplib::Client cli((const char*)OBFUSCATE("http://worldtimeapi.org"));
    auto res = cli.Get((const char*)OBFUSCATE("/api/timezone/Asia/Ho_Chi_Minh"));
    if (!res || res->status != 200) return std::string(OBFUSCATE("N/A"));
    json j = json::parse(res->body);
    time_t ts = j.value((const char*)oxorany("unixtime"), 0);
    if (ts == 0) return std::string(OBFUSCATE("N/A"));
    std::tm tmVN{};
    localtime_r(&ts, &tmVN);
    char buf[9];
    strftime(buf, sizeof(buf), (const char*)OBFUSCATE("%H:%M:%S"), &tmVN);
    return std::string(buf);
}

inline void initVnTime() {
    time_t ts = 0;
    while (true) {
        httplib::Client cli((const char*)OBFUSCATE("http://worldtimeapi.org"));
        auto res = cli.Get((const char*)OBFUSCATE("/api/timezone/Asia/Ho_Chi_Minh"));
        if (res && res->status == 200) {
            json j = json::parse(res->body);
            ts = j.value((const char*)oxorany("unixtime"), 0);
            if (ts != 0) break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    vnBaseTime = ts;
    vnStartTime = std::chrono::system_clock::now();
    std::tm tmVN{};
    localtime_r(&ts, &tmVN);
    g_vnDate.day = tmVN.tm_mday;
    g_vnDate.month = tmVN.tm_mon + 1;
    g_vnDate.year = tmVN.tm_year + 1900;
    g_dateInit = true;
}

inline std::tm getCurrentVnTime() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - vnStartTime).count();
    time_t current = vnBaseTime + elapsed;
    std::tm tmVN{};
    localtime_r(&current, &tmVN);
    if (g_dateInit && tmVN.tm_mday != g_vnDate.day) {
        g_vnDate.day = tmVN.tm_mday;
        g_vnDate.month = tmVN.tm_mon + 1;
        g_vnDate.year = tmVN.tm_year + 1900;
    }
    return tmVN;
}

struct VnDateTime {
    int year, month, day;
    int hour, minute, second;
};

inline VnDateTime getCurrentVnDateTime() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - vnStartTime).count();
    time_t current = vnBaseTime + elapsed;
    std::tm tmVN{};
    localtime_r(&current, &tmVN);
    if (g_dateInit && tmVN.tm_mday != g_vnDate.day) {
        g_vnDate.day = tmVN.tm_mday;
        g_vnDate.month = tmVN.tm_mon + 1;
        g_vnDate.year = tmVN.tm_year + 1900;
    }
    VnDateTime vn;
    vn.year = tmVN.tm_year + 1900;
    vn.month = tmVN.tm_mon + 1;
    vn.day = tmVN.tm_mday;
    vn.hour = tmVN.tm_hour;
    vn.minute = tmVN.tm_min;
    vn.second = tmVN.tm_sec;
    return vn;
}

inline void checkAutoLoginAt7h() {
    std::tm currentTime = getCurrentVnTime();
    if (currentTime.tm_hour == 7 && currentTime.tm_min == 0) {
        if (!alreadySyncedAt7h) {
            while (true) {
                err = Login(g_s);
                if (err == std::string(oxorany("OK"))) {
                    isLogin          = bValid;
                    saveKey();
                    showLoginSuccess = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            alreadySyncedAt7h = true;
        }
    } else {
        alreadySyncedAt7h = false;
    }
}