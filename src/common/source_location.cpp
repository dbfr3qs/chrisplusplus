#include "common/source_location.h"
#include <sstream>

namespace chris {

std::string SourceLocation::toString() const {
    std::ostringstream oss;
    oss << file << ":" << line << ":" << column;
    return oss.str();
}

} // namespace chris
