#pragma once

/*
 *
 * login.h - Sample login / key-validation module
 *
 * This is a TEMPLATE file. You are free (and expected) to modify it based on
 * your own login API. The current implementation demonstrates a typical flow:
 *
 *   1. Build a unique hardware ID from the device's build properties.
 *   2. POST the user's key + HWID + package info to your authentication server.
 *   3. Parse the JSON response and populate the global session tokens.
 *   4. Persist the validated key to disk so subsequent launches auto-login.
 *
 * Replace the API endpoint, JSON field names, and any server-side logic to
 * match whatever authentication backend you are running.
 *
 */

#include <android/log.h>
#include <curl/curl.h>
#include <jni.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <sys/system_properties.h>

#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include "Includes/Helper.h"

using json = nlohmann::json;

/* Active session data - populated after a successful login call */
std::string g_Token;       /* Auth token returned by the server            */
std::string g_Auth;        /* The raw key the user entered                 */
std::string g_EXP;         /* Remaining days as a string (display only)    */
std::string g_ExpireDate;  /* ISO-8601 expiry timestamp from the server    */

inline bool bValid = false;       /* true when the current session is authenticated */
char        g_s[512] = {0};       /* Key input buffer - shared with the UI          */

/* Declared in Main.cpp; updated here after login succeeds / fails */
extern bool  isLogin;
extern bool  showLoginSuccess;


/* -------------------------------------------------------------------------
 * Internal libcurl write-callback - accumulates the HTTP response body into
 * a heap-allocated MemoryStruct so we can parse it as JSON afterwards.
 * ------------------------------------------------------------------------- */
struct MemoryStruct {
    char*  memory;
    size_t size;
};

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t        realsize = size * nmemb;
    MemoryStruct* mem      = (MemoryStruct*)userp;
    mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) return 0;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}


/* 
 * Device fingerprint helpers
 * These read Android system properties to build a hardware-bound ID.
 * You can add or remove properties depending on how strict you want the
 * device binding to be.
 */

/* Returns the device build ID (ro.build.id) */
inline const char* GetAndroidID() {
    static char prop_value[PROP_VALUE_MAX];
    __system_property_get(oxorany("ro.build.id"), prop_value);
    return prop_value;
}

/* Returns the device model name (ro.product.model) */
inline const char* GetDeviceModel() {
    static char prop_value[PROP_VALUE_MAX];
    __system_property_get(oxorany("ro.product.model"), prop_value);
    return prop_value;
}

/* Returns the device brand (ro.product.brand) */
inline const char* GetDeviceBrand() {
    static char prop_value[PROP_VALUE_MAX];
    __system_property_get(oxorany("ro.product.brand"), prop_value);
    return prop_value;
}

/*
 * Hashes an arbitrary string (e.g. key+model+brand) into a hex string that
 * acts as the unique device identifier sent to the server.
 * Change the hashing strategy here if you need a stronger fingerprint.
 */
inline const char* GetDeviceUniqueIdentifier(const std::string& uuid) {
    static std::string formattedUuid;
    std::stringstream  ss;
    for (unsigned char c : uuid)
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    formattedUuid = ss.str();
    return formattedUuid.c_str();
}


/* 
 * Expiry / time-remaining utilities
 * Used in the About tab to display how long the key is still valid.
 *  */

struct TimeRemaining {
    int  days;
    int  hours;
    int  minutes;
    int  seconds;
    bool expired;
};

/* Parses an ISO-8601 datetime string (YYYY-MM-DDTHH:MM:SS) into std::tm */
inline std::tm parseISO8601(const std::string& dateStr) {
    std::tm tm = {};
    int year, month, day, hour, min, sec;
    if (sscanf(dateStr.c_str(),
               OBFUSCATE("%d-%d-%dT%d:%d:%d"),
               &year, &month, &day, &hour, &min, &sec) == 6) {
        tm.tm_year  = year - 1900;
        tm.tm_mon   = month - 1;
        tm.tm_mday  = day;
        tm.tm_hour  = hour;
        tm.tm_min   = min;
        tm.tm_sec   = sec;
        tm.tm_isdst = -1;
    }
    return tm;
}

/*
 * Computes days / hours / minutes / seconds remaining until the key expires.
 * Sets bValid = false and result.expired = true if the key has already lapsed.
 */
inline TimeRemaining calculateTimeRemaining(const std::string& expireDateStr) {
    TimeRemaining result = {0, 0, 0, 0, false};
    if (expireDateStr.empty()) {
        result.expired = true;
        return result;
    }
    try {
        std::tm  expireTm   = parseISO8601(expireDateStr);
        time_t   expireTime = mktime(&expireTm);
        time_t   now        = time(nullptr);
        double   diff       = difftime(expireTime, now);

        if (diff <= 0) {
            result.expired = true;
            bValid         = false;
            return result;
        }

        result.days    = (int)(diff / 86400); diff -= result.days    * 86400;
        result.hours   = (int)(diff / 3600);  diff -= result.hours   * 3600;
        result.minutes = (int)(diff / 60);
        result.seconds = (int)(diff - result.minutes * 60);
        result.expired = false;
    } catch (...) {
        result.expired = true;
        bValid         = false;
    }
    return result;
}


/* 
 * Login - sends the user's key to your authentication API
 * 
 *
 * HOW TO ADAPT THIS TO YOUR OWN API
 * ----------------------------------
 *  1. Change `api_key` to your server's endpoint URL.
 *  2. Update the POST field names in `snprintf(data, ...)` to match what
 *     your server expects (currently: auth, phienban, device, package).
 *  3. Adjust the JSON field names in the parser block:
 *       "ketqua"      -> your success/failure indicator field
 *       "thanhcong"   -> the value that means "success"
 *       "hansudung"   -> days remaining
 *       "auth"        -> session token
 *       "expire_date" -> ISO-8601 expiry timestamp
 *       "mes"         -> error message on failure
 *  4. If your server uses Bearer tokens instead of POST fields, replace
 *     CURLOPT_POSTFIELDS with an Authorization header.
 *
 * Returns "OK" on success, or an error string that is shown in the UI.
 *  */
inline std::string Login(const char* user_key) {

    /* Attach JNI so we can call Android APIs from this thread */
    JNIEnv* env;
    jvm->AttachCurrentThread(&env, nullptr);

    /* Some Android HTTP stacks require a Looper on the calling thread */
    auto looperClass   = env->FindClass(oxorany("android/os/Looper"));
    auto prepareMethod = env->GetStaticMethodID(looperClass, oxorany("prepare"), oxorany("()V"));
    env->CallStaticVoidMethod(looperClass, prepareMethod);

    /* Retrieve the Application context to read package name / version */
    jclass   activityThreadClass             = env->FindClass(oxorany("android/app/ActivityThread"));
    jfieldID sCurrentActivityThreadField     = env->GetStaticFieldID(activityThreadClass,
        oxorany("sCurrentActivityThread"), oxorany("Landroid/app/ActivityThread;"));
    jobject  sCurrentActivityThread          = env->GetStaticObjectField(activityThreadClass,
        sCurrentActivityThreadField);
    jfieldID mInitialApplicationField        = env->GetFieldID(activityThreadClass,
        oxorany("mInitialApplication"), oxorany("Landroid/app/Application;"));
    jobject  mInitialApplication             = env->GetObjectField(sCurrentActivityThread,
        mInitialApplicationField);

    /* Build the device fingerprint: key + build ID + model + brand, then hex-encoded */
    std::string hwid = std::string(user_key)
                     + GetAndroidID()
                     + GetDeviceModel()
                     + GetDeviceBrand();
    std::string UUID = GetDeviceUniqueIdentifier(hwid);

    std::string  errMsg;
    MemoryStruct chunk{};
    chunk.memory = (char*)malloc(1);
    chunk.size   = 0;

    CURL*     curl = curl_easy_init();
    CURLcode  res;

    if (curl) {
        /* ----------------------------------------------------------------
         * CHANGE THIS to your own API endpoint
         * ---------------------------------------------------------------- */
        std::string api_key = oxorany("https://your-domain/auth-login-java.php");

        std::string packageName = getPackageName(env, mInitialApplication);
        std::string appVersion  = getVersion(env, mInitialApplication);

        curl_easy_setopt(curl, CURLOPT_URL,              api_key.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,   1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, oxorany("https"));

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers,
            oxorany("Content-Type: application/x-www-form-urlencoded"));
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        /* ----------------------------------------------------------------
         * Build POST body - adjust field names to match your server
         * ---------------------------------------------------------------- */
        char data[1024];
        snprintf(data, sizeof(data),
            oxorany("auth=%s&phienban=%s&device=%s&package=%s"),
            user_key, appVersion.c_str(), UUID.c_str(), packageName.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,     (void*)&chunk);

        /* Disable SSL peer/host verification - enable in production if possible */
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            try {
                /* --------------------------------------------------------
                 * Parse server response - update JSON field names here
                 * -------------------------------------------------------- */
                json result = json::parse(chunk.memory);
                std::string kq = result[std::string(oxorany("ketqua"))].get<std::string>();

                if (kq == oxorany("thanhcong")) {
                    /* Successful login - store session data */
                    int days     = result[std::string(oxorany("hansudung"))].get<int>();
                    g_EXP        = std::to_string(days);
                    g_Token      = result[std::string(oxorany("auth"))].get<std::string>();
                    g_Auth       = std::string(user_key);
                    g_ExpireDate = result[std::string(oxorany("expire_date"))].get<std::string>();
                    bValid       = true;
                } else {
                    /* Server returned a failure - show the message in the UI */
                    errMsg = result[std::string(oxorany("mes"))].get<std::string>();
                }
            } catch (json::exception& e) {
                /* JSON parse error - include raw response for easier debugging */
                errMsg  = "{";
                errMsg += e.what();
                errMsg += "}\n{";
                errMsg += chunk.memory;
                errMsg += "}";
            }
        } else {
            /* Network / curl error */
            errMsg = curl_easy_strerror(res);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    free(chunk.memory);
    jvm->DetachCurrentThread();

    return bValid ? oxorany("OK") : errMsg;
}


/* 
 * Key persistence - save and load the validated key from local storage
 * 
 * The key is stored as JSON at:
 *   /storage/emulated/0/Android/data/<package>/files/key.txt
 *
 * You can encrypt this file if you want an extra layer of protection,
 * but obfuscation alone is usually enough for a mod menu context.
 */

/* Returns the full path to the key storage file for the given package */
inline std::string getKeyFilePath(JNIEnv* env, jobject context) {
    std::string package = getPackageName(env, context);
    return oxorany("/storage/emulated/0/Android/data/") + package + oxorany("/files/key.txt");
}

/* Writes the current value of g_s (the key buffer) to disk as JSON */
inline void saveKey(JNIEnv* env, jobject context) {
    std::string     path = getKeyFilePath(env, context);
    nlohmann::json  configJson;
    configJson[oxorany("Key")] = g_s;
    std::ofstream file(path);
    if (!file.is_open()) return;
    file << std::setw(4) << configJson << std::endl;
    file.close();
}

/* Reads the saved key from disk and copies it into g_s */
inline void loadKey(JNIEnv* env, jobject context) {
    std::string   path = getKeyFilePath(env, context);
    std::ifstream file(path);
    if (!file.is_open()) return;
    nlohmann::json configJson;
    file >> configJson;
    file.close();
    std::string savedKey = configJson.value(oxorany("Key"), "");
    strncpy(g_s, savedKey.c_str(), sizeof(g_s) - 1);
    g_s[sizeof(g_s) - 1] = '\0';
}

/* Convenience wrappers - attach JNI context automatically */
inline void saveKey() {
    JNIEnv*  env;
    jvm->AttachCurrentThread(&env, nullptr);
    jobject  context = getApplicationContext();
    saveKey(env, context);
    jvm->DetachCurrentThread();
}

inline void loadKey() {
    JNIEnv*  env;
    jvm->AttachCurrentThread(&env, nullptr);
    jobject  context = getApplicationContext();
    loadKey(env, context);
    jvm->DetachCurrentThread();
}
