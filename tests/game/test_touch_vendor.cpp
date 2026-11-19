// SPDX-License-Identifier: GPL-3.0-or-later
// test-links: dc6 dcc font game palette
// test-includes: apps/d2d
#include "store.hpp"
#include "touch_input.hpp"

#include <d2s_items.hpp>
#include <replication.hpp>
#include <rules.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

using namespace d2d::client;
using namespace d2d::client::touch;
using d2d::d2s::Item;

int main() {
    d2d::rules::Tables rules;
    rules.item_info["cap"] = { .width = 2, .height = 2 };
    rules.item_info["hax"] = { .width = 2, .height = 3 };
    rules.item_info["ssd"] = { .width = 1, .height = 3 };
    rules.item_base["hp1"] = { .cost = 30 };
    rules.item_base["tsc"] = { .cost = 150 };

    Store shop;
    shop.npc = 2;
    shop.npc_id = "Akara";
    const std::array codes{ "cap", "hax", "ssd", "hp1" };
    for (std::size_t page = 0; page < codes.size(); ++page) {
        const Item item{ .code = codes[page] };
        assert(d2d::rules::store_place(rules, shop, int(page), item));
    }
    assert(d2d::rules::store_place(rules, shop, 3, Item{ .code = "tsc" }));
    int next_id = -2;
    shop.assign_stock_ids(next_id);

    viewport_width = 1080.f;
    for (int page = 0; page < 4; ++page) {
        const int viewport_tab_x = int(to_viewport_x(120.f + 80.f * float(page)));
        shop.tab = store_tab_at(int(to_legacy_x(float(viewport_tab_x))), 75);
        assert(shop.tab == page);
        const float item_x = page == 3 ? 371.f : 110.f;
        assert(store_item_at(rules, shop, int(to_legacy_x(to_viewport_x(item_x))), 137) == 0);
        assert(store_item_at(rules, shop, int(item_x) + 320, 137, 400) == 0);
        assert(store_item_at(rules, shop, int(item_x) + 80, 137) == -1);
    }

    const auto scroll = StoreSelection::capture(shop, store_item_at(rules, shop, 371, 166));
    assert(scroll.index(shop) == 1);
    const auto potion = StoreSelection::capture(shop, store_item_at(rules, shop, 371, 137));
    assert(potion.id < -1 && potion.id != scroll.id);
    d2d::d2s::Stats stats;
    stats.values[d2d::d2s::kGold] = 500;
    std::vector<Item> carried;
    assert(d2d::rules::store_buy(rules, shop, potion.index(shop), carried, stats) == d2d::rules::Trade::done);
    assert(carried.size() == 1 && carried.back().code == "hp1" && stats.get(d2d::d2s::kGold) == 470);
    assert(potion.index(shop) == -1 && scroll.index(shop) == 0);

    const d2d::d2s::ItemTables item_tables{ .compact = { "tsc" } };
    d2d::game::wire::Out out;
    d2d::game::wire::items(out, shop.tabs[3], item_tables);
    d2d::game::wire::In in{ out.bytes, 0 };
    shop.tabs[3] = d2d::game::wire::items(in, item_tables);
    assert(in.ok && scroll.index(shop) == 0);
    shop.tab = 2;
    assert(scroll.index(shop) == -1);
    shop.tab = 3;
    ++shop.npc;
    assert(scroll.index(shop) == -1);
    --shop.npc;
    shop.tabs[3].clear();
    assert(d2d::rules::store_place(rules, shop, 3, Item{ .code = "tsc" }));
    shop.assign_stock_ids(next_id);
    assert(scroll.index(shop) == -1); // same vendor, code and tab after refill

    for (const bool buy : { true, false }) {
        auto sheet = make_item_sheet(false, !buy, buy);
        const auto& button = buy ? sheet.buy : sheet.sell;
        const float x = button.x + 1.f, y = button.y + 1.f;
        auto action = ItemAction::None;
        assert(sheet.open && !sheet.show_confirm());
        assert(sheet.hit(sheet.cancel.x + 1.f, sheet.cancel.y + 1.f) == ItemAction::Cancel);
        assert(sheet.trade_event(2, Phase::Down, x, y, action) && action == ItemAction::None);
        assert(sheet.trade_event(3, Phase::Up, x, y, action) && action == ItemAction::None);
        assert(sheet.trade_event(2, Phase::Cancel, x, y, action) && action == ItemAction::None);
        assert(!sheet.trade_event(2, Phase::Up, x, y, action) && action == ItemAction::None);
        assert(sheet.trade_event(2, Phase::Down, x, y, action));
        assert(sheet.trade_event(2, Phase::Up, 0.f, 0.f, action) && action == ItemAction::None);
        assert(sheet.trade_event(-2, Phase::Down, x, y, action));
        assert(sheet.trade_event(-2, Phase::Up, x, y, action) && action == (buy ? ItemAction::Buy : ItemAction::Sell));
        action = ItemAction::None;
        assert(!sheet.trade_event(-2, Phase::Up, x, y, action) && action == ItemAction::None);
    }
}
