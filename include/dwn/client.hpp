#pragma once
// Milan DwnClient example
#include <string>
namespace dwn{ class DwnClient{ public: explicit DwnClient(std::string url):baseUrl_(std::move(url)){} std::string baseUrl_; }; }
