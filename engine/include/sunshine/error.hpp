#pragma once

#include <stdexcept>
#include <string>
#include <source_location>
#include <iostream>

namespace Sunshine {

class GraphicsException : public std::runtime_error {
public:
    GraphicsException(const std::string& message, 
                      const std::string& suggestion = "",
                      const std::source_location location = std::source_location::current())
        : std::runtime_error(FormatMessage(message, suggestion, location)),
          m_rawMessage(message),
          m_suggestion(suggestion),
          m_file(location.file_name()),
          m_line(location.line()),
          m_function(location.function_name()) {}

    const std::string& GetRawMessage() const noexcept { return m_rawMessage; }
    const std::string& GetSuggestion() const noexcept { return m_suggestion; }
    const char* GetFile() const noexcept { return m_file; }
    uint32_t GetLine() const noexcept { return m_line; }
    const char* GetFunction() const noexcept { return m_function; }

private:
    static std::string FormatMessage(const std::string& msg, 
                                     const std::string& suggestion, 
                                     const std::source_location& loc) {
        std::string formatted = "\n[Sunshine Graphics Exception]\n";
        formatted += "  Location:   " + std::string(loc.file_name()) + ":" + std::to_string(loc.line()) + "\n";
        formatted += "  Function:   " + std::string(loc.function_name()) + "\n";
        formatted += "  Details:    " + msg + "\n";
        if (!suggestion.empty()) {
            formatted += "  Suggestion: " + suggestion + "\n";
        }
        return formatted;
    }

    std::string m_rawMessage;
    std::string m_suggestion;
    const char* m_file;
    uint32_t m_line;
    const char* m_function;
};

#define SUNSHINE_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            throw ::Sunshine::GraphicsException( \
                std::string("Assertion failed: ") + #cond + " - " + (msg), \
                "Verify input parameters and object state before calling this method."); \
        } \
    } while(0)

#define SUNSHINE_CHECK(cond, msg, suggestion) \
    do { \
        if (!(cond)) { \
            throw ::Sunshine::GraphicsException((msg), (suggestion)); \
        } \
    } while(0)

} // namespace Sunshine
