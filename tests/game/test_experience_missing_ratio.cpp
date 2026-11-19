// SPDX-License-Identifier: GPL-3.0-or-later
// test-links: game
// Missing CD-era ratios keep XP; an explicit zero still awards zero.
#include <gamedata_load.hpp>
#include <txt.hpp>

#include <cassert>
#include <cstddef>
#include <span>
#include <string_view>

int main() {
    using namespace std::literals;
    const auto parse = [](std::string_view text) {
        return d2d::txt::Table(std::as_bytes(std::span(text.data(), text.size())));
    };
    d2d::game::GameData data;
    d2d::game::load_experience_table(data, parse("Level\tAmazon\tExpRatio\n1\t0\t\n2\t500\t0\n3\t1500\t512\nMaxLvl\t99\t10\n"sv));
    assert(data.exp_ratio.at(1) == 1024);
    assert(data.exp_ratio.at(2) == 0);
    assert(data.exp_ratio.at(3) == 512);
    assert(data.exp_next.at(2) == 500);
    d2d::game::GameData classic;
    d2d::game::load_experience_table(classic, parse("Level\tAmazon\n1\t0\n2\t500\nMaxLvl\t99\n"sv));
    assert(classic.exp_ratio.at(2) == (1 << classic.exp_ratio_shift));
}
