#pragma once

#include <string>

namespace httplib {
class Server;
}

namespace autowhisper::settings {

// Generates a 64-char hex session token for the local settings server.
std::string generate_session_token();

// True when `host_header` (e.g. "127.0.0.1:8080", "localhost") refers to a
// loopback origin. Rejecting other hosts defends against DNS-rebinding
// attacks on the localhost server.
bool is_loopback_host(const std::string& host_header);

// True when the request carries the session token, either as
// "Authorization: Bearer <token>" or as a "?token=" query parameter.
bool token_matches(const std::string& expected,
                   const std::string& authorization_header,
                   const std::string& token_query_param);

// Registers the JSON API routes plus the security gate shared by the
// production server and the tests:
//   - every request must come from a loopback Host (403 otherwise)
//   - every /api/* request must carry `token` (401 otherwise)
// Throws std::invalid_argument when `token` is empty so an unauthenticated
// server cannot be started by accident.
void attach_api_routes(httplib::Server& srv,
                       const std::string& config_path,
                       const std::string& token);

}  // namespace autowhisper::settings
