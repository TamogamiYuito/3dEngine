#pragma once

#include <Siv3D.hpp>
#include <string>

struct AiResponse {
    bool ok = false;
    s3d::String command;
    s3d::String reply;
    s3d::String error;
};

AiResponse RequestAiResponse(const s3d::String& prompt, const std::string& apiKey);
