#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "Includes/Helper.h"
#include "Global.h"

static const unsigned char AES_SECRET_KEY[32] = {0x13, 0x2A, 0xB4, 0x55, 0x88, 0x0F, 0xAA, 0x3E, 0x17, 0x26, 0x9B, 0xCD, 0xEF, 0x1D, 0x23, 0x47, 0x59, 0x68, 0x70, 0x81, 0x9A, 0xAB, 0xBC, 0xCD, 0xDE, 0xEF, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6}; // This is the private key that can be changed to your own key.
static const size_t AES_IV_SIZE = 16;

std::vector<unsigned char> AES_Encrypt(const std::string& plaintext) {
    std::vector<unsigned char> iv(AES_IV_SIZE);
    RAND_bytes(iv.data(), AES_IV_SIZE);
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, AES_SECRET_KEY, iv.data());
    std::vector<unsigned char> ciphertext(plaintext.size() + AES_BLOCK_SIZE);
    int len, ciphertext_len;
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size());
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    ciphertext.insert(ciphertext.begin(), iv.begin(), iv.end());
    ciphertext.resize(AES_IV_SIZE + ciphertext_len);
    return ciphertext;
}

std::string AES_Decrypt(const std::vector<unsigned char>& encData) {
    if (encData.size() < AES_IV_SIZE) return {};
    std::vector<unsigned char> iv(encData.begin(), encData.begin() + AES_IV_SIZE);
    std::vector<unsigned char> ciphertext(encData.begin() + AES_IV_SIZE, encData.end());
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, AES_SECRET_KEY, iv.data());
    std::vector<unsigned char> plaintext(ciphertext.size() + AES_BLOCK_SIZE);
    int len, plaintext_len;
    EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size());
    plaintext_len = len;
    EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    return std::string(plaintext.begin(), plaintext.begin() + plaintext_len);
}

using json = nlohmann::json;
bool FoundProfile;

static bool showSaveSuccessPopup = false;
static bool showDeleteSuccessPopup = false;
static bool showDeleteConfirmPopup = false;

inline std::string getConfigPath() {
    JNIEnv* env;
    jvm->AttachCurrentThread(&env, nullptr);
    jobject context = getApplicationContext();
    std::string package = getPackageName(env, context);
    std::string path = std::string(oxorany("/storage/emulated/0/Android/data/")) + package + oxorany("/user_config");
    jvm->DetachCurrentThread();
    return path;
}

void DeleteFile(std::string FilePath) {
    remove(FilePath.c_str());
}

/*
 * To add a new variable, do the same thing in both functions below.
 * Example: config["MyBool"] = MyBool;   (in SaveConfig)
 *          MyBool = config.value("MyBool", MyBool);  (in Load_Profile)
 */

void SaveConfig(const std::string& FilePath) {
    json config;

    /* Save your variables here */
    config[static_cast<const char*>(OBFUSCATE("Volume"))] = Volume;
    /* config[static_cast<const char*>(OBFUSCATE("MyBool"))] = MyBool; */

    std::string jsonData = config.dump();
    auto encrypted = AES_Encrypt(jsonData);
    std::ofstream file(FilePath, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
        file.close();
    }
}

void Load_Profile(const std::string& FilePath) {
    std::ifstream file(FilePath, std::ios::binary);
    if (!file.is_open()) return;
    std::vector<unsigned char> encData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    if (encData.empty()) return;
    std::string decrypted = AES_Decrypt(encData);
    if (decrypted.empty()) return;
    json config = json::parse(decrypted, nullptr, false);
    if (config.is_discarded()) return;

    /* Load your variables here, second argument is the default fallback value */
    Volume = config.value(static_cast<const char*>(OBFUSCATE("Volume")), Volume);
    /* MyBool = config.value(static_cast<const char*>(OBFUSCATE("MyBool")), MyBool); */
}

bool Check_File(std::string FilePath) {
    std::ifstream fileStream(FilePath.c_str());
    bool isAlive = fileStream.good();
    fileStream.close();
    return isAlive;
}

void SaveLoad_GUI() {
    static constexpr float kPopupButtonWidth = 120.0f;
    if (FoundProfile) {
        ImGui::Text(static_cast<const char*>(OBFUSCATE("A saved menu already exists")));
    } else {
        ImGui::Text(static_cast<const char*>(OBFUSCATE("No menu is currently saved")));
    }
    if (ImGui::Button(static_cast<const char*>(OBFUSCATE("Save Menu")), ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        SaveConfig(getConfigPath());
        FoundProfile = true;
        showSaveSuccessPopup = true;
    }
    if (!FoundProfile) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    if (ImGui::Button(static_cast<const char*>(OBFUSCATE("Delete Menu")), ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        showDeleteConfirmPopup = true;
    }
    if (!FoundProfile) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
    if (showSaveSuccessPopup) {
        ImGui::OpenPopup(static_cast<const char*>(OBFUSCATE("Success")));
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal(OBFUSCATE("Success"), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            const char* text = OBFUSCATE("Menu saved successfully!");
            float textWidth = ImGui::CalcTextSize(text).x;
            float windowWidth = ImGui::GetWindowSize().x;
            ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
            ImGui::Text("%s", text);
            ImGui::Spacing();
            float buttonWidth = kPopupButtonWidth;
            ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
            if (ImGui::Button(OBFUSCATE("OK"), ImVec2(buttonWidth, 0))) {
                showSaveSuccessPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    if (showDeleteConfirmPopup) {
        ImGui::OpenPopup(OBFUSCATE("Confirm"));
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal(OBFUSCATE("Confirm"), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            const char* text = OBFUSCATE("Are you sure you want to delete the current menu?");
            float textWidth = ImGui::CalcTextSize(text).x;
            float windowWidth = ImGui::GetWindowSize().x;
            ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
            ImGui::Text("%s", text);
            ImGui::Spacing();
            float buttonWidth = kPopupButtonWidth;
            float totalButtonWidth = buttonWidth * 2 + ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetCursorPosX((windowWidth - totalButtonWidth) * 0.5f);
            if (ImGui::Button(OBFUSCATE("Yes"), ImVec2(buttonWidth, 0))) {
                DeleteFile(getConfigPath());
                FoundProfile = false;
                showDeleteConfirmPopup = false;
                showDeleteSuccessPopup = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(OBFUSCATE("No"), ImVec2(buttonWidth, 0))) {
                showDeleteConfirmPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    if (showDeleteSuccessPopup) {
        ImGui::OpenPopup(OBFUSCATE("Deleted"));
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal(OBFUSCATE("Deleted"), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            const char* text = OBFUSCATE("Menu deleted successfully!");
            float textWidth = ImGui::CalcTextSize(text).x;
            float windowWidth = ImGui::GetWindowSize().x;
            ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
            ImGui::Text("%s", text);
            ImGui::Spacing();
            float buttonWidth = kPopupButtonWidth;
            ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
            if (ImGui::Button(OBFUSCATE("OK"), ImVec2(buttonWidth, 0))) {
                showDeleteSuccessPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

void LoadSaveLoadMenu() {
    std::string path = getConfigPath();
    if (Check_File(path)) {
        FoundProfile = true;
        Load_Profile(path);
    }
}
