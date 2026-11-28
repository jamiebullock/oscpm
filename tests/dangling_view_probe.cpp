/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include <string>
#include <string_view>

struct Holder
{
    explicit Holder(std::string_view text [[clang::lifetimebound]])
        : m_text(text)
    {
    }

    std::string_view m_text;
};

int main()
{
    const Holder holder(std::string("text"));
    return holder.m_text.empty() ? 1 : 0;
}
