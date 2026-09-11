#ifndef TEZ_DIAGNOSTICS_HPP
#define TEZ_DIAGNOSTICS_HPP

#include <nlohmann/json.hpp>
#include <string>

// Paths and invalid configuration keys can contain terminal control bytes.
inline std::string quote_diagnostic(const std::string &value) {
    return nlohmann::json(value).dump(-1, ' ', true, nlohmann::json::error_handler_t::replace);
}

#endif
