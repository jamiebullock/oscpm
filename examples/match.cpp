// Matches a pattern against addresses given on the command line.
//
//   oscpm_example_match '/synth/*/{freq,amp}' /synth/1/freq /synth/1/pan

#include <oscpm/oscpm.hpp>

#include <cstdio>

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s PATTERN ADDRESS...\n", argv[0]);
        return 2;
    }

    const auto pattern = oscpm::Pattern::compile(argv[1]);
    if (!pattern) {
        const oscpm::Error error = oscpm::validate_pattern(argv[1]);
        std::fprintf(stderr, "invalid pattern: %s at offset %zu\n", error.message(), error.position);
        return 1;
    }

    for (int i = 2; i < argc; ++i)
        std::printf("%-8s %s\n", pattern->matches(argv[i]) ? "match" : "no", argv[i]);
    return 0;
}
