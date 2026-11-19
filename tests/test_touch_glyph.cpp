// SPDX-License-Identifier: GPL-3.0-or-later
// test-links:
// test-includes: apps/d2d
#include "touch_glyph.hpp"

#include <array>
#include <cassert>

int main() {
    using namespace d2d::client::touch;
    constexpr std::array glyphs{ Glyph::Inventory, Glyph::Skills, Glyph::Quest,
                                 Glyph::Bind, Glyph::Map, Glyph::More, Glyph::Hand, Glyph::Talk,
                                 Glyph::Back, Glyph::Character, Glyph::Belt };
    std::array<GlyphShape, glyphs.size()> shapes;
    for (std::size_t index = 0; index < glyphs.size(); ++index) {
        shapes[index] = glyph_shape(glyphs[index]);
        const auto& shape = shapes[index];
        assert(!shape.strokes.empty() || !shape.dots.empty());
        for (const auto& segment : shape.strokes) {
            for (const auto point : { segment.from, segment.to }) {
                assert(point.x >= 0.f && point.x <= 1.f);
                assert(point.y >= 0.f && point.y <= 1.f);
            }
        }
        for (const auto point : shape.dots) {
            assert(point.x >= 0.f && point.x <= 1.f);
            assert(point.y >= 0.f && point.y <= 1.f);
        }
        for (std::size_t prior = 0; prior < index; ++prior)
            assert(shape.strokes != shapes[prior].strokes || shape.dots != shapes[prior].dots);
    }
    assert(shapes[0].strokes.size() == 8);  // inventory bag
    assert(shapes[1].strokes.size() == 19); // book and star
    assert(shapes[2].strokes.size() == 14); // scroll
    assert(shapes[3].strokes.size() == 17); // chain links
    assert(shapes[4].strokes.size() == 16); // folded map
    assert(shapes[5].strokes.empty() && shapes[5].dots.size() == 3);
    assert(shapes[10].strokes.size() == 16); // four belt slots
}
