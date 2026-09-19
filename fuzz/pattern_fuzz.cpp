// libFuzzer entry for the matcher: Pattern::parse, matches, match and
// validateAddress over arbitrary bytes. The input is a Pattern and an
// Address separated by the first newline; with no newline the whole input
// is the Pattern and the Address is empty. fuzz/seed_corpus.cmake writes
// the conformance corpus in this format, so a crash reproducer reads as a
// corpus case does.
//
// Besides crashes and sanitizer reports, the entry aborts when the API
// contradicts itself: the two ways of matching must agree, Malformed must
// coincide with what validateAddress reports, a reported offset must point
// at a byte of the input, and a well-formed Address, which is also a
// Pattern with no Wildcards, must parse and match itself unless one of its
// Parts is longer than a Pattern Part may be (ADR 0004), the only rule
// that binds a Pattern and not an Address.

#include <oscpm/pattern.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace {

void require(bool condition) noexcept
{
    if (!condition) {
        std::abort();
    }
}

// The offset of an Error points at a byte of `text`, except that an empty
// input faults at 0.
bool pointsInto(const oscpm::Error& error, std::string_view text) noexcept
{
    return error.offset < text.size() || (text.empty() && error.offset == 0);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    const std::size_t newline = input.find('\n');
    const std::string_view pattern = input.substr(0, newline);
    const std::string_view address =
        newline == std::string_view::npos ? std::string_view() : input.substr(newline + 1);

    const oscpm::ParseResult parsed = oscpm::Pattern::parse(pattern);
    const std::optional<oscpm::Error> addressError = oscpm::validateAddress(address);
    const oscpm::MatchResult convenience = oscpm::match(pattern, address);

    if (!parsed.ok()) {
        require(pointsInto(parsed.error(), pattern));
        require(convenience == oscpm::MatchResult::Malformed);
    } else {
        const oscpm::MatchResult result = parsed.pattern().matches(address);
        require(result == convenience);
        require((result == oscpm::MatchResult::Malformed) == addressError.has_value());
    }

    if (addressError) {
        require(pointsInto(*addressError, address));
    } else {
        const oscpm::ParseResult self = oscpm::Pattern::parse(address);
        if (self.ok()) {
            require(self.pattern().matches(address) == oscpm::MatchResult::Match);
        } else {
            require(self.error().kind == oscpm::ErrorKind::PartTooLong);
        }
    }
    return 0;
}
