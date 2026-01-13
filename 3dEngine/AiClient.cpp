#include "AiClient.hpp"

#include <curl/curl.h>
#include <sstream>

namespace {
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), total);
    return total;
}

std::string EscapeJsonString(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '\"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

AiResponse ParseAiResponse(const std::string& body) {
    AiResponse response;
    s3d::JSON json = s3d::JSON::Parse(Unicode::FromUTF8(body));
    if (!json) {
        response.error = U"AI応答の解析に失敗しました。";
        return response;
    }

    if (json.contains(U"error")) {
        response.error = U"AIエラー: " + json[U"error"][U"message"].getString();
        return response;
    }

    s3d::String content;
    if (json.contains(U"choices") && json[U"choices"].isArray() && !json[U"choices"].isEmpty()) {
        const auto& first = json[U"choices"][0];
        if (first.contains(U"message")) {
            content = first[U"message"][U"content"].getString();
        } else if (first.contains(U"text")) {
            content = first[U"text"].getString();
        }
    }

    if (content.isEmpty()) {
        response.error = U"AIから空の応答が返りました。";
        return response;
    }

    s3d::JSON payload = s3d::JSON::Parse(content);
    if (!payload) {
        response.error = U"AI応答のJSON形式が不正です。";
        return response;
    }

    response.command = payload[U"command"].getString();
    response.reply = payload[U"reply"].getString();
    response.ok = !response.command.isEmpty();
    return response;
}
} // namespace

AiResponse RequestAiResponse(const s3d::String& prompt, const std::string& apiKey) {
    AiResponse response;
    if (apiKey.empty()) {
        response.error = U"AIキーが設定されていません。";
        return response;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = U"CURLの初期化に失敗しました。";
        return response;
    }

    std::string readBuffer;
    std::string promptUtf8 = Unicode::ToUTF8(prompt);
    std::string escapedPrompt = EscapeJsonString(promptUtf8);

    std::ostringstream payload;
    payload
        << "{"
        << "\"model\":\"gpt-4o-mini\","
        << "\"messages\":["
        << "{\"role\":\"system\",\"content\":\"You are a game assistant. Reply ONLY with a JSON object: "
           "{\\\"command\\\":\\\"background_blue|background_red|background_green|background_white|background_black|add_cube|add_light|reset_camera|unknown\\\","
           "\\\"reply\\\":\\\"short Japanese reply\\\"}.\"},"
        << "{\"role\":\"user\",\"content\":\"" << escapedPrompt << "\"}"
        << "]"
        << "}";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string authHeader = "Authorization: Bearer " + apiKey;
    headers = curl_slist_append(headers, authHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.str().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.error = U"AI通信に失敗しました: " + Unicode::FromUTF8(curl_easy_strerror(res));
    } else {
        response = ParseAiResponse(readBuffer);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}
