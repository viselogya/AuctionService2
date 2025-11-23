#include "auction/core/http_client.h"

#include <stdexcept>

#include <curl/curl.h>

namespace {

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* responseBody = static_cast<std::string*>(userdata);
  responseBody->append(ptr, size * nmemb);
  return size * nmemb;
}

}  // namespace

namespace auction::core {

HttpResponse HttpClient::postJson(const std::string& url, const nlohmann::json& payload,
                                  const std::vector<std::string>& headers) const {
  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize CURL");
  }

  std::string responseBody;
  HttpResponse response;
  struct curl_slist* headerList = nullptr;
  headerList = curl_slist_append(headerList, "Content-Type: application/json");

  for (const auto& header : headers) {
    headerList = curl_slist_append(headerList, header.c_str());
  }

  const std::string payloadStr = payload.dump();

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payloadStr.size());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

  CURLcode result = curl_easy_perform(curl);
  if (result != CURLE_OK) {
    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    throw std::runtime_error(std::string{"HTTP request failed: "} + curl_easy_strerror(result));
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
  response.body = std::move(responseBody);

  curl_slist_free_all(headerList);
  curl_easy_cleanup(curl);

  return response;
}

}  // namespace auction::core

